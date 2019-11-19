// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_factory.h"

#include <unistd.h>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {

ExternalVkImageFactory::ExternalVkImageFactory(
    SharedContextState* context_state)
    : context_state_(context_state),
      command_pool_(context_state_->vk_context_provider()
                        ->GetDeviceQueue()
                        ->CreateCommandPool()) {}

ExternalVkImageFactory::~ExternalVkImageFactory() {
  if (command_pool_) {
    context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetFenceHelper()
        ->EnqueueVulkanObjectCleanupForSubmittedWork(std::move(command_pool_));
  }
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    bool is_thread_safe) {
  DCHECK(!is_thread_safe);
  return ExternalVkImageBacking::Create(context_state_, command_pool_.get(),
                                        mailbox, format, size, color_space,
                                        usage, base::span<const uint8_t>());
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data) {
  return ExternalVkImageBacking::Create(context_state_, command_pool_.get(),
                                        mailbox, format, size, color_space,
                                        usage, pixel_data);
}

std::unique_ptr<SharedImageBacking> ExternalVkImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    int client_id,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  DCHECK(CanImportGpuMemoryBuffer(handle.type));
  return ExternalVkImageBacking::CreateFromGMB(
      context_state_, command_pool_.get(), mailbox, std::move(handle),
      buffer_format, size, color_space, usage);
}

bool ExternalVkImageFactory::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return context_state_->vk_context_provider()
             ->GetVulkanImplementation()
             ->CanImportGpuMemoryBuffer(memory_buffer_type) ||
         memory_buffer_type == gfx::SHARED_MEMORY_BUFFER;
}

}  // namespace gpu
