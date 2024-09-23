// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_systems_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <unordered_map>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_names.h"
#include "media/base/media.h"
#include "media/base/media_client.h"
#include "media/base/media_switches.h"
#include "media/base/mime_util.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/media_buildflags.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace media {

namespace {

struct MimeTypeToCodecs {
  const char* mime_type;
  SupportedCodecs codecs;
};

// Mapping between containers and their codecs.
// Only audio codecs can belong to a "audio/*" mime_type, and only video codecs
// can belong to a "video/*" mime_type.
static const MimeTypeToCodecs kMimeTypeToCodecsMap[] = {
    {"audio/webm", EME_CODEC_WEBM_AUDIO_ALL},
    {"video/webm", EME_CODEC_WEBM_VIDEO_ALL},
    {"audio/mp4", EME_CODEC_MP4_AUDIO_ALL},
    {"video/mp4", EME_CODEC_MP4_VIDEO_ALL},
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
    {"video/mp2t", EME_CODEC_MP2T_VIDEO_ALL},
#endif  // BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
};

EmeCodec ToAudioEmeCodec(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAAC:
      return EME_CODEC_AAC;
    case AudioCodec::kVorbis:
      return EME_CODEC_VORBIS;
    case AudioCodec::kFLAC:
      return EME_CODEC_FLAC;
    case AudioCodec::kOpus:
      return EME_CODEC_OPUS;
    case AudioCodec::kEAC3:
      return EME_CODEC_EAC3;
    case AudioCodec::kAC3:
      return EME_CODEC_AC3;
    case AudioCodec::kAC4:
      return EME_CODEC_AC4;
    case AudioCodec::kIAMF:
      return EME_CODEC_IAMF;
    case AudioCodec::kMpegHAudio:
      return EME_CODEC_MPEG_H_AUDIO;
    case AudioCodec::kDTS:
      return EME_CODEC_DTS;
    case AudioCodec::kDTSXP2:
      return EME_CODEC_DTSXP2;
    case AudioCodec::kDTSE:
      return EME_CODEC_DTSE;
    default:
      DVLOG(1) << "Unsupported AudioCodec " << codec;
      return EME_CODEC_NONE;
  }
}

EmeCodec ToVideoEmeCodec(VideoCodec codec, VideoCodecProfile profile) {
  switch (codec) {
    case VideoCodec::kH264:
      return EME_CODEC_AVC1;
    case VideoCodec::kVP8:
      return EME_CODEC_VP8;
    case VideoCodec::kVP9:
      // ParseVideoCodecString() returns VIDEO_CODEC_PROFILE_UNKNOWN for "vp9"
      // and "vp9.0". Since these codecs are essentially the same as profile 0,
      // return EME_CODEC_VP9_PROFILE0.
      if (profile == VP9PROFILE_PROFILE0 ||
          profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
        return EME_CODEC_VP9_PROFILE0;
      } else if (profile == VP9PROFILE_PROFILE2) {
        return EME_CODEC_VP9_PROFILE2;
      } else {
        // Profile 1 and 3 not supported by EME. See https://crbug.com/898298.
        return EME_CODEC_NONE;
      }
    case VideoCodec::kHEVC:
      // Only handle Main and Main10 profiles for HEVC.
      if (profile == HEVCPROFILE_MAIN)
        return EME_CODEC_HEVC_PROFILE_MAIN;
      if (profile == HEVCPROFILE_MAIN10)
        return EME_CODEC_HEVC_PROFILE_MAIN10;
      return EME_CODEC_NONE;
    case VideoCodec::kDolbyVision:
      // Only profiles 0, 5, 7, 8, 9 are valid. Profile 0 and 9 are encoded
      // based on AVC while profile 5, 7 and 8 are based on HEVC.
      if (profile == DOLBYVISION_PROFILE0)
        return EME_CODEC_DOLBY_VISION_PROFILE0;
      if (profile == DOLBYVISION_PROFILE5)
        return EME_CODEC_DOLBY_VISION_PROFILE5;
      if (profile == DOLBYVISION_PROFILE7)
        return EME_CODEC_DOLBY_VISION_PROFILE7;
      if (profile == DOLBYVISION_PROFILE8)
        return EME_CODEC_DOLBY_VISION_PROFILE8;
      if (profile == DOLBYVISION_PROFILE9)
        return EME_CODEC_DOLBY_VISION_PROFILE9;
      return EME_CODEC_NONE;
    case VideoCodec::kAV1:
      return EME_CODEC_AV1;
    default:
      DVLOG(1) << "Unsupported VideoCodec " << codec;
      return EME_CODEC_NONE;
  }
}

class ClearKeyKeySystemInfo : public KeySystemInfo {
 public:
  std::string GetBaseKeySystemName() const final { return kClearKeyKeySystem; }

  bool IsSupportedInitDataType(EmeInitDataType init_data_type) const final {
    return init_data_type == EmeInitDataType::CENC ||
           init_data_type == EmeInitDataType::WEBM ||
           init_data_type == EmeInitDataType::KEYIDS;
  }

  EmeConfig::Rule GetEncryptionSchemeConfigRule(
      EncryptionScheme encryption_scheme) const final {
    switch (encryption_scheme) {
      case EncryptionScheme::kCenc:
      case EncryptionScheme::kCbcs: {
        return EmeConfig::SupportedRule();
      }
      case EncryptionScheme::kUnencrypted:
        break;
    }
    NOTREACHED();
  }

  SupportedCodecs GetSupportedCodecs() const final {
    // On Android, Vorbis, VP8, AAC and AVC1 are supported in MediaCodec:
    // http://developer.android.com/guide/appendix/media-formats.html
    // VP9 support is device dependent.
    return EME_CODEC_WEBM_ALL | EME_CODEC_MP4_ALL;
  }

  EmeConfig::Rule GetRobustnessConfigRule(
      const std::string& key_system,
      EmeMediaType media_type,
      const std::string& requested_robustness,
      const bool* /*hw_secure_requirement*/) const final {
    if (requested_robustness.empty()) {
      return EmeConfig::SupportedRule();
    } else {
      return EmeConfig::UnsupportedRule();
    }
  }

  EmeConfig::Rule GetPersistentLicenseSessionSupport() const final {
    return EmeConfig::UnsupportedRule();
  }

  EmeFeatureSupport GetPersistentStateSupport() const final {
    return EmeFeatureSupport::NOT_SUPPORTED;
  }

  EmeFeatureSupport GetDistinctiveIdentifierSupport() const final {
    return EmeFeatureSupport::NOT_SUPPORTED;
  }

  bool UseAesDecryptor() const final { return true; }
};

// Returns whether the |key_system| is known to Chromium and is thus likely to
// be implemented in an interoperable way.
// True is always returned for a |key_system| that begins with "x-".
//
// As with other web platform features, advertising support for a key system
// implies that it adheres to a defined and interoperable specification.
//
// To ensure interoperability, implementations of a specific |key_system| string
// must conform to a specification for that identifier that defines
// key system-specific behaviors not fully defined by the EME specification.
// That specification should be provided by the owner of the domain that is the
// reverse of the |key_system| string.
// This involves more than calling a library, SDK, or platform API.
// KeySystemsImpl must be populated appropriately, and there will likely be glue
// code to adapt to the API of the library, SDK, or platform API.
//
// Chromium mainline contains this data and glue code for specific key systems,
// which should help ensure interoperability with other implementations using
// these key systems.
//
// If you need to add support for other key systems, ensure that you have
// obtained the specification for how to integrate it with EME, implemented the
// appropriate glue/adapter code, and added all the appropriate data to
// KeySystemsImpl. Only then should you change this function.
static bool IsPotentiallySupportedKeySystem(const std::string& key_system) {
  if (key_system == kWidevineKeySystem)
    return true;

  if (key_system == kClearKeyKeySystem) {
    return true;
  }

  // External or MediaFoundation Clear Key is known and supports suffixes for
  // testing.
  if (IsExternalClearKey(key_system))
    return true;

  // Chromecast defines behaviors for Cast clients within its reverse domain.
  const char kChromecastRoot[] = "com.chromecast";
  if (IsSubKeySystemOf(key_system, kChromecastRoot))
    return true;

  // Implementations that do not have a specification or appropriate glue code
  // can use the "x-" prefix to avoid conflicting with and advertising support
  // for real key system names. Use is discouraged.
  const char kExcludedPrefix[] = "x-";
  return base::StartsWith(key_system, kExcludedPrefix,
                          base::CompareCase::SENSITIVE);
}

// Returns whether distinctive identifiers and persistent state can be reliably
// blocked for |key_system_info| (and therefore be safely configurable).
static bool CanBlock(const KeySystemInfo& key_system_info) {
  // When AesDecryptor is used, we are sure we can block.
  if (key_system_info.UseAesDecryptor()) {
    return true;
  }

  // For External Clear Key, it is either implemented as a library CDM (Clear
  // Key CDM), which is covered above, or by using AesDecryptor remotely, e.g.
  // via MojoCdm. In both cases, we can block. This is only used for testing.
  if (base::FeatureList::IsEnabled(kExternalClearKeyForTesting) &&
      IsExternalClearKey(key_system_info.GetBaseKeySystemName())) {
    return true;
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // When library CDMs are enabled, we are either using AesDecryptor, or using
  // the library CDM hosted in a sandboxed process. In both cases distinctive
  // identifiers and persistent state can be reliably blocked.
  return true;
#else
  // For other platforms assume the CDM can and will do anything. So we cannot
  // block.
  return false;
#endif
}

}  // namespace

KeySystemsImpl::KeySystemsImpl(RegisterKeySystemsSupportCB cb)
    : register_key_systems_support_cb_(std::move(cb)) {
  Initialize();
}

KeySystemsImpl::~KeySystemsImpl() {
  if (!update_callbacks_.empty())
    update_callbacks_.Notify();
}

void KeySystemsImpl::Initialize() {
  for (const auto& [mime_type, codecs] : kMimeTypeToCodecsMap)
    RegisterMimeType(mime_type, codecs);

  UpdateSupportedKeySystems();
}

void KeySystemsImpl::UpdateSupportedKeySystems() {
  DCHECK(!is_updating_);
  is_updating_ = true;

  if (!GetMediaClient()) {
    OnSupportedKeySystemsUpdated({});
    return;
  }

  key_system_support_registration_ =
      std::move(register_key_systems_support_cb_)
          .Run(
              base::BindRepeating(&KeySystemsImpl::OnSupportedKeySystemsUpdated,
                                  weak_factory_.GetWeakPtr()));
}

void KeySystemsImpl::UpdateIfNeeded(base::OnceClosure done_cb) {
  if (is_updating_) {
    // The callback will be resolved in OnSupportedKeySystemsUpdated().
    update_callbacks_.AddUnsafe(std::move(done_cb));
    return;
  }

  std::move(done_cb).Run();
}

SupportedCodecs KeySystemsImpl::GetCodecMaskForMimeType(
    const std::string& container_mime_type) const {
  auto iter = mime_type_to_codecs_map_.find(container_mime_type);
  if (iter == mime_type_to_codecs_map_.end())
    return EME_CODEC_NONE;

  DCHECK(IsValidMimeTypeCodecsCombination(container_mime_type, iter->second));
  return iter->second;
}

EmeCodec KeySystemsImpl::GetEmeCodecForString(
    EmeMediaType media_type,
    const std::string& container_mime_type,
    const std::string& codec_string) const {
  // Per spec, we should already reject empty mime types in
  // GetSupportedCapabilities().
  DCHECK(!container_mime_type.empty());

  // This is not checked because MimeUtil declares "vp9" and "vp9.0" as
  // ambiguous, but they have always been supported by EME.
  // TODO(xhwang): Find out whether we should fix MimeUtil about these cases.
  bool is_ambiguous = true;

  // For testing only.
  auto iter = codec_map_for_testing_.find(codec_string);
  if (iter != codec_map_for_testing_.end())
    return iter->second;

  if (media_type == EmeMediaType::AUDIO) {
    AudioCodec audio_codec = AudioCodec::kUnknown;
    ParseAudioCodecString(container_mime_type, codec_string, &is_ambiguous,
                          &audio_codec);
    DVLOG(3) << "Audio codec = " << audio_codec;
    return ToAudioEmeCodec(audio_codec);
  }

  DCHECK_EQ(media_type, EmeMediaType::VIDEO);

  // In general EmeCodec doesn't care about codec profiles and assumes the same
  // level of profile support as Chromium, which is checked in
  // KeySystemConfigSelector::IsSupportedContentType(). However, there are a few
  // exceptions where we need to know the profile. For example, for VP9, there
  // are older CDMs only supporting profile 0, hence EmeCodec differentiate
  // between VP9 profile 0 and higher profiles.
  auto result = ParseVideoCodecString(container_mime_type, codec_string,
                                      /*allow_ambiguous_matches=*/true);
  if (!result) {
    return EME_CODEC_NONE;
  }

  DVLOG(3) << "Video codec = " << result->codec
           << ", profile = " << result->profile;
  return ToVideoEmeCodec(result->codec, result->profile);
}

void KeySystemsImpl::OnSupportedKeySystemsUpdated(KeySystemInfos key_systems) {
  DVLOG(1) << __func__;

  is_updating_ = false;

  // Clear Key is always supported.
  key_systems.emplace_back(std::make_unique<ClearKeyKeySystemInfo>());

  ProcessSupportedKeySystems(std::move(key_systems));

  update_callbacks_.Notify();
}

void KeySystemsImpl::ProcessSupportedKeySystems(KeySystemInfos key_systems) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Clear `key_system_info_vector_` before repopulating it.
  key_system_info_vector_.clear();

  for (auto& key_system : key_systems) {
    DCHECK(!key_system->GetBaseKeySystemName().empty());
    DCHECK(key_system->GetPersistentStateSupport() !=
           EmeFeatureSupport::INVALID);
    DCHECK(key_system->GetDistinctiveIdentifierSupport() !=
           EmeFeatureSupport::INVALID);

    if (!IsPotentiallySupportedKeySystem(key_system->GetBaseKeySystemName())) {
      // If you encounter this path, see the comments for the function above.
      DLOG(ERROR) << "Unsupported name '" << key_system->GetBaseKeySystemName()
                  << "'. See code comments.";
      continue;
    }

    // Supporting persistent state is a prerequisite for supporting persistent
    // sessions.
    if (key_system->GetPersistentStateSupport() ==
        EmeFeatureSupport::NOT_SUPPORTED) {
      DCHECK(!key_system->GetPersistentLicenseSessionSupport().has_value());
    }

    if (!CanBlock(*key_system)) {
      DCHECK(key_system->GetDistinctiveIdentifierSupport() ==
             EmeFeatureSupport::ALWAYS_ENABLED);
      DCHECK(key_system->GetPersistentStateSupport() ==
             EmeFeatureSupport::ALWAYS_ENABLED);
    }

    const auto base_key_system_name = key_system->GetBaseKeySystemName();
    DVLOG(1) << __func__ << ": Adding key system " << base_key_system_name;
    key_system_info_vector_.push_back(std::move(key_system));
  }
}

const KeySystemInfo* KeySystemsImpl::GetKeySystemInfo(
    const std::string& key_system) const {
  DCHECK(!is_updating_);
  for (const auto& key_system_info : key_system_info_vector_) {
    const auto& base_key_system = key_system_info->GetBaseKeySystemName();
    if ((key_system == base_key_system ||
         IsSubKeySystemOf(key_system, base_key_system)) &&
        key_system_info->IsSupportedKeySystem(key_system)) {
      return key_system_info.get();
    }
  }

  return nullptr;
}

// Adds the MIME type with the codec mask after verifying the validity.
// Only this function should modify |mime_type_to_codecs_map_|.
void KeySystemsImpl::RegisterMimeType(const std::string& mime_type,
                                      SupportedCodecs codecs) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!mime_type_to_codecs_map_.count(mime_type));
  DCHECK(IsValidMimeTypeCodecsCombination(mime_type, codecs))
      << ": mime_type = " << mime_type << ", codecs = " << codecs;

  mime_type_to_codecs_map_[mime_type] = codecs;
}

// Returns whether |mime_type| follows a valid format and the specified codecs
// are of the correct type based on |*_codec_mask_|.
// Only audio/ or video/ MIME types with their respective codecs are allowed.
bool KeySystemsImpl::IsValidMimeTypeCodecsCombination(
    const std::string& mime_type,
    SupportedCodecs codecs) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (codecs == EME_CODEC_NONE)
    return true;

  if (base::StartsWith(mime_type, "audio/", base::CompareCase::SENSITIVE))
    return !(codecs & ~audio_codec_mask_);
  if (base::StartsWith(mime_type, "video/", base::CompareCase::SENSITIVE))
    return !(codecs & ~video_codec_mask_);

  return false;
}

bool KeySystemsImpl::IsSupportedInitDataType(
    const std::string& key_system,
    EmeInitDataType init_data_type) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto* key_system_info = GetKeySystemInfo(key_system);
  CHECK(key_system_info);

  return key_system_info->IsSupportedInitDataType(init_data_type);
}

EmeConfig::Rule KeySystemsImpl::GetEncryptionSchemeConfigRule(
    const std::string& key_system,
    EncryptionScheme encryption_scheme) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto* key_system_info = GetKeySystemInfo(key_system);
  CHECK(key_system_info);

  return key_system_info->GetEncryptionSchemeConfigRule(encryption_scheme);
}

void KeySystemsImpl::AddCodecMaskForTesting(EmeMediaType media_type,
                                            const std::string& codec,
                                            uint32_t mask) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!codec_map_for_testing_.count(codec));
  codec_map_for_testing_[codec] = static_cast<EmeCodec>(mask);
  if (media_type == EmeMediaType::AUDIO) {
    audio_codec_mask_ |= mask;
  } else {
    video_codec_mask_ |= mask;
  }
}

void KeySystemsImpl::AddMimeTypeCodecMaskForTesting(
    const std::string& mime_type,
    uint32_t codecs_mask) {
  RegisterMimeType(mime_type, static_cast<EmeCodec>(codecs_mask));
}

void KeySystemsImpl::ResetForTesting() {
  weak_factory_.InvalidateWeakPtrs();
  is_updating_ = false;
  DCHECK(update_callbacks_.empty())
      << "Should have no update callbacks for a clean test.";
  key_system_info_vector_.clear();
  mime_type_to_codecs_map_.clear();
  codec_map_for_testing_.clear();
  audio_codec_mask_ = EME_CODEC_AUDIO_ALL;
  video_codec_mask_ = EME_CODEC_VIDEO_ALL;

  Initialize();
}

std::string KeySystemsImpl::GetBaseKeySystemName(
    const std::string& key_system) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto* key_system_info = GetKeySystemInfo(key_system);
  if (!key_system_info) {
    NOTREACHED_IN_MIGRATION() << "Key system support should have been checked";
    return key_system;
  }

  return key_system_info->GetBaseKeySystemName();
}

bool KeySystemsImpl::IsSupportedKeySystem(const std::string& key_system) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return GetKeySystemInfo(key_system);
}

bool KeySystemsImpl::ShouldUseBaseKeySystemName(
    const std::string& key_system) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto* key_system_info = GetKeySystemInfo(key_system);
  if (!key_system_info) {
    NOTREACHED_IN_MIGRATION() << "Key system support should have been checked";
    return false;
  }

  return key_system_info->ShouldUseBaseKeySystemName();
}

bool KeySystemsImpl::CanUseAesDecryptor(const std::string& key_system) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto* key_system_info = GetKeySystemInfo(key_system);
  if (!key_system_info) {
    DLOG(ERROR) << key_system << " is not a known supported key system";
    return false;
  }

  return key_system_info->UseAesDecryptor();
}

EmeConfig::Rule KeySystemsImpl::GetContentTypeConfigRule(
    const std::string& key_system,
    EmeMediaType media_type,
    const std::string& container_mime_type,
    const std::vector<std::string>& codecs) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Make sure the container MIME type matches |media_type|.
  switch (media_type) {
    case EmeMediaType::AUDIO:
      if (!base::StartsWith(container_mime_type, "audio/",
                            base::CompareCase::SENSITIVE)) {
        return EmeConfig::UnsupportedRule();
      }
      break;
    case EmeMediaType::VIDEO:
      if (!base::StartsWith(container_mime_type, "video/",
                            base::CompareCase::SENSITIVE)) {
        return EmeConfig::UnsupportedRule();
      }
      break;
  }

  // Double check whether the key system is supported.
  const auto* key_system_info = GetKeySystemInfo(key_system);
  if (!key_system_info) {
    NOTREACHED_IN_MIGRATION() << "Key system support should have been checked";
    return EmeConfig::UnsupportedRule();
  }

  // Look up the key system's supported codecs and secure codecs.
  SupportedCodecs key_system_codec_mask = key_system_info->GetSupportedCodecs();
  SupportedCodecs key_system_hw_secure_codec_mask =
      key_system_info->GetSupportedHwSecureCodecs();

  // Check that the container is supported by the key system. (This check is
  // necessary because |codecs| may be empty.)
  SupportedCodecs mime_type_codec_mask =
      GetCodecMaskForMimeType(container_mime_type);
  if ((key_system_codec_mask & mime_type_codec_mask) == 0) {
    DVLOG(2) << "Container " << container_mime_type << " not supported by "
             << key_system;
    return EmeConfig::UnsupportedRule();
  }

  // Check that the codecs are supported by the key system and container based
  // on the following rule:
  // SupportedCodecs  | SupportedSecureCodecs  | Result
  //       yes        |         yes            | SUPPORTED
  //       yes        |         no             | HW_SECURE_CODECS_NOT_ALLOWED
  //       no         |         yes            | HW_SECURE_CODECS_REQUIRED
  //       no         |         no             | NOT_SUPPORTED
  auto to_support = EmeConfig::SupportedRule();
  for (auto& codec_iterator : codecs) {
    EmeCodec codec =
        GetEmeCodecForString(media_type, container_mime_type, codec_iterator);
    if (codec == EME_CODEC_NONE) {
      DVLOG(2) << "Unsupported codec string \"" << codec_iterator << "\"";
      return EmeConfig::UnsupportedRule();
    }

    // Currently all EmeCodecs only have one bit set. In case there could be
    // codecs with multiple bits set, e.g. to cover multiple profiles, we check
    // (codec & mask) == codec instead of (codec & mask) != 0 to make sure all
    // bits are set. Same below.
    if ((codec & key_system_codec_mask & mime_type_codec_mask) != codec &&
        (codec & key_system_hw_secure_codec_mask & mime_type_codec_mask) !=
            codec) {
      DVLOG(2) << "Container/codec pair (" << container_mime_type << " / "
               << codec_iterator << ") not supported by " << key_system;
      return EmeConfig::UnsupportedRule();
    }

    // Check whether the codec supports a hardware-secure mode (any level).
    if ((codec & key_system_hw_secure_codec_mask) != codec) {
      DCHECK_EQ(codec & key_system_codec_mask, codec);
      if (to_support->hw_secure_codecs == EmeConfigRuleState::kRequired) {
        return EmeConfig::UnsupportedRule();
      }

      to_support->hw_secure_codecs = EmeConfigRuleState::kNotAllowed;
    }

    // Check whether the codec requires a hardware-secure mode (any level).
    if ((codec & key_system_codec_mask) != codec) {
      DCHECK_EQ(codec & key_system_hw_secure_codec_mask, codec);
      if (to_support->hw_secure_codecs == EmeConfigRuleState::kNotAllowed) {
        return EmeConfig::UnsupportedRule();
      }

      to_support->hw_secure_codecs = EmeConfigRuleState::kRequired;
    }
  }

  return to_support;
}

EmeConfig::Rule KeySystemsImpl::GetRobustnessConfigRule(
    const std::string& key_system,
    EmeMediaType media_type,
    const std::string& requested_robustness,
    const bool* hw_secure_requirement) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto* key_system_info = GetKeySystemInfo(key_system);
  CHECK(key_system_info);

  return key_system_info->GetRobustnessConfigRule(
      key_system, media_type, requested_robustness, hw_secure_requirement);
}

EmeConfig::Rule KeySystemsImpl::GetPersistentLicenseSessionSupport(
    const std::string& key_system) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto* key_system_info = GetKeySystemInfo(key_system);
  CHECK(key_system_info);

  return key_system_info->GetPersistentLicenseSessionSupport();
}

EmeFeatureSupport KeySystemsImpl::GetPersistentStateSupport(
    const std::string& key_system) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto* key_system_info = GetKeySystemInfo(key_system);
  CHECK(key_system_info);

  return key_system_info->GetPersistentStateSupport();
}

EmeFeatureSupport KeySystemsImpl::GetDistinctiveIdentifierSupport(
    const std::string& key_system) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto* key_system_info = GetKeySystemInfo(key_system);
  CHECK(key_system_info);

  return key_system_info->GetDistinctiveIdentifierSupport();
}

}  // namespace media
