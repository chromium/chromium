// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_decoder_configurator.h"

#include <d3d11.h>
#include <d3d9.h>
#include <dxva2api.h>

#include "base/feature_list.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/status_codes.h"
#include "media/base/win/hresult_status_helper.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/av1_guids.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/direct_composition_surface_win.h"

namespace media {

D3D11DecoderConfigurator::D3D11DecoderConfigurator(
    DXGI_FORMAT decoder_output_dxgifmt,
    GUID decoder_guid,
    gfx::Size coded_size,
    bool is_encrypted,
    bool supports_swap_chain)
    : dxgi_format_(decoder_output_dxgifmt), decoder_guid_(decoder_guid) {
  SetUpDecoderDescriptor(coded_size);
  SetUpTextureDescriptor(supports_swap_chain, is_encrypted);
}

// static
std::unique_ptr<D3D11DecoderConfigurator> D3D11DecoderConfigurator::Create(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const VideoDecoderConfig& config,
    uint8_t bit_depth,
    MediaLog* media_log) {
  const bool supports_nv12_decode_swap_chain =
      gl::DirectCompositionSurfaceWin::IsDecodeSwapChainSupported();
  const auto decoder_dxgi_format =
      bit_depth == 8 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_P010;
  GUID decoder_guid = {};
  if (config.codec() == kCodecH264) {
    decoder_guid = D3D11_DECODER_PROFILE_H264_VLD_NOFGT;
  } else if (config.profile() == VP9PROFILE_PROFILE0) {
    decoder_guid = D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0;
  } else if (config.profile() == VP9PROFILE_PROFILE2) {
    decoder_guid = D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2;
  } else if (config.profile() == AV1PROFILE_PROFILE_MAIN) {
    decoder_guid = DXVA_ModeAV1_VLD_Profile0;
  } else if (config.profile() == AV1PROFILE_PROFILE_HIGH) {
    decoder_guid = DXVA_ModeAV1_VLD_Profile1;
  } else if (config.profile() == AV1PROFILE_PROFILE_PRO) {
    decoder_guid = DXVA_ModeAV1_VLD_Profile2;
  } else {
    MEDIA_LOG(INFO, media_log)
        << "D3D11VideoDecoder does not support codec " << config.codec();
    return nullptr;
  }

  MEDIA_LOG(INFO, media_log)
      << "D3D11VideoDecoder is using " << GetProfileName(config.profile())
      << " / " << (decoder_dxgi_format == DXGI_FORMAT_NV12 ? "NV12" : "P010");

  return std::make_unique<D3D11DecoderConfigurator>(
      decoder_dxgi_format, decoder_guid, config.coded_size(),
      config.is_encrypted(), supports_nv12_decode_swap_chain);
}

bool D3D11DecoderConfigurator::SupportsDevice(
    ComD3D11VideoDevice video_device) {
  for (UINT i = video_device->GetVideoDecoderProfileCount(); i--;) {
    GUID profile = {};
    if (SUCCEEDED(video_device->GetVideoDecoderProfile(i, &profile))) {
      if (profile == decoder_guid_)
        return true;
    }
  }
  return false;
}

StatusOr<ComD3D11Texture2D> D3D11DecoderConfigurator::CreateOutputTexture(
    ComD3D11Device device,
    gfx::Size size,
    uint32_t array_size) {
  output_texture_desc_.Width = size.width();
  output_texture_desc_.Height = size.height();
  output_texture_desc_.ArraySize = array_size;

  ComD3D11Texture2D texture;
  HRESULT hr =
      device->CreateTexture2D(&output_texture_desc_, nullptr, &texture);
  if (FAILED(hr)) {
    return Status(StatusCode::kCreateDecoderOutputTextureFailed)
        .AddCause(HresultToStatus(hr));
  }
  hr = SetDebugName(texture.Get(), "D3D11Decoder_ConfiguratorOutput");
  if (FAILED(hr)) {
    return Status(StatusCode::kCreateDecoderOutputTextureFailed)
        .AddCause(HresultToStatus(hr));
  }
  return texture;
}

// private
void D3D11DecoderConfigurator::SetUpDecoderDescriptor(
    const gfx::Size& coded_size) {
  decoder_desc_ = {};
  decoder_desc_.Guid = decoder_guid_;
  decoder_desc_.SampleWidth = coded_size.width();
  decoder_desc_.SampleHeight = coded_size.height();
  decoder_desc_.OutputFormat = dxgi_format_;
}

// private
void D3D11DecoderConfigurator::SetUpTextureDescriptor(bool supports_swap_chain,
                                                      bool is_encrypted) {
  output_texture_desc_ = {};
  output_texture_desc_.MipLevels = 1;
  output_texture_desc_.Format = dxgi_format_;
  output_texture_desc_.SampleDesc.Count = 1;
  output_texture_desc_.Usage = D3D11_USAGE_DEFAULT;
  output_texture_desc_.BindFlags =
      D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;

  // Decode swap chains do not support shared resources.
  // TODO(sunnyps): Find a workaround for when the decoder moves to its own
  // thread and D3D device.  See https://crbug.com/911847
  // TODO(liberato): This depends on the configuration of the TextureSelector,
  // to some degree. We should unset the flag only if it's binding and the
  // decode swap chain is supported, as Intel driver is buggy on Gen9 and older
  // devices without the flag. See https://crbug.com/1107403
  output_texture_desc_.MiscFlags =
      supports_swap_chain ? 0 : D3D11_RESOURCE_MISC_SHARED;

  if (is_encrypted)
    output_texture_desc_.MiscFlags |= D3D11_RESOURCE_MISC_HW_PROTECTED;
}

}  // namespace media
