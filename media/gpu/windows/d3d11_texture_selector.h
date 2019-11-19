// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_TEXTURE_SELECTOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_TEXTURE_SELECTOR_H_

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <vector>

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MediaLog;

// Stores different pixel formats and DGXI formats, and checks for decoder
// GUID support.
class MEDIA_GPU_EXPORT TextureSelector {
 public:
  TextureSelector(VideoPixelFormat pixfmt,
                  DXGI_FORMAT dxgifmt,
                  GUID decoder_guid,
                  gfx::Size coded_size,
                  bool is_encrypted,
                  bool supports_swap_chain);
  virtual ~TextureSelector() = default;

  static std::unique_ptr<TextureSelector> Create(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const VideoDecoderConfig& config,
      MediaLog* media_log);

  bool SupportsDevice(Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device);
  ComD3D11Texture2D CreateOutputTexture(ComD3D11Device device, gfx::Size size);
  virtual std::unique_ptr<Texture2DWrapper> CreateTextureWrapper(
      ComD3D11Device device,
      ComD3D11VideoDevice video_device,
      ComD3D11DeviceContext,
      ComD3D11Texture2D input_texture,
      gfx::Size size);

  const D3D11_VIDEO_DECODER_DESC* DecoderDescriptor() { return &decoder_desc_; }
  const GUID DecoderGuid() { return decoder_guid_; }
  VideoPixelFormat PixelFormat() { return pixel_format_; }

  static constexpr size_t BUFFER_COUNT = 20;

 private:
  friend class CopyTextureSelector;
  // Set up instances of the parameter structs for D3D11 Functions
  void SetUpDecoderDescriptor();
  void SetUpTextureDescriptor();

  D3D11_TEXTURE2D_DESC texture_desc_;
  D3D11_VIDEO_DECODER_DESC decoder_desc_;

  const VideoPixelFormat pixel_format_;
  const DXGI_FORMAT dxgi_format_;
  const GUID decoder_guid_;
  const gfx::Size coded_size_;
  const bool is_encrypted_;
  const bool supports_swap_chain_;
};

class MEDIA_GPU_EXPORT CopyTextureSelector : public TextureSelector {
 public:
  CopyTextureSelector(VideoPixelFormat pixfmt,
                      DXGI_FORMAT input_dxgifmt,
                      DXGI_FORMAT output_dxgifmt,
                      GUID decoder_guid,
                      gfx::Size coded_size,
                      bool is_encrypted,
                      bool supports_swap_chain)
      : TextureSelector(pixfmt,
                        input_dxgifmt,
                        decoder_guid,
                        coded_size,
                        is_encrypted,
                        supports_swap_chain),
        output_dxgifmt_(output_dxgifmt) {}

  std::unique_ptr<Texture2DWrapper> CreateTextureWrapper(
      ComD3D11Device device,
      ComD3D11VideoDevice video_device,
      ComD3D11DeviceContext,
      ComD3D11Texture2D input_texture,
      gfx::Size size) override;

 private:
  DXGI_FORMAT output_dxgifmt_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_TEXTURE_SELECTOR_H_
