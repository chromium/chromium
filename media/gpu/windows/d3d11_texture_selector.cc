// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_selector.h"

#include <d3d11.h>

#include "base/feature_list.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
#include "ui/gfx/geometry/size.h"

namespace media {

TextureSelector::TextureSelector(VideoPixelFormat pixfmt,
                                 DXGI_FORMAT output_dxgifmt)
    : pixel_format_(pixfmt), output_dxgifmt_(output_dxgifmt) {}

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
    TextureSelector::HDRMode hdr_output_mode,
    const FormatSupportChecker* format_checker,
    MediaLog* media_log) {
  VideoPixelFormat output_pixel_format;
  DXGI_FORMAT output_dxgi_format;
  base::Optional<gfx::ColorSpace> output_color_space;

  // TODO(liberato): add other options here, like "copy to rgb" for NV12.
  switch (decoder_output_format) {
    case DXGI_FORMAT_NV12: {
      MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder producing NV12";
      output_pixel_format = PIXEL_FORMAT_NV12;
      output_dxgi_format = DXGI_FORMAT_NV12;
      // Leave |output_color_space| the same, since we'll bind either the
      // original or the copy.  Downstream will handle it, either in the shaders
      // or in the overlay, if needed.
      output_color_space.reset();
      break;
    }
    case DXGI_FORMAT_P010: {
      MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder producing P010";
      output_pixel_format = PIXEL_FORMAT_ARGB;

      // TODO(liberato): handle case where we bind P010 directly (see dxva).

      // Note that all of this should be handled later; we really don't know
      // enough about how this decoded frame will be used.  For example, we have
      // no idea if we should downsample while converting for display, or even
      // if it will be displayed on an HDR or SDR monitor, if there's more than
      // one.  However, to hide latency, we guess and do the conversion now.  If
      // the decoded frame is sampled by the web, then converting it might even
      // be the wrong thing to do; it's not unreasonable that it's expecting the
      // original data.
      //
      // In the future, consider just binding the P010 or NV12 texture directly,
      // and let the consumer figure out what to do with it.

      // Assume that we want HDR if it's supported by the display, and if we're
      // using an 11.1-capable device.
      // TODO(liberato): Get the context and ask it.
      const bool is_d3d_11_1 = true;

      if (!is_d3d_11_1 || hdr_output_mode == HDRMode::kSDROnly) {
        if (format_checker->CheckOutputFormatSupport(
                DXGI_FORMAT_B8G8R8A8_UNORM)) {
          // SDR output, so just use 8 bit and sRGB.
          // TODO(liberato): use the format checker, else bind P010.
          MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: 8 bit sRGB";
          output_dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
          output_color_space = gfx::ColorSpace::CreateSRGB();
        } else {
          // Bind P010 directly, since we can't copy.
          MEDIA_LOG(INFO, media_log)
              << "D3D11VideoDecoder: binding P010, no SDR output support";
          output_dxgi_format = DXGI_FORMAT_P010;
          // PIXEL_FORMAT_YUV422P10 would probably be a better choice, but it's
          // not supported by the rest of the pipeline yet.
          output_pixel_format = PIXEL_FORMAT_NV12;
          output_color_space.reset();
        }
      } else {
        // Will (may) be displayed in HDR, so switch to a high precision format.
        // For full screen, we might want 10 bit unorm instead of fp16.
        if (format_checker->CheckOutputFormatSupport(
                DXGI_FORMAT_R16G16B16A16_FLOAT)) {
          MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: fp16 scRGBLinear";
          output_dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
          output_color_space = gfx::ColorSpace::CreateSCRGBLinear();
        } else if (format_checker->CheckOutputFormatSupport(
                       DXGI_FORMAT_R10G10B10A2_UNORM)) {
          MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder: BGRA10 scRGBLinear";
          output_dxgi_format = DXGI_FORMAT_R10G10B10A2_UNORM;
          output_color_space = gfx::ColorSpace::CreateSCRGBLinear();
        } else {
          // No support at all.  Just bind P010, and hope for the best.
          MEDIA_LOG(INFO, media_log)
              << "D3D11VideoDecoder: binding P010, no HDR output support";
          output_dxgi_format = DXGI_FORMAT_P010;
          // PIXEL_FORMAT_YUV422P10 would probably be a better choice, but it's
          // not supported by the rest of the pipeline yet.
          output_pixel_format = PIXEL_FORMAT_NV12;
          output_color_space.reset();
        }

        // TODO(liberato): Handle HLG, if we can get the input color space.
        // The rough outline looks something like this:
#if 0
        if (hlg) {
        video_context1->VideoProcessorSetStreamColorSpace1(
        d3d11_processor_.Get(), 0,
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020);
    video_context1->VideoProcessorSetOutputColorSpace1(
        d3d11_processor_.Get(), DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
    dx11_converter_output_color_space_ = color_space.GetAsFullRangeRGB();
  }
#endif
      }
      break;
    }
    default: {
      // TODO(tmathmeyer) support other profiles in the future.
      MEDIA_LOG(INFO, media_log)
          << "D3D11VideoDecoder does not support " << decoder_output_format;
      return nullptr;
    }
  }

  // If we're trying to produce an output texture that's different from what
  // the decoder is providing, then we need to copy it.  If sharing decoder
  // textures is not allowed, then copy either way.
  bool needs_texture_copy = !SupportsZeroCopy(gpu_preferences, workarounds) ||
                            (decoder_output_format != output_dxgi_format) ||
               base::FeatureList::IsEnabled(kD3D11VideoDecoderAlwaysCopy);

  MEDIA_LOG(INFO, media_log)
      << "D3D11VideoDecoder output color space: "
      << (output_color_space ? output_color_space->ToString()
                             : "(same as input)");

  if (needs_texture_copy) {
    MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder is copying textures";
    return std::make_unique<CopyTextureSelector>(
        output_pixel_format, decoder_output_format, output_dxgi_format,
        output_color_space);
  } else {
    MEDIA_LOG(INFO, media_log) << "D3D11VideoDecoder is binding textures";
    // Binding can't change the color space.  The consumer has to do it, if they
    // want to.
    DCHECK(!output_color_space);
    return std::make_unique<TextureSelector>(output_pixel_format,
                                             output_dxgi_format);
  }
}

std::unique_ptr<Texture2DWrapper> TextureSelector::CreateTextureWrapper(
    ComD3D11Device device,
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext device_context,
    gfx::Size size) {
  // TODO(liberato): If the output format is rgb, then create a pbuffer wrapper.
  return std::make_unique<DefaultTexture2DWrapper>(size, OutputDXGIFormat(),
                                                   PixelFormat());
}

bool TextureSelector::WillCopyForTesting() const {
  return false;
}

CopyTextureSelector::CopyTextureSelector(
    VideoPixelFormat pixfmt,
    DXGI_FORMAT input_dxgifmt,
    DXGI_FORMAT output_dxgifmt,
    base::Optional<gfx::ColorSpace> output_color_space)
    : TextureSelector(pixfmt, output_dxgifmt),
      output_color_space_(std::move(output_color_space)) {}

CopyTextureSelector::~CopyTextureSelector() = default;

std::unique_ptr<Texture2DWrapper> CopyTextureSelector::CreateTextureWrapper(
    ComD3D11Device device,
    ComD3D11VideoDevice video_device,
    ComD3D11DeviceContext device_context,
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

  ComD3D11Texture2D out_texture;
  if (!SUCCEEDED(device->CreateTexture2D(&texture_desc, nullptr, &out_texture)))
    return nullptr;

  return std::make_unique<CopyingTexture2DWrapper>(
      size,
      std::make_unique<DefaultTexture2DWrapper>(size, OutputDXGIFormat(),
                                                PixelFormat()),
      std::make_unique<VideoProcessorProxy>(video_device, device_context),
      out_texture, output_color_space_);
}

bool CopyTextureSelector::WillCopyForTesting() const {
  return true;
}

}  // namespace media
