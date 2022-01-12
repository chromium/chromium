// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_IMAGE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_IMAGE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_gl_common.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_common.h"

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
struct GpuFeatureInfo;
struct GpuPreferences;
struct Mailbox;
class ImageFactory;

// Implementation of SharedImageBackingFactory that produces GL-image backed
// SharedImages.
class GPU_GLES2_EXPORT SharedImageBackingFactoryGLImage
    : public SharedImageBackingFactoryGLCommon {
 public:
  // for_shared_memory_gmbs is a temporary parameter which is used for checking
  // if gfx::SHARED_MEMORY_BUFFER is supported by the factory.
  // It is used for migrating GLImage backing, for part that works with
  // SharedMemory GMB with SharedMemoryBacking and Composite backings, and all
  // other parts with OzoneBacking and other backings.
  SharedImageBackingFactoryGLImage(const GpuPreferences& gpu_preferences,
                                   const GpuDriverBugWorkarounds& workarounds,
                                   const GpuFeatureInfo& gpu_feature_info,
                                   ImageFactory* image_factory,
                                   gl::ProgressReporter* progress_reporter,
                                   const bool for_shared_memory_gmbs);
  ~SharedImageBackingFactoryGLImage() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override;
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
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
                   viz::ResourceFormat format,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   bool* allow_legacy_mailbox,
                   bool is_pixel_used) override;

 private:
  scoped_refptr<gl::GLImage> MakeGLImage(int client_id,
                                         gfx::GpuMemoryBufferHandle handle,
                                         gfx::BufferFormat format,
                                         gfx::BufferPlane plane,
                                         SurfaceHandle surface_handle,
                                         const gfx::Size& size);

  std::unique_ptr<SharedImageBacking> CreateSharedImageInternal(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data);

  struct BufferFormatInfo {
    // Whether to allow SHARED_IMAGE_USAGE_SCANOUT.
    bool allow_scanout = false;

    // GL target to use for scanout images.
    GLenum target_for_scanout = GL_TEXTURE_2D;

    // BufferFormat for scanout images.
    gfx::BufferFormat buffer_format = gfx::BufferFormat::RGBA_8888;
  };

  // Factory used to generate GLImages for SCANOUT backings.
  const raw_ptr<ImageFactory> image_factory_ = nullptr;

  // Whether factory is specifically for SHARED_MEMORY Gmbs
  const bool for_shared_memory_gmbs_ = false;

  BufferFormatInfo buffer_format_info_[viz::RESOURCE_FORMAT_MAX + 1];
  GpuMemoryBufferFormatSet gpu_memory_buffer_formats_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_IMAGE_H_
