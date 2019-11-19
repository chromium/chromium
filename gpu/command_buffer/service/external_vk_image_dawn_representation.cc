// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_dawn_representation.h"

#include <dawn_native/VulkanBackend.h>

#include <iostream>
#include <utility>
#include <vector>

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "ui/gl/buildflags.h"

namespace gpu {

ExternalVkImageDawnRepresentation::ExternalVkImageDawnRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUTextureFormat wgpu_format,
    int memory_fd,
    VkDeviceSize allocation_size,
    uint32_t memory_type_index)
    : SharedImageRepresentationDawn(manager, backing, tracker),
      device_(device),
      wgpu_format_(wgpu_format),
      memory_fd_(memory_fd),
      allocation_size_(allocation_size),
      memory_type_index_(memory_type_index),
      dawn_procs_(dawn_native::GetProcs()) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_.deviceReference(device_);
}

ExternalVkImageDawnRepresentation::~ExternalVkImageDawnRepresentation() {
  EndAccess();
  dawn_procs_.deviceRelease(device_);
}

WGPUTexture ExternalVkImageDawnRepresentation::BeginAccess(
    WGPUTextureUsage usage) {
  std::vector<SemaphoreHandle> handles;

  if (!backing_impl()->BeginAccess(false, &handles, false /* is_gl */)) {
    return nullptr;
  }

  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.nextInChain = nullptr;
  texture_descriptor.format = wgpu_format_;
  texture_descriptor.usage = usage;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {size().width(), size().height(), 1};
  texture_descriptor.arrayLayerCount = 1;
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;

  dawn_native::vulkan::ExternalImageDescriptorOpaqueFD descriptor = {};
  descriptor.cTextureDescriptor = &texture_descriptor;
  descriptor.isCleared = true;
  descriptor.allocationSize = allocation_size_;
  descriptor.memoryTypeIndex = memory_type_index_;
  descriptor.memoryFD = memory_fd_;
  descriptor.waitFDs = {};

  // TODO(http://crbug.com/dawn/200): We may not be obeying all of the rules
  // specified by Vulkan for external queue transfer barriers. Investigate this.

  // Take ownership of file descriptors and transfer to dawn
  for (SemaphoreHandle& handle : handles) {
    descriptor.waitFDs.push_back(handle.TakeHandle().release());
  }

  texture_ = dawn_native::vulkan::WrapVulkanImageOpaqueFD(device_, &descriptor);

  if (texture_) {
    // Keep a reference to the texture so that it stays valid (its content
    // might be destroyed).
    dawn_procs_.textureReference(texture_);

    // Assume that the user of this representation will write to the texture
    // so set the cleared flag so that other representations don't overwrite
    // the result.
    // TODO(cwallez@chromium.org): This is incorrect and allows reading
    // uninitialized data. When !IsCleared we should tell dawn_native to
    // consider the texture lazy-cleared.
    SetCleared();
  }

  return texture_;
}

void ExternalVkImageDawnRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  // TODO(cwallez@chromium.org): query dawn_native to know if the texture was
  // cleared and set IsCleared appropriately.

  // Grab the signal semaphore from dawn
  int signal_semaphore_fd =
      dawn_native::vulkan::ExportSignalSemaphoreOpaqueFD(device_, texture_);

  // Wrap file descriptor in a handle
  SemaphoreHandle signal_semaphore(
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
      base::ScopedFD(signal_semaphore_fd));

  backing_impl()->EndAccess(false, std::move(signal_semaphore),
                            false /* is_gl */);

  // Destroy the texture, signaling the semaphore in dawn
  dawn_procs_.textureDestroy(texture_);
  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;
}

}  // namespace gpu
