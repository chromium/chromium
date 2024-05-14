// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_service.h"

#include <map>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/base/audio_codecs.h"
#include "media/base/cdm_capability.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/key_system_capability.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/cdm/win/media_foundation_cdm_module.h"
#include "media/cdm/win/media_foundation_cdm_util.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "media/mojo/services/interface_factory_impl.h"

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
    AudioCodec::kAAC,
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    AudioCodec::kEAC3,       AudioCodec::kAC3,
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
    AudioCodec::kAC4,
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#if BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
    AudioCodec::kMpegHAudio,
#endif  // BUILDFLAG(ENABLE_PLATFORM_MPEG_H_AUDIO)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    AudioCodec::kVorbis,     AudioCodec::kFLAC, AudioCodec::kOpus};

constexpr EncryptionScheme kAllEncryptionSchemes[] = {EncryptionScheme::kCenc,
                                                      EncryptionScheme::kCbcs};

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
      NOTREACHED_IN_MIGRATION()
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
    case AudioCodec::kAC4:
      return "ac-4";
    case AudioCodec::kMpegHAudio:
      return "mhm1";
    default:
      NOTREACHED_IN_MIGRATION()
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
      NOTREACHED_IN_MIGRATION() << "Only cenc and cbcs are supported";
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
      NOTREACHED_IN_MIGRATION() << "Only cenc and cbcs are supported";
  }
  return 0;
}

// Feature name:value mapping.
using FeatureMap = std::map<std::string, std::string>;

// Construct the query type string based on `video_codec`, optional
// `audio_codec`, `kDefaultFeatures` and `extra_features`.
std::string GetTypeString(VideoCodec video_codec,
                          std::optional<AudioCodec> audio_codec,
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

// Consolidates the information to construct the type string in only one place.
// This will help us avoid errors in faulty creation of the type string, and
// centralize from where we call IsTypeSupportedInternal()
bool IsTypeSupported(VideoCodec video_codec,
                     std::optional<AudioCodec> audio_codec,
                     const FeatureMap& extra_features,
                     ComPtr<IMFContentDecryptionModuleFactory> cdm_factory,
                     const std::string& key_system,
                     bool is_hw_secure) {
  auto type = GetTypeString(video_codec, audio_codec, extra_features);

  return IsTypeSupportedInternal(cdm_factory, key_system, is_hw_secure, type);
}

base::flat_set<EncryptionScheme> GetSupportedEncryptionSchemes(
    ComPtr<IMFContentDecryptionModuleFactory> cdm_factory,
    const std::string& key_system,
    bool is_hw_secure,
    VideoCodec video_codec,
    const std::string& robustness) {
  base::flat_set<EncryptionScheme> supported_schemes;
  for (const auto scheme : kAllEncryptionSchemes) {
    const FeatureMap extra_features = {
        {kEncryptionSchemeQueryName, GetName(scheme)},
        {kEncryptionIvQueryName, base::NumberToString(GetIvSize(scheme))},
        {kRobustnessQueryName, robustness.c_str()}};

    if (IsTypeSupported(video_codec, /*audio_codec=*/std::nullopt,
                        extra_features, cdm_factory, key_system,
                        is_hw_secure)) {
      supported_schemes.insert(scheme);
    }
  }
  return supported_schemes;
}

HRESULT CreateDummyMediaFoundationCdm(
    ComPtr<IMFContentDecryptionModuleFactory> cdm_factory,
    const std::string& key_system) {
  // Set `use_hw_secure_codecs` to indicate this for hardware secure mode,
  // which typically requires identifier and persistent storage.
  CdmConfig cdm_config = {key_system, /*allow_distinctive_identifier=*/true,
                          /*allow_persistent_state=*/true,
                          /*use_hw_secure_codecs=*/true};

  // Use a random CDM origin.
  auto cdm_origin_id = base::UnguessableToken::Create();

  // Use a dummy CDM store path root under the temp dir here. Since this code
  // runs in the LPAC process, the temp dir will be something like:
  //   C:\Users\<user>\AppData\Local\Packages\cr.sb.cdm<...>\AC\Temp
  // This folder is specifically for the CDM app container, so there's no need
  // to set ACL explicitly.
  base::FilePath temp_dir;
  base::PathService::Get(base::DIR_TEMP, &temp_dir);
  const char kDummyCdmStore[] = "DummyMediaFoundationCdmStore";
  auto dummy_cdm_store_path_root = temp_dir.AppendASCII(kDummyCdmStore);

  // Create the dummy CDM.
  Microsoft::WRL::ComPtr<IMFContentDecryptionModule> mf_cdm;
  auto hr = CreateMediaFoundationCdm(cdm_factory, cdm_config, cdm_origin_id,
                                     /*cdm_client_token=*/std::nullopt,
                                     dummy_cdm_store_path_root, mf_cdm);
  DLOG_IF(ERROR, FAILED(hr)) << __func__ << ": Failed for " << key_system;
  mf_cdm.Reset();

  // Delete the dummy CDM store folder so we don't leave files behind. This may
  // fail since the CDM and related objects may have the files open longer than
  // the total delete retry period or before the process terminates. This is
  // fine since they will be cleaned next time so files will not accumulate.
  // Ignore the `reply_callback` since nothing can be done with the result.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::GetDeletePathRecursivelyCallback(dummy_cdm_store_path_root));

  return hr;
}

std::optional<CdmCapability> GetCdmCapability(
    ComPtr<IMFContentDecryptionModuleFactory> cdm_factory,
    const std::string& key_system,
    bool is_hw_secure) {
  DVLOG(2) << __func__ << ", is_hw_secure=" << is_hw_secure;

  // For hardware secure decryption, even when IsTypeSupportedInternal() says
  // it's supported, CDM creation could fail immediately. Therefore, create a
  // dummy CDM instance to detect this case.
  if (is_hw_secure &&
      FAILED(CreateDummyMediaFoundationCdm(cdm_factory, key_system))) {
    return std::nullopt;
  }

  // TODO(hmchen): make this generic for more key systems.
  const std::string robustness =
      is_hw_secure ? kHwSecureRobustness : kSwSecureRobustness;

  CdmCapability capability;

  // Query video codecs.
  for (const auto video_codec : kAllVideoCodecs) {
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    // Only query encrypted HEVC when the feature is enabled.
    if (video_codec == VideoCodec::kHEVC &&
        !base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport)) {
      continue;
    }
#endif

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
    // Only query encrypted Dolby Vision when the feature is enabled.
    if (video_codec == VideoCodec::kDolbyVision &&
        !base::FeatureList::IsEnabled(kPlatformEncryptedDolbyVision)) {
      continue;
    }
#endif

    const FeatureMap extra_features = {{kRobustnessQueryName, robustness}};

    if (IsTypeSupported(video_codec, /*audio_codec=*/std::nullopt,
                        extra_features, cdm_factory, key_system,
                        is_hw_secure)) {
      // IsTypeSupported() does not support querying profiling, in general
      // assume all relevant profiles are supported.
      VideoCodecInfo video_codec_info;

      // `supports_clear_lead` should be set to false until detection for clear
      // lead support is fixed and the query works as expected.
      video_codec_info.supports_clear_lead = false;

#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
      // Dolby Vision on Windows only support profile 4/5/8 now. But profile 4
      // is rarely used and being deprecated, so only declare the support for
      // profile 5/8.
      if (video_codec == VideoCodec::kDolbyVision) {
        video_codec_info.supported_profiles = {
            VideoCodecProfile::DOLBYVISION_PROFILE5,
            VideoCodecProfile::DOLBYVISION_PROFILE8};
      }
#endif

      // We check for `!is_hw_secure` because clear lead should always be
      // supported for software security. When clear lead is supported
      // for hardware security (b/219818166), we will add a query to
      // set supports_clear_lead.
      if (!is_hw_secure) {
        video_codec_info.supports_clear_lead = true;
      }

      capability.video_codecs.emplace(video_codec, video_codec_info);
    }
  }

  // IsTypeSupported query string requires video codec, so stops if no video
  // codecs are supported.
  if (capability.video_codecs.empty()) {
    DVLOG(2) << "No video codecs supported for is_hw_secure=" << is_hw_secure;
    return std::nullopt;
  }

  // Query audio codecs.
  // Audio is usually independent to the video codec. So we use <one of the
  // supported video codecs> + <audio codec> to query the audio capability.
  for (const auto audio_codec : kAllAudioCodecs) {
    const auto& video_codec = capability.video_codecs.begin()->first;
    const FeatureMap extra_features = {{kRobustnessQueryName, robustness}};

    if (IsTypeSupported(video_codec, audio_codec, extra_features, cdm_factory,
                        key_system, is_hw_secure)) {
      capability.audio_codecs.emplace(audio_codec);
    }
  }

  // Query encryption scheme.

  // Note that the CdmCapability assumes all `video_codecs` + `encryption_
  // schemes` combinations are supported. However, in Media Foundation,
  // encryption scheme may be dependent on video codecs, so we query the
  // encryption scheme for all supported video codecs and get the intersection
  // of the encryption schemes which work for all codecs.
  base::flat_set<EncryptionScheme> intersection(
      std::begin(kAllEncryptionSchemes), std::end(kAllEncryptionSchemes));
  for (const auto& [video_codec, _] : capability.video_codecs) {
    const auto schemes = GetSupportedEncryptionSchemes(
        cdm_factory, key_system, is_hw_secure, video_codec, robustness);
    intersection = base::STLSetIntersection<base::flat_set<EncryptionScheme>>(
        intersection, schemes);
  }

  if (intersection.empty()) {
    // Fail if no supported encryption scheme.
    return std::nullopt;
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
    std::move(callback).Run(false, std::nullopt);
    return;
  }

  std::optional<CdmCapability> sw_secure_capability =
      GetCdmCapability(cdm_factory, key_system, /*is_hw_secure=*/false);
  std::optional<CdmCapability> hw_secure_capability =
      GetCdmCapability(cdm_factory, key_system, /*is_hw_secure=*/true);

  if (!sw_secure_capability && !hw_secure_capability) {
    DVLOG(2) << "Get empty CdmCapability.";
    std::move(callback).Run(false, std::nullopt);
    return;
  }

  auto capability = media::KeySystemCapability();
  capability.sw_secure_capability = sw_secure_capability;
  capability.hw_secure_capability = hw_secure_capability;
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
