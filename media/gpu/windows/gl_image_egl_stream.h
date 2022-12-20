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
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  gfx::Size GetSize() override;
  Type GetType() const override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  void ReleaseTexImage(unsigned target) override;

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

// This copies to a new texture on bind.
class CopyingGLImageEGLStream : public GLImageEGLStream {
 public:
  CopyingGLImageEGLStream(
      const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device,
      const gfx::Size& size,
      EGLStreamKHR stream);

  bool Initialize();
  bool InitializeVideoProcessor(
      const Microsoft::WRL::ComPtr<ID3D11VideoProcessor>& video_processor,
      const Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>& enumerator);
  void UnbindFromTexture();

  // GLImage implementation.
  bool BindTexImage(unsigned target) override;

 private:
  ~CopyingGLImageEGLStream() override;

  bool copied_ = false;

  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> d3d11_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> enumerator_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> decoder_copy_texture_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> output_view_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_GL_IMAGE_EGL_STREAM_H_
