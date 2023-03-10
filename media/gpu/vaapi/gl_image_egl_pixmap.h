// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_GL_IMAGE_EGL_PIXMAP_H_
#define MEDIA_GPU_VAAPI_GL_IMAGE_EGL_PIXMAP_H_

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/glx.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/native_pixmap_egl_x11_binding_helper.h"

namespace media {

class VaapiPictureNativePixmapAngle;

class GLImageEGLPixmap : public gl::GLImage {
  // NOTE: We are in the process of eliminating this class, so no new usages
  // of it should be introduced.
 private:
  friend class VaapiPictureNativePixmapAngle;

  explicit GLImageEGLPixmap(const gfx::Size& size);

  GLImageEGLPixmap(const GLImageEGLPixmap&) = delete;
  GLImageEGLPixmap& operator=(const GLImageEGLPixmap&) = delete;

  bool Initialize(x11::Pixmap pixmap);

  // Overridden from GLImage:
  gfx::Size GetSize() override;

  // Binds image to texture currently bound to |target|. Returns true on
  // success.
  bool BindTexImage(unsigned target);

  // Releases the image that was bound via BindTexImage().
  void ReleaseEGLImage();

  ~GLImageEGLPixmap() override;

  gl::NativePixmapEGLX11BindingHelper binding_helper_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_GL_IMAGE_EGL_PIXMAP_H_
