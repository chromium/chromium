// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_BACKING_H_

#include <dawn/webgpu_cpp.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/gl_ozone_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_context.h"

namespace gpu {
class OzoneImageGLTexturesHolder;
class VaapiDependencies;

// Implementation of SharedImageBacking that uses a NativePixmap created via
// an Ozone surface factory. The memory associated with the pixmap can be
// aliased by both GL and Vulkan for use in rendering or compositing.
class GPU_GLES2_EXPORT OzoneImageBacking final
    : public ClearTrackingSharedImageBacking,
      public gl::GLContext::GLContextObserver {
 public:
  OzoneImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      gfx::BufferPlane plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      scoped_refptr<SharedContextState> context_state,
      scoped_refptr<gfx::NativePixmap> pixmap,
      const GpuDriverBugWorkarounds& workarounds,
      bool use_passthrough,
      std::optional<gfx::BufferUsage> buffer_usage = std::nullopt);

  OzoneImageBacking(const OzoneImageBacking&) = delete;
  OzoneImageBacking& operator=(const OzoneImageBacking&) = delete;

  ~OzoneImageBacking() override;

  // gpu::SharedImageBacking:
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

  enum class AccessStream { kGL, kVulkan, kWebGPU, kOverlay, kLast };

 protected:
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats) override;
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
  std::unique_ptr<VaapiImageRepresentation> ProduceVASurface(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      VaapiDependenciesFactory* dep_factory) override;

#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<VulkanImageRepresentation> ProduceVulkan(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      gpu::VulkanDeviceQueue* vulkan_device_queue,
      gpu::VulkanImplementation& vulkan_impl) override;
#endif

 private:
  friend class GLOzoneImageRepresentationShared;
  friend class DawnOzoneImageRepresentation;
  friend class SkiaVkOzoneImageRepresentation;
  friend class VulkanOzoneImageRepresentation;
  class VaapiOzoneImageRepresentation;
  class OverlayOzoneImageRepresentation;

  FRIEND_TEST_ALL_PREFIXES(OzoneImageBackingFactoryTest,
                           UsesCacheForTextureHolders);
  FRIEND_TEST_ALL_PREFIXES(OzoneImageBackingFactoryTest,
                           UsesCacheForTextureHolders2);
  FRIEND_TEST_ALL_PREFIXES(OzoneImageBackingFactoryTest,
                           MarksContextLostOnContextLost);
  FRIEND_TEST_ALL_PREFIXES(OzoneImageBackingFactoryTest,
                           MarksContextLostOnContextLost2);
  FRIEND_TEST_ALL_PREFIXES(OzoneImageBackingFactoryTest,
                           RemovesTextureHoldersOnContextDestroy);
  FRIEND_TEST_ALL_PREFIXES(OzoneImageBackingFactoryTest,
                           FindsCompatibleContextAndReusesTexture);
  FRIEND_TEST_ALL_PREFIXES(OzoneImageBackingFactoryTest,
                           CorrectlyDestroysAndMarksContextLost);

  bool VaSync();

  void FlushAndSubmitIfNecessary(
      std::vector<GrBackendSemaphore> signal_semaphores,
      SharedContextState* const shared_context_state);

  bool BeginAccess(bool readonly,
                   AccessStream access_stream,
                   std::vector<gfx::GpuFenceHandle>* fences,
                   bool& need_end_fence);
  void EndAccess(bool readonly,
                 AccessStream access_stream,
                 gfx::GpuFenceHandle fence);

  template <typename T>
  std::unique_ptr<T> ProduceGLTextureInternal(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker,
                                              bool is_passthrough);

  scoped_refptr<OzoneImageGLTexturesHolder> RetainGLTexture(
      bool is_passthrough);
  scoped_refptr<OzoneImageGLTexturesHolder> RetainGLTextureForCacheWorkaround(
      bool is_passthrough);
  scoped_refptr<OzoneImageGLTexturesHolder> RetainGLTexturePerContextCache(
      bool is_passthrough);

  // gl::GLContext::GLContextObserver:
  void OnGLContextLost(gl::GLContext* context) override;
  void OnGLContextWillDestroy(gl::GLContext* context) override;

  void OnGLContextLostOrDestroy(gl::GLContext* context, bool mark_context_lost);

  // Returns a GpuMemoryBufferHandle for a single plane of the backing pixmap.
  gfx::GpuMemoryBufferHandle GetSinglePlaneGpuMemoryBufferHandle(
      uint32_t index);

  void DestroyTexturesOnContext(OzoneImageGLTexturesHolder* holder,
                                gl::GLContext* context);

  // Indicates if this backing produced a VASurface that may have pending work.
  bool has_pending_va_writes_ = false;
  std::unique_ptr<VaapiDependencies> vaapi_deps_;
  gfx::BufferPlane plane_;
  uint32_t reads_in_progress_ = 0;
  bool is_write_in_progress_ = false;
  int write_streams_count_;

  scoped_refptr<gfx::NativePixmap> pixmap_;
  // A texture holder cache for the |cache_texture_in_ozone_backing| workaround.
  // Not used if features::Blah is enabled. See more details below.
  scoped_refptr<OzoneImageGLTexturesHolder> cached_texture_holder_;
  // Per-context texture holders that are cached to reduce the number of
  // allocations/deallocations of textures and their EGLImages. That's
  // especially handy for raster tasks as there can be tens of tasks resulting
  // in creation and destruction of EGLImages, which is costly.
  std::map<gl::GLContext*, scoped_refptr<OzoneImageGLTexturesHolder>>
      per_context_cached_textures_holders_;
  const bool use_per_context_cache_ = false;

  // Write fence that is external and does not do Begin/EndAccess (eg. exo)
  gfx::GpuFenceHandle external_write_fence_;
  gfx::GpuFenceHandle write_fence_;
  base::flat_map<AccessStream, gfx::GpuFenceHandle> read_fences_;
  AccessStream last_write_stream_;
  scoped_refptr<SharedContextState> context_state_;
  const GpuDriverBugWorkarounds workarounds_;
  bool use_passthrough_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_BACKING_H_
