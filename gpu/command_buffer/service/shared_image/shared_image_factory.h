// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"

namespace viz {
class VulkanContextProvider;
}  // namespace viz

namespace gpu {
class GpuDriverBugWorkarounds;
class MemoryTracker;
class SharedContextState;
class SharedImageBackingFactory;
class D3DImageBackingFactory;
struct GpuFeatureInfo;
struct GpuPreferences;

class GPU_GLES2_EXPORT SharedImageFactory {
 public:
  // All objects passed are expected to outlive this class.
  SharedImageFactory(const GpuPreferences& gpu_preferences,
                     const GpuDriverBugWorkarounds& workarounds,
                     const GpuFeatureInfo& gpu_feature_info,
                     SharedContextState* context_state,
                     SharedImageManager* manager,
                     MemoryTracker* tracker,
                     bool is_for_display_compositor);
  ~SharedImageFactory();

  bool CreateSharedImage(const Mailbox& mailbox,
                         viz::SharedImageFormat si_format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         SurfaceHandle surface_handle,
                         uint32_t usage);
  bool CreateSharedImage(const Mailbox& mailbox,
                         viz::SharedImageFormat si_format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         base::span<const uint8_t> pixel_data);
  bool CreateSharedImage(const Mailbox& mailbox,
                         viz::SharedImageFormat si_format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         gfx::GpuMemoryBufferHandle buffer_handle);
  bool CreateSharedImage(const Mailbox& mailbox,
                         gfx::GpuMemoryBufferHandle handle,
                         gfx::BufferFormat format,
                         gfx::BufferPlane plane,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage);
  bool UpdateSharedImage(const Mailbox& mailbox);
  bool UpdateSharedImage(const Mailbox& mailbox,
                         std::unique_ptr<gfx::GpuFence> in_fence);
  bool DestroySharedImage(const Mailbox& mailbox);
  bool HasImages() const { return !shared_images_.empty(); }
  void DestroyAllSharedImages(bool have_context);

#if BUILDFLAG(IS_WIN)
  bool CreateSwapChain(const Mailbox& front_buffer_mailbox,
                       const Mailbox& back_buffer_mailbox,
                       viz::SharedImageFormat format,
                       const gfx::Size& size,
                       const gfx::ColorSpace& color_space,
                       GrSurfaceOrigin surface_origin,
                       SkAlphaType alpha_type,
                       uint32_t usage);
  bool PresentSwapChain(const Mailbox& mailbox);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(zx::eventpair service_handle,
                                      zx::channel sysmem_token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe);
#endif  // BUILDFLAG(IS_FUCHSIA)

  bool RegisterBacking(std::unique_ptr<SharedImageBacking> backing);

  SharedContextState* GetSharedContextState() const {
    return shared_context_state_;
  }

#if BUILDFLAG(IS_WIN)
  bool CopyToGpuMemoryBuffer(const Mailbox& mailbox);
#endif

  void RegisterSharedImageBackingFactoryForTesting(
      SharedImageBackingFactory* factory);

 private:
  bool IsSharedBetweenThreads(uint32_t usage);

  SharedImageBackingFactory* GetFactoryByUsage(
      uint32_t usage,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      base::span<const uint8_t> pixel_data,
      gfx::GpuMemoryBufferType gmb_type);
  void LogGetFactoryFailed(uint32_t usage,
                           viz::SharedImageFormat format,
                           gfx::GpuMemoryBufferType gmb_type);

  raw_ptr<SharedImageManager> shared_image_manager_;
  raw_ptr<SharedContextState> shared_context_state_;
  std::unique_ptr<MemoryTypeTracker> memory_tracker_;

  // This is used if the factory is created on display compositor to check for
  // sharing between threads.
  const bool is_for_display_compositor_;

  // This is |shared_context_state_|'s context type. Some tests leave
  // |shared_context_state_| as nullptr, in which case this is set to a default
  /// of kGL.
  const GrContextType gr_context_type_;

  // The set of SharedImages which have been created (and are being kept alive)
  // by this factory.
  base::flat_set<std::unique_ptr<SharedImageRepresentationFactoryRef>>
      shared_images_;

  // Array of all the backing factories to choose from for creating shared
  // images.
  std::vector<std::unique_ptr<SharedImageBackingFactory>> factories_;

#if BUILDFLAG(IS_WIN)
  // Used for creating swap chains
  raw_ptr<D3DImageBackingFactory> d3d_backing_factory_ = nullptr;
#endif

#if BUILDFLAG(IS_FUCHSIA)
  viz::VulkanContextProvider* vulkan_context_provider_;
#endif  // BUILDFLAG(IS_FUCHSIA)

  raw_ptr<SharedImageBackingFactory> backing_factory_for_testing_ = nullptr;
};

class GPU_GLES2_EXPORT SharedImageRepresentationFactory {
 public:
  // All arguments must outlive this object.
  SharedImageRepresentationFactory(SharedImageManager* manager,
                                   MemoryTracker* tracker);
  ~SharedImageRepresentationFactory();

  // Helpers which call similar classes on SharedImageManager, providing a
  // MemoryTypeTracker.
  // NOTE: This object *must* outlive all objects created via the below methods,
  // as the |tracker_| instance variable that it supplies to them is used in
  // their destruction process.
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      const Mailbox& mailbox);
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(const Mailbox& mailbox);
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      const Mailbox& mailbox,
      scoped_refptr<SharedContextState> context_State);
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      const Mailbox& mailbox,
      WGPUDevice device,
      WGPUBackendType backend_type,
      std::vector<WGPUTextureFormat> view_formats);
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      const Mailbox& mailbox);
  std::unique_ptr<MemoryImageRepresentation> ProduceMemory(
      const Mailbox& mailbox);
  std::unique_ptr<RasterImageRepresentation> ProduceRaster(
      const Mailbox& mailbox);

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<LegacyOverlayImageRepresentation> ProduceLegacyOverlay(
      const Mailbox& mailbox);
#endif

 private:
  const raw_ptr<SharedImageManager> manager_;
  std::unique_ptr<MemoryTypeTracker> tracker_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_FACTORY_H_
