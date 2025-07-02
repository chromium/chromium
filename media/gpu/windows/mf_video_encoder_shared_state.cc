// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/mf_video_encoder_shared_state.h"

#include <objbase.h>

#include <codecapi.h>
#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <utility>

#include "base/features.h"
#include "base/logging.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/win_util.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "media/gpu/windows/mf_video_encoder_switches.h"

namespace media {

namespace {

int GetNumSupportedTemporalLayers(
    IMFActivate* activate,
    VideoCodec codec,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  auto vendor = GetDriverVendor(activate);
  int max_temporal_layer_vendor_limit =
      GetMaxTemporalLayerVendorLimit(vendor, codec, workarounds);
  if (max_temporal_layer_vendor_limit == 1) {
    return 1;
  }

  ComMFTransform encoder;
  ComCodecAPI codec_api;
  HRESULT hr = activate->ActivateObject(IID_PPV_ARGS(&encoder));
  if (FAILED(hr)) {
    // Log to VLOG since errors are expected as part of GetSupportedProfiles().
    DVLOG(2) << "Failed to activate encoder: " << PrintHr(hr);
    return 1;
  }

  hr = encoder.As(&codec_api);
  if (FAILED(hr)) {
    // Log to VLOG since errors are expected as part of GetSupportedProfiles().
    DVLOG(2) << "Failed to get encoder as CodecAPI: " << PrintHr(hr);
    return 1;
  }

  if (codec_api->IsSupported(&CODECAPI_AVEncVideoTemporalLayerCount) != S_OK) {
    return 1;
  }

  base::win::ScopedVariant min, max, step;
  if (FAILED(codec_api->GetParameterRange(
          &CODECAPI_AVEncVideoTemporalLayerCount, min.AsInput(), max.AsInput(),
          step.AsInput()))) {
    return 1;
  }

  // Temporal encoding is only considered supported if the driver reports at
  // least a span of 1-3 temporal layers.
  if (V_UI4(min.ptr()) > 1u || V_UI4(max.ptr()) < 3u) {
    return 1;
  }
  return max_temporal_layer_vendor_limit;
}

int GetMaxTemporalLayer(
    VideoCodec codec,
    std::vector<Microsoft::WRL::ComPtr<IMFActivate>>& activates,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  int num_temporal_layers = 1;

  for (size_t i = 0; i < activates.size(); i++) {
    num_temporal_layers = std::max(
        GetNumSupportedTemporalLayers(activates[i].Get(), codec, workarounds),
        num_temporal_layers);
  }

  return num_temporal_layers;
}

}  // namespace

// static
MediaFoundationVideoEncoderSharedState*
MediaFoundationVideoEncoderSharedState::GetInstance(
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  static MediaFoundationVideoEncoderSharedState* instance =
      new MediaFoundationVideoEncoderSharedState(workarounds);
  return instance;
}

MediaFoundationVideoEncoderSharedState::
    ~MediaFoundationVideoEncoderSharedState() = default;

MediaFoundationVideoEncoderSharedState::MediaFoundationVideoEncoderSharedState(
    const gpu::GpuDriverBugWorkarounds& workarounds)
    : workarounds_(workarounds) {
  GetSupportedProfilesInternal();
}

const std::vector<FramerateAndResolution>
MediaFoundationVideoEncoderSharedState::GetMaxFramerateAndResolutions(
    size_t activate_hash) const {
  auto it = max_framerate_and_resolutions_.find(activate_hash);
  if (it != max_framerate_and_resolutions_.end()) {
    return it->second;
  }
  return {};
}

const gfx::Size MediaFoundationVideoEncoderSharedState::GetMinResolution(
    size_t activate_hash) const {
  auto it = min_resolutions_.find(activate_hash);
  if (it != min_resolutions_.end()) {
    return it->second;
  }
  return {};
}

void MediaFoundationVideoEncoderSharedState::GetSupportedProfilesInternal() {
  std::vector<VideoCodec> supported_codecs(
      {VideoCodec::kH264, VideoCodec::kVP9, VideoCodec::kAV1});

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  if (base::FeatureList::IsEnabled(kPlatformHEVCEncoderSupport)) {
    supported_codecs.emplace_back(VideoCodec::kHEVC);
  }
#endif

  for (auto codec : supported_codecs) {
    auto activates = EnumerateHardwareEncoders(codec);
    if (activates.empty()) {
      DVLOG(1) << "Hardware encode acceleration is not available for "
               << GetCodecName(codec);
      continue;
    }

    int num_temporal_layers =
        GetMaxTemporalLayer(codec, activates, workarounds_);
    auto bitrate_mode = VideoEncodeAccelerator::kConstantMode |
                        VideoEncodeAccelerator::kVariableMode;
    if (codec == VideoCodec::kH264 || codec == VideoCodec::kHEVC) {
      bitrate_mode |= VideoEncodeAccelerator::kExternalMode;
    }

    std::vector<FramerateAndResolution> max_framerate_and_resolutions = {
        kDefaultMaxFramerateAndResolution};
    gfx::Size min_resolution = kDefaultMinResolution;

    size_t activate_hash = 0;
    if (base::FeatureList::IsEnabled(
            kExpandMediaFoundationEncodingResolutions)) {
      // https://crbug.com/40233328, Ideally we'd want supported profiles to
      // return the max supported resolution and then during configure() to
      // find the encoder which can support the right resolution.
      // For now checking only the first encoder seems okay, but we probably
      // still need the configure() part: ensure that selected one supports the
      // given resolution of the first encoder.
      IMFActivate* activate = activates[0].Get();
      Microsoft::WRL::ComPtr<IMFTransform> encoder;
      if (FAILED(activate->ActivateObject(IID_PPV_ARGS(&encoder)))) {
        continue;
      }

      activate_hash = GetMFTGuidHash(activate);
      CHECK(encoder);
      max_framerate_and_resolutions = GetMaxFramerateAndResolutionsFromMFT(
          codec, encoder.Get(), /*allow_set_output_type=*/true);
      min_resolution =
          ::media::GetMinResolution(codec, GetDriverVendor(activate));
      activate->ShutdownObject();
    }

    for (auto& max_framerate_and_resolution : max_framerate_and_resolutions) {
      DVLOG(3) << __func__ << ": " << codec << " codec, max resolution width: "
               << max_framerate_and_resolution.resolution.width()
               << ", height: "
               << max_framerate_and_resolution.resolution.height()
               << ", min resolution width: " << min_resolution.width()
               << ", height: " << min_resolution.height()
               << ", framerate: " << max_framerate_and_resolution.frame_rate;

      VideoEncodeAccelerator::SupportedProfile profile(
          VIDEO_CODEC_PROFILE_UNKNOWN, max_framerate_and_resolution.resolution,
          max_framerate_and_resolution.frame_rate *
              kDefaultFrameRateDenominator,
          kDefaultFrameRateDenominator, bitrate_mode,
          {SVCScalabilityMode::kL1T1});
      profile.min_resolution = min_resolution;

      if (!workarounds_.disable_svc_encoding) {
        if (num_temporal_layers >= 2) {
          profile.scalability_modes.push_back(SVCScalabilityMode::kL1T2);
        }
        if (num_temporal_layers >= 3) {
          profile.scalability_modes.push_back(SVCScalabilityMode::kL1T3);
        }
      }

      if (base::FeatureList::IsEnabled(kMediaFoundationD3DVideoProcessing)) {
        std::ranges::copy(
            kSupportedPixelFormatsD3DVideoProcessing,
            std::back_inserter(profile.gpu_supported_pixel_formats));
      }

      VideoEncodeAccelerator::SupportedProfile portrait_profile(profile);
      portrait_profile.max_resolution.Transpose();

      if (base::FeatureList::IsEnabled(kMediaFoundationSharedImageEncode)) {
        profile.supports_gpu_shared_images = true;
      }

      std::vector<VideoCodecProfile> codec_profiles;
      if (codec == VideoCodec::kH264) {
        codec_profiles = {H264PROFILE_BASELINE, H264PROFILE_MAIN,
                          H264PROFILE_HIGH};
      } else if (codec == VideoCodec::kVP9) {
        codec_profiles = {VP9PROFILE_PROFILE0};
      } else if (codec == VideoCodec::kAV1) {
        codec_profiles = {AV1PROFILE_PROFILE_MAIN};
      } else if (codec == VideoCodec::kHEVC) {
        codec_profiles = {HEVCPROFILE_MAIN};
      }

      for (const auto codec_profile : codec_profiles) {
        profile.profile = portrait_profile.profile = codec_profile;
        supported_profiles_.push_back(profile);
        supported_profiles_.push_back(portrait_profile);
      }
    }

    max_framerate_and_resolutions_[activate_hash] =
        std::move(max_framerate_and_resolutions);
    min_resolutions_[activate_hash] = std::move(min_resolution);
  }
}

}  // namespace media
