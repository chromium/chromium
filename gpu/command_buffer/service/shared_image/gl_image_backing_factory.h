// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_BACKING_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"

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

// Implementation of SharedImageBackingFactory that produces GL-image backed
// SharedImages.
class GPU_GLES2_EXPORT GLImageBackingFactory
    : public GLCommonImageBackingFactory {
 public:
  // It is used for migrating GLImage backing, for part that works with
  // SharedMemory GMB with SharedMemoryImageBacking and Composite backings, and
  // all other parts with OzoneImageBacking and other backings.
  GLImageBackingFactory(const GpuPreferences& gpu_preferences,
                        const GpuDriverBugWorkarounds& workarounds,
                        const gles2::FeatureInfo* feature_info,
                        ImageFactory* image_factory,
                        gl::ProgressReporter* progress_reporter);
  ~GLImageBackingFactory() override;

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
      SurfaceHandle surface_handle,
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
  struct BufferFormatInfo {
    // Whether to allow SHARED_IMAGE_USAGE_SCANOUT.
    bool allow_scanout = false;

    // GL target to use for scanout images.
    GLenum target_for_scanout = GL_TEXTURE_2D;

    // BufferFormat for scanout images.
    gfx::BufferFormat buffer_format = gfx::BufferFormat::RGBA_8888;
  };

  scoped_refptr<gl::GLImage> MakeGLImage(int client_id,
                                         gfx::GpuMemoryBufferHandle handle,
                                         gfx::BufferFormat format,
                                         const gfx::ColorSpace& color_space,
                                         gfx::BufferPlane plane,
                                         SurfaceHandle surface_handle,
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

  // WARNING: Format must be single plane.
  const BufferFormatInfo& GetBufferFormatInfo(viz::SharedImageFormat format) {
    return buffer_format_info_[format.resource_format()];
  }

  // Factory used to generate GLImages for SCANOUT backings.
  const raw_ptr<ImageFactory> image_factory_ = nullptr;

  BufferFormatInfo buffer_format_info_[viz::RESOURCE_FORMAT_MAX + 1];
  GpuMemoryBufferFormatSet gpu_memory_buffer_formats_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_BACKING_FACTORY_H_
