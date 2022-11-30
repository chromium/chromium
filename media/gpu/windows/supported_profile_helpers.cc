// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/supported_profile_helpers.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include <d3d9.h>
#include <dxva2api.h>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "media/base/media_switches.h"
#include "media/gpu/windows/av1_guids.h"

namespace {

// Windows Media Foundation H.264 decoding does not support decoding videos
// with any dimension smaller than 48 pixels:
// http://msdn.microsoft.com/en-us/library/windows/desktop/dd797815
//
// TODO(dalecurtis): These values are too low. We should only be using
// hardware decode for videos above ~360p, see http://crbug.com/684792.
constexpr gfx::Size kMinResolution(64, 64);

bool IsResolutionSupportedForDevice(const gfx::Size& resolution_to_test,
                                    const GUID& decoder_guid,
                                    ID3D11VideoDevice* video_device,
                                    DXGI_FORMAT format) {
  D3D11_VIDEO_DECODER_DESC desc = {
      decoder_guid,                                    // Guid
      static_cast<UINT>(resolution_to_test.width()),   // SampleWidth
      static_cast<UINT>(resolution_to_test.height()),  // SampleHeight
      format                                           // OutputFormat
  };

  // We've chosen the least expensive test for identifying if a given resolution
  // is supported. Actually creating the VideoDecoder instance only fails ~0.4%
  // of the time and the outcome is that we will offer support and then
  // immediately fall back to software; e.g., playback still works. Since these
  // calls can take hundreds of milliseconds to complete and are often executed
  // during startup, this seems a reasonably trade off.
  //
  // See the deprecated histograms Media.DXVAVDA.GetDecoderConfigStatus which
  // succeeds 100% of the time and Media.DXVAVDA.CreateDecoderStatus which
  // only succeeds 99.6% of the time (in a 28 day aggregation).
  UINT config_count;
  return SUCCEEDED(
             video_device->GetVideoDecoderConfigCount(&desc, &config_count)) &&
         config_count > 0;
}

media::SupportedResolutionRange GetResolutionsForGUID(
    ID3D11VideoDevice* video_device,
    const GUID& decoder_guid,
    const std::vector<gfx::Size>& resolutions_to_test,
    DXGI_FORMAT format = DXGI_FORMAT_NV12,
    const gfx::Size& min_resolution = kMinResolution) {
  media::SupportedResolutionRange result;

  // Verify input is in ascending order by height.
  DCHECK(std::is_sorted(resolutions_to_test.begin(), resolutions_to_test.end(),
                        [](const gfx::Size& a, const gfx::Size& b) {
                          return a.height() < b.height();
                        }));

  for (const auto& res : resolutions_to_test) {
    if (!IsResolutionSupportedForDevice(res, decoder_guid, video_device,
                                        format)) {
      break;
    }
    result.max_landscape_resolution = res;
  }

  // The max supported portrait resolution should be just be a w/h flip of the
  // max supported landscape resolution.
  const gfx::Size flipped(result.max_landscape_resolution.height(),
                          result.max_landscape_resolution.width());
  if (flipped == result.max_landscape_resolution ||
      IsResolutionSupportedForDevice(flipped, decoder_guid, video_device,
                                     format)) {
    result.max_portrait_resolution = flipped;
  }

  if (!result.max_landscape_resolution.IsEmpty())
    result.min_resolution = min_resolution;

  return result;
}

}  // namespace

namespace media {

SupportedResolutionRangeMap GetSupportedD3D11VideoDecoderResolutions(
    ComD3D11Device device,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  TRACE_EVENT0("gpu,startup", "GetSupportedD3D11VideoDecoderResolutions");
  SupportedResolutionRangeMap supported_resolutions;

  // We always insert support for H.264 regardless of the tests below. It's old
  // enough to be ubiquitous.
  //
  // On Windows 7 the maximum resolution supported by media foundation is
  // 1920 x 1088. We use 1088 to account for 16x16 macro-blocks.
  constexpr gfx::Size kDefaultMaxH264Resolution(1920, 1088);
  SupportedResolutionRange h264_profile;
  h264_profile.min_resolution = kMinResolution;
  h264_profile.max_landscape_resolution = kDefaultMaxH264Resolution;

  // We don't have a way to map DXVA support to specific H.264 profiles, so just
  // mark all the common ones with the same level of support.
  constexpr VideoCodecProfile kSupportedH264Profiles[] = {
      H264PROFILE_BASELINE, H264PROFILE_MAIN, H264PROFILE_HIGH};
  for (const auto profile : kSupportedH264Profiles)
    supported_resolutions[profile] = h264_profile;

  if (base::win::GetVersion() <= base::win::Version::WIN7)
    return supported_resolutions;

  if (!device)
    return supported_resolutions;

  // To detect if a driver supports the desired resolutions, we try and create
  // a DXVA decoder instance for that resolution and profile. If that succeeds
  // we assume that the driver supports decoding for that resolution.
  // Legacy AMD drivers with UVD3 or earlier and some Intel GPU's crash while
  // creating surfaces larger than 1920 x 1088.
  ComD3D11VideoDevice video_device;
  if (FAILED(device.As(&video_device)))
    return supported_resolutions;

  const std::vector<gfx::Size> kModernResolutions = {
      gfx::Size(4096, 2160), gfx::Size(4096, 2304), gfx::Size(7680, 4320),
      gfx::Size(8192, 4320), gfx::Size(8192, 8192)};

  // Enumerate supported video profiles and look for the known profile for each
  // codec. We first look through the the decoder profiles so we don't run N
  // resolution tests for a profile that's unsupported.
  UINT profile_count = video_device->GetVideoDecoderProfileCount();
  for (UINT i = 0; i < profile_count; i++) {
    GUID profile_id;
    if (FAILED(video_device->GetVideoDecoderProfile(i, &profile_id)))
      continue;

    if (profile_id == D3D11_DECODER_PROFILE_H264_VLD_NOFGT) {
      const auto result = GetResolutionsForGUID(
          video_device.Get(), profile_id,
          {gfx::Size(2560, 1440), gfx::Size(3840, 2160), gfx::Size(4096, 2160),
           gfx::Size(4096, 2304), gfx::Size(4096, 4096)});

      // Unlike the other codecs, H.264 support is assumed up to 1080p, even if
      // our initial queries fail. If they fail, we use the defaults set above.
      if (!result.max_landscape_resolution.IsEmpty()) {
        for (const auto profile : kSupportedH264Profiles)
          supported_resolutions[profile] = result;
      }
      continue;
    }

    // Note: Each bit depth of AV1 uses a different DXGI_FORMAT, here we only
    // test for the 8-bit one (NV12).
    if (!workarounds.disable_accelerated_av1_decode) {
      if (profile_id == DXVA_ModeAV1_VLD_Profile0) {
        supported_resolutions[AV1PROFILE_PROFILE_MAIN] = GetResolutionsForGUID(
            video_device.Get(), profile_id, kModernResolutions);
        continue;
      }
      if (profile_id == DXVA_ModeAV1_VLD_Profile1) {
        supported_resolutions[AV1PROFILE_PROFILE_HIGH] = GetResolutionsForGUID(
            video_device.Get(), profile_id, kModernResolutions);
        continue;
      }
      if (profile_id == DXVA_ModeAV1_VLD_Profile2) {
        // TODO(dalecurtis): 12-bit profile 2 support is complicated. Ideally,
        // we should test DXVA_ModeAV1_VLD_12bit_Profile2 and
        // DXVA_ModeAV1_VLD_12bit_Profile2_420 when the bit depth of the content
        // is 12-bit. However we don't know the bit depth or pixel format until
        // too late. In these cases we'll end up initializing the decoder and
        // failing on the first decode (which will trigger software fallback).
        supported_resolutions[AV1PROFILE_PROFILE_PRO] = GetResolutionsForGUID(
            video_device.Get(), profile_id, kModernResolutions);
        continue;
      }
    }

    if (!workarounds.disable_accelerated_vp9_decode) {
      if (profile_id == D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0) {
        supported_resolutions[VP9PROFILE_PROFILE0] = GetResolutionsForGUID(
            video_device.Get(), profile_id, kModernResolutions);
        continue;
      }

      // RS3 has issues with VP9.2 decoding. See https://crbug.com/937108.
      if (!workarounds.disable_accelerated_vp9_profile2_decode &&
          profile_id == D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2 &&
          base::win::GetVersion() != base::win::Version::WIN10_RS3) {
        supported_resolutions[VP9PROFILE_PROFILE2] =
            GetResolutionsForGUID(video_device.Get(), profile_id,
                                  kModernResolutions, DXGI_FORMAT_P010);
        continue;
      }
    }

    if (!workarounds.disable_accelerated_vp8_decode &&
        profile_id == D3D11_DECODER_PROFILE_VP8_VLD &&
        base::FeatureList::IsEnabled(kMediaFoundationVP8Decoding)) {
      // VP8 decoding is cheap on modern devices compared to other codecs, so
      // much so that hardware decoding performance is actually worse at low
      // resolutions than software decoding. See https://crbug.com/1136495.
      constexpr gfx::Size kMinVp8Resolution = gfx::Size(640, 480);

      supported_resolutions[VP8PROFILE_ANY] = GetResolutionsForGUID(
          video_device.Get(), profile_id,
          {gfx::Size(4096, 2160), gfx::Size(4096, 2304), gfx::Size(4096, 4096)},
          DXGI_FORMAT_NV12, kMinVp8Resolution);
      continue;
    }

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    if (!workarounds.disable_accelerated_hevc_decode &&
        base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport)) {
      if (profile_id == D3D11_DECODER_PROFILE_HEVC_VLD_MAIN) {
        supported_resolutions[HEVCPROFILE_MAIN] = GetResolutionsForGUID(
            video_device.Get(), profile_id, kModernResolutions);
        continue;
      }
      // For range extensions only test main10_422 with P010, and apply
      // the same resolution range to main420 & main10_YUV420. Ideally we
      // should be also testing against NV12 & Y210 for YUV422, and Y410 for
      // YUV444 8/10/12 bit.
      if (profile_id == DXVA_ModeHEVC_VLD_Main422_10_Intel) {
        supported_resolutions[HEVCPROFILE_REXT] =
            GetResolutionsForGUID(video_device.Get(), profile_id,
                                  kModernResolutions, DXGI_FORMAT_P010);
        continue;
      }
      if (profile_id == D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10) {
        supported_resolutions[HEVCPROFILE_MAIN10] =
            GetResolutionsForGUID(video_device.Get(), profile_id,
                                  kModernResolutions, DXGI_FORMAT_P010);
        continue;
      }
    }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  }

  return supported_resolutions;
}

}  // namespace media
