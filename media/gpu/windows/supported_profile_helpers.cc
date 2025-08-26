// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/supported_profile_helpers.h"

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/gpu/windows/av1_guids.h"

namespace media {

namespace {

// To detect if a driver supports the desired resolutions, we use
// GetVideoDecoderConfigCount (D3D11) or CheckFeatureSupport (D3D12), both of
// which are fast to execute. If that succeeds we assume that the driver
// supports decoding for that resolution.
//
// Actually creating the VideoDecoder instance only fails ~0.4% of the time and
// the outcome is that we will offer support and then immediately fall back to
// software; e.g., playback still works. Since these calls can take hundreds of
// milliseconds to complete and are often executed during startup, this seems a
// reasonably trade off.
//
// See the deprecated histograms Media.DXVAVDA.GetDecoderConfigStatus which
// succeeds 100% of the time and Media.DXVAVDA.CreateDecoderStatus which
// only succeeds 99.6% of the time (in a 28 day aggregation).
class D3DVideoDeviceWrapper {
 public:
  virtual ~D3DVideoDeviceWrapper() = default;
  virtual std::vector<GUID> GetVideoDecodeProfileGuids() = 0;
  virtual bool IsResolutionSupported(const GUID& profile,
                                     const gfx::Size& resolution,
                                     DXGI_FORMAT format) = 0;
};

class D3D11VideoDeviceWrapper : public D3DVideoDeviceWrapper {
 public:
  explicit D3D11VideoDeviceWrapper(ComD3D11VideoDevice video_device)
      : video_device_(video_device) {
    CHECK(video_device);
  }
  ~D3D11VideoDeviceWrapper() override = default;

  std::vector<GUID> GetVideoDecodeProfileGuids() override {
    std::vector<GUID> result;
    UINT profile_count = video_device_->GetVideoDecoderProfileCount();
    for (UINT i = 0; i < profile_count; i++) {
      GUID profile_id;
      if (SUCCEEDED(video_device_->GetVideoDecoderProfile(i, &profile_id))) {
        result.push_back(profile_id);
      }
    }
    return result;
  }

  bool IsResolutionSupported(const GUID& profile,
                             const gfx::Size& resolution,
                             DXGI_FORMAT format) override {
    D3D11_VIDEO_DECODER_DESC desc = {
        profile,                                 // Guid
        static_cast<UINT>(resolution.width()),   // SampleWidth
        static_cast<UINT>(resolution.height()),  // SampleHeight
        format                                   // OutputFormat
    };
    UINT config_count;
    return SUCCEEDED(video_device_->GetVideoDecoderConfigCount(
               &desc, &config_count)) &&
           config_count > 0;
  }

 private:
  ComD3D11VideoDevice video_device_;
};

class D3D12VideoDeviceWrapper : public D3DVideoDeviceWrapper {
 public:
  explicit D3D12VideoDeviceWrapper(ComD3D12VideoDevice video_device)
      : video_device_(video_device) {
    CHECK(video_device);
  }
  ~D3D12VideoDeviceWrapper() override = default;

  std::vector<GUID> GetVideoDecodeProfileGuids() override {
    std::vector<GUID> result;
    D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILE_COUNT profile_count{};
    HRESULT hr = video_device_->CheckFeatureSupport(
        D3D12_FEATURE_VIDEO_DECODE_PROFILE_COUNT, &profile_count,
        sizeof(profile_count));
    if (FAILED(hr)) {
      return {};
    }
    result.resize(profile_count.ProfileCount);
    D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILES profiles{
        .ProfileCount = profile_count.ProfileCount, .pProfiles = result.data()};
    hr = video_device_->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_PROFILES,
                                            &profiles, sizeof(profiles));
    if (FAILED(hr)) {
      return {};
    }
    return result;
  }

  bool IsResolutionSupported(const GUID& profile,
                             const gfx::Size& resolution,
                             DXGI_FORMAT format) override {
    D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT support{
        .Configuration = {profile},
        .Width = static_cast<UINT>(resolution.width()),
        .Height = static_cast<UINT>(resolution.height()),
        .DecodeFormat = format};
    return SUCCEEDED(video_device_->CheckFeatureSupport(
               D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &support,
               sizeof(support))) &&
           support.SupportFlags == D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED;
  }

 private:
  ComD3D12VideoDevice video_device_;
};

// Windows Media Foundation H.264 decoding does not support decoding videos
// with any dimension smaller than 48 pixels:
// http://msdn.microsoft.com/en-us/library/windows/desktop/dd797815
//
// TODO(dalecurtis): These values are too low. We should only be using
// hardware decode for videos above ~360p, see http://crbug.com/684792.
constexpr gfx::Size kMinResolution(64, 64);

std::optional<SupportedResolutionRange> GetResolutionsForGUID(
    D3DVideoDeviceWrapper* video_device_wrapper,
    const GUID& decoder_guid,
    DXGI_FORMAT format = DXGI_FORMAT_NV12) {
  constexpr auto kModernResolutions = std::to_array<gfx::Size>(
      {gfx::Size(1920, 1080), gfx::Size(2560, 1440), gfx::Size(3840, 2160),
       gfx::Size(4096, 2160), gfx::Size(4096, 2304), gfx::Size(4096, 4096),
       gfx::Size(7680, 4320), gfx::Size(8192, 4352), gfx::Size(8192, 8192),
       gfx::Size(16384, 16384)});
  static_assert(
      std::is_sorted(kModernResolutions.begin(), kModernResolutions.end(),
                     [](const gfx::Size& a, const gfx::Size& b) {
                       return a.height() < b.height();
                     }),
      "Resolution map must be sorted by height ascending");

  std::optional<gfx::Size> max_landscape_resolution;
  for (const auto& res : kModernResolutions) {
    if (!video_device_wrapper->IsResolutionSupported(decoder_guid, res,
                                                     format)) {
      break;
    }
    max_landscape_resolution = res;
  }

  if (!max_landscape_resolution) {
    return std::nullopt;
  }

  SupportedResolutionRange result = {
      .min_resolution = kMinResolution,
      .max_landscape_resolution = *max_landscape_resolution,
  };

  // The max supported portrait resolution should be just be a w/h flip of the
  // max supported landscape resolution.
  const gfx::Size flipped(result.max_landscape_resolution.height(),
                          result.max_landscape_resolution.width());
  if (flipped != result.max_landscape_resolution &&
      video_device_wrapper->IsResolutionSupported(decoder_guid, flipped,
                                                  format)) {
    result.max_portrait_resolution = flipped;
  }

  return result;
}

}  // namespace

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
const struct HEVCProfileGUID {
  uint8_t bitdepth;
  VideoChromaSampling chroma_sampling;
  GUID private_guid;
  GUID public_guid;
} kHEVCProfileGUIDs[] = {
    // Use main profile GUID for 8b-420 range extension. Ideally we should use
    // DXVA_ModeHEVC_VLD_Main10_Ext, but not all devices support it.
    {8, VideoChromaSampling::k420, DXVA_ModeHEVC_VLD_Main_Intel,
     DXVA_ModeHEVC_VLD_Main},
    // 8b-422 uses same device GUID as 10b-422.
    {8, VideoChromaSampling::k422, DXVA_ModeHEVC_VLD_Main422_10_Intel,
     DXVA_ModeHEVC_VLD_Main10_422},
    {8, VideoChromaSampling::k444, DXVA_ModeHEVC_VLD_Main444_Intel,
     DXVA_ModeHEVC_VLD_Main_444},
    // Use main10 profile GUID for 10b-420 range extension. Ideally we should
    // use DXVA_ModeHEVC_VLD_Main10_Ext, but not all devices support it.
    {10, VideoChromaSampling::k420, DXVA_ModeHEVC_VLD_Main10_Intel,
     DXVA_ModeHEVC_VLD_Main10},
    {10, VideoChromaSampling::k422, DXVA_ModeHEVC_VLD_Main422_10_Intel,
     DXVA_ModeHEVC_VLD_Main10_422},
    {10, VideoChromaSampling::k444, DXVA_ModeHEVC_VLD_Main444_10_Intel,
     DXVA_ModeHEVC_VLD_Main10_444},
    {12, VideoChromaSampling::k420, DXVA_ModeHEVC_VLD_Main12_Intel,
     DXVA_ModeHEVC_VLD_Main12},
    {12, VideoChromaSampling::k422, DXVA_ModeHEVC_VLD_Main422_12_Intel,
     DXVA_ModeHEVC_VLD_Main12_422},
    {12, VideoChromaSampling::k444, DXVA_ModeHEVC_VLD_Main444_12_Intel,
     DXVA_ModeHEVC_VLD_Main12_444},
};

GUID GetHEVCRangeExtensionGUID(uint8_t bitdepth,
                               VideoChromaSampling chroma_sampling,
                               bool use_dxva_device_for_hevc_rext) {
  for (const auto& entry : kHEVCProfileGUIDs) {
    if (entry.bitdepth == bitdepth &&
        entry.chroma_sampling == chroma_sampling) {
      return use_dxva_device_for_hevc_rext ? entry.public_guid
                                           : entry.private_guid;
    }
  }
  return {};
}

bool SupportsHEVCRangeExtensionDXVAProfile(ComD3D11Device device) {
  ComD3D11VideoDevice video_device;
  if (device && SUCCEEDED(device.As(&video_device))) {
    for (UINT i = video_device->GetVideoDecoderProfileCount(); i--;) {
      GUID profile = {};
      if (SUCCEEDED(video_device->GetVideoDecoderProfile(i, &profile))) {
        if (profile == DXVA_ModeHEVC_VLD_Main10_422 ||
            profile == DXVA_ModeHEVC_VLD_Main10_444 ||
            profile == DXVA_ModeHEVC_VLD_Main12_422 ||
            profile == DXVA_ModeHEVC_VLD_Main12_444) {
          return true;
        }
      }
    }
  }
  return false;
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

DXGI_FORMAT GetOutputDXGIFormat(uint8_t bitdepth,
                                VideoChromaSampling chroma_sampling) {
  if (bitdepth == 8) {
    if (chroma_sampling == VideoChromaSampling::k420) {
      return DXGI_FORMAT_NV12;
    } else if (chroma_sampling == VideoChromaSampling::k422) {
      return DXGI_FORMAT_YUY2;
    } else if (chroma_sampling == VideoChromaSampling::k444) {
      return DXGI_FORMAT_AYUV;
    }
  } else if (bitdepth == 10) {
    if (chroma_sampling == VideoChromaSampling::k420) {
      return DXGI_FORMAT_P010;
    } else if (chroma_sampling == VideoChromaSampling::k422) {
      return DXGI_FORMAT_Y210;
    } else if (chroma_sampling == VideoChromaSampling::k444) {
      return DXGI_FORMAT_Y410;
    }
  } else if (bitdepth == 12 || bitdepth == 16) {
    if (chroma_sampling == VideoChromaSampling::k420) {
      return DXGI_FORMAT_P016;
    } else if (chroma_sampling == VideoChromaSampling::k422) {
      return DXGI_FORMAT_Y216;
    } else if (chroma_sampling == VideoChromaSampling::k444) {
      return DXGI_FORMAT_Y416;
    }
  }
  return {};
}

namespace {

SupportedResolutionRangeMap GetSupportedD3DVideoDecoderResolutions(
    D3DVideoDeviceWrapper* video_device_wrapper,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  TRACE_EVENT0("gpu,startup", "GetSupportedD3DVideoDecoderResolutions");
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

  if (!video_device_wrapper) {
    return supported_resolutions;
  }

  // Enumerate supported video profiles and look for the known profile for each
  // codec. We first look through the the decoder profiles so we don't run N
  // resolution tests for a profile that's unsupported.
  for (const GUID& profile_id :
       video_device_wrapper->GetVideoDecodeProfileGuids()) {
    if (profile_id == D3D11_DECODER_PROFILE_H264_VLD_NOFGT) {
      const auto result =
          GetResolutionsForGUID(video_device_wrapper, profile_id);

      // Unlike the other codecs, H.264 support is assumed up to 1080p, even if
      // our initial queries fail. If they fail, we use the defaults set above.
      if (result) {
        for (const auto profile : kSupportedH264Profiles)
          supported_resolutions[profile] = *result;
      }
      continue;
    }

    // Note: Each bit depth of AV1 uses a different DXGI_FORMAT, here we only
    // test for the 8-bit one (NV12).
    if (!workarounds.disable_accelerated_av1_decode) {
      if (profile_id == DXVA_ModeAV1_VLD_Profile0) {
        if (auto result =
                GetResolutionsForGUID(video_device_wrapper, profile_id)) {
          supported_resolutions.emplace(AV1PROFILE_PROFILE_MAIN,
                                        std::move(*result));
        }
        continue;
      }
      if (profile_id == DXVA_ModeAV1_VLD_Profile1) {
        // DXVA spec for high profile (section 7.2) does not include NV12 as
        // mandatory format, here we only test 8b-444 (AYUV) and skip check of
        // Y410.
        if (auto result = GetResolutionsForGUID(video_device_wrapper,
                                                profile_id, DXGI_FORMAT_AYUV)) {
          supported_resolutions.emplace(AV1PROFILE_PROFILE_HIGH,
                                        std::move(*result));
        }
        continue;
      }
      if (profile_id == DXVA_ModeAV1_VLD_Profile2) {
        // TODO(dalecurtis): 12-bit profile 2 support is complicated. Ideally,
        // we should test DXVA_ModeAV1_VLD_12bit_Profile2 and
        // DXVA_ModeAV1_VLD_12bit_Profile2_420 when the bit depth of the content
        // is 12-bit. However we don't know the bit depth or pixel format until
        // too late. In these cases we'll end up initializing the decoder and
        // failing on the first decode (which will trigger software fallback).
        if (auto result = GetResolutionsForGUID(video_device_wrapper,
                                                profile_id, DXGI_FORMAT_YUY2)) {
          supported_resolutions.emplace(AV1PROFILE_PROFILE_PRO,
                                        std::move(*result));
        }
        continue;
      }
    }

    if (!workarounds.disable_accelerated_vp9_decode) {
      if (profile_id == D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0) {
        if (auto result =
                GetResolutionsForGUID(video_device_wrapper, profile_id)) {
          supported_resolutions.emplace(VP9PROFILE_PROFILE0,
                                        std::move(*result));
        }
        continue;
      }

      // RS3 has issues with VP9.2 decoding. See https://crbug.com/937108.
      if (!workarounds.disable_accelerated_vp9_profile2_decode &&
          profile_id == D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2 &&
          base::win::GetVersion() != base::win::Version::WIN10_RS3) {
        if (auto result = GetResolutionsForGUID(video_device_wrapper,
                                                profile_id, DXGI_FORMAT_P010)) {
          supported_resolutions.emplace(VP9PROFILE_PROFILE2,
                                        std::move(*result));
        }
        continue;
      }
    }

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    if (!workarounds.disable_accelerated_hevc_decode &&
        base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport)) {
      // Per DirectX Video Acceleration Specification for High Efficiency Video
      // Coding - 7.4, DXVA_ModeHEVC_VLD_Main GUID can be used for both main and
      // main still picture profile.
      if (profile_id == D3D11_DECODER_PROFILE_HEVC_VLD_MAIN) {
        const auto result =
            GetResolutionsForGUID(video_device_wrapper, profile_id);
        if (result) {
          supported_resolutions[HEVCPROFILE_MAIN] = *result;
          supported_resolutions[HEVCPROFILE_MAIN_STILL_PICTURE] = *result;
        }
        continue;
      }
      // For range extensions only test main10_444 with Y410, and apply
      // the same resolution range to other formats to reduce profile
      // enumeration time for decoders. The selection of main10 444 is due to
      // the fact that IHV drivers' support on this is more common than other
      // range extension formats.
      // Ideally we should be also testing P016 for 12-bit 4:2:0, for example,
      // to get the precise resolution range of 12-bit 4:2:0. Same for other
      // range extension formats.
      if (profile_id == DXVA_ModeHEVC_VLD_Main10_444 ||
          profile_id == DXVA_ModeHEVC_VLD_Main444_10_Intel) {
        // Intel private GUID reports the same capability as DXVA GUID, so
        // it is fine to override supported resolutions here.
        if (auto result = GetResolutionsForGUID(video_device_wrapper,
                                                profile_id, DXGI_FORMAT_Y410)) {
          supported_resolutions.emplace(HEVCPROFILE_REXT, std::move(*result));
        }
        continue;
      }
      if (profile_id == D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10) {
        if (auto result = GetResolutionsForGUID(video_device_wrapper,
                                                profile_id, DXGI_FORMAT_P010)) {
          supported_resolutions.emplace(HEVCPROFILE_MAIN10, std::move(*result));
        }
        continue;
      }
    }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  }

  return supported_resolutions;
}

}  // namespace

SupportedResolutionRangeMap GetSupportedD3D11VideoDecoderResolutions(
    ComD3D11Device device,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  ComD3D11VideoDevice video_device;
  std::unique_ptr<D3D11VideoDeviceWrapper> video_device_wrapper;
  if (device && SUCCEEDED(device.As(&video_device))) {
    video_device_wrapper =
        std::make_unique<D3D11VideoDeviceWrapper>(video_device);
  }
  return GetSupportedD3DVideoDecoderResolutions(video_device_wrapper.get(),
                                                workarounds);
}

SupportedResolutionRangeMap GetSupportedD3D12VideoDecoderResolutions(
    ComD3D12Device device,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  ComD3D12VideoDevice video_device;
  std::unique_ptr<D3D12VideoDeviceWrapper> video_device_wrapper;
  if (device && SUCCEEDED(device.As(&video_device))) {
    video_device_wrapper =
        std::make_unique<D3D12VideoDeviceWrapper>(video_device);
  }
  return GetSupportedD3DVideoDecoderResolutions(video_device_wrapper.get(),
                                                workarounds);
}

UINT GetGPUVendorID(ComDXGIDevice dxgi_device) {
  if (!dxgi_device) {
    return 0;
  }
  ComDXGIAdapter dxgi_adapter;
  HRESULT hr = dxgi_device->GetAdapter(&dxgi_adapter);
  if (FAILED(hr)) {
    return 0;
  }
  DXGI_ADAPTER_DESC desc{};
  hr = dxgi_adapter->GetDesc(&desc);
  if (FAILED(hr)) {
    return 0;
  }
  return desc.VendorId;
}

}  // namespace media
