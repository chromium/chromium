// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VULKAN_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_CHROMEOS_VULKAN_IMAGE_PROCESSOR_H_

#include <vulkan/vulkan_core.h>
#include <memory>
#include <vector>

#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "media/gpu/chromeos/image_processor_backend.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// An image processor using Vulkan to perform MM21 detiling.
class MEDIA_GPU_EXPORT VulkanImageProcessor {
 public:
  // Abstraction for the mapping from Mailbox to GpuMemoryBuffer.
  using BackingCB =
      base::RepeatingCallback<gfx::GpuMemoryBufferHandle(gpu::Mailbox&)>;
  // Abstraction for the recycling of the input frame.
  using ReleaseCB = base::OnceCallback<void(gpu::Mailbox&)>;

  VulkanImageProcessor(const VulkanImageProcessor&) = delete;
  VulkanImageProcessor& operator=(const VulkanImageProcessor&) = delete;

  ~VulkanImageProcessor();

  static std::unique_ptr<VulkanImageProcessor> Create(
      BackingCB input_backing_cb,
      BackingCB output_backing_cb);

  gpu::SemaphoreHandle Process(
      gpu::Mailbox in_mailbox,
      const gfx::Size& input_coded_size,
      const gfx::Size& input_visible_size,
      ReleaseCB input_release_cb,
      gpu::Mailbox out_mailbox,
      const gfx::Size& output_coded_size,
      const gfx::Size& output_visible_size,
      absl::optional<gpu::SemaphoreHandle> in_semaphore_handle);

 private:
  class VulkanRenderPass;
  class VulkanShader;
  class VulkanPipeline;
  class VulkanDescriptorPool;
  class VulkanDeviceQueueWrapper;
  class VulkanCommandBufferWrapper;
  class VulkanCommandPoolWrapper;
  class VulkanTextureImage;

  VulkanImageProcessor(
      BackingCB input_backing_cb,
      BackingCB output_backing_cb,
      std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation,
      std::unique_ptr<VulkanImageProcessor::VulkanDeviceQueueWrapper>
          vulkan_device_queue,
      std::unique_ptr<VulkanImageProcessor::VulkanCommandPoolWrapper>
          command_pool,
      std::unique_ptr<VulkanImageProcessor::VulkanRenderPass> render_pass,
      std::unique_ptr<VulkanImageProcessor::VulkanPipeline> pipeline,
      std::unique_ptr<VulkanImageProcessor::VulkanDescriptorPool>
          descriptor_pool);

  BackingCB input_backing_cb_;
  BackingCB output_backing_cb_;
  std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation_;
  std::unique_ptr<VulkanImageProcessor::VulkanDeviceQueueWrapper>
      vulkan_device_queue_;
  std::unique_ptr<VulkanImageProcessor::VulkanCommandPoolWrapper> command_pool_;
  std::unique_ptr<VulkanImageProcessor::VulkanRenderPass> render_pass_;
  std::unique_ptr<VulkanImageProcessor::VulkanPipeline> pipeline_;
  std::unique_ptr<VulkanImageProcessor::VulkanDescriptorPool> descriptor_pool_;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_VULKAN_IMAGE_PROCESSOR_H_
