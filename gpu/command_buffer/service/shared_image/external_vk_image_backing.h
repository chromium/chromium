// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_BACKING_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "gpu/command_buffer/service/external_semaphore_pool.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/texture_holder_vk.h"
#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

class GLTextureHolder;
class VulkanCommandPool;
class VulkanImage;

class ExternalVkImageBacking final : public ClearTrackingSharedImageBacking {
 public:
  static std::unique_ptr<ExternalVkImageBacking> Create(
      scoped_refptr<SharedContextState> context_state,
      VulkanCommandPool* command_pool,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      const base::flat_map<VkFormat, VkImageUsageFlags>& image_usage_cache,
      base::span<const uint8_t> pixel_data);

  static std::unique_ptr<ExternalVkImageBacking> CreateFromGMB(
      scoped_refptr<SharedContextState> context_state,
      VulkanCommandPool* command_pool,
      const Mailbox& mailbox,
      gfx::GpuMemoryBufferHandle handle,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      std::optional<gfx::BufferUsage> buffer_usage = std::nullopt);

  static std::unique_ptr<ExternalVkImageBacking> CreateWithPixmap(
      scoped_refptr<SharedContextState> context_state,
      VulkanCommandPool* command_pool,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      gfx::BufferUsage buffer_usage);

  ExternalVkImageBacking(
      base::PassKey<ExternalVkImageBacking>,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      std::string debug_label,
      size_t estimated_size_bytes,
      scoped_refptr<SharedContextState> context_state,
      std::vector<TextureHolderVk> vk_textures,
      VulkanCommandPool* command_pool,
      bool use_separate_gl_texture,
      gfx::GpuMemoryBufferHandle handle = gfx::GpuMemoryBufferHandle(),
      std::optional<gfx::BufferUsage> buffer_usage = std::nullopt);

  ExternalVkImageBacking(const ExternalVkImageBacking&) = delete;
  ExternalVkImageBacking& operator=(const ExternalVkImageBacking&) = delete;

  ~ExternalVkImageBacking() override;

  SharedContextState* context_state() const { return context_state_.get(); }
  const GrBackendTexture& backend_texture() const {
    return vk_textures_[0].backend_texture;
  }
  VulkanImage* image() const { return vk_textures_[0].vulkan_image.get(); }
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
    if (usage().HasAny(SHARED_IMAGE_USAGE_WEBGPU_READ |
                       SHARED_IMAGE_USAGE_WEBGPU_WRITE)) {
      return true;
    }

    if (HasGLES2ReadOrWriteUsage(usage())) {
      return !use_separate_gl_texture() && !gl_textures_.empty();
    }

    if (usage().HasAny(SHARED_IMAGE_USAGE_RASTER_READ |
                       SHARED_IMAGE_USAGE_RASTER_WRITE) &&
        usage().Has(SHARED_IMAGE_USAGE_SCANOUT)) {
      return true;
    }

    return false;
  }
  uint32_t reads_in_progress() const { return reads_in_progress_; }
  uint32_t gl_reads_in_progress() const { return gl_reads_in_progress_; }

  // Returns VkImage layouts for each plane as GL layouts.
  std::vector<GLenum> GetVkImageLayoutsForGL();

  // Returns skia promise images for each plane.
  std::vector<sk_sp<GrPromiseImageTexture>> GetPromiseTextures();

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
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

  // Add semaphores to a pending list for reusing or being released immediately.
  void AddSemaphoresToPendingListOrRelease(
      std::vector<ExternalSemaphore> semaphores);
  // Release semaphores immediately.
  void ReleaseSemaphoresWithFenceHelper(
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
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& dawnDevice,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  // Holds format + offset information for Vulkan mapped memory.
  struct MapPlaneData {
    SkImageInfo image_info;
    size_t offset = 0;
  };

  // Makes GL context current if not already. Will return false if MakeCurrent()
  // failed.
  bool MakeGLContextCurrent();

  // Allocates GL texture(s) and returns true if successful.
  bool ProduceGLTextureInternal(bool is_passthrough);
  bool CreateGLTexture(bool is_passthrough, size_t plane_index);

  bool UploadToVkImage(const std::vector<SkPixmap>& pixmap);
  bool UploadToGLTexture(const std::vector<SkPixmap>& pixmaps);

  // Return format+offset per plane along with total data bytes required when
  // mapping VkImage.
  std::pair<std::vector<MapPlaneData>, size_t> GetMapPlaneData() const;

  void CopyPixelsFromGLTextureToVkImage();
  void CopyPixelsFromVkImageToGLTexture();

  scoped_refptr<SharedContextState> context_state_;
  std::vector<TextureHolderVk> vk_textures_;

  const raw_ptr<VulkanCommandPool, DanglingUntriaged> command_pool_;
  const bool use_separate_gl_texture_;

  ExternalSemaphore write_semaphore_;
  std::vector<ExternalSemaphore> read_semaphores_;

  bool is_write_in_progress_ = false;
  uint32_t reads_in_progress_ = 0;
  uint32_t gl_reads_in_progress_ = 0;

  std::vector<GLTextureHolder> gl_textures_;

  enum LatestContent {
    kInVkImage = 1 << 0,
    kInGLTexture = 1 << 1,
  };
  uint32_t latest_content_ = 0;

  // Semaphores pending for returning to ExternalSemaphorePool.
  // When the backing is accessed by the vulkan device for GrContext, they can
  // be returned to ExternalSemaphorePool through VulkanFenceHelper.
  std::vector<ExternalSemaphore> pending_semaphores_;

  // This is set when backing is created as CPU mappable or is created from
  // GpuMemoryBufferHandle.
  scoped_refptr<gfx::NativePixmap> pixmap_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_BACKING_H_
