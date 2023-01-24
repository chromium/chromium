// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_android.h"

#include <utility>
#include <vector>

#include "base/android/build_info.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/audio_codecs.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"

using media::MediaCodecUtil;
using media::MediaDrmBridge;

namespace content {

namespace {

// Vorbis is not supported. See http://crbug.com/710924 for details.

const media::AudioCodec kWebMAudioCodecsToQuery[] = {
    media::AudioCodec::kOpus,
};

const media::AudioCodec kMP4AudioCodecsToQuery[] = {
    media::AudioCodec::kFLAC,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    media::AudioCodec::kAAC,
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    media::AudioCodec::kAC3,  media::AudioCodec::kEAC3,
#endif
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    media::AudioCodec::kDTS,  media::AudioCodec::kDTSXP2,
#endif
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
};

const media::VideoCodec kWebMVideoCodecsToQuery[] = {
    media::VideoCodec::kVP8,
    media::VideoCodec::kVP9,
};

const media::VideoCodec kMP4VideoCodecsToQuery[] = {
    media::VideoCodec::kVP9,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    media::VideoCodec::kH264,
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    media::VideoCodec::kHEVC,
#endif
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    media::VideoCodec::kDolbyVision,
#endif
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
};

// Is an audio sink connected which supports the given codec?
static bool CanPassthrough(media::AudioCodec codec) {
  return (media::AudioManagerAndroid::GetSinkAudioEncodingFormats() &
          media::ConvertAudioCodecToBitstreamFormat(codec)) != 0;
}

}  // namespace

void GetAndroidCdmCapability(const std::string& key_system,
                             CdmInfo::Robustness robustness,
                             media::CdmCapabilityCB cdm_capability_cb) {
  const bool is_secure = robustness == CdmInfo::Robustness::kHardwareSecure;
  if (!MediaDrmBridge::IsKeySystemSupported(key_system)) {
    DVLOG(1) << "Key system " << key_system << " not supported.";
    std::move(cdm_capability_cb).Run(absl::nullopt);
    return;
  }

  const std::vector<media::VideoCodecProfile> kAllProfiles = {};
  media::CdmCapability capability;
  if (MediaDrmBridge::IsKeySystemSupportedWithType(key_system, "video/webm")) {
    for (const auto& codec : kWebMAudioCodecsToQuery) {
      if (MediaCodecUtil::CanDecode(codec)) {
        capability.audio_codecs.insert(codec);
      }
    }
    for (const auto& codec : kWebMVideoCodecsToQuery) {
      if (MediaCodecUtil::CanDecode(codec, is_secure)) {
        capability.video_codecs.emplace(codec, kAllProfiles);
      }
    }
  }

  // |audio_codecs| and |video_codecs| should not have multiple entries with
  // the same codec, so if the loop above added them, no need to test the same
  // codec again.
  if (MediaDrmBridge::IsKeySystemSupportedWithType(key_system, "video/mp4")) {
    // It is possible that a device that is not able to decode the audio stream
    // is connected to an audiosink device that can. In this case, CanDecode()
    // returns false but CanPassthrough() will return true. CanPassthrough()
    // calls AudioManagerAndroid::GetSinkAudioEncodingFormats() to retrieve a
    // bitmask representing audio bitstream formats that are supported by the
    // connected audiosink device. This bitmask is then matched against current
    // audio stream's codec type. A match indicates that the connected
    // audiosink device is able to decode the current audio stream and Chromium
    // should passthrough the audio bitstream instead of trying to decode it.
    for (const auto& codec : kMP4AudioCodecsToQuery) {
      if (!capability.audio_codecs.contains(codec)) {
        if (MediaCodecUtil::CanDecode(codec) || CanPassthrough(codec)) {
          capability.audio_codecs.insert(codec);
        }
      }
    }
    for (const auto& codec : kMP4VideoCodecsToQuery) {
      if (!capability.video_codecs.contains(codec)) {
        if (MediaCodecUtil::CanDecode(codec, is_secure)) {
          capability.video_codecs.emplace(codec, kAllProfiles);
        }
      }
    }
  }

  // For VP9 and HEVC, if they are supported, then determine which profiles
  // are supported.
  auto vp9 = capability.video_codecs.find(media::VideoCodec::kVP9);
  auto hevc = capability.video_codecs.find(media::VideoCodec::kHEVC);
  if (vp9 != capability.video_codecs.end() ||
      hevc != capability.video_codecs.end()) {
    std::vector<media::CodecProfileLevel> profiles;
    media::MediaCodecUtil::AddSupportedCodecProfileLevels(&profiles);
    for (const auto& profile : profiles) {
      switch (profile.codec) {
        case media::VideoCodec::kVP9:
          if (vp9 != capability.video_codecs.end()) {
            vp9->second.supported_profiles.insert(profile.profile);
          }
          break;
        case media::VideoCodec::kHEVC:
          if (hevc != capability.video_codecs.end()) {
            hevc->second.supported_profiles.insert(profile.profile);
          }
          break;
        default:
          break;
      }
    }
  }

  // 'cenc' is always supported. 'cbcs' may or may not be available.
  capability.encryption_schemes.insert(media::EncryptionScheme::kCenc);
  if (MediaCodecUtil::PlatformSupportsCbcsEncryption(
          base::android::BuildInfo::GetInstance()->sdk_int())) {
    capability.encryption_schemes.insert(media::EncryptionScheme::kCbcs);
  }

  capability.session_types.insert(media::CdmSessionType::kTemporary);
  if (MediaDrmBridge::IsPersistentLicenseTypeSupported(key_system)) {
    capability.session_types.insert(media::CdmSessionType::kPersistentLicense);
  }

  std::move(cdm_capability_cb).Run(capability);
}

}  // namespace content
