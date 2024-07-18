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

SupportedResolutionRange GetResolutionsForGUID(
    D3DVideoDeviceWrapper* video_device_wrapper,
    const GUID& decoder_guid,
    const std::vector<gfx::Size>& resolutions_to_test,
    DXGI_FORMAT format = DXGI_FORMAT_NV12,
    const gfx::Size& min_resolution = kMinResolution) {
  SupportedResolutionRange result;

  // Verify input is in ascending order by height.
  DCHECK(std::is_sorted(resolutions_to_test.begin(), resolutions_to_test.end(),
                        [](const gfx::Size& a, const gfx::Size& b) {
                          return a.height() < b.height();
                        }));

  for (const auto& res : resolutions_to_test) {
    // We've chosen the least expensive test for identifying if a given
    // resolution is supported. Actually creating the VideoDecoder instance only
    // fails ~0.4% of the time and the outcome is that we will offer support and
    // then immediately fall back to software; e.g., playback still works. Since
    // these calls can take hundreds of milliseconds to complete and are often
    // executed during startup, this seems a reasonably trade off.
    //
    // See the deprecated histograms Media.DXVAVDA.GetDecoderConfigStatus which
    // succeeds 100% of the time and Media.DXVAVDA.CreateDecoderStatus which
    // only succeeds 99.6% of the time (in a 28 day aggregation).
    if (!video_device_wrapper->IsResolutionSupported(decoder_guid, res,
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
      video_device_wrapper->IsResolutionSupported(decoder_guid, flipped,
                                                  format)) {
    result.max_portrait_resolution = flipped;
  }

  if (!result.max_landscape_resolution.IsEmpty())
    result.min_resolution = min_resolution;

  return result;
}

}  // namespace

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
GUID GetHEVCRangeExtensionPrivateGUID(uint8_t bitdepth,
                                      VideoChromaSampling chroma_sampling) {
  if (bitdepth == 8) {
    if (chroma_sampling == VideoChromaSampling::k420) {
      return DXVA_ModeHEVC_VLD_Main_Intel;
    } else if (chroma_sampling == VideoChromaSampling::k422) {
      // For D3D11/D3D12, 8b/10b-422 HEVC will share 10b-422 GUID no matter
      // it is defined by Intel or DXVA spec(as part of Windows SDK).
      return DXVA_ModeHEVC_VLD_Main422_10_Intel;
    } else if (chroma_sampling == VideoChromaSampling::k444) {
      return DXVA_ModeHEVC_VLD_Main444_Intel;
    }

  } else if (bitdepth == 10) {
    if (chroma_sampling == VideoChromaSampling::k420) {
      return DXVA_ModeHEVC_VLD_Main10_Intel;
    } else if (chroma_sampling == VideoChromaSampling::k422) {
      return DXVA_ModeHEVC_VLD_Main422_10_Intel;
    } else if (chroma_sampling == VideoChromaSampling::k444) {
      return DXVA_ModeHEVC_VLD_Main444_10_Intel;
    }
  } else if (bitdepth == 12) {
    if (chroma_sampling == VideoChromaSampling::k420) {
      return DXVA_ModeHEVC_VLD_Main12_Intel;
    } else if (chroma_sampling == VideoChromaSampling::k422) {
      return DXVA_ModeHEVC_VLD_Main422_12_Intel;
    } else if (chroma_sampling == VideoChromaSampling::k444) {
      return DXVA_ModeHEVC_VLD_Main444_12_Intel;
    }
  }
  return {};
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

  // To detect if a driver supports the desired resolutions, we try and create
  // a DXVA decoder instance for that resolution and profile. If that succeeds
  // we assume that the driver supports decoding for that resolution.
  // Legacy AMD drivers with UVD3 or earlier and some Intel GPU's crash while
  // creating surfaces larger than 1920 x 1088.
  const std::vector<gfx::Size> kModernResolutions = {
      gfx::Size(4096, 2160), gfx::Size(4096, 2304), gfx::Size(7680, 4320),
      gfx::Size(8192, 4320), gfx::Size(8192, 8192)};

  // Enumerate supported video profiles and look for the known profile for each
  // codec. We first look through the the decoder profiles so we don't run N
  // resolution tests for a profile that's unsupported.
  for (const GUID& profile_id :
       video_device_wrapper->GetVideoDecodeProfileGuids()) {
    if (profile_id == D3D11_DECODER_PROFILE_H264_VLD_NOFGT) {
      const auto result = GetResolutionsForGUID(
          video_device_wrapper, profile_id,
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
            video_device_wrapper, profile_id, kModernResolutions);
        continue;
      }
      if (profile_id == DXVA_ModeAV1_VLD_Profile1) {
        supported_resolutions[AV1PROFILE_PROFILE_HIGH] = GetResolutionsForGUID(
            video_device_wrapper, profile_id, kModernResolutions);
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
            video_device_wrapper, profile_id, kModernResolutions);
        continue;
      }
    }

    if (!workarounds.disable_accelerated_vp9_decode) {
      if (profile_id == D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0) {
        supported_resolutions[VP9PROFILE_PROFILE0] = GetResolutionsForGUID(
            video_device_wrapper, profile_id, kModernResolutions);
        continue;
      }

      // RS3 has issues with VP9.2 decoding. See https://crbug.com/937108.
      if (!workarounds.disable_accelerated_vp9_profile2_decode &&
          profile_id == D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2 &&
          base::win::GetVersion() != base::win::Version::WIN10_RS3) {
        supported_resolutions[VP9PROFILE_PROFILE2] =
            GetResolutionsForGUID(video_device_wrapper, profile_id,
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
          video_device_wrapper, profile_id,
          {gfx::Size(4096, 2160), gfx::Size(4096, 2304), gfx::Size(4096, 4096)},
          DXGI_FORMAT_NV12, kMinVp8Resolution);
      continue;
    }

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    if (!workarounds.disable_accelerated_hevc_decode &&
        base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport)) {
      // Per DirectX Video Acceleration Specification for High Efficiency Video
      // Coding - 7.4, DXVA_ModeHEVC_VLD_Main GUID can be used for both main and
      // main still picture profile.
      if (profile_id == D3D11_DECODER_PROFILE_HEVC_VLD_MAIN) {
        auto supported_resolution = GetResolutionsForGUID(
            video_device_wrapper, profile_id, kModernResolutions);
        supported_resolutions[HEVCPROFILE_MAIN] = supported_resolution;
        supported_resolutions[HEVCPROFILE_MAIN_STILL_PICTURE] =
            supported_resolution;
        continue;
      }
      // For range extensions only test main10_422 with P010, and apply
      // the same resolution range to main420 & main10_YUV420. Ideally we
      // should be also testing against NV12 & Y210 for YUV422, and Y410 for
      // YUV444 8/10/12 bit.
      if (profile_id == DXVA_ModeHEVC_VLD_Main422_10_Intel) {
        supported_resolutions[HEVCPROFILE_REXT] =
            GetResolutionsForGUID(video_device_wrapper, profile_id,
                                  kModernResolutions, DXGI_FORMAT_P010);
        continue;
      }
      if (profile_id == D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10) {
        supported_resolutions[HEVCPROFILE_MAIN10] =
            GetResolutionsForGUID(video_device_wrapper, profile_id,
                                  kModernResolutions, DXGI_FORMAT_P010);
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

}  // namespace media
