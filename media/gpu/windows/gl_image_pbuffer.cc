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
unsigned GLImagePbuffer::GetInternalFormat() {
  return GL_BGRA_EXT;
}
unsigned GLImagePbuffer::GetDataType() {
  return GL_UNSIGNED_BYTE;
}
gl::GLImage::Type GLImagePbuffer::GetType() const {
  return gl::GLImage::Type::PBUFFER;
}
gl::GLImage::BindOrCopy GLImagePbuffer::ShouldBindOrCopy() {
  return gl::GLImage::BindOrCopy::BIND;
}
// PbufferPictureBuffer::CopySurfaceComplete does the actual binding, so
// this doesn't do anything and always succeeds.
bool GLImagePbuffer::BindTexImage(unsigned target) {
  return true;
}
void ReleaseTexImage(unsigned target) {}
bool GLImagePbuffer::CopyTexImage(unsigned target) {
  NOTREACHED();
  return false;
}
bool GLImagePbuffer::CopyTexSubImage(unsigned target,
                                     const gfx::Point& offset,
                                     const gfx::Rect& rect) {
  return false;
}
void SetColorSpace(const gfx::ColorSpace& color_space) {}
void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                  uint64_t process_tracing_id,
                  const std::string& dump_name) {}

GLImagePbuffer::~GLImagePbuffer() {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay();

  eglReleaseTexImage(egl_display, surface_, EGL_BACK_BUFFER);

  eglDestroySurface(egl_display, surface_);
}

}  // namespace media
