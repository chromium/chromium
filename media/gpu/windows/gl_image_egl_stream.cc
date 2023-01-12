// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/gl_image_egl_stream.h"

#include <d3d11_1.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"

namespace media {

GLImageEGLStream::GLImageEGLStream(const gfx::Size& size, EGLStreamKHR stream)
    : size_(size), stream_(stream) {}

bool GLImageEGLStream::BindTexImage(unsigned target) {
  return true;
}

unsigned GLImageEGLStream::GetInternalFormat() {
  return GL_RGBA;
}

unsigned GLImageEGLStream::GetDataType() {
  return GL_UNSIGNED_BYTE;
}

gfx::Size GLImageEGLStream::GetSize() {
  return size_;
}

gl::GLImage::Type GLImageEGLStream::GetType() const {
  return Type::EGL_STREAM;
}

void GLImageEGLStream::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                    uint64_t process_tracing_id,
                                    const std::string& dump_name) {}

void GLImageEGLStream::SetTexture(
    const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
    size_t level) {
  texture_ = texture;
  level_ = level;
}

GLImageEGLStream::~GLImageEGLStream() {
  if (stream_) {
    eglDestroyStreamKHR(gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(),
                        stream_);
  }
}

}  // namespace media
