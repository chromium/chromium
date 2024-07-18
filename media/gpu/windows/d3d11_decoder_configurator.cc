// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_decoder_configurator.h"

#include <d3d11.h>
#include <d3d9.h>
#include <dxva2api.h>

#include "base/feature_list.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/av1_guids.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/direct_composition_support.h"

namespace media {

namespace {

GUID GetD3D11DecoderGUID(const VideoCodecProfile& profile,
                         uint8_t bit_depth,
                         VideoChromaSampling chroma_sampling) {
  switch (profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_EXTENDED:
    case H264PROFILE_HIGH:
    case H264PROFILE_HIGH10PROFILE:
    case H264PROFILE_HIGH422PROFILE:
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
    case H264PROFILE_SCALABLEBASELINE:
    case H264PROFILE_SCALABLEHIGH:
    case H264PROFILE_STEREOHIGH:
    case H264PROFILE_MULTIVIEWHIGH:
      return D3D11_DECODER_PROFILE_H264_VLD_NOFGT;
    case VP9PROFILE_PROFILE0:
      return D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0;
    case VP9PROFILE_PROFILE2:
      return D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2;
    case AV1PROFILE_PROFILE_MAIN:
      return DXVA_ModeAV1_VLD_Profile0;
    case AV1PROFILE_PROFILE_HIGH:
      return DXVA_ModeAV1_VLD_Profile1;
    case AV1PROFILE_PROFILE_PRO:
      return DXVA_ModeAV1_VLD_Profile2;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    // Per DirectX Video Acceleration Specification for High Efficiency Video
    // Coding - 7.4, DXVA_ModeHEVC_VLD_Main GUID can be used for both main and
    // main still picture profile.
    case HEVCPROFILE_MAIN:
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return D3D11_DECODER_PROFILE_HEVC_VLD_MAIN;
    case HEVCPROFILE_MAIN10:
      return D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10;
    case HEVCPROFILE_REXT:
      return GetHEVCRangeExtensionPrivateGUID(bit_depth, chroma_sampling);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    default:
      return {};
  }
}

}  // namespace

D3D11DecoderConfigurator::D3D11DecoderConfigurator(
    DXGI_FORMAT decoder_output_dxgifmt,
    GUID decoder_guid,
    gfx::Size coded_size,
    bool is_encrypted,
    bool supports_swap_chain)
    : dxgi_format_(decoder_output_dxgifmt),
      decoder_guid_(decoder_guid),
      supports_swap_chain_(supports_swap_chain),
      is_encrypted_(is_encrypted) {
  SetUpDecoderDescriptor(coded_size);
  SetUpTextureDescriptor();
}

// static
std::unique_ptr<D3D11DecoderConfigurator> D3D11DecoderConfigurator::Create(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    const VideoDecoderConfig& config,
    uint8_t bit_depth,
    VideoChromaSampling chroma_sampling,
    MediaLog* media_log,
    bool use_shared_handle) {
  // Decoder swap chains do not support shared resources. More info in
  // https://crbug.com/911847. To enable Kaby Lake+ systems for using shared
  // handle, we disable decode swap chain support if shared handle is enabled.
  const bool supports_nv12_decode_swap_chain =
      gl::DirectCompositionDecodeSwapChainSupported() && !use_shared_handle;

  DXGI_FORMAT decoder_dxgi_format =
      GetOutputDXGIFormat(bit_depth, chroma_sampling);
  if (decoder_dxgi_format == DXGI_FORMAT_UNKNOWN) {
    MEDIA_LOG(WARNING, media_log)
        << "D3D11VideoDecoder does not support bit depth "
        << base::strict_cast<int>(bit_depth)
        << " with chroma subsampling format "
        << VideoChromaSamplingToString(chroma_sampling);
    return nullptr;
  }

  GUID decoder_guid =
      GetD3D11DecoderGUID(config.profile(), bit_depth, chroma_sampling);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  // For D3D11/D3D12, 8b/10b-422 HEVC will share 10b-422 GUID no matter
  // it is defined by Intel or DXVA spec(as part of Windows SDK).
  if (decoder_guid == DXVA_ModeHEVC_VLD_Main422_10_Intel) {
    decoder_dxgi_format = DXGI_FORMAT_Y210;
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (decoder_guid == GUID()) {
    if (config.profile() == HEVCPROFILE_REXT) {
      MEDIA_LOG(INFO, media_log)
          << "D3D11VideoDecoder does not support HEVC range extension "
          << config.codec() << " with chroma subsampling format "
          << VideoChromaSamplingToString(chroma_sampling) << " and bit depth "
          << base::strict_cast<int>(bit_depth);
    } else {
      MEDIA_LOG(INFO, media_log)
          << "D3D11VideoDecoder does not support codec " << config.codec();
    }
    return nullptr;
  }

  MEDIA_LOG(INFO, media_log)
      << "D3D11VideoDecoder is using " << GetProfileName(config.profile())
      << " / " << VideoChromaSamplingToString(chroma_sampling);

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

D3D11Status::Or<ComD3D11Texture2D>
D3D11DecoderConfigurator::CreateOutputTexture(ComD3D11Device device,
                                              gfx::Size size,
                                              uint32_t array_size,
                                              bool use_shared_handle) {
  output_texture_desc_.Width = size.width();
  output_texture_desc_.Height = size.height();
  output_texture_desc_.ArraySize = array_size;

  if (use_shared_handle) {
    // Update the decoder output texture usage to support shared handle
    // if required. SwapChain should be disabled and the frame shouldn't
    // be encrypted.
    DCHECK(!supports_swap_chain_);
    DCHECK(!is_encrypted_);
    output_texture_desc_.MiscFlags =
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
  } else if (supports_swap_chain_) {
    // Decode swap chains do not support shared resources.
    // TODO(sunnyps): Find a workaround for when the decoder moves to its own
    // thread and D3D device.  See https://crbug.com/911847
    // TODO(liberato): This depends on the configuration of the TextureSelector,
    // to some degree. We should unset the flag only if it's binding and the
    // decode swap chain is supported, as Intel driver is buggy on Gen9 and
    // older devices without the flag. See https://crbug.com/1107403
    output_texture_desc_.MiscFlags = 0;
  } else {
    // Create non-shareable texture for d3d11 video decoder.
    output_texture_desc_.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  }

  if (is_encrypted_)
    output_texture_desc_.MiscFlags |= D3D11_RESOURCE_MISC_HW_PROTECTED;

  ComD3D11Texture2D texture;
  HRESULT hr =
      device->CreateTexture2D(&output_texture_desc_, nullptr, &texture);
  if (FAILED(hr))
    return {D3D11Status::Codes::kCreateDecoderOutputTextureFailed, hr};
  hr = SetDebugName(texture.Get(), "D3D11Decoder_ConfiguratorOutput");
  if (FAILED(hr))
    return {D3D11Status::Codes::kCreateDecoderOutputTextureFailed, hr};
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
void D3D11DecoderConfigurator::SetUpTextureDescriptor() {
  output_texture_desc_ = {};
  output_texture_desc_.MipLevels = 1;
  output_texture_desc_.Format = dxgi_format_;
  output_texture_desc_.SampleDesc.Count = 1;
  output_texture_desc_.Usage = D3D11_USAGE_DEFAULT;
  output_texture_desc_.BindFlags =
      D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
}

}  // namespace media
