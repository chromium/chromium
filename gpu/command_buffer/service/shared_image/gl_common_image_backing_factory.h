// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_COMMON_IMAGE_BACKING_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_COMMON_IMAGE_BACKING_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace gpu {

namespace gles2 {
class FeatureInfo;
}  // namespace gles2

class GpuDriverBugWorkarounds;
struct GpuPreferences;

// Common constructor and helper functions for
// various GL-based backing factories.
class GPU_GLES2_EXPORT GLCommonImageBackingFactory
    : public SharedImageBackingFactory {
 public:
  struct FormatInfo {
    FormatInfo();
    FormatInfo(const FormatInfo& other);
    ~FormatInfo();

    // Whether this format supports TexStorage2D.
    bool supports_storage = false;

    // Whether the texture is a compressed type.
    bool is_compressed = false;

    GLenum gl_format = 0;
    GLenum gl_type = 0;

    // The internalformat portion of the format/type/internalformat triplet
    // used when calling TexImage2D
    GLuint image_internal_format = 0;

    // The internalformat portion of the format/type/internalformat triplet
    // used when calling TexStorage2D
    GLuint storage_internal_format = 0;

    // The adjusted formats which may be different when using the validating
    // command decoder.
    GLenum adjusted_format = 0;
    GLuint adjusted_storage_internal_format = 0;

    raw_ptr<const gles2::Texture::CompatibilitySwizzle> swizzle = nullptr;
  };

 protected:
  GLCommonImageBackingFactory(SharedImageUsageSet supported_usages,
                              const GpuPreferences& gpu_preferences,
                              const GpuDriverBugWorkarounds& workarounds,
                              const gles2::FeatureInfo* feature_info,
                              gl::ProgressReporter* progress_reporter);
  ~GLCommonImageBackingFactory() override;

  // Returns FormatInfo for each plane in format. Will return an empty vector
  // if format isn't supported.
  std::vector<FormatInfo> GetFormatInfo(viz::SharedImageFormat format) const;

  bool CanCreateTexture(viz::SharedImageFormat format,
                        const gfx::Size& size,
                        base::span<const uint8_t> pixel_data,
                        GLenum target);

  // Whether we're using the passthrough command decoder and should generate
  // passthrough textures.
  bool use_passthrough_ = false;

  int32_t max_texture_size_ = 0;
  bool texture_usage_angle_ = false;
  GpuDriverBugWorkarounds workarounds_;
  const GLFormatCaps gl_format_caps_;
  WebGPUAdapterName use_webgpu_adapter_ = WebGPUAdapterName::kDefault;

  // Used to notify the watchdog before a buffer allocation in case it takes
  // long.
  const raw_ptr<gl::ProgressReporter> progress_reporter_ = nullptr;

 private:
  // Map of supported single planar SharedImageFormats and associated GL format
  // info for them.
  std::map<viz::SharedImageFormat, FormatInfo> supported_formats_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_COMMON_IMAGE_BACKING_FACTORY_H_
