// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/optional.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/semaphore_handle.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

class VulkanCommandPool;

class ExternalVkImageBacking final : public SharedImageBacking {
 public:
  static std::unique_ptr<ExternalVkImageBacking> Create(
      SharedContextState* context_state,
      VulkanCommandPool* command_pool,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      base::span<const uint8_t> pixel_data,
      bool using_gmb = false);

  static std::unique_ptr<ExternalVkImageBacking> CreateFromGMB(
      SharedContextState* context_state,
      VulkanCommandPool* command_pool,
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat buffer_format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage);

  ~ExternalVkImageBacking() override;

  SharedContextState* context_state() const { return context_state_; }
  const GrBackendTexture& backend_texture() const { return backend_texture_; }
  VulkanImplementation* vulkan_implementation() const {
    return context_state()->vk_context_provider()->GetVulkanImplementation();
  }
  VkDevice device() const {
    return context_state()
        ->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanDevice();
  }
  bool need_sychronization() const {
    if (usage() & SHARED_IMAGE_USAGE_WEBGPU) {
      return true;
    }

    if (usage() & SHARED_IMAGE_USAGE_GLES2) {
      return !use_separate_gl_texture();
    }
    return false;
  }
  bool use_separate_gl_texture() const {
    return !context_state()->support_vulkan_external_object();
  }

  // Notifies the backing that an access will start. Return false if there is
  // currently any other conflict access in progress. Otherwise, returns true
  // and semaphore handles which will be waited on before accessing.
  bool BeginAccess(bool readonly,
                   std::vector<SemaphoreHandle>* semaphore_handles,
                   bool is_gl);

  // Notifies the backing that an access has ended. The representation must
  // provide a semaphore handle that has been signaled at the end of the write
  // access.
  void EndAccess(bool readonly, SemaphoreHandle semaphore_handle, bool is_gl);

  // SharedImageBacking implementation.
  bool IsCleared() const override;
  void SetCleared() override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  void Destroy() override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;

 protected:
  void UpdateContent(uint32_t content_flags);
  bool BeginAccessInternal(bool readonly,
                           std::vector<SemaphoreHandle>* semaphore_handles);
  void EndAccessInternal(bool readonly, SemaphoreHandle semaphore_handle);

  // SharedImageBacking implementation.
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice dawnDevice) override;
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

 private:
  ExternalVkImageBacking(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage,
                         SharedContextState* context_state,
                         VkImage image,
                         VkDeviceMemory memory,
                         size_t memory_size,
                         VkFormat vk_format,
                         VulkanCommandPool* command_pool,
                         const GrVkYcbcrConversionInfo& ycbcr_info,
                         base::Optional<WGPUTextureFormat> wgpu_format,
                         base::Optional<uint32_t> memory_type_index);

#ifdef OS_LINUX
  // Extract file descriptor from image
  int GetMemoryFd(const GrVkImageInfo& image_info);
#endif

  // Install a shared memory GMB to the backing.
  void InstallSharedMemory(
      base::WritableSharedMemoryMapping shared_memory_mapping,
      size_t stride,
      size_t memory_offset);

  using FillBufferCallback = base::OnceCallback<void(void* buffer)>;
  bool WritePixels(size_t data_size,
                   size_t stride,
                   FillBufferCallback callback);
  void CopyPixelsFromGLTextureToVkImage();
  void CopyPixelsFromShmToGLTexture();

  SharedContextState* const context_state_;
  GrBackendTexture backend_texture_;
  VulkanCommandPool* const command_pool_;

  SemaphoreHandle write_semaphore_handle_;
  std::vector<SemaphoreHandle> read_semaphore_handles_;
  bool is_cleared_ = false;

  bool is_write_in_progress_ = false;
  uint32_t reads_in_progress_ = 0;
  gles2::Texture* texture_ = nullptr;

  // GMB related stuff.
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  size_t stride_ = 0;
  size_t memory_offset_ = 0;

  enum LatestContent {
    kInVkImage = 1 << 0,
    kInSharedMemory = 1 << 1,
    kInGLTexture = 1 << 2,
  };
  uint32_t latest_content_ = 0;

  base::Optional<WGPUTextureFormat> wgpu_format_;
  base::Optional<uint32_t> memory_type_index_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageBacking);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_
