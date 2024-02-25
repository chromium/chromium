// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_HOLDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_HOLDER_H_

#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "ui/gl/progress_reporter.h"

class GrPromiseImageTexture;

namespace gpu {

class SharedContextState;

// Helper class that holds a single GL texture, that works with either
// validating or passthrough command decoder.
class GLTextureHolder {
 public:
  // Returns the equivalent SharedImageFormat for plane specified by
  // `plane_index`.
  static viz::SharedImageFormat GetPlaneFormat(viz::SharedImageFormat format,
                                               int plane_index);

  // `format` must be single-planar format.
  GLTextureHolder(viz::SharedImageFormat format,
                  const gfx::Size& size,
                  bool is_passthrough,
                  gl::ProgressReporter* progress_reporter);
  GLTextureHolder(GLTextureHolder&& other);
  GLTextureHolder& operator=(GLTextureHolder&& other);
  ~GLTextureHolder();

  gles2::Texture* texture() { return texture_; }
  const scoped_refptr<gles2::TexturePassthrough>& passthrough_texture() {
    return passthrough_texture_;
  }

  // Returns the service GL texture id.
  GLuint GetServiceId() const;

  // Initialize by creating a new GL texture.
  void Initialize(const GLCommonImageBackingFactory::FormatInfo& format_info,
                  bool framebuffer_attachment_angle,
                  base::span<const uint8_t> pixel_data,
                  const std::string& debug_label);

  // Initialize with a GL texture created elsewhere.
  void InitializeWithTexture(const GLFormatDesc& format_desc,
                             scoped_refptr<gles2::TexturePassthrough> texture);

  // Initialize with a GL texture created elsewhere. Takes ownership of
  // lightweight ref on `texture`.
  void InitializeWithTexture(const GLFormatDesc& format_desc,
                             gles2::Texture* texture);

  // Uploads pixels from `pixmap` to GL texture.
  bool UploadFromMemory(const SkPixmap& pixmap);

  // Readback pixels from GL texture to `pixmap`.
  bool ReadbackToMemory(const SkPixmap& pixmap);

  // Returns a promise image for the GL texture.
  sk_sp<GrPromiseImageTexture> GetPromiseImage(
      SharedContextState* context_state);

  // Gets/sets cleared rect from gles2::Texture. Only valid to call with
  // validating command decoder.
  gfx::Rect GetClearedRect() const;
  void SetClearedRect(const gfx::Rect& cleared_rect);

  void SetContextLost();

 private:
  viz::SharedImageFormat format_;
  gfx::Size size_;
  bool is_passthrough_;
  bool context_lost_ = false;

  raw_ptr<gles2::Texture> texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;
  GLFormatDesc format_desc_;
  raw_ptr<gl::ProgressReporter> progress_reporter_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_HOLDER_H_
