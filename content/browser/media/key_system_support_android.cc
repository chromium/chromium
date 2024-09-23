// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_android.h"

#include <utility>
#include <vector>

#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "content/public/browser/android/android_overlay_provider.h"
#include "content/public/browser/service_process_host.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/audio_codecs.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/mediadrm_support.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

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
    media::VideoCodec::kVP9,         media::VideoCodec::kAV1,
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

// Determine the capabilities for `key_system` based on `webm_supported` and
// `mp4_supported`, and call `cdm_capability_cb` with the resulting capability.
// This class assumes that MediaDrm does support `key_system`.
void DetermineKeySystemSupport(const std::string& key_system,
                               bool is_secure,
                               media::CdmCapabilityCB cdm_capability_cb,
                               bool webm_supported,
                               bool mp4_supported) {
  const std::vector<media::VideoCodecProfile> kAllProfiles = {};
  media::CdmCapability capability;

  DVLOG(1) << __func__ << " mp4_supported: " << mp4_supported
           << ", webm_supported: " << webm_supported;

  if (webm_supported) {
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
  if (mp4_supported) {
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

  if (is_secure && capability.video_codecs.empty()) {
    // There are currently no hardware secure audio codecs on Android.
    // If there are no hardware secure video codecs available and the check
    // is for hardware secure, then this key system is not available.
    // TODO(b/266240828): Add checking for hardware secure audio codecs
    // once they exist.
    DVLOG(1) << "Key system " << key_system
             << " not supported as no hardware secure video codecs available.";
    std::move(cdm_capability_cb).Run(std::nullopt);
    return;
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

  capability.encryption_schemes.insert(media::EncryptionScheme::kCenc);
  capability.encryption_schemes.insert(media::EncryptionScheme::kCbcs);

  capability.session_types.insert(media::CdmSessionType::kTemporary);
  if (MediaDrmBridge::IsPersistentLicenseTypeSupported(key_system)) {
    capability.session_types.insert(media::CdmSessionType::kPersistentLicense);
  }

  std::move(cdm_capability_cb).Run(capability);
}

// Used to determine if `key_system` is supported, and if it is whether WebM and
// MP4 mime types are also supported. Done via a separate process so that
// crashes in MediaDrm do not crash the browser. This class destructs itself
// when complete.
class CheckCdmCompatibility {
 public:
  CheckCdmCompatibility(const std::string& key_system,
                        bool is_secure,
                        media::CdmCapabilityCB cdm_capability_cb)
      : key_system_(key_system),
        is_secure_(is_secure),
        cdm_capability_cb_(std::move(cdm_capability_cb)) {}

  ~CheckCdmCompatibility() = default;

  // Creates the remote process and calls it.
  void CheckKeySystemSupport() {
    media_drm_service_ =
        content::ServiceProcessHost::Launch<media::mojom::MediaDrmSupport>(
            content::ServiceProcessHost::Options()
                .WithDisplayName("MediaDrmSupport")
                .Pass());

    // As the calls to MediaDrm can crash and take out the utility process,
    // set up a handler for when this happens.
    media_drm_service_.set_disconnect_handler(base::BindOnce(
        &CheckCdmCompatibility::OnServiceClosed, base::Unretained(this)));

    DVLOG(1) << __func__ << " calling IsKeySystemSupported for " << key_system_;
    media_drm_service_->IsKeySystemSupported(
        key_system_,
        base::BindOnce(&CheckCdmCompatibility::VerifyKeySystemSupport,
                       base::Unretained(this)));
  }

  // Called when the remote process has determined if `key_system` and WebM/MP4
  // are supported.
  void VerifyKeySystemSupport(
      media::mojom::MediaDrmSupportResultPtr key_system_support_result) {
    if (key_system_support_result.is_null()) {
      DVLOG(1) << "Key system " << key_system_ << " not supported.";
      std::move(cdm_capability_cb_).Run(std::nullopt);
      delete this;
      return;
    }

    DetermineKeySystemSupport(
        key_system_, is_secure_, std::move(cdm_capability_cb_),
        key_system_support_result->key_system_supports_video_webm,
        key_system_support_result->key_system_supports_video_mp4);
    delete this;
  }

  // If the remote service fails, assume it has crashed and thus `key_system`
  // is not supported.
  void OnServiceClosed() {
    DVLOG(1) << "IsKeySystemSupported failed for " << key_system_;
    std::move(cdm_capability_cb_).Run(std::nullopt);
    delete this;
  }

 private:
  const std::string key_system_;
  const bool is_secure_;
  media::CdmCapabilityCB cdm_capability_cb_;
  mojo::Remote<media::mojom::MediaDrmSupport> media_drm_service_;
};

}  // namespace

void GetAndroidCdmCapability(const std::string& key_system,
                             CdmInfo::Robustness robustness,
                             media::CdmCapabilityCB cdm_capability_cb) {
  // Rendering of hardware secure codecs is only supported when AndroidOverlay
  // is enabled.
  const bool is_secure = robustness == CdmInfo::Robustness::kHardwareSecure;
  if (is_secure) {
    bool are_overlay_supported =
        content::AndroidOverlayProvider::GetInstance()->AreOverlaysSupported();
    bool overlay_fullscreen_video =
        base::FeatureList::IsEnabled(media::kOverlayFullscreenVideo);
    if (!are_overlay_supported || !overlay_fullscreen_video) {
      DVLOG(1) << "Hardware secure codecs not supported for key system"
               << key_system << ".";
      std::move(cdm_capability_cb).Run(std::nullopt);
      return;
    }
  }

  // Calls to MediaDrm.isCryptoSchemeSupported() are known to crash
  // (see b/308692917), so calling them via a utility process to avoid
  // crashing the browser if allowed.
  if (base::FeatureList::IsEnabled(
          media::kAllowMediaCodecCallsInSeparateProcess)) {
    // The class CheckCdmCompatibility will manage it's own lifetime
    // (destruct after calling `cdm_capability_cb`).
    auto* check_cdm_compatibility = new CheckCdmCompatibility(
        key_system, is_secure, std::move(cdm_capability_cb));
    check_cdm_compatibility->CheckKeySystemSupport();
    return;
  }

  // Multiple processes are not allowed, so call MediaDrmBridge directly.
  if (!MediaDrmBridge::IsKeySystemSupported(key_system)) {
    std::move(cdm_capability_cb).Run(std::nullopt);
    return;
  }

  bool webm_supported =
      MediaDrmBridge::IsKeySystemSupportedWithType(key_system, "video/webm");
  bool mp4_supported =
      MediaDrmBridge::IsKeySystemSupportedWithType(key_system, "video/mp4");
  DetermineKeySystemSupport(key_system, is_secure, std::move(cdm_capability_cb),
                            webm_supported, mp4_supported);
}

}  // namespace content
