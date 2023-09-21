// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VULKAN_IMAGE_PROCESSOR_BACKEND_H_
#define MEDIA_GPU_CHROMEOS_VULKAN_IMAGE_PROCESSOR_BACKEND_H_

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
class MEDIA_GPU_EXPORT VulkanImageProcessorBackend
    : public ImageProcessorBackend {
 public:
  VulkanImageProcessorBackend(const VulkanImageProcessorBackend&) = delete;
  VulkanImageProcessorBackend& operator=(const VulkanImageProcessorBackend&) =
      delete;

  ~VulkanImageProcessorBackend() override;

  static std::unique_ptr<VulkanImageProcessorBackend> Create(
      const PortConfig& input_config,
      const PortConfig& output_config,
      OutputMode output_mode,
      ErrorCB error_cb);

  void Process(scoped_refptr<VideoFrame> input_frame,
               scoped_refptr<VideoFrame> output_frame,
               FrameReadyCB cb) override;

  std::string type() const override;

 private:
  class VulkanRenderPass;
  class VulkanShader;
  class VulkanPipeline;
  class VulkanDescriptorPool;
  class VulkanDeviceQueueWrapper;
  class VulkanCommandPoolWrapper;
  class VulkanTextureImage;

  VulkanImageProcessorBackend(
      gfx::Size input_size,
      gfx::Size output_size,
      std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation,
      std::unique_ptr<VulkanImageProcessorBackend::VulkanDeviceQueueWrapper>
          vulkan_device_queue,
      std::unique_ptr<VulkanImageProcessorBackend::VulkanCommandPoolWrapper>
          command_pool,
      std::unique_ptr<VulkanImageProcessorBackend::VulkanRenderPass>
          render_pass,
      std::unique_ptr<VulkanImageProcessorBackend::VulkanPipeline> pipeline,
      std::unique_ptr<VulkanImageProcessorBackend::VulkanDescriptorPool>
          descriptor_pool,
      const PortConfig& input_config,
      const PortConfig& output_config,
      OutputMode output_mode,
      ErrorCB error_cb);

  static bool IsSupported(const PortConfig& input_config,
                          const PortConfig& output_config,
                          OutputMode output_mode);

  gfx::Size input_size_;
  gfx::Size output_size_;
  std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation_;
  std::unique_ptr<VulkanImageProcessorBackend::VulkanDeviceQueueWrapper>
      vulkan_device_queue_;
  std::unique_ptr<VulkanImageProcessorBackend::VulkanCommandPoolWrapper>
      command_pool_;
  std::unique_ptr<VulkanImageProcessorBackend::VulkanRenderPass> render_pass_;
  std::unique_ptr<VulkanImageProcessorBackend::VulkanPipeline> pipeline_;
  std::unique_ptr<VulkanImageProcessorBackend::VulkanDescriptorPool>
      descriptor_pool_;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_VULKAN_IMAGE_PROCESSOR_BACKEND_H_
