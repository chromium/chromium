// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_common.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace gpu {
class SharedImageBacking;
class SharedImageBatchAccessManager;
class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;
struct GpuPreferences;
struct Mailbox;
class ImageFactory;

// Implementation of SharedImageBackingFactory that produces GL-texture backed
// SharedImages.
// TODO(ericrk): Remove support for buffer / GLImage based backings and move
// to its own type of backing.
class GPU_GLES2_EXPORT SharedImageBackingFactoryGLTexture
    : public SharedImageBackingFactory {
 public:
  SharedImageBackingFactoryGLTexture(
      const GpuPreferences& gpu_preferences,
      const GpuDriverBugWorkarounds& workarounds,
      const GpuFeatureInfo& gpu_feature_info,
      ImageFactory* image_factory,
      SharedImageBatchAccessManager* batch_access_manager,
      gl::ProgressReporter* progress_reporter);
  ~SharedImageBackingFactoryGLTexture() override;

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
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override;
  bool CanImportGpuMemoryBuffer(
      gfx::GpuMemoryBufferType memory_buffer_type) override;

  static std::unique_ptr<SharedImageBacking> CreateSharedImageForTest(
      const Mailbox& mailbox,
      GLenum target,
      GLuint service_id,
      bool is_cleared,
      viz::ResourceFormat format,
      const gfx::Size& size,
      uint32_t usage);

 private:
  scoped_refptr<gl::GLImage> MakeGLImage(int client_id,
                                         gfx::GpuMemoryBufferHandle handle,
                                         gfx::BufferFormat format,
                                         SurfaceHandle surface_handle,
                                         const gfx::Size& size);

  // This is meant to be used only on Android. Return nullptr for other
  // platforms.
  std::unique_ptr<SharedImageBacking> MakeEglImageBacking(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage);

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

  struct FormatInfo {
    FormatInfo();
    ~FormatInfo();

    // Whether this format is supported.
    bool enabled = false;

    // Whether this format supports TexStorage2D.
    bool supports_storage = false;

    // Whether to allow SHARED_IMAGE_USAGE_SCANOUT.
    bool allow_scanout = false;

    // Whether the texture is a compressed type.
    bool is_compressed = false;

    GLenum gl_format = 0;
    GLenum gl_type = 0;
    const gles2::Texture::CompatibilitySwizzle* swizzle = nullptr;
    GLenum adjusted_format = 0;

    // The internalformat portion of the format/type/internalformat triplet
    // used when calling TexImage2D
    GLuint image_internal_format = 0;

    // The internalformat portion of the format/type/internalformat triplet
    // used when calling TexStorage2D
    GLuint storage_internal_format = 0;

    // GL target to use for scanout images.
    GLenum target_for_scanout = GL_TEXTURE_2D;

    // BufferFormat for scanout images.
    gfx::BufferFormat buffer_format = gfx::BufferFormat::RGBA_8888;

    DISALLOW_COPY_AND_ASSIGN(FormatInfo);
  };

  // Whether we're using the passthrough command decoder and should generate
  // passthrough textures.
  bool use_passthrough_ = false;

  // Factory used to generate GLImages for SCANOUT backings.
  ImageFactory* image_factory_ = nullptr;

  FormatInfo format_info_[viz::RESOURCE_FORMAT_MAX + 1];
  GpuMemoryBufferFormatSet gpu_memory_buffer_formats_;
  int32_t max_texture_size_ = 0;
  bool texture_usage_angle_ = false;
  SharedImageBackingGLCommon::UnpackStateAttribs attribs;
  GpuDriverBugWorkarounds workarounds_;

  // Used to notify the watchdog before a buffer allocation in case it takes
  // long.
  gl::ProgressReporter* const progress_reporter_ = nullptr;

#if defined(OS_ANDROID)
  SharedImageBatchAccessManager* batch_access_manager_ = nullptr;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_H_
