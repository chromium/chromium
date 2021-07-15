// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_service.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "media/base/audio_codecs.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_capability.h"
#include "media/cdm/win/media_foundation_cdm_module.h"
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
    VideoCodec::kCodecH264,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    VideoCodec::kCodecHEVC,
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    VideoCodec::kCodecDolbyVision,
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    VideoCodec::kCodecVP9, VideoCodec::kCodecAV1};

constexpr AudioCodec kAllAudioCodecs[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    AudioCodec::kCodecAAC,    AudioCodec::kCodecEAC3,
    AudioCodec::kCodecAC3,    AudioCodec::kCodecMpegHAudio,
#endif
    AudioCodec::kCodecVorbis, AudioCodec::kCodecFLAC,
    AudioCodec::kCodecOpus};

constexpr EncryptionScheme kAllEncryptionSchemes[] = {EncryptionScheme::kCenc,
                                                      EncryptionScheme::kCbcs};
using IsTypeSupportedCB =
    base::RepeatingCallback<bool(const std::string& content_type)>;

bool IsTypeSupportedInternal(
    ComPtr<IMFContentDecryptionModuleFactory> cdm_factory,
    const std::string& key_system,
    const std::string& content_type) {
  return cdm_factory->IsTypeSupported(base::UTF8ToWide(key_system).c_str(),
                                      base::UTF8ToWide(content_type).c_str());
}

std::string GetFourCCString(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kCodecH264:
      return "avc1";
    case VideoCodec::kCodecVP9:
      return "vp09";
    case VideoCodec::kCodecHEVC:
      return "hvc1";
    case VideoCodec::kCodecDolbyVision:
      return "dvhe";
    case VideoCodec::kCodecAV1:
      return "av01";
    default:
      NOTREACHED()
          << "This video codec is not supported by MediaFoundationCDM. codec="
          << GetCodecName(codec);
  }
  return "";
}

std::string GetFourCCString(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kCodecAAC:
      return "mp4a";
    case AudioCodec::kCodecVorbis:
      return "vrbs";
    case AudioCodec::kCodecFLAC:
      return "fLaC";
    case AudioCodec::kCodecOpus:
      return "Opus";
    case AudioCodec::kCodecEAC3:
      return "ec-3";
    case AudioCodec::kCodecAC3:
      return "ac-3";
    case AudioCodec::kCodecMpegHAudio:
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

base::flat_set<EncryptionScheme> GetSupportedEncryptionSchemes(
    IsTypeSupportedCB callback,
    VideoCodec codec,
    const std::string& robustness) {
  base::flat_set<EncryptionScheme> supported_schemes;
  for (const auto scheme : kAllEncryptionSchemes) {
    auto type = base::ReplaceStringPlaceholders(
        "video/mp4;codecs=\"$1\";features=\"$2,$3=$4,$5=$6,$7=$8\"",
        {GetFourCCString(codec), kDefaultFeatures, kEncryptionSchemeQueryName,
         GetName(scheme), kEncryptionIvQueryName,
         base::NumberToString(GetIvSize(scheme)), kRobustnessQueryName,
         robustness.c_str()},
        0);

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
  for (const auto codec : kAllVideoCodecs) {
    auto content_type = base::ReplaceStringPlaceholders(
        "video/mp4;codecs=\"$1\";features=\"$2,$3=$4\"",
        {GetFourCCString(codec), kDefaultFeatures, kRobustnessQueryName,
         robustness},
        /*offsets=*/nullptr);

    if (callback.Run(content_type)) {
      // IsTypeSupported() does not support querying profiling, so specify {}
      // to indicate all relevant profiles should be considered supported.
      const std::vector<media::VideoCodecProfile> kAllProfiles = {};
      capability.video_codecs.emplace(codec, kAllProfiles);
    }
  }

  // IsTypeSupported query string requires video codec, so stops if no video
  // codecs are supported.
  if (capability.video_codecs.empty()) {
    DVLOG(2) << "No video codecs are supported.";
    return absl::nullopt;
  }

  // Query audio codecs.
  // Audio is usually independent to the video codec. So we use <one of the
  // supported video codecs> + <audio codec> to query the audio capability.
  for (const auto codec : kAllAudioCodecs) {
    auto type = base::ReplaceStringPlaceholders(
        "video/mp4;codecs=\"$1,$2\";features=\"$3,$4=$5\"",
        {GetFourCCString(capability.video_codecs.begin()->first),
         GetFourCCString(codec), kDefaultFeatures, kRobustnessQueryName,
         robustness},
        /*offsets=*/nullptr);

    if (callback.Run(type))
      capability.audio_codecs.push_back(codec);
  }

  // Query encryption scheme.

  // Note that the CdmCapability assumes all `video_codecs` + `encryotion_
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
    mojo::PendingReceiver<mojom::MediaFoundationService> receiver,
    const base::FilePath& user_data_dir)
    : receiver_(this, std::move(receiver)), mojo_media_client_(user_data_dir) {
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
  ComPtr<IMFContentDecryptionModuleFactory> cdm_factory;
  HRESULT hr = MediaFoundationCdmModule::GetInstance()->GetCdmFactory(
      key_system, cdm_factory);

  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to GetCdmFactory.";
    std::move(callback).Run(false, nullptr);
    return;
  }

  IsTypeSupportedCB is_type_supported_cb =
      base::BindRepeating(&IsTypeSupportedInternal, cdm_factory, key_system);

  absl::optional<CdmCapability> sw_secure_capability =
      GetCdmCapability(is_type_supported_cb, /*is_hw_secure=*/false);
  absl::optional<CdmCapability> hw_secure_capability =
      GetCdmCapability(is_type_supported_cb, /*is_hw_secure=*/true);

  if (!sw_secure_capability && !hw_secure_capability) {
    DVLOG(2) << "Get empty CdmCapbility.";
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
