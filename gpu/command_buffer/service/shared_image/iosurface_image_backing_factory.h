// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/gl_image.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace gpu {

class SharedImageBacking;
class GpuDriverBugWorkarounds;
struct GpuPreferences;
struct Mailbox;
class ImageFactory;

// Helper functions used used by SharedImageRepresentationGLImage to do
// IOSurface-specific sharing.
class GPU_GLES2_EXPORT IOSurfaceImageBackingFactory
    : public GLCommonImageBackingFactory {
 public:
  static sk_sp<SkPromiseImageTexture> ProduceSkiaPromiseTextureMetal(
      SharedImageBacking* backing,
      scoped_refptr<SharedContextState> context_state,
      gfx::ScopedIOSurface io_surface,
      uint32_t io_surface_plane);
  static std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      std::vector<WGPUTextureFormat> view_formats,
      gfx::ScopedIOSurface io_surface,
      uint32_t io_surface_plane);
  static bool InitializePixels(SharedImageBacking* backing,
                               gfx::ScopedIOSurface io_surface,
                               uint32_t io_surface_plane,
                               const uint8_t* pixel_data);

  // It is used for migrating GLImage backing, for part that works with
  // SharedMemory GMB with SharedMemoryImageBacking and Composite backings, and
  // all other parts with OzoneImageBacking and other backings.
  IOSurfaceImageBackingFactory(const GpuPreferences& gpu_preferences,
                               const GpuDriverBugWorkarounds& workarounds,
                               const gles2::FeatureInfo* feature_info,
                               ImageFactory* image_factory,
                               gl::ProgressReporter* progress_reporter);
  ~IOSurfaceImageBackingFactory() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override;
  bool IsSupported(uint32_t usage,
                   viz::SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override;

 private:
  scoped_refptr<gl::GLImage> MakeGLImage(int client_id,
                                         gfx::GpuMemoryBufferHandle handle,
                                         gfx::BufferFormat format,
                                         const gfx::ColorSpace& color_space,
                                         gfx::BufferPlane plane,
                                         const gfx::Size& size);

  std::unique_ptr<SharedImageBacking> CreateSharedImageInternal(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data);

  // Factory used to generate GLImages for SCANOUT backings.
  const raw_ptr<ImageFactory> image_factory_ = nullptr;

  GpuMemoryBufferFormatSet gpu_memory_buffer_formats_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_FACTORY_H_
