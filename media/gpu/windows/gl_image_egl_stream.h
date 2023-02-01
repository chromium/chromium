// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_GL_IMAGE_EGL_STREAM_H_
#define MEDIA_GPU_WINDOWS_GL_IMAGE_EGL_STREAM_H_

#include <d3d11.h>
#include <wrl/client.h>

#include "ui/gl/gl_image.h"

typedef void* EGLStreamKHR;
typedef void* EGLConfig;
typedef void* EGLSurface;

namespace media {
class GLImageEGLStream : public gl::GLImage {
 public:
  GLImageEGLStream(const gfx::Size& size, EGLStreamKHR stream);

  // GLImage implementation.
  gfx::Size GetSize() override;
  Type GetType() const override;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture() { return texture_; }
  size_t level() const { return level_; }

  void SetTexture(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
                  size_t level);

 protected:
  ~GLImageEGLStream() override;

  gfx::Size size_;
  EGLStreamKHR stream_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
  size_t level_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_GL_IMAGE_EGL_STREAM_H_
