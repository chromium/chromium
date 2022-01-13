// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_service.h"

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/key_systems.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_capability.h"
#include "media/cdm/win/media_foundation_cdm_module.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "media/mojo/services/interface_factory_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using Microsoft::WRL::ComPtr;

namespace media {

namespace {

// The feature parameters follow Windows API documentation:
// https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.protectioncapabilities.istypesupported?view=winrt-19041
// This default feature string is required to query capability related to video
// decoder. Since we only care about the codec support rather than the specific
// resolution or bitrate capability, we use the following typical values which
// should be supported by most devices for a certain video codec.
const char kDefaultFeatures[] =
    "decode-bpp=8,decode-res-x=1920,decode-res-y=1080,decode-bitrate=10000000,"
    "decode-fps=30";

// These three parameters are an extension of the parameters supported
// in the above documentation to support the encryption capability query.
const char kRobustnessQueryName[] = "encryption-robustness";
const char kEncryptionSchemeQueryName[] = "encryption-type";
const char kEncryptionIvQueryName[] = "encryption-iv-size";

const char kSwSecureRobustness[] = "SW_SECURE_DECODE";
const char kHwSecureRobustness[] = "HW_SECURE_ALL";

// The followings define the supported codecs and encryption schemes that we try
// to query.
constexpr VideoCodec kAllVideoCodecs[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    VideoCodec::kH264,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    VideoCodec::kHEVC,
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    VideoCodec::kDolbyVision,
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    VideoCodec::kVP9, VideoCodec::kAV1};

constexpr AudioCodec kAllAudioCodecs[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    AudioCodec::kAAC,        AudioCodec::kEAC3, AudioCodec::kAC3,
    AudioCodec::kMpegHAudio,
#endif
    AudioCodec::kVorbis,     AudioCodec::kFLAC, AudioCodec::kOpus};

constexpr EncryptionScheme kAllEncryptionSchemes[] = {EncryptionScheme::kCenc,
                                                      EncryptionScheme::kCbcs};
using IsTypeSupportedCB =
    base::RepeatingCallback<bool(const std::string& content_type)>;

bool IsTypeSupportedInternal(
    ComPtr<IMFContentDecryptionModuleFactory> cdm_factory,
    const std::string& key_system,
    bool is_hw_secure,
    const std::string& content_type) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  bool supported =
      cdm_factory->IsTypeSupported(base::UTF8ToWide(key_system).c_str(),
                                   base::UTF8ToWide(content_type).c_str());
  // The above function may take seconds to run. Report UMA to understand the
  // actual performance impact. Report UMA only for success cases.
  if (supported) {
    auto uma_name = "Media.EME.MediaFoundationService." +
                    GetKeySystemNameForUMA(key_system, is_hw_secure) +
                    ".IsTypeSupported";
    base::UmaHistogramTimes(uma_name, base::TimeTicks::Now() - start_time);
  }

  DVLOG(3) << __func__ << " " << (supported ? "[yes]" : "[no]") << ": "
           << key_system << ", " << content_type;

  return supported;
}

std::string GetFourCCString(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return "avc1";
    case VideoCodec::kVP9:
      return "vp09";
    case VideoCodec::kHEVC:
    case VideoCodec::kDolbyVision:
      return "hvc1";
    case VideoCodec::kAV1:
      return "av01";
    default:
      NOTREACHED()
          << "This video codec is not supported by MediaFoundationCDM. codec="
          << GetCodecName(codec);
  }
  return "";
}

// Returns an "ext-profile" feature query (with ending comma) for a video codec.
// Returns an empty string if "ext-profile" is not needed.
std::string GetExtProfile(VideoCodec codec) {
  if (codec == VideoCodec::kDolbyVision)
    return "ext-profile=dvhe.05,";

  return "";
}

std::string GetFourCCString(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAAC:
      return "mp4a";
    case AudioCodec::kVorbis:
      return "vrbs";
    case AudioCodec::kFLAC:
      return "fLaC";
    case AudioCodec::kOpus:
      return "Opus";
    case AudioCodec::kEAC3:
      return "ec-3";
    case AudioCodec::kAC3:
      return "ac-3";
    case AudioCodec::kMpegHAudio:
      return "mhm1";
    default:
      NOTREACHED()
          << "This audio codec is not supported by MediaFoundationCDM. codec="
          << GetCodecName(codec);
  }
  return "";
}

std::string GetName(EncryptionScheme scheme) {
  switch (scheme) {
    case EncryptionScheme::kCenc:
      return "cenc";
    case EncryptionScheme::kCbcs:
      return "cbcs";
    default:
      NOTREACHED() << "Only cenc and cbcs are supported";
  }
  return "";
}

// According to the common encryption spec, both 8 and 16 bytes IV are allowed
// for CENC and CBCS. But some platforms may only support 8 byte IV CENC and
// Chromium does not differentiate IV size for each encryption scheme, so we use
// 8 for CENC and 16 for CBCS to provide the best coverage as those combination
// are recommended.
int GetIvSize(EncryptionScheme scheme) {
  switch (scheme) {
    case EncryptionScheme::kCenc:
      return 8;
    case EncryptionScheme::kCbcs:
      return 16;
    default:
      NOTREACHED() << "Only cenc and cbcs are supported";
  }
  return 0;
}

// Feature name:value mapping.
using FeatureMap = std::map<std::string, std::string>;

// Construct the query type string based on `video_codec`, optional
// `audio_codec`, `kDefaultFeatures` and `extra_features`.
std::string GetTypeString(VideoCodec video_codec,
                          absl::optional<AudioCodec> audio_codec,
                          const FeatureMap& extra_features) {
  auto codec_string = GetFourCCString(video_codec);
  if (audio_codec.has_value())
    codec_string += "," + GetFourCCString(audio_codec.value());

  auto feature_string = GetExtProfile(video_codec) + kDefaultFeatures;
  DCHECK(!feature_string.empty()) << "default feature cannot be empty";
  for (const auto& feature : extra_features) {
    DCHECK(!feature.first.empty() && !feature.second.empty());
    feature_string += "," + feature.first + "=" + feature.second;
  }

  return base::ReplaceStringPlaceholders(
      "video/mp4;codecs=\"$1\";features=\"$2\"", {codec_string, feature_string},
      /*offsets=*/nullptr);
}

base::flat_set<EncryptionScheme> GetSupportedEncryptionSchemes(
    IsTypeSupportedCB callback,
    VideoCodec video_codec,
    const std::string& robustness) {
  base::flat_set<EncryptionScheme> supported_schemes;
  for (const auto scheme : kAllEncryptionSchemes) {
    auto type = GetTypeString(
        video_codec, /*audio_codec=*/absl::nullopt,
        {{kEncryptionSchemeQueryName, GetName(scheme)},
         {kEncryptionIvQueryName, base::NumberToString(GetIvSize(scheme))},
         {kRobustnessQueryName, robustness.c_str()}});

    if (callback.Run(type))
      supported_schemes.insert(scheme);
  }
  return supported_schemes;
}

absl::optional<CdmCapability> GetCdmCapability(IsTypeSupportedCB callback,
                                               bool is_hw_secure) {
  DVLOG(2) << __func__ << ", is_hw_secure=" << is_hw_secure;

  // TODO(hmchen): make this generic for more key systems.
  const std::string robustness =
      is_hw_secure ? kHwSecureRobustness : kSwSecureRobustness;

  CdmCapability capability;

  // Query video codecs.
  for (const auto video_codec : kAllVideoCodecs) {
    auto type = GetTypeString(video_codec, /*audio_codec=*/absl::nullopt,
                              {{kRobustnessQueryName, robustness}});

    if (callback.Run(type)) {
      // IsTypeSupported() does not support querying profiling, so specify {}
      // to indicate all relevant profiles should be considered supported.
      const std::vector<media::VideoCodecProfile> kAllProfiles = {};
      capability.video_codecs.emplace(video_codec, kAllProfiles);
    }
  }

  // IsTypeSupported query string requires video codec, so stops if no video
  // codecs are supported.
  if (capability.video_codecs.empty()) {
    DVLOG(2) << "No video codecs supported for is_hw_secure=" << is_hw_secure;
    return absl::nullopt;
  }

  // Query audio codecs.
  // Audio is usually independent to the video codec. So we use <one of the
  // supported video codecs> + <audio codec> to query the audio capability.
  for (const auto audio_codec : kAllAudioCodecs) {
    const auto& video_codec = capability.video_codecs.begin()->first;
    auto type = GetTypeString(video_codec, audio_codec,
                              {{kRobustnessQueryName, robustness}});

    if (callback.Run(type))
      capability.audio_codecs.push_back(audio_codec);
  }

  // Query encryption scheme.

  // Note that the CdmCapability assumes all `video_codecs` + `encryption_
  // schemes` combinations are supported. However, in Media Foundation,
  // encryption scheme may be dependent on video codecs, so we query the
  // encryption scheme for all supported video codecs and get the intersection
  // of the encryption schemes which work for all codecs.
  base::flat_set<EncryptionScheme> intersection(
      std::begin(kAllEncryptionSchemes), std::end(kAllEncryptionSchemes));
  for (auto codec : capability.video_codecs) {
    const auto schemes =
        GetSupportedEncryptionSchemes(callback, codec.first, robustness);
    intersection = base::STLSetIntersection<base::flat_set<EncryptionScheme>>(
        intersection, schemes);
  }

  if (intersection.empty()) {
    // Fail if no supported encryption scheme.
    return absl::nullopt;
  }

  capability.encryption_schemes = intersection;

  // IsTypeSupported does not support session type yet. So just use temporary
  // session which is required by EME spec.
  capability.session_types.insert(CdmSessionType::kTemporary);
  return capability;
}

}  // namespace

MediaFoundationService::MediaFoundationService(
    mojo::PendingReceiver<mojom::MediaFoundationService> receiver)
    : receiver_(this, std::move(receiver)) {
  DVLOG(1) << __func__;
  mojo_media_client_.Initialize();
}

MediaFoundationService::~MediaFoundationService() {
  DVLOG(1) << __func__;
}

void MediaFoundationService::IsKeySystemSupported(
    const std::string& key_system,
    IsKeySystemSupportedCallback callback) {
  DVLOG(2) << __func__ << ", key_system=" << key_system;

  SCOPED_UMA_HISTOGRAM_TIMER(
      "Media.EME.MediaFoundationService.IsKeySystemSupported");

  ComPtr<IMFContentDecryptionModuleFactory> cdm_factory;
  HRESULT hr = MediaFoundationCdmModule::GetInstance()->GetCdmFactory(
      key_system, cdm_factory);

  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to GetCdmFactory.";
    std::move(callback).Run(false, nullptr);
    return;
  }

  absl::optional<CdmCapability> sw_secure_capability = GetCdmCapability(
      base::BindRepeating(&IsTypeSupportedInternal, cdm_factory, key_system,
                          /*is_hw_secure=*/false),
      /*is_hw_secure=*/false);
  absl::optional<CdmCapability> hw_secure_capability = GetCdmCapability(
      base::BindRepeating(&IsTypeSupportedInternal, cdm_factory, key_system,
                          /*is_hw_secure=*/true),
      /*is_hw_secure=*/true);

  if (!sw_secure_capability && !hw_secure_capability) {
    DVLOG(2) << "Get empty CdmCapability.";
    std::move(callback).Run(false, nullptr);
    return;
  }

  auto capability = media::mojom::KeySystemCapability::New();
  capability->sw_secure_capability = sw_secure_capability;
  capability->hw_secure_capability = hw_secure_capability;
  std::move(callback).Run(true, std::move(capability));
}

void MediaFoundationService::CreateInterfaceFactory(
    mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
    mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces) {
  DVLOG(2) << __func__;
  interface_factory_receivers_.Add(
      std::make_unique<InterfaceFactoryImpl>(std::move(frame_interfaces),
                                             &mojo_media_client_),
      std::move(receiver));
}

}  // namespace media
