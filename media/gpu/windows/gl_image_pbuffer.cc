// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/gl_image_pbuffer.h"

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"

namespace media {

GLImagePbuffer::GLImagePbuffer(const gfx::Size& size, EGLSurface surface)
    : size_(size), surface_(surface) {}

gfx::Size GLImagePbuffer::GetSize() {
  return size_;
}
gl::GLImage::Type GLImagePbuffer::GetType() const {
  return gl::GLImage::Type::PBUFFER;
}
void SetColorSpace(const gfx::ColorSpace& color_space) {}

GLImagePbuffer::~GLImagePbuffer() {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay();

  eglReleaseTexImage(egl_display, surface_, EGL_BACK_BUFFER);

  eglDestroySurface(egl_display, surface_);
}

}  // namespace media
