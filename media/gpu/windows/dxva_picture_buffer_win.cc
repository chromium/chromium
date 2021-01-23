// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/dxva_picture_buffer_win.h"

#include "base/metrics/histogram_functions.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/dxva_video_decode_accelerator_win.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace media {

namespace {

// These GLImage subclasses are just used to hold references to the underlying
// image content so it can be destroyed when the textures are.
class DummyGLImage : public gl::GLImage {
 public:
  DummyGLImage(const gfx::Size& size) : size_(size) {}

  // gl::GLImage implementation.
  gfx::Size GetSize() override { return size_; }
  unsigned GetInternalFormat() override { return GL_BGRA_EXT; }
  unsigned GetDataType() override { return GL_UNSIGNED_BYTE; }
  BindOrCopy ShouldBindOrCopy() override { return BIND; }
  // PbufferPictureBuffer::CopySurfaceComplete does the actual binding, so
  // this doesn't do anything and always succeeds.
  bool BindTexImage(unsigned target) override { return true; }
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override {
    NOTREACHED();
    return false;
  }
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override {
    return false;
  }
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override {
    return false;
  }
  void SetColorSpace(const gfx::ColorSpace& color_space) override {}
  void Flush() override {}
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override {}

 protected:
  ~DummyGLImage() override {}

 private:
  gfx::Size size_;
};

class GLImagePbuffer : public DummyGLImage {
 public:
  GLImagePbuffer(const gfx::Size& size, EGLSurface surface)
      : DummyGLImage(size), surface_(surface) {}

 private:
  ~GLImagePbuffer() override {
    EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

    eglReleaseTexImage(egl_display, surface_, EGL_BACK_BUFFER);

    eglDestroySurface(egl_display, surface_);
  }

  EGLSurface surface_;
};

}  // namespace

enum {
  // The keyed mutex should always be released before the other thread
  // attempts to acquire it, so AcquireSync should always return immediately.
  kAcquireSyncWaitMs = 0,
};

// static
std::unique_ptr<DXVAPictureBuffer> DXVAPictureBuffer::Create(
    const DXVAVideoDecodeAccelerator& decoder,
    const PictureBuffer& buffer,
    EGLConfig egl_config) {
  switch (decoder.GetPictureBufferMechanism()) {
    case DXVAVideoDecodeAccelerator::PictureBufferMechanism::BIND: {
      auto picture_buffer = std::make_unique<EGLStreamPictureBuffer>(buffer);
      if (!picture_buffer->Initialize())
        return nullptr;

      return picture_buffer;
    }
    case DXVAVideoDecodeAccelerator::PictureBufferMechanism::
        DELAYED_COPY_TO_NV12: {
      auto picture_buffer =
          std::make_unique<EGLStreamDelayedCopyPictureBuffer>(buffer);
      if (!picture_buffer->Initialize(decoder))
        return nullptr;

      return picture_buffer;
    }
    case DXVAVideoDecodeAccelerator::PictureBufferMechanism::COPY_TO_NV12: {
      auto picture_buffer =
          std::make_unique<EGLStreamCopyPictureBuffer>(buffer);
      if (!picture_buffer->Initialize(decoder))
        return nullptr;

      return picture_buffer;
    }
    case DXVAVideoDecodeAccelerator::PictureBufferMechanism::COPY_TO_RGB: {
      auto picture_buffer = std::make_unique<PbufferPictureBuffer>(buffer);

      if (!picture_buffer->Initialize(decoder, egl_config))
        return nullptr;

      return picture_buffer;
    }
  }
  NOTREACHED();
  return nullptr;
}

DXVAPictureBuffer::~DXVAPictureBuffer() {}

void DXVAPictureBuffer::ResetReuseFence() {
  NOTREACHED();
}

bool DXVAPictureBuffer::CopyOutputSampleDataToPictureBuffer(
    DXVAVideoDecodeAccelerator* decoder,
    IDirect3DSurface9* dest_surface,
    ID3D11Texture2D* dx11_texture,
    int input_buffer_id) {
  NOTREACHED();
  return false;
}

void DXVAPictureBuffer::set_bound() {
  DCHECK_EQ(UNUSED, state_);
  state_ = BOUND;
}

gl::GLFence* DXVAPictureBuffer::reuse_fence() {
  return nullptr;
}

bool DXVAPictureBuffer::CopySurfaceComplete(IDirect3DSurface9* src_surface,
                                            IDirect3DSurface9* dest_surface) {
  NOTREACHED();
  return false;
}

DXVAPictureBuffer::DXVAPictureBuffer(const PictureBuffer& buffer)
    : picture_buffer_(buffer) {}

bool DXVAPictureBuffer::BindSampleToTexture(
    DXVAVideoDecodeAccelerator* decoder,
    Microsoft::WRL::ComPtr<IMFSample> sample) {
  NOTREACHED();
  return false;
}

bool PbufferPictureBuffer::Initialize(const DXVAVideoDecodeAccelerator& decoder,
                                      EGLConfig egl_config) {
  RETURN_ON_FAILURE(!picture_buffer_.service_texture_ids().empty(),
                    "No service texture ids provided", false);

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  EGLint use_rgb = 1;
  eglGetConfigAttrib(egl_display, egl_config, EGL_BIND_TO_TEXTURE_RGB,
                     &use_rgb);

  EGLint red_bits = 8;
  eglGetConfigAttrib(egl_display, egl_config, EGL_RED_SIZE, &red_bits);

  if (!InitializeTexture(decoder, !!use_rgb, red_bits == 16))
    return false;

  EGLint attrib_list[] = {EGL_WIDTH,
                          size().width(),
                          EGL_HEIGHT,
                          size().height(),
                          EGL_TEXTURE_FORMAT,
                          use_rgb ? EGL_TEXTURE_RGB : EGL_TEXTURE_RGBA,
                          EGL_TEXTURE_TARGET,
                          EGL_TEXTURE_2D,
                          EGL_NONE};

  decoding_surface_ = eglCreatePbufferFromClientBuffer(
      egl_display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, texture_share_handle_,
      egl_config, attrib_list);
  RETURN_ON_FAILURE(decoding_surface_, "Failed to create surface", false);
  gl_image_ = base::MakeRefCounted<GLImagePbuffer>(size(), decoding_surface_);
  if (decoder.d3d11_device_ && decoder.use_keyed_mutex_) {
    void* keyed_mutex = nullptr;
    EGLBoolean ret =
        eglQuerySurfacePointerANGLE(egl_display, decoding_surface_,
                                    EGL_DXGI_KEYED_MUTEX_ANGLE, &keyed_mutex);
    RETURN_ON_FAILURE(keyed_mutex && ret == EGL_TRUE,
                      "Failed to query ANGLE keyed mutex", false);
    egl_keyed_mutex_ = Microsoft::WRL::ComPtr<IDXGIKeyedMutex>(
        static_cast<IDXGIKeyedMutex*>(keyed_mutex));
  }
  use_rgb_ = !!use_rgb;
  return true;
}

bool PbufferPictureBuffer::InitializeTexture(
    const DXVAVideoDecodeAccelerator& decoder,
    bool use_rgb,
    bool use_fp16) {
  DCHECK(!texture_share_handle_);
  if (decoder.d3d11_device_) {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = picture_buffer_.size().width();
    desc.Height = picture_buffer_.size().height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    if (use_fp16) {
      desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    } else {
      desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = decoder.use_keyed_mutex_
                         ? D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX
                         : D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = decoder.d3d11_device_->CreateTexture2D(
        &desc, nullptr, &dx11_decoding_texture_);
    RETURN_ON_HR_FAILURE(hr, "Failed to create texture", false);
    if (decoder.use_keyed_mutex_) {
      hr = dx11_decoding_texture_.As(&dx11_keyed_mutex_);
      RETURN_ON_HR_FAILURE(hr, "Failed to get keyed mutex", false);
    }

    Microsoft::WRL::ComPtr<IDXGIResource> resource;
    hr = dx11_decoding_texture_.As(&resource);
    DCHECK(SUCCEEDED(hr));
    hr = resource->GetSharedHandle(&texture_share_handle_);
    RETURN_ON_FAILURE(SUCCEEDED(hr) && texture_share_handle_,
                      "Failed to query shared handle", false);

  } else {
    HRESULT hr = E_FAIL;
    hr = decoder.d3d9_device_ex_->CreateTexture(
        picture_buffer_.size().width(), picture_buffer_.size().height(), 1,
        D3DUSAGE_RENDERTARGET, use_rgb ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &decoding_texture_, &texture_share_handle_);
    RETURN_ON_HR_FAILURE(hr, "Failed to create texture", false);
    RETURN_ON_FAILURE(texture_share_handle_, "Failed to query shared handle",
                      false);
  }
  return true;
}

void PbufferPictureBuffer::ResetReuseFence() {
  DCHECK_EQ(IN_CLIENT, state_);
  if (!reuse_fence_ || !reuse_fence_->ResetSupported())
    reuse_fence_ = gl::GLFence::Create();
  else
    reuse_fence_->ResetState();
  state_ = WAITING_TO_REUSE;
}

bool PbufferPictureBuffer::CopyOutputSampleDataToPictureBuffer(
    DXVAVideoDecodeAccelerator* decoder,
    IDirect3DSurface9* dest_surface,
    ID3D11Texture2D* dx11_texture,
    int input_buffer_id) {
  DCHECK_EQ(BOUND, state_);
  state_ = COPYING;
  DCHECK(dest_surface || dx11_texture);
  if (dx11_texture) {
    // Grab a reference on the decoder texture. This reference will be released
    // when we receive a notification that the copy was completed or when the
    // DXVAPictureBuffer instance is destroyed.
    decoder_dx11_texture_ = dx11_texture;
    if (!decoder->CopyTexture(dx11_texture, dx11_decoding_texture_.Get(),
                              dx11_keyed_mutex_, keyed_mutex_value_, id(),
                              input_buffer_id, color_space_)) {
      // |this| might be destroyed.
      return false;
    }
    return true;
  }
  D3DSURFACE_DESC surface_desc;
  HRESULT hr = dest_surface->GetDesc(&surface_desc);
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface description", false);

  D3DSURFACE_DESC texture_desc;
  decoding_texture_->GetLevelDesc(0, &texture_desc);

  if (texture_desc.Width != surface_desc.Width ||
      texture_desc.Height != surface_desc.Height) {
    NOTREACHED() << "Decode surface of different dimension than texture";
    return false;
  }

  hr = decoder->d3d9_->CheckDeviceFormatConversion(
      D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, surface_desc.Format,
      use_rgb_ ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8);
  RETURN_ON_HR_FAILURE(hr, "Device does not support format converision", false);

  // The same picture buffer can be reused for a different frame. Release the
  // target surface and the decoder references here.
  target_surface_.Reset();
  decoder_surface_.Reset();

  // Grab a reference on the decoder surface and the target surface. These
  // references will be released when we receive a notification that the
  // copy was completed or when the DXVAPictureBuffer instance is destroyed.
  // We hold references here as it is easier to manage their lifetimes.
  hr = decoding_texture_->GetSurfaceLevel(0, &target_surface_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get surface from texture", false);

  decoder_surface_ = dest_surface;

  decoder->CopySurface(decoder_surface_.Get(), target_surface_.Get(), id(),
                       input_buffer_id, color_space_);
  color_space_ = gfx::ColorSpace();
  return true;
}

gl::GLFence* PbufferPictureBuffer::reuse_fence() {
  return reuse_fence_.get();
}

bool PbufferPictureBuffer::CopySurfaceComplete(
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface) {
  DCHECK_EQ(COPYING, state_);
  state_ = IN_CLIENT;

  GLint current_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

  glBindTexture(GL_TEXTURE_2D, picture_buffer_.service_texture_ids()[0]);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  if (src_surface && dest_surface) {
    DCHECK_EQ(src_surface, decoder_surface_.Get());
    DCHECK_EQ(dest_surface, target_surface_.Get());
    decoder_surface_.Reset();
    target_surface_.Reset();
  } else {
    DCHECK(decoder_dx11_texture_.Get());
    decoder_dx11_texture_.Reset();
  }
  if (egl_keyed_mutex_) {
    keyed_mutex_value_++;
    HRESULT result =
        egl_keyed_mutex_->AcquireSync(keyed_mutex_value_, kAcquireSyncWaitMs);
    RETURN_ON_FAILURE(result == S_OK, "Could not acquire sync mutex", false);
  }

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  eglBindTexImage(egl_display, decoding_surface_, EGL_BACK_BUFFER);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, current_texture);
  return true;
}

bool PbufferPictureBuffer::AllowOverlay() const {
  return false;
}

bool PbufferPictureBuffer::CanBindSamples() const {
  return false;
}

PbufferPictureBuffer::PbufferPictureBuffer(const PictureBuffer& buffer)
    : DXVAPictureBuffer(buffer),
      decoding_surface_(NULL),
      texture_share_handle_(nullptr),
      keyed_mutex_value_(0),
      use_rgb_(true) {}

PbufferPictureBuffer::~PbufferPictureBuffer() {
  // decoding_surface_ will be deleted by gl_image_.
}

bool PbufferPictureBuffer::ReusePictureBuffer() {
  DCHECK_NE(UNUSED, state_);
  DCHECK(decoding_surface_);
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  eglReleaseTexImage(egl_display, decoding_surface_, EGL_BACK_BUFFER);

  decoder_surface_.Reset();
  target_surface_.Reset();
  decoder_dx11_texture_.Reset();
  state_ = UNUSED;
  if (egl_keyed_mutex_) {
    HRESULT hr = egl_keyed_mutex_->ReleaseSync(++keyed_mutex_value_);
    RETURN_ON_FAILURE(hr == S_OK, "Could not release sync mutex", false);
  }
  return true;
}

EGLStreamPictureBuffer::EGLStreamPictureBuffer(const PictureBuffer& buffer)
    : DXVAPictureBuffer(buffer), stream_(nullptr) {}

EGLStreamPictureBuffer::~EGLStreamPictureBuffer() {
  // stream_ will be deleted by gl_image_.
}

bool EGLStreamPictureBuffer::Initialize() {
  RETURN_ON_FAILURE(picture_buffer_.service_texture_ids().size() >= 2,
                    "Not enough texture ids provided", false);

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLint stream_attributes[] = {
      EGL_CONSUMER_LATENCY_USEC_KHR,
      0,
      EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR,
      0,
      EGL_NONE,
  };
  stream_ = eglCreateStreamKHR(egl_display, stream_attributes);
  RETURN_ON_FAILURE(!!stream_, "Could not create stream", false);
  gl_image_ = base::MakeRefCounted<gl::GLImageDXGI>(size(), stream_);
  gl::ScopedActiveTexture texture0(GL_TEXTURE0);
  gl::ScopedTextureBinder texture0_binder(
      GL_TEXTURE_EXTERNAL_OES, picture_buffer_.service_texture_ids()[0]);
  gl::ScopedActiveTexture texture1(GL_TEXTURE1);
  gl::ScopedTextureBinder texture1_binder(
      GL_TEXTURE_EXTERNAL_OES, picture_buffer_.service_texture_ids()[1]);

  EGLAttrib consumer_attributes[] = {
      EGL_COLOR_BUFFER_TYPE,
      EGL_YUV_BUFFER_EXT,
      EGL_YUV_NUMBER_OF_PLANES_EXT,
      2,
      EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
      0,
      EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
      1,
      EGL_NONE,
  };
  EGLBoolean result = eglStreamConsumerGLTextureExternalAttribsNV(
      egl_display, stream_, consumer_attributes);
  RETURN_ON_FAILURE(result, "Could not set stream consumer", false);

  EGLAttrib producer_attributes[] = {
      EGL_NONE,
  };

  result = eglCreateStreamProducerD3DTextureANGLE(egl_display, stream_,
                                                  producer_attributes);
  RETURN_ON_FAILURE(result, "Could not create stream producer", false);
  return true;
}

bool EGLStreamPictureBuffer::ReusePictureBuffer() {
  DCHECK_NE(UNUSED, state_);
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  if (stream_) {
    EGLBoolean result = eglStreamConsumerReleaseKHR(egl_display, stream_);
    RETURN_ON_FAILURE(result, "Could not release stream", false);
  }
  if (current_d3d_sample_) {
    dx11_decoding_texture_.Reset();
    current_d3d_sample_.Reset();
  }
  state_ = UNUSED;
  return true;
}

bool EGLStreamPictureBuffer::BindSampleToTexture(
    DXVAVideoDecodeAccelerator* decoder,
    Microsoft::WRL::ComPtr<IMFSample> sample) {
  DCHECK_EQ(BOUND, state_);
  state_ = IN_CLIENT;

  current_d3d_sample_ = sample;
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  HRESULT hr = current_d3d_sample_->GetBufferByIndex(0, &output_buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from output sample", false);

  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  hr = output_buffer.As(&dxgi_buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to get DXGIBuffer from output sample",
                       false);
  hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&dx11_decoding_texture_));
  RETURN_ON_HR_FAILURE(hr, "Failed to get texture from output sample", false);
  UINT subresource;
  dxgi_buffer->GetSubresourceIndex(&subresource);

  EGLAttrib frame_attributes[] = {
      EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, subresource, EGL_NONE,
  };

  EGLBoolean result = eglStreamPostD3DTextureANGLE(
      egl_display, stream_, static_cast<void*>(dx11_decoding_texture_.Get()),
      frame_attributes);
  RETURN_ON_FAILURE(result, "Could not post texture", false);
  result = eglStreamConsumerAcquireKHR(egl_display, stream_);
  RETURN_ON_FAILURE(result, "Could not post acquire stream", false);
  gl::GLImageDXGI* gl_image_dxgi =
      static_cast<gl::GLImageDXGI*>(gl_image_.get());
  DCHECK(gl_image_dxgi);

  gl_image_dxgi->SetTexture(dx11_decoding_texture_, subresource);
  return true;
}

bool EGLStreamPictureBuffer::AllowOverlay() const {
  return true;
}

bool EGLStreamPictureBuffer::CanBindSamples() const {
  return true;
}

EGLStreamDelayedCopyPictureBuffer::EGLStreamDelayedCopyPictureBuffer(
    const PictureBuffer& buffer)
    : DXVAPictureBuffer(buffer), stream_(nullptr) {}

EGLStreamDelayedCopyPictureBuffer::~EGLStreamDelayedCopyPictureBuffer() {
  // stream_ will be deleted by gl_image_.
}

bool EGLStreamDelayedCopyPictureBuffer::Initialize(
    const DXVAVideoDecodeAccelerator& decoder) {
  RETURN_ON_FAILURE(picture_buffer_.service_texture_ids().size() >= 2,
                    "Not enough texture ids provided", false);

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLint stream_attributes[] = {
      EGL_CONSUMER_LATENCY_USEC_KHR,
      0,
      EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR,
      0,
      EGL_NONE,
  };
  stream_ = eglCreateStreamKHR(egl_display, stream_attributes);
  RETURN_ON_FAILURE(!!stream_, "Could not create stream", false);
  gl::ScopedActiveTexture texture0(GL_TEXTURE0);
  gl::ScopedTextureBinder texture0_binder(
      GL_TEXTURE_EXTERNAL_OES, picture_buffer_.service_texture_ids()[0]);
  gl::ScopedActiveTexture texture1(GL_TEXTURE1);
  gl::ScopedTextureBinder texture1_binder(
      GL_TEXTURE_EXTERNAL_OES, picture_buffer_.service_texture_ids()[1]);

  EGLAttrib consumer_attributes[] = {
      EGL_COLOR_BUFFER_TYPE,
      EGL_YUV_BUFFER_EXT,
      EGL_YUV_NUMBER_OF_PLANES_EXT,
      2,
      EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
      0,
      EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
      1,
      EGL_NONE,
  };
  EGLBoolean result = eglStreamConsumerGLTextureExternalAttribsNV(
      egl_display, stream_, consumer_attributes);
  RETURN_ON_FAILURE(result, "Could not set stream consumer", false);

  EGLAttrib producer_attributes[] = {
      EGL_NONE,
  };

  result = eglCreateStreamProducerD3DTextureANGLE(egl_display, stream_,
                                                  producer_attributes);
  RETURN_ON_FAILURE(result, "Could not create stream producer", false);
  scoped_refptr<gl::CopyingGLImageDXGI> copying_image_ =
      base::MakeRefCounted<gl::CopyingGLImageDXGI>(
          ComD3D11Device(decoder.D3D11Device()), size(), stream_);
  gl_image_ = copying_image_;
  return copying_image_->Initialize();
}

bool EGLStreamDelayedCopyPictureBuffer::ReusePictureBuffer() {
  DCHECK_NE(UNUSED, state_);

  static_cast<gl::CopyingGLImageDXGI*>(gl_image_.get())->UnbindFromTexture();
  if (current_d3d_sample_) {
    dx11_decoding_texture_.Reset();
    current_d3d_sample_.Reset();
  }
  state_ = UNUSED;
  return true;
}

bool EGLStreamDelayedCopyPictureBuffer::BindSampleToTexture(
    DXVAVideoDecodeAccelerator* decoder,
    Microsoft::WRL::ComPtr<IMFSample> sample) {
  DCHECK_EQ(BOUND, state_);
  state_ = IN_CLIENT;

  current_d3d_sample_ = sample;

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  HRESULT hr = current_d3d_sample_->GetBufferByIndex(0, &output_buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from output sample", false);

  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  hr = output_buffer.As(&dxgi_buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to get DXGIBuffer from output sample",
                       false);
  hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&dx11_decoding_texture_));
  RETURN_ON_HR_FAILURE(hr, "Failed to get texture from output sample", false);
  UINT subresource;
  dxgi_buffer->GetSubresourceIndex(&subresource);
  if (!decoder->InitializeID3D11VideoProcessor(size().width(), size().height(),
                                               color_space_))
    return false;

  DCHECK(decoder->d3d11_processor_);
  DCHECK(decoder->enumerator_);

  gl::CopyingGLImageDXGI* gl_image_dxgi =
      static_cast<gl::CopyingGLImageDXGI*>(gl_image_.get());
  DCHECK(gl_image_dxgi);

  gl_image_dxgi->SetTexture(dx11_decoding_texture_, subresource);
  return gl_image_dxgi->InitializeVideoProcessor(decoder->d3d11_processor_,
                                                 decoder->enumerator_);
}

bool EGLStreamDelayedCopyPictureBuffer::AllowOverlay() const {
  return true;
}

bool EGLStreamDelayedCopyPictureBuffer::CanBindSamples() const {
  return true;
}

EGLStreamCopyPictureBuffer::EGLStreamCopyPictureBuffer(
    const PictureBuffer& buffer)
    : DXVAPictureBuffer(buffer), stream_(nullptr) {}

EGLStreamCopyPictureBuffer::~EGLStreamCopyPictureBuffer() {
  // stream_ will be deleted by gl_image_.
}

bool EGLStreamCopyPictureBuffer::Initialize(
    const DXVAVideoDecodeAccelerator& decoder) {
  RETURN_ON_FAILURE(picture_buffer_.service_texture_ids().size() >= 2,
                    "Not enough texture ids provided", false);

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLint stream_attributes[] = {
      EGL_CONSUMER_LATENCY_USEC_KHR,
      0,
      EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR,
      0,
      EGL_NONE,
  };
  stream_ = eglCreateStreamKHR(egl_display, stream_attributes);
  RETURN_ON_FAILURE(!!stream_, "Could not create stream", false);
  gl_image_ = base::MakeRefCounted<gl::GLImageDXGI>(size(), stream_);
  gl::ScopedActiveTexture texture0(GL_TEXTURE0);
  gl::ScopedTextureBinder texture0_binder(
      GL_TEXTURE_EXTERNAL_OES, picture_buffer_.service_texture_ids()[0]);
  gl::ScopedActiveTexture texture1(GL_TEXTURE1);
  gl::ScopedTextureBinder texture1_binder(
      GL_TEXTURE_EXTERNAL_OES, picture_buffer_.service_texture_ids()[1]);

  EGLAttrib consumer_attributes[] = {
      EGL_COLOR_BUFFER_TYPE,
      EGL_YUV_BUFFER_EXT,
      EGL_YUV_NUMBER_OF_PLANES_EXT,
      2,
      EGL_YUV_PLANE0_TEXTURE_UNIT_NV,
      0,
      EGL_YUV_PLANE1_TEXTURE_UNIT_NV,
      1,
      EGL_NONE,
  };
  EGLBoolean result = eglStreamConsumerGLTextureExternalAttribsNV(
      egl_display, stream_, consumer_attributes);
  RETURN_ON_FAILURE(result, "Could not set stream consumer", false);

  EGLAttrib producer_attributes[] = {
      EGL_NONE,
  };

  result = eglCreateStreamProducerD3DTextureANGLE(egl_display, stream_,
                                                  producer_attributes);
  RETURN_ON_FAILURE(result, "Could not create stream producer", false);

  DCHECK(decoder.use_keyed_mutex_);
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = picture_buffer_.size().width();
  desc.Height = picture_buffer_.size().height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  HRESULT hr = decoder.d3d11_device_->CreateTexture2D(&desc, nullptr,
                                                      &decoder_copy_texture_);
  RETURN_ON_HR_FAILURE(hr, "Failed to create texture", false);
  DCHECK(decoder.use_keyed_mutex_);
  hr = decoder_copy_texture_.As(&dx11_keyed_mutex_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get keyed mutex", false);

  Microsoft::WRL::ComPtr<IDXGIResource> resource;
  hr = decoder_copy_texture_.As(&resource);
  DCHECK(SUCCEEDED(hr));
  hr = resource->GetSharedHandle(&texture_share_handle_);
  RETURN_ON_FAILURE(SUCCEEDED(hr) && texture_share_handle_,
                    "Failed to query shared handle", false);

  hr = decoder.angle_device_->OpenSharedResource(
      texture_share_handle_, IID_PPV_ARGS(&angle_copy_texture_));
  RETURN_ON_HR_FAILURE(hr, "Failed to open shared resource", false);
  hr = angle_copy_texture_.As(&egl_keyed_mutex_);
  RETURN_ON_HR_FAILURE(hr, "Failed to get ANGLE mutex", false);
  return true;
}

bool EGLStreamCopyPictureBuffer::CopyOutputSampleDataToPictureBuffer(
    DXVAVideoDecodeAccelerator* decoder,
    IDirect3DSurface9* dest_surface,
    ID3D11Texture2D* dx11_texture,
    int input_buffer_id) {
  DCHECK_EQ(BOUND, state_);
  state_ = COPYING;
  DCHECK(dx11_texture);
  // Grab a reference on the decoder texture. This reference will be released
  // when we receive a notification that the copy was completed or when the
  // DXVAPictureBuffer instance is destroyed.
  dx11_decoding_texture_ = dx11_texture;
  if (!decoder->CopyTexture(dx11_texture, decoder_copy_texture_.Get(),
                            dx11_keyed_mutex_, keyed_mutex_value_, id(),
                            input_buffer_id, color_space_)) {
    // |this| might be destroyed
    return false;
  }
  // The texture copy will acquire the current keyed mutex value and release
  // with the value + 1.
  keyed_mutex_value_++;
  return true;
}

bool EGLStreamCopyPictureBuffer::CopySurfaceComplete(
    IDirect3DSurface9* src_surface,
    IDirect3DSurface9* dest_surface) {
  DCHECK(!src_surface);
  DCHECK(!dest_surface);
  DCHECK_EQ(COPYING, state_);
  state_ = IN_CLIENT;

  dx11_decoding_texture_.Reset();

  HRESULT hr =
      egl_keyed_mutex_->AcquireSync(keyed_mutex_value_, kAcquireSyncWaitMs);
  RETURN_ON_FAILURE(hr == S_OK, "Could not acquire sync mutex", false);

  EGLAttrib frame_attributes[] = {
      EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, 0, EGL_NONE,
  };

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  EGLBoolean result = eglStreamPostD3DTextureANGLE(
      egl_display, stream_, static_cast<void*>(angle_copy_texture_.Get()),
      frame_attributes);
  RETURN_ON_FAILURE(result, "Could not post stream", false);
  result = eglStreamConsumerAcquireKHR(egl_display, stream_);
  RETURN_ON_FAILURE(result, "Could not post acquire stream", false);
  gl::GLImageDXGI* gl_image_dxgi =
      static_cast<gl::GLImageDXGI*>(gl_image_.get());
  DCHECK(gl_image_dxgi);

  gl_image_dxgi->SetTexture(angle_copy_texture_, 0);

  return true;
}

bool EGLStreamCopyPictureBuffer::ReusePictureBuffer() {
  DCHECK_NE(UNUSED, state_);
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  if (state_ == IN_CLIENT) {
    HRESULT hr = egl_keyed_mutex_->ReleaseSync(++keyed_mutex_value_);
    RETURN_ON_FAILURE(hr == S_OK, "Could not release sync mutex", false);
  }
  state_ = UNUSED;

  if (stream_) {
    EGLBoolean result = eglStreamConsumerReleaseKHR(egl_display, stream_);
    RETURN_ON_FAILURE(result, "Could not release stream", false);
  }
  return true;
}

bool EGLStreamCopyPictureBuffer::AllowOverlay() const {
  return true;
}

bool EGLStreamCopyPictureBuffer::CanBindSamples() const {
  return false;
}

}  // namespace media
