// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_DECODER_CONFIGURATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_DECODER_CONFIGURATOR_H_

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <vector>

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class MediaLog;

// Stores different pixel formats and DGXI formats, and checks for decoder
// GUID support.  Generally provides a centralized place to figure out which
// decoder to use, and how its output texture should be configured.
class MEDIA_GPU_EXPORT D3D11DecoderConfigurator {
 public:
  D3D11DecoderConfigurator(DXGI_FORMAT decoder_output_dxgifmt,
                           GUID decoder_guid,
                           gfx::Size coded_size,
                           bool is_encrypted,
                           bool supports_swap_chain);
  virtual ~D3D11DecoderConfigurator() = default;

  static std::unique_ptr<D3D11DecoderConfigurator> Create(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const VideoDecoderConfig& config,
      uint8_t bit_depth,
      VideoChromaSampling chroma_sampling,
      MediaLog* media_log,
      bool use_shared_handle);

  bool SupportsDevice(ComD3D11VideoDevice video_device);

  // Create the decoder's output texture.
  D3D11Status::Or<ComD3D11Texture2D> CreateOutputTexture(
      ComD3D11Device device,
      gfx::Size size,
      uint32_t array_size,
      bool use_shared_handle);

  const D3D11_VIDEO_DECODER_DESC* DecoderDescriptor() const {
    return &decoder_desc_;
  }
  const GUID DecoderGuid() const { return decoder_guid_; }
  DXGI_FORMAT TextureFormat() const { return dxgi_format_; }

 private:
  // Set up instances of the parameter structs for D3D11 Functions
  void SetUpDecoderDescriptor(const gfx::Size& coded_size);
  void SetUpTextureDescriptor();

  D3D11_TEXTURE2D_DESC output_texture_desc_;
  D3D11_VIDEO_DECODER_DESC decoder_desc_;

  const DXGI_FORMAT dxgi_format_;
  const GUID decoder_guid_;

  const bool supports_swap_chain_;
  const bool is_encrypted_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_DECODER_CONFIGURATOR_H_
