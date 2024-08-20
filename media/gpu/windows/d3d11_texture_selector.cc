// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_selector.h"

#include <d3d11.h>

#include "base/feature_list.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
#include "media/gpu/windows/format_utils.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/size.h"

namespace media {

TextureSelector::TextureSelector(VideoPixelFormat pixfmt,
                                 DXGI_FORMAT output_dxgifmt,
                                 ComD3D11VideoDevice video_device,
                                 ComD3D11DeviceContext device_context,
                                 bool shared_image_use_shared_handle)
    : pixel_format_(pixfmt),
      output_dxgifmt_(output_dxgifmt),
      video_device_(std::move(video_device)),
      device_context_(std::move(device_context)),
      shared_image_use_shared_handle_(shared_image_use_shared_handle) {}

TextureSelector::~TextureSelector() = default;

bool SupportsZeroCopy(const gpu::GpuPreferences& preferences,
                      const gpu::GpuDriverBugWorkarounds& workarounds) {
  if (!preferences.enable_zero_copy_dxgi_video)
    return false;

  if (workarounds.disable_dxgi_zero_copy_video)
    return false;

  return true;
}

// static
std::unique_ptr<TextureSelector> TextureSelector::Create(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& workarounds,
    DXGI_FORMAT decoder_output_format,
    const FormatSupportChecker* format_checker,
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext device_context,
    MediaLog* media_log,
    gfx::ColorSpace input_color_space,
    bool shared_image_use_shared_handle) {
  VideoPixelFormat output_pixel_format;
  DXGI_FORMAT output_dxgi_format;

  bool needs_texture_copy = !SupportsZeroCopy(gpu_preferences, workarounds);

  auto supports_fmt = [format_checker](auto fmt) {
    return format_checker->CheckOutputFormatSupport(fmt);
  };

  switch (decoder_output_format) {
    case DXGI_FORMAT_AYUV: {
      MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder producing "
                                 << DxgiFormatToString(decoder_output_format);
      // AYUV output from decoder is always 8-bit 4:4:4 which we prefer to
      // be rendered in ARGB formats to avoid chroma downsampling. For
      // HDR contents, we should not let YUV to RGB conversion happens
      // inside D3D11VideoDecoder, the only place for the conversion
      // should be Gfx::ColorTransform or SwapChainPresenter. For color
      // spaces that VP isn't able to handle the correct color conversion,
      // the current workaround is to output a 4:2:0 YUV format and let
      // viz handle the conversion at the expense of losing 4:4:4 chroma
      // sampling. See https://crbug.com/343014700.
      if (!input_color_space.IsHDR() &&
          gfx::ColorSpaceWin::CanConvertToDXGIColorSpace(input_color_space) &&
          supports_fmt(DXGI_FORMAT_B8G8R8A8_UNORM)) {
        output_pixel_format = PIXEL_FORMAT_ARGB;
        output_dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected ARGB";
      } else if (!needs_texture_copy || supports_fmt(DXGI_FORMAT_NV12)) {
        output_pixel_format = PIXEL_FORMAT_NV12;
        output_dxgi_format = DXGI_FORMAT_NV12;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected NV12";
      } else {
        MEDIA_LOG(INFO, media_log)
            << DxgiFormatToString(decoder_output_format) << " not supported";
        return nullptr;
      }
      break;
    }
    case DXGI_FORMAT_NV12: {
      MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder producing "
                                 << DxgiFormatToString(decoder_output_format);
      if (!needs_texture_copy || supports_fmt(DXGI_FORMAT_NV12)) {
        output_pixel_format = PIXEL_FORMAT_NV12;
        output_dxgi_format = DXGI_FORMAT_NV12;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected NV12";
      } else if (supports_fmt(DXGI_FORMAT_B8G8R8A8_UNORM)) {
        output_pixel_format = PIXEL_FORMAT_ARGB;
        output_dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected ARGB";
      } else {
        MEDIA_LOG(INFO, media_log)
            << DxgiFormatToString(decoder_output_format) << " not supported";
        return nullptr;
      }
      break;
    }
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y216:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_Y210: {
      MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder producing "
                                 << DxgiFormatToString(decoder_output_format);
      // Y416/Y216/Y410/Y210 output from decoder is always 10/12-bit 4:2:2/4:4:4
      // which we prefer to be rendered in ARGB formats to avoid chroma
      // downsampling. For HDR contents, we should not let YUV to RGB
      // conversion happens inside D3D11VideoDecoder, the only place
      // for the conversion should be Gfx::ColorTransform or
      // SwapChainPresenter. For color spaces that VP isn't able to handle
      // the correct color conversion, the current workaround is to output
      // a 4:2:0 YUV format and let viz handle the conversion at the expense
      // of losing 4:4:4 chroma sampling. See https://crbug.com/343014700.
      if (!input_color_space.IsHDR() &&
          gfx::ColorSpaceWin::CanConvertToDXGIColorSpace(input_color_space) &&
          supports_fmt(DXGI_FORMAT_R10G10B10A2_UNORM)) {
        output_dxgi_format = DXGI_FORMAT_R10G10B10A2_UNORM;
        output_pixel_format = PIXEL_FORMAT_XB30;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected XB30";
      } else if (!needs_texture_copy || supports_fmt(DXGI_FORMAT_P010)) {
        output_dxgi_format = DXGI_FORMAT_P010;
        output_pixel_format = PIXEL_FORMAT_P010LE;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected P010LE";
      } else if (supports_fmt(DXGI_FORMAT_B8G8R8A8_UNORM)) {
        output_dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        output_pixel_format = PIXEL_FORMAT_ARGB;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected ARGB";
      } else {
        MEDIA_LOG(INFO, media_log)
            << DxgiFormatToString(decoder_output_format) << " not supported";
        return nullptr;
      }
      break;
    }
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016: {
      MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder producing "
                                 << DxgiFormatToString(decoder_output_format);
      // If device support P010 zero copy, then try P010 firstly.
      if (!needs_texture_copy || supports_fmt(DXGI_FORMAT_P010)) {
        output_dxgi_format = DXGI_FORMAT_P010;
        output_pixel_format = PIXEL_FORMAT_P010LE;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected P010LE";
      } else if (supports_fmt(DXGI_FORMAT_R10G10B10A2_UNORM)) {
        output_dxgi_format = DXGI_FORMAT_R10G10B10A2_UNORM;
        output_pixel_format = PIXEL_FORMAT_XB30;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected XB30";
      } else if (supports_fmt(DXGI_FORMAT_B8G8R8A8_UNORM)) {
        output_dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        output_pixel_format = PIXEL_FORMAT_ARGB;
        MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: Selected ARGB";
      } else {
        MEDIA_LOG(INFO, media_log)
            << DxgiFormatToString(decoder_output_format) << " not supported";
        return nullptr;
      }
      break;
    }
    default: {
      MEDIA_LOG(INFO, media_log)
          << "D3D11VideoDecoder does not support " << decoder_output_format;
      return nullptr;
    }
  }

  // If we're trying to produce an output texture that's different from what
  // the decoder is providing, then we need to copy it. If sharing decoder
  // textures is not allowed, then copy either way.
  needs_texture_copy |= (decoder_output_format != output_dxgi_format);

  if (needs_texture_copy) {
    MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder is copying textures";
    return std::make_unique<CopyTextureSelector>(
        output_pixel_format, output_dxgi_format, std::move(video_device),
        std::move(device_context), shared_image_use_shared_handle);
  } else {
    MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder is binding textures";
    return std::make_unique<TextureSelector>(
        output_pixel_format, output_dxgi_format, std::move(video_device),
        std::move(device_context), shared_image_use_shared_handle);
  }
}

std::unique_ptr<Texture2DWrapper> TextureSelector::CreateTextureWrapper(
    ComD3D11Device device,
    gfx::ColorSpace color_space,
    gfx::Size size) {
  // TODO(liberato): If the output format is rgb, then create a pbuffer wrapper.
  return std::make_unique<DefaultTexture2DWrapper>(size, color_space,
                                                   OutputDXGIFormat(), device);
}

bool TextureSelector::DoesDecoderOutputUseSharedHandle() const {
  return shared_image_use_shared_handle_;
}

bool TextureSelector::WillCopyForTesting() const {
  return false;
}

CopyTextureSelector::CopyTextureSelector(
    VideoPixelFormat pixfmt,
    DXGI_FORMAT output_dxgifmt,
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext device_context,
    bool shared_image_use_shared_handle)
    : TextureSelector(pixfmt,
                      output_dxgifmt,
                      std::move(video_device),
                      std::move(device_context),
                      shared_image_use_shared_handle),
      video_processor_proxy_(
          base::MakeRefCounted<VideoProcessorProxy>(this->video_device(),
                                                    this->device_context())) {}

CopyTextureSelector::~CopyTextureSelector() = default;

std::unique_ptr<Texture2DWrapper> CopyTextureSelector::CreateTextureWrapper(
    ComD3D11Device device,
    gfx::ColorSpace color_space,
    gfx::Size size) {
  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = 1;
  texture_desc.CPUAccessFlags = 0;
  texture_desc.Format = output_dxgifmt_;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags =
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  texture_desc.Width = size.width();
  texture_desc.Height = size.height();
  if (DoesSharedImageUseSharedHandle()) {
    texture_desc.MiscFlags =
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
  }

  ComD3D11Texture2D out_texture;
  if (FAILED(device->CreateTexture2D(&texture_desc, nullptr, &out_texture)))
    return nullptr;

  if (FAILED(
          SetDebugName(out_texture.Get(), "D3D11Decoder_CopyTextureSelector")))
    return nullptr;

  return std::make_unique<CopyingTexture2DWrapper>(
      size,
      std::make_unique<DefaultTexture2DWrapper>(size, color_space,
                                                OutputDXGIFormat(), device),
      video_processor_proxy_, out_texture);
}

bool CopyTextureSelector::DoesDecoderOutputUseSharedHandle() const {
  return false;
}

bool CopyTextureSelector::WillCopyForTesting() const {
  return true;
}

}  // namespace media
