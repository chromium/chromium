// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VULKAN_OVERLAY_ADAPTOR_H_
#define MEDIA_GPU_CHROMEOS_VULKAN_OVERLAY_ADAPTOR_H_

#include <vulkan/vulkan_core.h>

#include <memory>
#include <vector>

#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace media {

enum TiledImageFormat { kMM21, kMT2T };

// An image processor using Vulkan to perform MM21 detiling.
class MEDIA_GPU_EXPORT VulkanOverlayAdaptor {
 public:
  VulkanOverlayAdaptor(const VulkanOverlayAdaptor&) = delete;
  VulkanOverlayAdaptor& operator=(const VulkanOverlayAdaptor&) = delete;

  ~VulkanOverlayAdaptor();

  // TODO(greenjustin): Change the |is_protected| bool to an enum.
  static std::unique_ptr<VulkanOverlayAdaptor> Create(
      bool is_protected,
      TiledImageFormat format = kMM21,
      const gfx::Size& max_size = gfx::Size(3840, 2160));

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
  TiledImageFormat GetTileFormat() const;

 private:
  class VulkanRenderPass;
  class VulkanShader;
  class VulkanPipeline;
  class VulkanDescriptorPool;
  class VulkanDeviceQueueWrapper;
  class VulkanCommandBufferWrapper;
  class VulkanCommandPoolWrapper;
  class VulkanTextureImage;
  class VulkanSampler;

  VulkanOverlayAdaptor(
      std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanDeviceQueueWrapper>
          vulkan_device_queue,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanCommandPoolWrapper>
          command_pool,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanRenderPass>
          convert_render_pass,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanRenderPass>
          transform_render_pass,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanPipeline> convert_pipeline,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanPipeline> transform_pipeline,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanDescriptorPool>
          convert_descriptor_pool,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanDescriptorPool>
          transform_descriptor_pool,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanSampler> sampler,
      std::unique_ptr<gpu::VulkanImage> pivot_image,
      std::unique_ptr<VulkanOverlayAdaptor::VulkanTextureImage> pivot_texture,
      bool is_protected,
      TiledImageFormat tile_format);

  std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanDeviceQueueWrapper>
      vulkan_device_queue_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanCommandPoolWrapper> command_pool_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanRenderPass> convert_render_pass_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanRenderPass>
      transform_render_pass_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanPipeline> convert_pipeline_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanPipeline> transform_pipeline_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanDescriptorPool>
      convert_descriptor_pool_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanDescriptorPool>
      transform_descriptor_pool_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanSampler> sampler_;

  std::unique_ptr<gpu::VulkanImage> pivot_image_;
  std::unique_ptr<VulkanOverlayAdaptor::VulkanTextureImage> pivot_texture_;

  bool is_protected_;
  const TiledImageFormat tile_format_;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_VULKAN_OVERLAY_ADAPTOR_H_
