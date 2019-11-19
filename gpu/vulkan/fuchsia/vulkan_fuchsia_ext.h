// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header defines Fuchsia-specific Vulkan extensions that haven't been
// upstreamed to the official Vulkan headers.

#ifndef GPU_VULKAN_FUCHSIA_VULKAN_FUCHSIA_EXT_H_
#define GPU_VULKAN_FUCHSIA_VULKAN_FUCHSIA_EXT_H_

#include <vulkan/vulkan.h>
#include <zircon/types.h>

#ifdef __cplusplus
extern "C" {
#endif

const VkStructureType
    VK_STRUCTURE_TYPE_TEMP_IMPORT_MEMORY_ZIRCON_HANDLE_INFO_FUCHSIA =
        static_cast<VkStructureType>(1001005000);
const VkStructureType
    VK_STRUCTURE_TYPE_TEMP_MEMORY_ZIRCON_HANDLE_PROPERTIES_FUCHSIA =
        static_cast<VkStructureType>(1001005001);
const VkStructureType
    VK_STRUCTURE_TYPE_TEMP_MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA =
        static_cast<VkStructureType>(1001005002);
const VkStructureType
    VK_STRUCTURE_TYPE_TEMP_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA =
        static_cast<VkStructureType>(1001006000);
const VkStructureType
    VK_STRUCTURE_TYPE_TEMP_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA =
        static_cast<VkStructureType>(1001006001);
const VkStructureType VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CREATE_INFO_FUCHSIA =
    static_cast<VkStructureType>(1001004000);
const VkStructureType VK_STRUCTURE_TYPE_FUCHSIA_IMAGE_FORMAT_FUCHSIA =
    static_cast<VkStructureType>(1001004001);
const VkStructureType
    VK_STRUCTURE_TYPE_IMPORT_MEMORY_BUFFER_COLLECTION_FUCHSIA =
        static_cast<VkStructureType>(1001004004);
const VkStructureType
    VK_STRUCTURE_TYPE_BUFFER_COLLECTION_IMAGE_CREATE_INFO_FUCHSIA =
        static_cast<VkStructureType>(1001004005);
const VkStructureType VK_STRUCTURE_TYPE_BUFFER_COLLECTION_PROPERTIES_FUCHSIA =
    static_cast<VkStructureType>(1001004006);

const VkExternalMemoryHandleTypeFlagBits
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA =
        static_cast<VkExternalMemoryHandleTypeFlagBits>(0x00100000);

const VkExternalSemaphoreHandleTypeFlagBits
    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA =
        static_cast<VkExternalSemaphoreHandleTypeFlagBits>(0x00100000);

const VkObjectType VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA =
    static_cast<VkObjectType>(1001004002);

#define VK_FUCHSIA_external_memory 1
#define VK_FUCHSIA_EXTERNAL_MEMORY_SPEC_VERSION 1
#define VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME "VK_FUCHSIA_external_memory"

typedef struct VkImportMemoryZirconHandleInfoFUCHSIA {
  VkStructureType sType;
  const void* pNext;
  VkExternalMemoryHandleTypeFlagBits handleType;
  zx_handle_t handle;
} VkImportMemoryZirconHandleInfoFUCHSIA;

typedef struct VkMemoryZirconHandlePropertiesFUCHSIA {
  VkStructureType sType;
  void* pNext;
  zx_handle_t memoryTypeBits;
} VkMemoryZirconHandlePropertiesFUCHSIA;

typedef struct VkMemoryGetZirconHandleInfoFUCHSIA {
  VkStructureType sType;
  const void* pNext;
  VkDeviceMemory memory;
  VkExternalMemoryHandleTypeFlagBits handleType;
} VkMemoryGetZirconHandleInfoFUCHSIA;

typedef VkResult(VKAPI_PTR* PFN_vkGetMemoryZirconHandleFUCHSIA)(
    VkDevice device,
    const VkMemoryGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo,
    zx_handle_t* pZirconHandle);
typedef VkResult(VKAPI_PTR* PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA)(
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBits handleType,
    zx_handle_t ZirconHandle,
    VkMemoryZirconHandlePropertiesFUCHSIA* pMemoryZirconHandleProperties);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryZirconHandleFUCHSIA(
    VkDevice device,
    const VkMemoryGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo,
    zx_handle_t* pZirconHandle);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryZirconHandlePropertiesFUCHSIA(
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBits handleType,
    zx_handle_t ZirconHandle,
    VkMemoryZirconHandlePropertiesFUCHSIA* pMemoryZirconHandleProperties);
#endif

#define VK_FUCHSIA_external_semaphore 1
#define VK_FUCHSIA_EXTERNAL_SEMAPHORE_SPEC_VERSION 1
#define VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME \
  "VK_FUCHSIA_external_semaphore"

typedef struct VkImportSemaphoreZirconHandleInfoFUCHSIA {
  VkStructureType sType;
  const void* pNext;
  VkSemaphore semaphore;
  VkSemaphoreImportFlags flags;
  VkExternalSemaphoreHandleTypeFlagBits handleType;
  zx_handle_t handle;
} VkImportSemaphoreZirconHandleInfoFUCHSIA;

typedef struct VkSemaphoreGetZirconHandleInfoFUCHSIA {
  VkStructureType sType;
  const void* pNext;
  VkSemaphore semaphore;
  VkExternalSemaphoreHandleTypeFlagBits handleType;
} VkSemaphoreGetZirconHandleInfoFUCHSIA;

typedef VkResult(VKAPI_PTR* PFN_vkImportSemaphoreZirconHandleFUCHSIA)(
    VkDevice device,
    const VkImportSemaphoreZirconHandleInfoFUCHSIA*
        pImportSemaphoreZirconHandleInfo);
typedef VkResult(VKAPI_PTR* PFN_vkGetSemaphoreZirconHandleFUCHSIA)(
    VkDevice device,
    const VkSemaphoreGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo,
    zx_handle_t* pZirconHandle);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreZirconHandleFUCHSIA(
    VkDevice device,
    const VkImportSemaphoreZirconHandleInfoFUCHSIA*
        pImportSemaphoreZirconHandleInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreZirconHandleFUCHSIA(
    VkDevice device,
    const VkSemaphoreGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo,
    zx_handle_t* pZirconHandle);
#endif

#define VK_FUCHSIA_buffer_collection 1
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkBufferCollectionFUCHSIA)

#define VK_FUCHSIA_BUFFER_COLLECTION_SPEC_VERSION 1
#define VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME \
  "VK_FUCHSIA_buffer_collection"

typedef struct VkBufferCollectionCreateInfoFUCHSIA {
  VkStructureType sType;
  const void* pNext;
  zx_handle_t collectionToken;
} VkBufferCollectionCreateInfoFUCHSIA;

typedef struct VkFuchsiaImageFormatFUCHSIA {
  VkStructureType sType;
  const void* pNext;
  const void* imageFormat;
  uint32_t imageFormatSize;
} VkFuchsiaImageFormatFUCHSIA;

typedef struct VkImportMemoryBufferCollectionFUCHSIA {
  VkStructureType sType;
  const void* pNext;
  VkBufferCollectionFUCHSIA collection;
  uint32_t index;
} VkImportMemoryBufferCollectionFUCHSIA;

typedef struct VkBufferCollectionImageCreateInfoFUCHSIA {
  VkStructureType sType;
  const void* pNext;
  VkBufferCollectionFUCHSIA collection;
  uint32_t index;
} VkBufferCollectionImageCreateInfoFUCHSIA;

typedef struct VkBufferCollectionPropertiesFUCHSIA {
  VkStructureType sType;
  void* pNext;
  uint32_t memoryTypeBits;
  uint32_t count;
} VkBufferCollectionPropertiesFUCHSIA;

typedef VkResult(VKAPI_PTR* PFN_vkCreateBufferCollectionFUCHSIA)(
    VkDevice device,
    const VkBufferCollectionCreateInfoFUCHSIA* pImportInfo,
    const VkAllocationCallbacks* pAllocator,
    VkBufferCollectionFUCHSIA* pCollection);
typedef VkResult(VKAPI_PTR* PFN_vkSetBufferCollectionConstraintsFUCHSIA)(
    VkDevice device,
    VkBufferCollectionFUCHSIA collection,
    const VkImageCreateInfo* pImageInfo);
typedef void(VKAPI_PTR* PFN_vkDestroyBufferCollectionFUCHSIA)(
    VkDevice device,
    VkBufferCollectionFUCHSIA collection,
    const VkAllocationCallbacks* pAllocator);
typedef VkResult(VKAPI_PTR* PFN_vkGetBufferCollectionPropertiesFUCHSIA)(
    VkDevice device,
    VkBufferCollectionFUCHSIA collection,
    VkBufferCollectionPropertiesFUCHSIA* pProperties);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateBufferCollectionFUCHSIA(
    VkDevice device,
    const VkBufferCollectionCreateInfoFUCHSIA* pImportInfo,
    const VkAllocationCallbacks* pAllocator,
    VkBufferCollectionFUCHSIA* pCollection);

VKAPI_ATTR VkResult VKAPI_CALL
vkSetBufferCollectionConstraintsFUCHSIA(VkDevice device,
                                        VkBufferCollectionFUCHSIA collection,
                                        const VkImageCreateInfo* pImageInfo);

VKAPI_ATTR void VKAPI_CALL
vkDestroyBufferCollectionFUCHSIA(VkDevice device,
                                 VkBufferCollectionFUCHSIA collection,
                                 const VkAllocationCallbacks* pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkGetBufferCollectionPropertiesFUCHSIA(
    VkDevice device,
    VkBufferCollectionFUCHSIA collection,
    VkBufferCollectionPropertiesFUCHSIA* pProperties);
#endif

#ifdef __cplusplus
}
#endif

#endif  // GPU_VULKAN_FUCHSIA_VULKAN_FUCHSIA_EXT_H_
