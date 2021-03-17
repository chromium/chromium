// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "gpu/command_buffer/service/external_semaphore_pool.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

class VulkanCommandPool;
class VulkanImage;

struct VulkanImageUsageCache {
  // Maximal usage flags for VK_IMAGE_TILING_OPTIMAL each ResourceFormat.
  VkImageUsageFlags optimal_tiling_usage[viz::RESOURCE_FORMAT_MAX + 1];
};

class ExternalVkImageBacking final : public ClearTrackingSharedImageBacking {
 public:
  static std::unique_ptr<ExternalVkImageBacking> Create(
      scoped_refptr<SharedContextState> context_state,
      VulkanCommandPool* command_pool,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      const VulkanImageUsageCache* image_usage_cache,
      base::span<const uint8_t> pixel_data,
      bool using_gmb = false);

  static std::unique_ptr<ExternalVkImageBacking> CreateFromGMB(
      scoped_refptr<SharedContextState> context_state,
      VulkanCommandPool* command_pool,
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat buffer_format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      const VulkanImageUsageCache* image_usage_cache);

  ExternalVkImageBacking(base::PassKey<ExternalVkImageBacking>,
                         const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         scoped_refptr<SharedContextState> context_state,
                         std::unique_ptr<VulkanImage> image,
                         VulkanCommandPool* command_pool,
                         bool use_separate_gl_texture);

  ~ExternalVkImageBacking() override;

  SharedContextState* context_state() const { return context_state_.get(); }
  const GrBackendTexture& backend_texture() const { return backend_texture_; }
  VulkanImage* image() const { return image_.get(); }
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      const {
    return texture_passthrough_;
  }
  viz::VulkanContextProvider* context_provider() const {
    return context_state()->vk_context_provider();
  }
  VulkanImplementation* vulkan_implementation() const {
    return context_provider()->GetVulkanImplementation();
  }
  VulkanFenceHelper* fence_helper() const {
    return context_provider()->GetDeviceQueue()->GetFenceHelper();
  }
  ExternalSemaphorePool* external_semaphore_pool() {
    return context_state()->external_semaphore_pool();
  }
  bool use_separate_gl_texture() const { return use_separate_gl_texture_; }
  bool need_synchronization() const {
    if (usage() & SHARED_IMAGE_USAGE_WEBGPU) {
      return true;
    }

    if (usage() & SHARED_IMAGE_USAGE_GLES2) {
      return !use_separate_gl_texture() && (texture_ || texture_passthrough_);
    }

    if (usage() & SHARED_IMAGE_USAGE_SCANOUT) {
      return true;
    }

    return false;
  }
  uint32_t reads_in_progress() const { return reads_in_progress_; }
  uint32_t gl_reads_in_progress() const { return gl_reads_in_progress_; }

  // Notifies the backing that an access will start. Return false if there is
  // currently any other conflict access in progress. Otherwise, returns true
  // and semaphores which will be waited on before accessing.
  bool BeginAccess(bool readonly,
                   std::vector<ExternalSemaphore>* external_semaphores,
                   bool is_gl);

  // Notifies the backing that an access has ended. The representation must
  // provide a semaphore handle that has been signaled at the end of the write
  // access.
  void EndAccess(bool readonly,
                 ExternalSemaphore external_semaphore,
                 bool is_gl);

  // SharedImageBacking implementation.
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;

  // Add semaphores to a pending list for reusing or being released immediately.
  void AddSemaphoresToPendingListOrRelease(
      std::vector<ExternalSemaphore> semaphores);
  // Return |pending_semaphores_| and passed in |semaphores| to
  // ExternalSemaphorePool for reusing.
  void ReturnPendingSemaphoresWithFenceHelper(
      std::vector<ExternalSemaphore> semaphores);

 protected:
  void UpdateContent(uint32_t content_flags);
  bool BeginAccessInternal(bool readonly,
                           std::vector<ExternalSemaphore>* external_semaphores);
  void EndAccessInternal(bool readonly, ExternalSemaphore external_semaphore);

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
  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  // Install a shared memory GMB to the backing.
  void InstallSharedMemory(SharedMemoryRegionWrapper shared_memory_wrapper);
  // Returns texture_service_id for ProduceGLTexture and GLTexturePassthrough.
  GLuint ProduceGLTextureInternal();

  using FillBufferCallback = base::OnceCallback<void(void* buffer)>;
  // TODO(penghuang): Remove it when GrContext::updateBackendTexture() supports
  // compressed texture and callback.
  bool WritePixelsWithCallback(size_t data_size,
                               size_t stride,
                               FillBufferCallback callback);
  bool WritePixelsWithData(base::span<const uint8_t> pixel_data, size_t stride);
  bool WritePixels();
  void CopyPixelsFromGLTextureToVkImage();
  void CopyPixelsFromShmToGLTexture();

  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<VulkanImage> image_;
  GrBackendTexture backend_texture_;
  VulkanCommandPool* const command_pool_;
  const bool use_separate_gl_texture_;

  ExternalSemaphore write_semaphore_;
  std::vector<ExternalSemaphore> read_semaphores_;

  bool is_write_in_progress_ = false;
  uint32_t reads_in_progress_ = 0;
  uint32_t gl_reads_in_progress_ = 0;
  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;

  // GMB related stuff.
  SharedMemoryRegionWrapper shared_memory_wrapper_;

  enum LatestContent {
    kInVkImage = 1 << 0,
    kInSharedMemory = 1 << 1,
    kInGLTexture = 1 << 2,
  };
  uint32_t latest_content_ = 0;

  // Semaphores pending for returning to ExternalSemaphorePool.
  // When the backing is accessed by the vulkan device for GrContext, they can
  // be returned to ExternalSemaphorePool through VulkanFenceHelper.
  std::vector<ExternalSemaphore> pending_semaphores_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageBacking);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_BACKING_H_
