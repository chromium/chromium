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

GLImageEGLStream::BindOrCopy GLImageEGLStream::ShouldBindOrCopy() {
  return BIND;
}

bool GLImageEGLStream::BindTexImage(unsigned target) {
  return true;
}

bool GLImageEGLStream::CopyTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

bool GLImageEGLStream::CopyTexSubImage(unsigned target,
                                       const gfx::Point& offset,
                                       const gfx::Rect& rect) {
  return false;
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

void GLImageEGLStream::ReleaseTexImage(unsigned target) {}

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

CopyingGLImageEGLStream::CopyingGLImageEGLStream(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device,
    const gfx::Size& size,
    EGLStreamKHR stream)
    : GLImageEGLStream(size, stream), d3d11_device_(d3d11_device) {}

bool CopyingGLImageEGLStream::Initialize() {
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = size_.width();
  desc.Height = size_.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;

  HRESULT hr =
      d3d11_device_->CreateTexture2D(&desc, nullptr, &decoder_copy_texture_);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateTexture2D failed: " << std::hex << hr;
    return false;
  }
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay();

  EGLAttrib frame_attributes[] = {
      EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE,
      0,
      EGL_NONE,
  };

  EGLBoolean result = eglStreamPostD3DTextureANGLE(
      egl_display, stream_, static_cast<void*>(decoder_copy_texture_.Get()),
      frame_attributes);
  if (!result) {
    DLOG(ERROR) << "eglStreamPostD3DTextureANGLE failed";
    return false;
  }
  result = eglStreamConsumerAcquireKHR(egl_display, stream_);
  if (!result) {
    DLOG(ERROR) << "eglStreamConsumerAcquireKHR failed";
    return false;
  }

  d3d11_device_.As(&video_device_);
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(&context);
  context.As(&video_context_);

#if DCHECK_IS_ON()
  Microsoft::WRL::ComPtr<ID3D10Multithread> multithread;
  d3d11_device_.As(&multithread);
  DCHECK(multithread->GetMultithreadProtected());
#endif  // DCHECK_IS_ON()

  return true;
}

bool CopyingGLImageEGLStream::InitializeVideoProcessor(
    const Microsoft::WRL::ComPtr<ID3D11VideoProcessor>& video_processor,
    const Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>& enumerator) {
  output_view_.Reset();

  Microsoft::WRL::ComPtr<ID3D11Device> processor_device;
  video_processor->GetDevice(&processor_device);
  DCHECK_EQ(d3d11_device_.Get(), processor_device.Get());

  d3d11_processor_ = video_processor;
  enumerator_ = enumerator;
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {
      D3D11_VPOV_DIMENSION_TEXTURE2D};
  output_view_desc.Texture2D.MipSlice = 0;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> output_view;
  HRESULT hr = video_device_->CreateVideoProcessorOutputView(
      decoder_copy_texture_.Get(), enumerator_.Get(), &output_view_desc,
      &output_view_);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get output view";
    return false;
  }
  return true;
}

void CopyingGLImageEGLStream::UnbindFromTexture() {
  copied_ = false;
}

bool CopyingGLImageEGLStream::BindTexImage(unsigned target) {
  if (copied_) {
    return true;
  }

  DCHECK(video_device_);
  Microsoft::WRL::ComPtr<ID3D11Device> texture_device;
  texture_->GetDevice(&texture_device);
  DCHECK_EQ(d3d11_device_.Get(), texture_device.Get());

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {0};
  input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_view_desc.Texture2D.ArraySlice = (UINT)level_;
  input_view_desc.Texture2D.MipSlice = 0;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> input_view;
  HRESULT hr = video_device_->CreateVideoProcessorInputView(
      texture_.Get(), enumerator_.Get(), &input_view_desc, &input_view);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create video processor input view.";
    return false;
  }

  D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
  streams.Enable = TRUE;
  streams.pInputSurface = input_view.Get();

  hr = video_context_->VideoProcessorBlt(d3d11_processor_.Get(),
                                         output_view_.Get(), 0, 1, &streams);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to process video";
    return false;
  }
  copied_ = true;
  return true;
}

CopyingGLImageEGLStream::~CopyingGLImageEGLStream() = default;

}  // namespace media
