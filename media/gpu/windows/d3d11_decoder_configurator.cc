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
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/av1_guids.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/direct_composition_support.h"

namespace media {

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

  DXGI_FORMAT decoder_dxgi_format = DXGI_FORMAT_UNKNOWN;
  // Assume YUV420 format.
  switch (bit_depth) {
    case 8:
      decoder_dxgi_format = DXGI_FORMAT_NV12;
      break;
    case 10:
      decoder_dxgi_format = DXGI_FORMAT_P010;
      break;
    case 12:
      decoder_dxgi_format = DXGI_FORMAT_P016;
      break;
    default:
      MEDIA_LOG(WARNING, media_log)
          << "D3D11VideoDecoder does not support bit depth "
          << base::strict_cast<int>(bit_depth);
      return nullptr;
  }

  GUID decoder_guid = {};
  if (config.codec() == VideoCodec::kH264) {
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
  }
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  else if (config.profile() == HEVCPROFILE_MAIN) {
    decoder_guid = D3D11_DECODER_PROFILE_HEVC_VLD_MAIN;
  } else if (config.profile() == HEVCPROFILE_MAIN10) {
    decoder_guid = D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10;
  } else if (config.profile() == HEVCPROFILE_REXT) {
    // TODO(crbug.com/1345568): Enable 8-bit 444 decoding when AYUV
    // is added into video pixel format histogram enumerations.
    if (bit_depth == 8) {
      if (chroma_sampling == VideoChromaSampling::k420) {
        decoder_guid = DXVA_ModeHEVC_VLD_Main_Intel;
        decoder_dxgi_format = DXGI_FORMAT_NV12;
      } else {
        MEDIA_LOG(INFO, media_log)
            << "D3D11VideoDecoder does not support HEVC range extension "
            << config.codec() << " with chroma subsampling format "
            << VideoChromaSamplingToString(chroma_sampling) << " and bit depth "
            << base::strict_cast<int>(bit_depth);
        return nullptr;
      }
    } else if (bit_depth == 10) {
      if (chroma_sampling == VideoChromaSampling::k420) {
        decoder_guid = DXVA_ModeHEVC_VLD_Main10_Intel;
        decoder_dxgi_format = DXGI_FORMAT_P010;
      } else if (chroma_sampling == VideoChromaSampling::k422) {
        decoder_guid = DXVA_ModeHEVC_VLD_Main422_10_Intel;
        decoder_dxgi_format = DXGI_FORMAT_Y210;
      } else if (chroma_sampling == VideoChromaSampling::k444) {
        decoder_guid = DXVA_ModeHEVC_VLD_Main444_10_Intel;
        decoder_dxgi_format = DXGI_FORMAT_Y410;
      }
    } else if (bit_depth == 12) {
      // TODO(crbug.com/1345568): Enable 12-bit 422/444 decoding.
      // 12-bit decoding with 422 & 444 format does not work well
      // on Intel platforms.
      if (chroma_sampling == VideoChromaSampling::k420) {
        decoder_guid = DXVA_ModeHEVC_VLD_Main12_Intel;
        decoder_dxgi_format = DXGI_FORMAT_P016;
      } else {
        MEDIA_LOG(INFO, media_log)
            << "D3D11VideoDecoder does not support HEVC range extension "
            << config.codec() << " with chroma subsampling format "
            << VideoChromaSamplingToString(chroma_sampling) << " and bit depth "
            << base::strict_cast<int>(bit_depth);
        return nullptr;
      }
    }
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  else {
    MEDIA_LOG(INFO, media_log)
        << "D3D11VideoDecoder does not support codec " << config.codec();
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
    // Update the decoder output texture usage to support shared handle and
    // keyed_mutex if required. SwapChain should be disabled and the frame
    // shouldn't be encrypted.
    DCHECK(!supports_swap_chain_);
    DCHECK(!is_encrypted_);
    output_texture_desc_.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                                     D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
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
