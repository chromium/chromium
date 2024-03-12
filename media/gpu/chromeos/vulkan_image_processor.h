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
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace media {

enum TiledImageFormat { kMM21, kMT2T };

// An image processor using Vulkan to perform MM21 detiling.
class MEDIA_GPU_EXPORT VulkanImageProcessor {
 public:
  VulkanImageProcessor(const VulkanImageProcessor&) = delete;
  VulkanImageProcessor& operator=(const VulkanImageProcessor&) = delete;

  ~VulkanImageProcessor();

  static std::unique_ptr<VulkanImageProcessor> Create(
      TiledImageFormat format = kMM21);

  // Note: |crop_rect| is actually the crop *in addition* to the |visible_rect|
  // cropping. It is equivalent to |uv_rect| in an OverlayCandidate.
  void Process(gpu::VulkanImage& in_image,
               const gfx::Size& input_visible_size,
               gpu::VulkanImage& out_image,
               const gfx::RectF& display_rect,
               const gfx::RectF& crop_rect,
               gfx::OverlayTransform transform,
               std::vector<VkSemaphore>& begin_semaphores,
               std::vector<VkSemaphore>& end_sempahores);

  gpu::VulkanDeviceQueue* GetVulkanDeviceQueue();
  gpu::VulkanImplementation& GetVulkanImplementation();

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
      std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation,
      std::unique_ptr<VulkanImageProcessor::VulkanDeviceQueueWrapper>
          vulkan_device_queue,
      std::unique_ptr<VulkanImageProcessor::VulkanCommandPoolWrapper>
          command_pool,
      std::unique_ptr<VulkanImageProcessor::VulkanRenderPass> render_pass,
      std::unique_ptr<VulkanImageProcessor::VulkanPipeline> pipeline,
      std::unique_ptr<VulkanImageProcessor::VulkanDescriptorPool>
          descriptor_pool);

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
