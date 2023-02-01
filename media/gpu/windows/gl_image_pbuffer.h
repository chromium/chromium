// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_GL_IMAGE_PBUFFER_H_
#define MEDIA_GPU_WINDOWS_GL_IMAGE_PBUFFER_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_image.h"

namespace media {

// GLImagePbuffer is just used to hold references to the underlying
// image content so it can be destroyed when the textures are.
class GLImagePbuffer final : public gl::GLImage {
 public:
  GLImagePbuffer(const gfx::Size& size, EGLSurface surface);

  // gl::GLImage implementation.
  gfx::Size GetSize() override;
  gl::GLImage::Type GetType() const override;

 private:
  ~GLImagePbuffer() override;

  gfx::Size size_;
  EGLSurface surface_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_GL_IMAGE_PBUFFER_H_
