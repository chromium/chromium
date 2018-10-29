// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class SharedImageBacking;
class GpuDriverBugWorkarounds;
struct GpuFeatureInfo;
struct GpuPreferences;
struct Mailbox;
class ImageFactory;
class MemoryTracker;

// Implementation of SharedImageBackingFactory that produces GL-texture backed
// SharedImages.
// TODO(ericrk): Remove support for buffer / GLImage based backings and move
// to its own type of backing.
class GPU_GLES2_EXPORT SharedImageBackingFactoryGLTexture
    : public SharedImageBackingFactory {
 public:
  SharedImageBackingFactoryGLTexture(const GpuPreferences& gpu_preferences,
                                     const GpuDriverBugWorkarounds& workarounds,
                                     const GpuFeatureInfo& gpu_feature_info,
                                     ImageFactory* image_factory,
                                     MemoryTracker* tracker);
  ~SharedImageBackingFactoryGLTexture() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage) override;

 private:
  struct FormatInfo {
    FormatInfo();
    ~FormatInfo();

    // Whether this format is supported.
    bool enabled = false;

    // Whether to use glTexStorage2D or glTexImage2D.
    bool use_storage = false;

    // Whether to allow SHARED_IMAGE_USAGE_SCANOUT.
    bool allow_scanout = false;

    // GL internal_format/format/type triplet.
    GLuint internal_format = 0;
    GLenum gl_format = 0;
    GLenum gl_type = 0;

    const gles2::Texture::CompatibilitySwizzle* swizzle = nullptr;
    GLuint adjusted_internal_format = 0;
    GLenum adjusted_format = 0;

    // GL target to use for scanout images.
    GLenum target_for_scanout = GL_TEXTURE_2D;

    // BufferFormat for scanout images.
    gfx::BufferFormat buffer_format = gfx::BufferFormat::RGBA_8888;
  };

  // Whether we're using the passthrough command decoder and should generate
  // passthrough textures.
  bool use_passthrough_ = false;

  // Factory used to generate GLImages for SCANOUT backings.
  ImageFactory* image_factory_ = nullptr;

  std::unique_ptr<MemoryTypeTracker> memory_tracker_;
  FormatInfo format_info_[viz::RESOURCE_FORMAT_MAX + 1];
  int32_t max_texture_size_ = 0;
  bool texture_usage_angle_ = false;
  bool es3_capable_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_H_
