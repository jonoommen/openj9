/*******************************************************************************
 * Copyright (c) 2017, 2021 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "AllocateInitialization.hpp"
#include "EnvironmentBase.hpp"
#include "IndexableObjectAllocationModel.hpp"
#include "MixedObjectAllocationModel.hpp"
#include "ObjectModel.hpp"

omrobjectptr_t
GC_ObjectModelDelegate::initializeAllocation(MM_EnvironmentBase *env, void *allocatedBytes, MM_AllocateInitialization *allocateInitialization)
{
	omrobjectptr_t objectPtr = NULL;

	switch (allocateInitialization->getAllocationCategory()) {
	case MM_JavaObjectAllocationModel::allocation_category_mixed: {
		MM_MixedObjectAllocationModel *mixedObjectAllocationModel = (MM_MixedObjectAllocationModel *)allocateInitialization;
		objectPtr = mixedObjectAllocationModel->initializeMixedObject(env, allocatedBytes);
		break;
	}
	case MM_JavaObjectAllocationModel::allocation_category_indexable: {
		MM_IndexableObjectAllocationModel *indexableObjectAllocationModel = (MM_IndexableObjectAllocationModel *)allocateInitialization;
		objectPtr = (omrobjectptr_t)indexableObjectAllocationModel->initializeIndexableObject(env, allocatedBytes);
		break;
	}
	default:
		Assert_MM_unreachable();
		break;
	}

	return objectPtr;
}

#if defined(OMR_GC_MODRON_SCAVENGER)
bool
GC_ObjectModelDelegate::calculateObjectDetailsForCopy(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, uintptr_t *objectCopySizeInBytes, uintptr_t *reservedObjectSizeInBytes, uintptr_t *hotFieldAlignmentDescriptor)
{
	bool isIndexable = calculateObjectDetailsForCopy(env, forwardedHeader, objectCopySizeInBytes, reservedObjectSizeInBytes);
	J9Class* clazz = env->getExtensions()->objectModel.getPreservedClass(forwardedHeader);
	*hotFieldAlignmentDescriptor = clazz->instanceHotFieldDescription;
	return isIndexable;
}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

bool
GC_ObjectModelDelegate::calculateObjectDetailsForCopy(MM_EnvironmentBase *env, MM_ForwardedHeader* forwardedHeader, uintptr_t *objectCopySizeInBytes, uintptr_t *objectReserveSizeInBytes)
{
	/* NOTE: the size is fetched by hand from the class in the mixed case because a forwarding pointer could have been substituted into the clazz slot.
	 * the class pointer passed into this routine is guaranteed to have been checked
	 */
	GC_ObjectModel *objectModel = &env->getExtensions()->objectModel;
	J9Class* clazz = objectModel->getPreservedClass(forwardedHeader);
	uintptr_t actualObjectCopySizeInBytes = 0;
	uintptr_t hashcodeOffset = 0;
	bool isIndexable = false;

	if (objectModel->isIndexable(clazz)) {
		*objectCopySizeInBytes = env->getExtensions()->indexableObjectModel.calculateObjectSizeAndHashcode(forwardedHeader, &hashcodeOffset);
		isIndexable = true;
	} else {
		*objectCopySizeInBytes = clazz->totalInstanceSize + J9GC_OBJECT_HEADER_SIZE(env->getExtensions());
		hashcodeOffset = env->getExtensions()->mixedObjectModel.getHashcodeOffset(clazz);
	}

	/* IF the object has been hashed and has not been moved, then we need generate hash from the old address */
	uintptr_t forwardedHeaderPreservedFlags = objectModel->getPreservedFlags(forwardedHeader);

	if (hashcodeOffset == *objectCopySizeInBytes) {
		if (objectModel->hasBeenMoved(forwardedHeaderPreservedFlags)) {
			*objectCopySizeInBytes += sizeof(uintptr_t);
		} else if (objectModel->hasBeenHashed(forwardedHeaderPreservedFlags)) {
			actualObjectCopySizeInBytes += sizeof(uintptr_t);
		}
	}
	actualObjectCopySizeInBytes += *objectCopySizeInBytes;
	*objectReserveSizeInBytes = objectModel->adjustSizeInBytes(actualObjectCopySizeInBytes);
	return isIndexable;
}
