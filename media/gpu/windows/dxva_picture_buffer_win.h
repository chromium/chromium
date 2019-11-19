// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_DXVA_PICTURE_BUFFER_WIN_H_
#define MEDIA_GPU_WINDOWS_DXVA_PICTURE_BUFFER_WIN_H_

#include <d3d11.h>
#include <d3d9.h>
#include <mfidl.h>
#include <wrl/client.h>

#include <memory>

#include "media/gpu/windows/d3d11_com_defs.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image.h"

interface IMFSample;

namespace media {
class DXVAVideoDecodeAccelerator;

// Maintains information about a DXVA picture buffer, i.e. whether it is
// available for rendering, the texture information, etc.
class DXVAPictureBuffer {
 public:
  enum State { UNUSED, BOUND, COPYING, IN_CLIENT, WAITING_TO_REUSE };
  static std::unique_ptr<DXVAPictureBuffer> Create(
      const DXVAVideoDecodeAccelerator& decoder,
      const PictureBuffer& buffer,
      EGLConfig egl_config);
  virtual ~DXVAPictureBuffer();

  virtual bool ReusePictureBuffer() = 0;
  virtual void ResetReuseFence();
  // Copies the output sample data to the picture buffer provided by the
  // client.
  // The dest_surface parameter contains the decoded bits.
  virtual bool CopyOutputSampleDataToPictureBuffer(
      DXVAVideoDecodeAccelerator* decoder,
      IDirect3DSurface9* dest_surface,
      ID3D11Texture2D* dx11_texture,
      int input_buffer_id);

  bool available() const { return state_ == UNUSED; }
  State state() const { return state_; }
  const PictureBuffer& picture_buffer() const { return picture_buffer_; }

  int id() const { return picture_buffer_.id(); }

  gfx::Size size() const { return picture_buffer_.size(); }
  void set_bound();

  scoped_refptr<gl::GLImage> gl_image() { return gl_image_; }

  const gfx::Rect& visible_rect() const { return visible_rect_; }
  void set_visible_rect(const gfx::Rect& visible_rect) {
    visible_rect_ = visible_rect;
  }

  const gfx::ColorSpace& color_space() const { return color_space_; }
  void set_color_space(const gfx::ColorSpace& color_space) {
    color_space_ = color_space;
  }

  // Returns true if these could in theory be used as an overlay. May
  // still be drawn using GL depending on the scene and precise hardware
  // support.
  virtual bool AllowOverlay() const = 0;

  // Returns true if BindSampleToTexture should be used. Otherwise
  // CopyOutputSampleDataToPicture should be used.
  virtual bool CanBindSamples() const = 0;

  bool waiting_to_reuse() const { return state_ == WAITING_TO_REUSE; }
  virtual gl::GLFence* reuse_fence();

  // Called when the source surface |src_surface| is copied to the destination
  // |dest_surface|
  virtual bool CopySurfaceComplete(IDirect3DSurface9* src_surface,
                                   IDirect3DSurface9* dest_surface);
  virtual bool BindSampleToTexture(DXVAVideoDecodeAccelerator* decoder,
                                   Microsoft::WRL::ComPtr<IMFSample> sample);

 protected:
  explicit DXVAPictureBuffer(const PictureBuffer& buffer);

  State state_ = UNUSED;
  PictureBuffer picture_buffer_;
  gfx::Rect visible_rect_;
  gfx::ColorSpace color_space_;
  scoped_refptr<gl::GLImage> gl_image_;

  DISALLOW_COPY_AND_ASSIGN(DXVAPictureBuffer);
};

// Copies the video result into an RGBA EGL pbuffer.
class PbufferPictureBuffer : public DXVAPictureBuffer {
 public:
  explicit PbufferPictureBuffer(const PictureBuffer& buffer);
  ~PbufferPictureBuffer() override;

  bool Initialize(const DXVAVideoDecodeAccelerator& decoder,
                  EGLConfig egl_config);
  bool InitializeTexture(const DXVAVideoDecodeAccelerator& decoder,
                         bool use_rgb,
                         bool use_fp16);

  bool ReusePictureBuffer() override;
  void ResetReuseFence() override;
  bool CopyOutputSampleDataToPictureBuffer(DXVAVideoDecodeAccelerator* decoder,
                                           IDirect3DSurface9* dest_surface,
                                           ID3D11Texture2D* dx11_texture,
                                           int input_buffer_id) override;
  gl::GLFence* reuse_fence() override;
  bool CopySurfaceComplete(IDirect3DSurface9* src_surface,
                           IDirect3DSurface9* dest_surface) override;
  bool AllowOverlay() const override;
  bool CanBindSamples() const override;

 protected:
  EGLSurface decoding_surface_;

  std::unique_ptr<gl::GLFence> reuse_fence_;

  HANDLE texture_share_handle_;
  Microsoft::WRL::ComPtr<IDirect3DTexture9> decoding_texture_;
  ComD3D11Texture2D dx11_decoding_texture_;

  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> egl_keyed_mutex_;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dx11_keyed_mutex_;

  // This is the last value that was used to release the keyed mutex.
  uint64_t keyed_mutex_value_;

  // The following |IDirect3DSurface9| interface pointers are used to hold
  // references on the surfaces during the course of a StretchRect operation
  // to copy the source surface to the target. The references are released
  // when the StretchRect operation i.e. the copy completes.
  Microsoft::WRL::ComPtr<IDirect3DSurface9> decoder_surface_;
  Microsoft::WRL::ComPtr<IDirect3DSurface9> target_surface_;

  // This ID3D11Texture2D interface pointer is used to hold a reference to the
  // decoder texture during the course of a copy operation. This reference is
  // released when the copy completes.
  ComD3D11Texture2D decoder_dx11_texture_;

  // Set to true if RGB is supported by the texture.
  // Defaults to true.
  bool use_rgb_;
};

// Shares the decoded texture with ANGLE without copying by using an EGL stream.
class EGLStreamPictureBuffer : public DXVAPictureBuffer {
 public:
  explicit EGLStreamPictureBuffer(const PictureBuffer& buffer);
  ~EGLStreamPictureBuffer() override;

  bool Initialize();
  bool ReusePictureBuffer() override;
  bool BindSampleToTexture(DXVAVideoDecodeAccelerator* decoder,
                           Microsoft::WRL::ComPtr<IMFSample> sample) override;
  bool AllowOverlay() const override;
  bool CanBindSamples() const override;

 private:
  EGLStreamKHR stream_;

  Microsoft::WRL::ComPtr<IMFSample> current_d3d_sample_;
  ComD3D11Texture2D dx11_decoding_texture_;
};

// Shares the decoded texture with ANGLE without copying by using an EGL stream.
class EGLStreamDelayedCopyPictureBuffer : public DXVAPictureBuffer {
 public:
  explicit EGLStreamDelayedCopyPictureBuffer(const PictureBuffer& buffer);
  ~EGLStreamDelayedCopyPictureBuffer() override;

  bool Initialize(const DXVAVideoDecodeAccelerator& decoder);
  bool ReusePictureBuffer() override;
  bool BindSampleToTexture(DXVAVideoDecodeAccelerator* decoder,
                           Microsoft::WRL::ComPtr<IMFSample> sample) override;
  bool AllowOverlay() const override;
  bool CanBindSamples() const override;

 private:
  EGLStreamKHR stream_;

  Microsoft::WRL::ComPtr<IMFSample> current_d3d_sample_;
  ComD3D11Texture2D dx11_decoding_texture_;
};

// Creates an NV12 texture and copies to it, then shares that with ANGLE.
class EGLStreamCopyPictureBuffer : public DXVAPictureBuffer {
 public:
  explicit EGLStreamCopyPictureBuffer(const PictureBuffer& buffer);
  ~EGLStreamCopyPictureBuffer() override;

  bool Initialize(const DXVAVideoDecodeAccelerator& decoder);
  bool ReusePictureBuffer() override;

  bool CopyOutputSampleDataToPictureBuffer(DXVAVideoDecodeAccelerator* decoder,
                                           IDirect3DSurface9* dest_surface,
                                           ID3D11Texture2D* dx11_texture,
                                           int input_buffer_id) override;
  bool CopySurfaceComplete(IDirect3DSurface9* src_surface,
                           IDirect3DSurface9* dest_surface) override;
  bool AllowOverlay() const override;
  bool CanBindSamples() const override;

 private:
  EGLStreamKHR stream_;

  // This ID3D11Texture2D interface pointer is used to hold a reference to the
  // MFT decoder texture during the course of a copy operation. This reference
  // is released when the copy completes.
  ComD3D11Texture2D dx11_decoding_texture_;

  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> egl_keyed_mutex_;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dx11_keyed_mutex_;

  HANDLE texture_share_handle_;
  // This is the texture (created on ANGLE's device) that will be put in the
  // EGLStream.
  ComD3D11Texture2D angle_copy_texture_;
  // This is another copy of that shared resource that will be copied to from
  // the decoder.
  ComD3D11Texture2D decoder_copy_texture_;

  // This is the last value that was used to release the keyed mutex.
  uint64_t keyed_mutex_value_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_DXVA_PICTURE_BUFFER_WIN_H_
