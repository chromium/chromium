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

const VkStructureType VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CREATE_INFO_FUCHSIAX =
    static_cast<VkStructureType>(1000367000);
const VkStructureType VK_STRUCTURE_TYPE_FUCHSIA_IMAGE_FORMAT_FUCHSIAX =
    static_cast<VkStructureType>(1001004001);
const VkStructureType
    VK_STRUCTURE_TYPE_IMPORT_MEMORY_BUFFER_COLLECTION_FUCHSIAX =
        static_cast<VkStructureType>(1000367004);
const VkStructureType
    VK_STRUCTURE_TYPE_BUFFER_COLLECTION_IMAGE_CREATE_INFO_FUCHSIAX =
        static_cast<VkStructureType>(1000367005);
const VkStructureType VK_STRUCTURE_TYPE_BUFFER_COLLECTION_PROPERTIES_FUCHSIAX =
    static_cast<VkStructureType>(1000367006);

const VkObjectType VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIAX =
    static_cast<VkObjectType>(1000367002);

#define VK_FUCHSIA_buffer_collection_x 1
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkBufferCollectionFUCHSIAX)

#define VK_FUCHSIA_BUFFER_COLLECTION_X_SPEC_VERSION 1
#define VK_FUCHSIA_BUFFER_COLLECTION_X_EXTENSION_NAME \
  "VK_FUCHSIA_buffer_collection_x"

typedef struct VkBufferCollectionCreateInfoFUCHSIAX {
  VkStructureType sType;
  const void* pNext;
  zx_handle_t collectionToken;
} VkBufferCollectionCreateInfoFUCHSIAX;

typedef struct VkFuchsiaImageFormatFUCHSIAX {
  VkStructureType sType;
  const void* pNext;
  const void* imageFormat;
  uint32_t imageFormatSize;
} VkFuchsiaImageFormatFUCHSIAX;

typedef struct VkImportMemoryBufferCollectionFUCHSIAX {
  VkStructureType sType;
  const void* pNext;
  VkBufferCollectionFUCHSIAX collection;
  uint32_t index;
} VkImportMemoryBufferCollectionFUCHSIAX;

typedef struct VkBufferCollectionImageCreateInfoFUCHSIAX {
  VkStructureType sType;
  const void* pNext;
  VkBufferCollectionFUCHSIAX collection;
  uint32_t index;
} VkBufferCollectionImageCreateInfoFUCHSIAX;

typedef struct VkBufferCollectionPropertiesFUCHSIAX {
  VkStructureType sType;
  void* pNext;
  uint32_t memoryTypeBits;
  uint32_t count;
} VkBufferCollectionPropertiesFUCHSIAX;

typedef VkResult(VKAPI_PTR* PFN_vkCreateBufferCollectionFUCHSIAX)(
    VkDevice device,
    const VkBufferCollectionCreateInfoFUCHSIAX* pImportInfo,
    const VkAllocationCallbacks* pAllocator,
    VkBufferCollectionFUCHSIAX* pCollection);
typedef VkResult(VKAPI_PTR* PFN_vkSetBufferCollectionConstraintsFUCHSIAX)(
    VkDevice device,
    VkBufferCollectionFUCHSIAX collection,
    const VkImageCreateInfo* pImageInfo);
typedef void(VKAPI_PTR* PFN_vkDestroyBufferCollectionFUCHSIAX)(
    VkDevice device,
    VkBufferCollectionFUCHSIAX collection,
    const VkAllocationCallbacks* pAllocator);
typedef VkResult(VKAPI_PTR* PFN_vkGetBufferCollectionPropertiesFUCHSIAX)(
    VkDevice device,
    VkBufferCollectionFUCHSIAX collection,
    VkBufferCollectionPropertiesFUCHSIAX* pProperties);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateBufferCollectionFUCHSIAX(
    VkDevice device,
    const VkBufferCollectionCreateInfoFUCHSIAX* pImportInfo,
    const VkAllocationCallbacks* pAllocator,
    VkBufferCollectionFUCHSIAX* pCollection);

VKAPI_ATTR VkResult VKAPI_CALL
vkSetBufferCollectionConstraintsFUCHSIAX(VkDevice device,
                                         VkBufferCollectionFUCHSIAX collection,
                                         const VkImageCreateInfo* pImageInfo);

VKAPI_ATTR void VKAPI_CALL
vkDestroyBufferCollectionFUCHSIAX(VkDevice device,
                                  VkBufferCollectionFUCHSIAX collection,
                                  const VkAllocationCallbacks* pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkGetBufferCollectionPropertiesFUCHSIAX(
    VkDevice device,
    VkBufferCollectionFUCHSIAX collection,
    VkBufferCollectionPropertiesFUCHSIAX* pProperties);
#endif

#ifdef __cplusplus
}
#endif

#endif  // GPU_VULKAN_FUCHSIA_VULKAN_FUCHSIA_EXT_H_
