// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/media/key_system_config_selector.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/cdm_config.h"
#include "media/base/eme_constants.h"
#include "media/base/key_system_names.h"
#include "media/base/key_systems.h"
#include "media/base/logging_override_if_enabled.h"
#include "media/base/media_permission.h"
#include "media/base/media_switches.h"
#include "media/base/mime_util.h"
#include "media/base/video_codec_string_parsers.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/media/web_media_player_util.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace blink {
namespace {

using ::media::EmeConfig;
using ::media::EmeConfigRuleState;
using ::media::EmeFeatureSupport;
using ::media::EmeMediaType;
using ::media::EncryptionScheme;
using EmeFeatureRequirement = WebMediaKeySystemConfiguration::Requirement;
using EmeEncryptionScheme = WebMediaKeySystemMediaCapability::EncryptionScheme;

EmeConfig::Rule GetDistinctiveIdentifierConfigRule(
    EmeFeatureSupport support,
    EmeFeatureRequirement requirement) {
  if (support == EmeFeatureSupport::INVALID) {
    NOTREACHED_IN_MIGRATION();
    return EmeConfig::UnsupportedRule();
  }

  // For kNotAllowed and kRequired, the result is as expected. For kRecommended,
  // we return the most restrictive rule that is not more restrictive than for
  // kNotAllowed or kRequired. Those values will be checked individually when
  // the option is resolved.
  //
  //                  |---------------Requirement-------------------
  //   Support        | kNotAllowed   | kRecommended  | kRequired
  //    NOT_SUPPORTED | I_NOT_ALLOWED | I_NOT_ALLOWED | NOT_SUPPORTED
  //      REQUESTABLE | I_NOT_ALLOWED | SUPPORTED     | I_REQUIRED
  //   ALWAYS_ENABLED | NOT_SUPPORTED | I_REQUIRED    | I_REQUIRED
  DCHECK(support == EmeFeatureSupport::NOT_SUPPORTED ||
         support == EmeFeatureSupport::REQUESTABLE ||
         support == EmeFeatureSupport::ALWAYS_ENABLED);
  DCHECK(requirement == EmeFeatureRequirement::kNotAllowed ||
         requirement == EmeFeatureRequirement::kOptional ||
         requirement == EmeFeatureRequirement::kRequired);
  if ((support == EmeFeatureSupport::NOT_SUPPORTED &&
       requirement == EmeFeatureRequirement::kRequired) ||
      (support == EmeFeatureSupport::ALWAYS_ENABLED &&
       requirement == EmeFeatureRequirement::kNotAllowed)) {
    return EmeConfig::UnsupportedRule();
  }
  if (support == EmeFeatureSupport::REQUESTABLE &&
      requirement == EmeFeatureRequirement::kOptional) {
    return EmeConfig::SupportedRule();
  }
  if (support == EmeFeatureSupport::NOT_SUPPORTED ||
      requirement == EmeFeatureRequirement::kNotAllowed) {
    return EmeConfig{.identifier = EmeConfigRuleState::kNotAllowed};
  }
  return EmeConfig{.identifier = EmeConfigRuleState::kRequired};
}

EmeConfig::Rule GetPersistentStateConfigRule(
    EmeFeatureSupport support,
    EmeFeatureRequirement requirement) {
  if (support == EmeFeatureSupport::INVALID) {
    NOTREACHED_IN_MIGRATION();
    return EmeConfig::UnsupportedRule();
  }

  // For kNotAllowed and kRequired, the result is as expected. For kRecommended,
  // we return the most restrictive rule that is not more restrictive than for
  // kNotAllowed or kRequired. Those values will be checked individually when
  // the option is resolved.
  //
  // Note that even though a distinctive identifier can not be required for
  // persistent state, it may still be required for persistent sessions.
  //
  //                  |---------------Requirement-------------------
  //   Support        | kNotAllowed   | kRecommended      | kRequired
  //    NOT_SUPPORTED | P_NOT_ALLOWED | P_NOT_ALLOWED | NOT_SUPPORTED
  //      REQUESTABLE | P_NOT_ALLOWED | SUPPORTED     | P_REQUIRED
  //   ALWAYS_ENABLED | NOT_SUPPORTED | P_REQUIRED    | P_REQUIRED
  DCHECK(support == EmeFeatureSupport::NOT_SUPPORTED ||
         support == EmeFeatureSupport::REQUESTABLE ||
         support == EmeFeatureSupport::ALWAYS_ENABLED);
  DCHECK(requirement == EmeFeatureRequirement::kNotAllowed ||
         requirement == EmeFeatureRequirement::kOptional ||
         requirement == EmeFeatureRequirement::kRequired);
  if ((support == EmeFeatureSupport::NOT_SUPPORTED &&
       requirement == EmeFeatureRequirement::kRequired) ||
      (support == EmeFeatureSupport::ALWAYS_ENABLED &&
       requirement == EmeFeatureRequirement::kNotAllowed)) {
    return EmeConfig::UnsupportedRule();
  }
  if (support == EmeFeatureSupport::REQUESTABLE &&
      requirement == EmeFeatureRequirement::kOptional) {
    return EmeConfig::SupportedRule();
  }
  if (support == EmeFeatureSupport::NOT_SUPPORTED ||
      requirement == EmeFeatureRequirement::kNotAllowed) {
    return EmeConfig{.persistence = EmeConfigRuleState::kNotAllowed};
  }
  return EmeConfig{.persistence = EmeConfigRuleState::kRequired};
}

bool IsPersistentSessionType(WebEncryptedMediaSessionType sessionType) {
  switch (sessionType) {
    case WebEncryptedMediaSessionType::kTemporary:
      return false;
    case WebEncryptedMediaSessionType::kPersistentLicense:
      return true;
    case WebEncryptedMediaSessionType::kUnknown:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool IsSupportedMediaType(const std::string& container_mime_type,
                          const std::string& codecs,
                          bool use_aes_decryptor) {
  DVLOG(3) << __func__ << ": container_mime_type=" << container_mime_type
           << ", codecs=" << codecs
           << ", use_aes_decryptor=" << use_aes_decryptor;

  std::vector<std::string> codec_vector;
  media::SplitCodecs(codecs, &codec_vector);

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
  // When build flag ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION and feature
  // kPlatformEncryptedDolbyVision are both enabled, encrypted Dolby Vision is
  // allowed when supported by the platform, but it is not supported for clear
  // playback or when using ClearKey. Remove the DV codec strings to avoid
  // asking IsSupported*MediaFormat() about DV. EME support for DV is described
  // via KeySystemInfo::GetSupportedCodecs().
  // TODO(crbug.com/1156282): Decouple the rest of clear vs EME codec support.
  if (base::FeatureList::IsEnabled(media::kPlatformEncryptedDolbyVision) &&
      !use_aes_decryptor &&
      base::ToLowerASCII(container_mime_type) == "video/mp4" &&
      !codec_vector.empty()) {
    std::vector<std::string> filtered_codec_vector;
    for (const auto& codec : codec_vector) {
      if (!media::ParseDolbyVisionCodecId(codec)) {
        filtered_codec_vector.push_back(codec);
      }
    }
    codec_vector = std::move(filtered_codec_vector);

    // Avoid calling IsSupported*MediaFormat() with an empty vector. For
    // "video/mp4", this will return MaybeSupported, which we would otherwise
    // consider "false" below.
    if (codec_vector.empty())
      return true;
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)

  // AesDecryptor decrypts the stream in the demuxer before it reaches the
  // decoder so check whether the media format is supported when clear.
  media::SupportsType support_result =
      use_aes_decryptor
          ? media::IsSupportedMediaFormat(container_mime_type, codec_vector)
          : media::IsSupportedEncryptedMediaFormat(container_mime_type,
                                                   codec_vector);
  return (support_result == media::SupportsType::kSupported);
}

}  // namespace

bool KeySystemConfigSelector::WebLocalFrameDelegate::
    IsCrossOriginToOutermostMainFrame() {
  DCHECK(web_frame_);
  return web_frame_->IsCrossOriginToOutermostMainFrame();
}

bool KeySystemConfigSelector::WebLocalFrameDelegate::AllowStorageAccessSync(
    WebContentSettingsClient::StorageType storage_type) {
  DCHECK(web_frame_);
  return web_frame_->AllowStorageAccessSyncAndNotify(storage_type);
}

struct KeySystemConfigSelector::SelectionRequest {
  std::string key_system;
  WebVector<WebMediaKeySystemConfiguration> candidate_configurations;
  SelectConfigCB cb;
  bool was_permission_requested = false;
  bool is_permission_granted = false;
  bool was_hardware_secure_decryption_preferences_requested = false;
  bool is_hardware_secure_decryption_allowed = true;
};

// Accumulates configuration rules to determine if a feature (additional
// configuration rule) can be added to an accumulated configuration.
class KeySystemConfigSelector::ConfigState {
 public:
  ConfigState(bool was_permission_requested,
              bool is_permission_granted,
              bool was_hardware_secure_decryption_preferences_requested,
              bool is_hardware_secure_decryption_allowed)
      : was_permission_requested_(was_permission_requested),
        is_permission_granted_(is_permission_granted),
        was_hardware_secure_decryption_preferences_requested_(
            was_hardware_secure_decryption_preferences_requested),
        is_hardware_secure_decryption_allowed_(
            is_hardware_secure_decryption_allowed) {}

  bool IsPermissionGranted() const { return is_permission_granted_; }

  // Permission is possible if it has not been denied.
  bool IsPermissionPossible() const {
    return is_permission_granted_ || !was_permission_requested_;
  }

  bool IsIdentifierRequired() const {
    return rules.identifier == EmeConfigRuleState::kRequired;
  }

  bool IsIdentifierRecommended() const {
    return rules.identifier == EmeConfigRuleState::kRecommended;
  }

  bool AreHwSecureCodecsRequired() const {
    return rules.hw_secure_codecs == EmeConfigRuleState::kRequired;
  }

  bool AreHwSecureCodesNotAllowed() const {
    return rules.hw_secure_codecs == EmeConfigRuleState::kNotAllowed;
  }

  bool IsHardwareSecureDecryptionAllowed() const {
    return was_hardware_secure_decryption_preferences_requested_ &&
           is_hardware_secure_decryption_allowed_;
  }

  // Checks whether a rule is compatible with all previously added rules.
  bool IsRuleSupported(EmeConfig::Rule rule) const {
    bool result = true;

    // NOT_SUPPORTED
    if (!rule.has_value()) {
      return false;
    }

    // SUPPORTED
    if (rule->hw_secure_codecs == EmeConfigRuleState::kUnset &&
        rule->persistence == EmeConfigRuleState::kUnset &&
        rule->identifier == EmeConfigRuleState::kUnset) {
      return true;
    }

    // For identifier, if the rule we are evaluating is kNotAllowed,
    // as long as our rules does not have a rule in place already
    // that says identifier = kRequired, then we can proceed
    // to evaluating the other rules.

    // If the rule we are evaluating is kRequired, then we have to
    // evaluate whether our rules does not have a rule in place already
    //  that says identifier = kNotAllowed and we have to make sure
    //  permission is possible. Then we can proceed to evaluating the
    //  other rules.
    if (rule->identifier == EmeConfigRuleState::kNotAllowed) {
      result = result && rules.identifier != EmeConfigRuleState::kRequired;
    } else if (rule->identifier == EmeConfigRuleState::kRequired) {
      result = result && rules.identifier != EmeConfigRuleState::kNotAllowed &&
               IsPermissionPossible();
    }

    // For persistence, if the rule we are evaluating is kNotAllowed,
    // as long as our rules does not have a rule in place already
    // that says persistence = kRequired, then we can proceed
    // to evaluating the other rules.

    /// If the rule we are evaluating is kRequired, then we have to
    // evaluate whether our rules does not have a rule in place already
    //  that says persistence = kNotAllowed. Then we can proceed to
    //  evaluating the other rules.
    if (rule->persistence == EmeConfigRuleState::kNotAllowed) {
      result = result && rules.persistence != EmeConfigRuleState::kRequired;
    } else if (rule->persistence == EmeConfigRuleState::kRequired) {
      result = result && rules.persistence != EmeConfigRuleState::kNotAllowed;
    }

    // For hw_secure_codecs, if the rule we are evaluating is kNotAllowed,
    // as long as our rules does not have a rule in place already
    // that says hw_secure_codecs = kRequired, then we can proceed
    // to evaluating the other rules.

    /// If the rule we are evaluating is kRequired, then we have to
    // evaluate whether our rules does not have a rule in place already
    //  that says hw_secure_codecs = kNotAllowed. Then we can proceed to
    //  evaluating the other rules.
    if (rule->hw_secure_codecs == EmeConfigRuleState::kNotAllowed) {
      result =
          result && rules.hw_secure_codecs != EmeConfigRuleState::kRequired;
    } else if (rule->hw_secure_codecs == EmeConfigRuleState::kRequired) {
      result =
          result && rules.hw_secure_codecs != EmeConfigRuleState::kNotAllowed;
    }
    return result;
  }

  // Add a rule to the accumulated configuration state.
  void AddRule(EmeConfig::Rule rule) {
    DCHECK(IsRuleSupported(rule));

    // No rule specified, this should not happen
    if (!rule.has_value()) {
      NOTREACHED_IN_MIGRATION();
      return;
    }

    // Rule does not require or prohibit anything, so can be skipped.
    if (rule->hw_secure_codecs == EmeConfigRuleState::kUnset &&
        rule->persistence == EmeConfigRuleState::kUnset &&
        rule->identifier == EmeConfigRuleState::kUnset) {
      return;
    }

    // In the three statements below, we first check if the rule is
    // not specified. Then, as long as the rule we are adding to our rules
    // does not override a kNotAllowed or kRequired, or if the
    // collection of rules does not have anything associated, we should
    // change the value to the incoming rule. Else, we ignore.
    if (rule->identifier != EmeConfigRuleState::kUnset) {
      if (rule->identifier != EmeConfigRuleState::kRecommended ||
          rules.identifier == EmeConfigRuleState::kUnset) {
        rules.identifier = rule->identifier;
      }
    }
    if (rule->persistence != EmeConfigRuleState::kUnset) {
      if (rule->persistence != EmeConfigRuleState::kRecommended ||
          rules.persistence == EmeConfigRuleState::kUnset) {
        rules.persistence = rule->persistence;
      }
    }
    if (rule->hw_secure_codecs != EmeConfigRuleState::kUnset) {
      if (rule->hw_secure_codecs != EmeConfigRuleState::kRecommended &&
          rules.hw_secure_codecs == EmeConfigRuleState::kUnset) {
        rules.hw_secure_codecs = rule->hw_secure_codecs;
      }
    }
    return;
  }

 private:
  // Whether permission to use a distinctive identifier was requested. If set,
  // |is_permission_granted_| represents the final decision.
  // (Not changed by adding rules.)
  bool was_permission_requested_;

  // Whether permission to use a distinctive identifier has been granted.
  // (Not changed by adding rules.)
  bool is_permission_granted_;

  // Whether hardware secure decryption preferences was requested. If set,
  // |is_hardware_secure_decryption_preferences_allowed_| represents the final
  // decision. (Not changed by adding rules.)
  bool was_hardware_secure_decryption_preferences_requested_ = false;

  // Whether hardware secure decryption is allowed. (Not changed by
  // adding rules.)
  bool is_hardware_secure_decryption_allowed_ = true;

  EmeConfig rules = EmeConfig();
};

KeySystemConfigSelector::KeySystemConfigSelector(
    media::KeySystems* key_systems,
    media::MediaPermission* media_permission,
    std::unique_ptr<WebLocalFrameDelegate> web_frame_delegate)
    : key_systems_(key_systems),
      media_permission_(media_permission),
      web_frame_delegate_(std::move(web_frame_delegate)),
      is_supported_media_type_cb_(base::BindRepeating(&IsSupportedMediaType)) {
  DCHECK(key_systems_);
  DCHECK(media_permission_);
  DCHECK(web_frame_delegate_);
}

KeySystemConfigSelector::~KeySystemConfigSelector() = default;

// TODO(sandersd): Move contentType parsing from Blink to here so that invalid
// parameters can be rejected. http://crbug.com/449690, http://crbug.com/690131
bool KeySystemConfigSelector::IsSupportedContentType(
    const std::string& key_system,
    EmeMediaType media_type,
    const std::string& container_mime_type,
    const std::string& codecs,
    KeySystemConfigSelector::ConfigState* config_state) {
  DVLOG(3) << __func__ << ": key_system = " << key_system
           << ", container_mime_type = " << container_mime_type
           << ", codecs = " << codecs;

  // From RFC6838: "Both top-level type and subtype names are case-insensitive."
  std::string container_lower = base::ToLowerASCII(container_mime_type);

  // contentTypes must provide a codec string unless the container implies a
  // particular codec. For EME, none of the currently supported containers
  // imply a codec, so |codecs| must be provided.
  if (codecs.empty()) {
    DVLOG(3) << "KeySystemConfig for " << container_mime_type
             << " does not specify necessary codecs.";
    return false;
  }

  // Check that |container_mime_type| and |codecs| are supported by Chrome. This
  // is done primarily to validate extended codecs, but it also ensures that the
  // CDM cannot support codecs that Chrome does not (which could complicate the
  // robustness algorithm).
  if (!is_supported_media_type_cb_.Run(
          container_lower, codecs,
          key_systems_->CanUseAesDecryptor(key_system))) {
    DVLOG(3) << "Container mime type and codecs are not supported";
    return false;
  }

  // Before checking CDM support, split |codecs| into a vector of codecs.
  std::vector<std::string> codec_vector;
  media::SplitCodecs(codecs, &codec_vector);

  // Check that |container_lower| and |codec_vector| are supported by the CDM.
  EmeConfig::Rule codecs_rule = key_systems_->GetContentTypeConfigRule(
      key_system, media_type, container_lower, codec_vector);
  if (!config_state->IsRuleSupported(codecs_rule)) {
    DVLOG(3) << "Container mime type and codecs are not supported by CDM";
    return false;
  }
  config_state->AddRule(codecs_rule);

  return true;
}

EmeConfig::Rule KeySystemConfigSelector::GetEncryptionSchemeConfigRule(
    const std::string& key_system,
    const EmeEncryptionScheme encryption_scheme) {
  switch (encryption_scheme) {
    // https://w3c.github.io/encrypted-media/#dom-mediakeysystemmediacapability-encryptionscheme
    // "A value which is null or not present indicates to the user agent that
    // no specific encryption scheme is required by the application, and
    // therefore any encryption scheme is acceptable."
    // To fully implement this, we need to get the config rules for both kCenc
    // and kCbcs, which could be conflicting, and choose a final config rule
    // somehow. If we end up choosing the rule for kCbcs, we could actually
    // break legacy players which serves kCenc streams. Therefore, for backward
    // compatibility and simplicity, we treat kNotSpecified the same as kCenc.
    case EmeEncryptionScheme::kNotSpecified:
    case EmeEncryptionScheme::kCenc:
      return key_systems_->GetEncryptionSchemeConfigRule(
          key_system, EncryptionScheme::kCenc);
    case EmeEncryptionScheme::kCbcs:
    case EmeEncryptionScheme::kCbcs_1_9:
      return key_systems_->GetEncryptionSchemeConfigRule(
          key_system, EncryptionScheme::kCbcs);
    case EmeEncryptionScheme::kUnrecognized:
      // https://w3c.github.io/encrypted-media/#get-supported-capabilities-for-audio-video-type
      // "If encryption scheme is non-null and is not recognized or not
      // supported by implementation, continue to the next iteration."
      // The value provided was an empty string or some other value that is
      // not recognized, so treat it as a scheme that is not supported.
      return EmeConfig::UnsupportedRule();
  }

  NOTREACHED_IN_MIGRATION();
  return EmeConfig::UnsupportedRule();
}

bool KeySystemConfigSelector::GetSupportedCapabilities(
    const std::string& key_system,
    EmeMediaType media_type,
    const WebVector<WebMediaKeySystemMediaCapability>&
        requested_media_capabilities,
    // Corresponds to the partial configuration, plus restrictions.
    KeySystemConfigSelector::ConfigState* config_state,
    std::vector<WebMediaKeySystemMediaCapability>*
        supported_media_capabilities) {
  // From "3.1.1.3 Get Supported Capabilities for Audio/Video Type".
  // https://w3c.github.io/encrypted-media/#get-supported-capabilities-for-audio-video-type
  // 1. Let local accumulated capabilities be a local copy of partial
  //    configuration.
  //    (Skipped as we directly update |config_state|. This is safe because we
  //    only do so when at least one requested media capability is supported.)
  // 2. Let supported media capabilities be an empty sequence of
  //    MediaKeySystemMediaCapability dictionaries.
  DCHECK_EQ(supported_media_capabilities->size(), 0ul);
  // 3. For each requested media capability in requested media capabilities:
  for (size_t i = 0; i < requested_media_capabilities.size(); i++) {
    // 3.1. Let content type be requested media capability's contentType member.
    // 3.2. Let robustness be requested media capability's robustness member.
    const WebMediaKeySystemMediaCapability& capability =
        requested_media_capabilities[i];
    // 3.3. If contentType is the empty string, return null.
    if (capability.mime_type.IsEmpty()) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "a capability contentType was empty.";
      return false;
    }

    // Corresponds to the local accumulated configuration, plus restrictions.
    ConfigState proposed_config_state = *config_state;

    // 3.4-3.11. (Implemented by IsSupportedContentType().)
    if (!capability.mime_type.ContainsOnlyASCII() ||
        !capability.codecs.ContainsOnlyASCII() ||
        !IsSupportedContentType(
            key_system, media_type, capability.mime_type.Ascii(),
            capability.codecs.Ascii(), &proposed_config_state)) {
      DVLOG(3) << "The current capability is not supported.";
      continue;
    }

    // 3.12. If robustness is not the empty string and contains an unrecognized
    //       value or a value not supported by implementation, continue to the
    //       next iteration. String comparison is case-sensitive.
    // Note: If the robustness is empty, we still try to get the config rule
    //       from |key_systems_| for the empty robustness.
    std::string requested_robustness_ascii;
    if (!capability.robustness.IsEmpty()) {
      if (!capability.robustness.ContainsOnlyASCII())
        continue;
      requested_robustness_ascii = capability.robustness.Ascii();
    }
    // Both of these should not be true.
    DCHECK(!(proposed_config_state.AreHwSecureCodecsRequired() &&
             proposed_config_state.AreHwSecureCodesNotAllowed()));
    bool hw_secure_requirement;
    bool* hw_secure_requirement_ptr = &hw_secure_requirement;
    if (proposed_config_state.AreHwSecureCodecsRequired())
      hw_secure_requirement = true;
    else if (proposed_config_state.AreHwSecureCodesNotAllowed())
      hw_secure_requirement = false;
    else
      hw_secure_requirement_ptr = nullptr;
    EmeConfig::Rule robustness_rule = key_systems_->GetRobustnessConfigRule(
        key_system, media_type, requested_robustness_ascii,
        hw_secure_requirement_ptr);

    // 3.13. If the user agent and implementation definitely support playback of
    //       encrypted media data for the combination of container, media types,
    //       robustness and local accumulated configuration in combination with
    //       restrictions:
    if (!proposed_config_state.IsRuleSupported(robustness_rule)) {
      DVLOG(3) << "The current robustness rule is not supported.";
      continue;
    }
    proposed_config_state.AddRule(robustness_rule);

    // Check for encryption scheme support.
    // https://github.com/WICG/encrypted-media-encryption-scheme/blob/master/explainer.md.
    EmeConfig::Rule encryption_scheme_rule =
        GetEncryptionSchemeConfigRule(key_system, capability.encryption_scheme);
    if (!proposed_config_state.IsRuleSupported(encryption_scheme_rule)) {
      DVLOG(3) << "The current encryption scheme rule is not supported.";
      continue;
    }

    // 3.13.1. Add requested media capability to supported media capabilities.
    supported_media_capabilities->push_back(capability);

    // 3.13.2. Add requested media capability to the {audio|video}Capabilities
    // member of local accumulated configuration.
    proposed_config_state.AddRule(encryption_scheme_rule);

    // This is used as an intermediate variable so that |proposed_config_state|
    // is updated in the next iteration of the for loop.
    //
    // Since |config_state| is also the output parameter, this also updates the
    // "partial configuration" as specified in
    // "Get Supported Configuration and Consent"
    // https://w3c.github.io/encrypted-media/#get-supported-configuration-and-consent
    // Step 16.3 and 17.3: Set the {video|audio}Capabilities member of
    // accumulated configuration to {video|audio} capabilities.
    //
    // TODO(xhwang): Refactor this to be more consistent with the spec steps.
    *config_state = proposed_config_state;
  }

  // 4. If supported media capabilities is empty, return null.
  if (supported_media_capabilities->empty()) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "no capabilities were supported.";
    return false;
  }
  // 5. Return media type capabilities.
  // Note: |supported_media_capabilities| has already been populated.
  return true;
}

KeySystemConfigSelector::ConfigurationSupport
KeySystemConfigSelector::GetSupportedConfiguration(
    const std::string& key_system,
    const WebMediaKeySystemConfiguration& candidate,
    ConfigState* config_state,
    WebMediaKeySystemConfiguration* accumulated_configuration) {
  DVLOG(3) << __func__;

  // From
  // http://w3c.github.io/encrypted-media/#get-supported-configuration-and-consent
  // 1. Let accumulated configuration be a new MediaKeySystemConfiguration
  //    dictionary. (Done by caller.)
  // 2. Set the label member of accumulated configuration to equal the label
  //    member of candidate configuration.
  accumulated_configuration->label = candidate.label;

  // 3. If the initDataTypes member of candidate configuration is non-empty,
  //    run the following steps:
  if (!candidate.init_data_types.empty()) {
    // 3.1. Let supported types be an empty sequence of DOMStrings.
    std::vector<media::EmeInitDataType> supported_types;

    // 3.2. For each value in candidate configuration's initDataTypes member:
    for (size_t i = 0; i < candidate.init_data_types.size(); i++) {
      // 3.2.1. Let initDataType be the value.
      auto init_data_type = candidate.init_data_types[i];

      // 3.2.2. If the implementation supports generating requests based on
      //        initDataType, add initDataType to supported types. String
      //        comparison is case-sensitive. The empty string is never
      //        supported.
      if (key_systems_->IsSupportedInitDataType(key_system, init_data_type)) {
        supported_types.push_back(init_data_type);
      }
    }

    // 3.3. If supported types is empty, return null.
    if (supported_types.empty()) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "no initDataType values were supported.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 3.4. Set the initDataTypes member of accumulated configuration to
    //      supported types.
    accumulated_configuration->init_data_types = supported_types;
  }

  // 4. Let distinctive identifier requirement be the value of candidate
  //    configuration's distinctiveIdentifier member.
  EmeFeatureRequirement distinctive_identifier =
      candidate.distinctive_identifier;

  // 5. If distinctive identifier requirement is "optional" and Distinctive
  //    Identifiers are not allowed according to restrictions, set distinctive
  //    identifier requirement to "not-allowed".

  EmeConfig::Rule identifier_required =
      EmeConfig{.identifier = EmeConfigRuleState::kRequired};
  if (distinctive_identifier == EmeFeatureRequirement::kOptional &&
      !config_state->IsRuleSupported(identifier_required)) {
    distinctive_identifier = EmeFeatureRequirement::kNotAllowed;
  }

  // 6. Follow the steps for distinctive identifier requirement from the
  //    following list:
  //      - "required": If the implementation does not support use of
  //         Distinctive Identifier(s) in combination with accumulated
  //         configuration and restrictions, return NotSupported.
  //      - "optional": Continue with the following steps.
  //      - "not-allowed": If the implementation requires use Distinctive
  //        Identifier(s) or Distinctive Permanent Identifier(s) in
  //        combination with accumulated configuration and restrictions,
  //        return NotSupported.
  // We also reject OPTIONAL when distinctive identifiers are ALWAYS_ENABLED and
  // permission has already been denied. This would happen anyway later.
  EmeFeatureSupport distinctive_identifier_support =
      key_systems_->GetDistinctiveIdentifierSupport(key_system);
#if !BUILDFLAG(IS_ANDROID)
  // NOTE: This is an additional action we are taking here that is not in the
  // spec currently.  Specifically, we are not allowing a distinctive identifier
  // for cross-origin frames. We do not do this on Android because there is no
  // CDM selection available to Chrome that doesn't require a distinct
  // identifier.
  if (web_frame_delegate_->IsCrossOriginToOutermostMainFrame()) {
    if (distinctive_identifier_support == EmeFeatureSupport::ALWAYS_ENABLED)
      return CONFIGURATION_NOT_SUPPORTED;
    distinctive_identifier_support = EmeFeatureSupport::NOT_SUPPORTED;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  EmeConfig::Rule di_rule = GetDistinctiveIdentifierConfigRule(
      distinctive_identifier_support, distinctive_identifier);
  if (!config_state->IsRuleSupported(di_rule)) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "the distinctiveIdentifier requirement was not supported.";
    return CONFIGURATION_NOT_SUPPORTED;
  }
  config_state->AddRule(di_rule);

  // 7. Set the distinctiveIdentifier member of accumulated configuration to
  //    equal distinctive identifier requirement.
  accumulated_configuration->distinctive_identifier = distinctive_identifier;

  // 8. Let persistent state requirement be equal to the value of candidate
  //    configuration's persistentState member.
  EmeFeatureRequirement persistent_state = candidate.persistent_state;

  // 9. If persistent state requirement is "optional" and persisting state is
  //    not allowed according to restrictions, set persistent state requirement
  //    to "not-allowed".
  EmeConfig::Rule persistence_required =
      EmeConfig{.persistence = EmeConfigRuleState::kRequired};
  if (persistent_state == EmeFeatureRequirement::kOptional &&
      !config_state->IsRuleSupported(persistence_required)) {
    persistent_state = EmeFeatureRequirement::kNotAllowed;
  }

  // 10. Follow the steps for persistent state requirement from the following
  //     list:
  //       - "required": If the implementation does not support persisting
  //         state in combination with accumulated configuration and
  //         restrictions, return NotSupported.
  //       - "optional": Continue with the following steps.
  //       - "not-allowed": If the implementation requires persisting state in
  //         combination with accumulated configuration and restrictions,
  //         return NotSupported.
  EmeFeatureSupport persistent_state_support =
      key_systems_->GetPersistentStateSupport(key_system);
  // If preferences disallow storage access, then indicate persistent state is
  // not supported. A quota managed storage type is used in lieu of a dedicated
  // StorageType, as Media Licenses are a quota managed managed type.
  // TODO(crbug.com/1465299): Simplify the WebContentSettingsClient::StorageType
  // to remove unnecessary distinctions between storage types.
  if (!web_frame_delegate_->AllowStorageAccessSync(
          WebContentSettingsClient::StorageType::kIndexedDB)) {
    if (persistent_state_support == EmeFeatureSupport::ALWAYS_ENABLED)
      return CONFIGURATION_NOT_SUPPORTED;
    persistent_state_support = EmeFeatureSupport::NOT_SUPPORTED;
  }
  EmeConfig::Rule ps_rule =
      GetPersistentStateConfigRule(persistent_state_support, persistent_state);
  if (!config_state->IsRuleSupported(ps_rule)) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "the persistentState requirement was not supported.";
    return CONFIGURATION_NOT_SUPPORTED;
  }
  config_state->AddRule(ps_rule);

  // 11. Set the persistentState member of accumulated configuration to equal
  //     the value of persistent state requirement.
  accumulated_configuration->persistent_state = persistent_state;

  // 12. Follow the steps for the first matching condition from the following
  //     list:
  //       - If the sessionTypes member is present in candidate configuration,
  //         let session types be candidate configuration's sessionTypes member.
  //       - Otherwise, let session types be [ "temporary" ].
  //         (Done in MediaKeySystemAccessInitializer.)
  WebVector<WebEncryptedMediaSessionType> session_types =
      candidate.session_types;

  // 13. For each value in session types:
  for (size_t i = 0; i < session_types.size(); i++) {
    // 13.1. Let session type be the value.
    WebEncryptedMediaSessionType session_type = session_types[i];
    if (session_type == WebEncryptedMediaSessionType::kUnknown) {
      DVLOG(2) << "Rejecting requested configuration because the session type "
                  "was not recognized.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 13.2. If accumulated configuration's persistentState value is
    //       "not-allowed" and the Is persistent session type? algorithm
    //       returns true for session type return NotSupported.
    if (accumulated_configuration->persistent_state ==
            EmeFeatureRequirement::kNotAllowed &&
        IsPersistentSessionType(session_type)) {
      DVLOG(2) << "Rejecting requested configuration because persistent state "
                  "is not allowed.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 13.3. If the implementation does not support session type in combination
    //       with accumulated configuration and restrictions for other reasons,
    //       return NotSupported.
    EmeConfig::Rule session_type_rule = EmeConfig::UnsupportedRule();
    switch (session_type) {
      case WebEncryptedMediaSessionType::kUnknown:
        NOTREACHED_IN_MIGRATION();
        return CONFIGURATION_NOT_SUPPORTED;
      case WebEncryptedMediaSessionType::kTemporary:
        session_type_rule = EmeConfig::SupportedRule();
        break;
      case WebEncryptedMediaSessionType::kPersistentLicense:
        session_type_rule =
            key_systems_->GetPersistentLicenseSessionSupport(key_system);
        break;
    }

    if (!config_state->IsRuleSupported(session_type_rule)) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "a required session type was not supported.";
      return CONFIGURATION_NOT_SUPPORTED;
    }
    config_state->AddRule(session_type_rule);

    // 13.4. If accumulated configuration's persistentState value is "optional"
    //       and the result of running the Is persistent session type?
    //       algorithm on session type is true, change accumulated
    //       configuration's persistentState value to "required".
    if (accumulated_configuration->persistent_state ==
            EmeFeatureRequirement::kOptional &&
        IsPersistentSessionType(session_type)) {
      accumulated_configuration->persistent_state =
          EmeFeatureRequirement::kRequired;
    }
  }

  // 14. Set the sessionTypes member of accumulated configuration to
  //     session types.
  accumulated_configuration->session_types = session_types;

  // 15. If the videoCapabilities and audioCapabilities members in candidate
  //     configuration are both empty, return NotSupported.
  if (candidate.video_capabilities.empty() &&
      candidate.audio_capabilities.empty()) {
    DVLOG(2) << "Rejecting requested configuration because "
             << "neither audioCapabilities nor videoCapabilities is specified";
    return CONFIGURATION_NOT_SUPPORTED;
  }

  // 16. If the videoCapabilities member in candidate configuration is
  //     non-empty:
  std::vector<WebMediaKeySystemMediaCapability> video_capabilities;
  if (!candidate.video_capabilities.empty()) {
    // 16.1. Let video capabilities be the result of executing the Get
    //       Supported Capabilities for Audio/Video Type algorithm on Video,
    //       candidate configuration's videoCapabilities member, accumulated
    //       configuration, and restrictions.
    // 16.2. If video capabilities is null, return NotSupported.
    if (!GetSupportedCapabilities(key_system, EmeMediaType::VIDEO,
                                  candidate.video_capabilities, config_state,
                                  &video_capabilities)) {
      DVLOG(2) << "Rejecting requested configuration because the specified "
                  "videoCapabilities are not supported.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 16.3. Set the videoCapabilities member of accumulated configuration
    //       to video capabilities.
    accumulated_configuration->video_capabilities = video_capabilities;
  } else {
    // Otherwise set the videoCapabilities member of accumulated configuration
    // to an empty sequence.
    accumulated_configuration->video_capabilities = video_capabilities;
  }

  // 17. If the audioCapabilities member in candidate configuration is
  //     non-empty:
  std::vector<WebMediaKeySystemMediaCapability> audio_capabilities;
  if (!candidate.audio_capabilities.empty()) {
    // 17.1. Let audio capabilities be the result of executing the Get
    //       Supported Capabilities for Audio/Video Type algorithm on Audio,
    //       candidate configuration's audioCapabilities member, accumulated
    //       configuration, and restrictions.
    // 17.2. If audio capabilities is null, return NotSupported.
    if (!GetSupportedCapabilities(key_system, EmeMediaType::AUDIO,
                                  candidate.audio_capabilities, config_state,
                                  &audio_capabilities)) {
      DVLOG(2) << "Rejecting requested configuration because the specified "
                  "audioCapabilities are not supported.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 17.3. Set the audioCapabilities member of accumulated configuration
    //       to audio capabilities.
    accumulated_configuration->audio_capabilities = audio_capabilities;
  } else {
    // Otherwise set the audioCapabilities member of accumulated configuration
    // to an empty sequence.
    accumulated_configuration->audio_capabilities = audio_capabilities;
  }

  // 18. If accumulated configuration's distinctiveIdentifier value is
  //     "optional", follow the steps for the first matching condition
  //      from the following list:
  //       - If the implementation requires use Distinctive Identifier(s) or
  //         Distinctive Permanent Identifier(s) for any of the combinations
  //         in accumulated configuration, change accumulated configuration's
  //         distinctiveIdentifier value to "required".
  //       - Otherwise, change accumulated configuration's
  //         distinctiveIdentifier value to "not-allowed".
  if (accumulated_configuration->distinctive_identifier ==
      EmeFeatureRequirement::kOptional) {
    EmeConfig::Rule not_allowed_rule = GetDistinctiveIdentifierConfigRule(
        key_systems_->GetDistinctiveIdentifierSupport(key_system),
        EmeFeatureRequirement::kNotAllowed);
    EmeConfig::Rule required_rule = GetDistinctiveIdentifierConfigRule(
        key_systems_->GetDistinctiveIdentifierSupport(key_system),
        EmeFeatureRequirement::kRequired);
    bool not_allowed_supported =
        config_state->IsRuleSupported(not_allowed_rule);
    bool required_supported = config_state->IsRuleSupported(required_rule);
    // If a distinctive identifier is recommend and that is a possible outcome,
    // prefer that.
    if (required_supported && config_state->IsIdentifierRecommended() &&
        config_state->IsPermissionPossible()) {
      not_allowed_supported = false;
    }
    if (not_allowed_supported) {
      accumulated_configuration->distinctive_identifier =
          EmeFeatureRequirement::kNotAllowed;
      config_state->AddRule(not_allowed_rule);
    } else if (required_supported) {
      accumulated_configuration->distinctive_identifier =
          EmeFeatureRequirement::kRequired;
      config_state->AddRule(required_rule);
    } else {
      // We should not have passed step 6.
      NOTREACHED_IN_MIGRATION();
      return CONFIGURATION_NOT_SUPPORTED;
    }
  }

  // 19. If accumulated configuration's persistentState value is "optional",
  //     follow the steps for the first matching condition from the following
  //     list:
  //       - If the implementation requires persisting state for any of the
  //         combinations in accumulated configuration, change accumulated
  //         configuration's persistentState value to "required".
  //       - Otherwise, change accumulated configuration's persistentState
  //         value to "not-allowed".
  if (accumulated_configuration->persistent_state ==
      EmeFeatureRequirement::kOptional) {
    EmeConfig::Rule not_allowed_rule = GetPersistentStateConfigRule(
        key_systems_->GetPersistentStateSupport(key_system),
        EmeFeatureRequirement::kNotAllowed);
    EmeConfig::Rule required_rule = GetPersistentStateConfigRule(
        key_systems_->GetPersistentStateSupport(key_system),
        EmeFeatureRequirement::kRequired);
    // |persistent_state| should not be affected after it is decided.
    DCHECK(!not_allowed_rule.has_value() ||
           not_allowed_rule->persistence == EmeConfigRuleState::kNotAllowed);
    DCHECK(!required_rule.has_value() ||
           required_rule->persistence == EmeConfigRuleState::kRequired);
    bool not_allowed_supported =
        config_state->IsRuleSupported(not_allowed_rule);
    bool required_supported = config_state->IsRuleSupported(required_rule);
    if (not_allowed_supported) {
      accumulated_configuration->persistent_state =
          EmeFeatureRequirement::kNotAllowed;
      config_state->AddRule(not_allowed_rule);
    } else if (required_supported) {
      accumulated_configuration->persistent_state =
          EmeFeatureRequirement::kRequired;
      config_state->AddRule(required_rule);
    } else {
      // We should not have passed step 5.
      NOTREACHED_IN_MIGRATION();
      return CONFIGURATION_NOT_SUPPORTED;
    }
  }

  // 20. If implementation in the configuration specified by the combination of
  //     the values in accumulated configuration is not supported or not allowed
  //     in the origin, return NotSupported.
  // 21. If accumulated configuration's distinctiveIdentifier value is
  //     "required" and the Distinctive Identifier(s) associated with
  //     accumulated configuration are not unique per origin and profile
  //     and clearable:
  // 21.1. Update restrictions to reflect that all configurations described
  //       by accumulated configuration do not have user consent.
  // 21.2. Return ConsentDenied and restrictions.
  // (Not required as data is unique per origin and clearable.)

  // 22. Let consent status and updated restrictions be the result of running
  //     the Get Consent Status algorithm on accumulated configuration,
  //     restrictions and origin and follow the steps for the value of consent
  //     status from the following list:
  //       - "ConsentDenied": Return ConsentDenied and updated restrictions.
  //       - "InformUser": Inform the user that accumulated configuration is
  //         in use in the origin including, specifically, the information
  //         that Distinctive Identifier(s) and/or Distinctive Permanent
  //         Identifier(s) as appropriate will be used if the
  //         distinctiveIdentifier member of accumulated configuration is
  //         "required". Continue to the next step.
  //       - "Allowed": Continue to the next step.
  // Accumulated configuration's distinctiveIdentifier should be "required" or
  // "notallowed"" due to step 18. If it is "required", prompt the user for
  // consent unless it has already been granted.
  if (accumulated_configuration->distinctive_identifier ==
      EmeFeatureRequirement::kRequired) {
    // The caller is responsible for resolving what to do if permission is
    // required but has been denied (it should treat it as NOT_SUPPORTED).
    if (!config_state->IsPermissionGranted())
      return CONFIGURATION_REQUIRES_PERMISSION;
  }

  // 23. Return accumulated configuration.
  return CONFIGURATION_SUPPORTED;
}

void KeySystemConfigSelector::SelectConfig(
    const WebString& key_system,
    const WebVector<WebMediaKeySystemConfiguration>& candidate_configurations,
    SelectConfigCB cb) {
  // Continued from requestMediaKeySystemAccess(), step 6, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 6.1 If keySystem is not one of the Key Systems supported by the user
  //     agent, reject promise with a NotSupportedError. String comparison
  //     is case-sensitive.
  if (!key_system.ContainsOnlyASCII()) {
    DVLOG(1) << "Rejecting requested configuration because "
             << "key system contains unsupported characters.";
    std::move(cb).Run(Status::kUnsupportedKeySystem, nullptr, nullptr);
    return;
  }

  std::string key_system_ascii = key_system.Ascii();
  if (!key_systems_->IsSupportedKeySystem(key_system_ascii)) {
    DVLOG(1) << "Rejecting requested configuration because "
             << "key system " << key_system_ascii << " is not supported.";
    std::move(cb).Run(Status::kUnsupportedKeySystem, nullptr, nullptr);
    return;
  }

  const bool is_encrypted_media_enabled =
      media_permission_->IsEncryptedMediaEnabled();

  // Only report this UMA at most once per renderer process.
  static bool has_reported_encrypted_media_enabled_uma = false;
  if (!has_reported_encrypted_media_enabled_uma) {
    has_reported_encrypted_media_enabled_uma = true;
    UMA_HISTOGRAM_BOOLEAN("Media.EME.EncryptedMediaEnabled",
                          is_encrypted_media_enabled);
  }

  // According to Section 9 "Common Key Systems": All user agents MUST support
  // the common key systems described in this section.
  //   9.1 Clear Key
  //
  // Therefore, always support Clear Key key system and only check settings for
  // other key systems.
  if (!is_encrypted_media_enabled && !media::IsClearKey(key_system_ascii)) {
    std::move(cb).Run(Status::kUnsupportedKeySystem, nullptr, nullptr);
    return;
  }

  // 6.2-6.4. Implemented by OnSelectConfig().
  // TODO(sandersd): This should be async, ideally not on the main thread.
  auto request = std::make_unique<SelectionRequest>();
  request->key_system = key_system_ascii;
  request->candidate_configurations = candidate_configurations;
  request->cb = std::move(cb);

  SelectConfigInternal(std::move(request));
}

void KeySystemConfigSelector::SelectConfigInternal(
    std::unique_ptr<SelectionRequest> request) {
  DVLOG(3) << __func__;

  // Continued from requestMediaKeySystemAccess(), step 6, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 6.2. Let implementation be the implementation of keySystem.
  //      (|key_systems_| fills this role.)
  // 6.3. For each value in supportedConfigurations:
  for (size_t i = 0; i < request->candidate_configurations.size(); i++) {
    // 6.3.1. Let candidate configuration be the value.
    // 6.3.2. Let supported configuration be the result of executing the Get
    //        Supported Configuration algorithm on implementation, candidate
    //        configuration, and origin.
    // 6.3.3. If supported configuration is not NotSupported, [initialize
    //        and return a new MediaKeySystemAccess object.]
    ConfigState config_state(
        request->was_permission_requested, request->is_permission_granted,
        request->was_hardware_secure_decryption_preferences_requested,
        request->is_hardware_secure_decryption_allowed);
    WebMediaKeySystemConfiguration accumulated_configuration;
    media::CdmConfig cdm_config;
    ConfigurationSupport support = GetSupportedConfiguration(
        request->key_system, request->candidate_configurations[i],
        &config_state, &accumulated_configuration);
    switch (support) {
      case CONFIGURATION_NOT_SUPPORTED:
        continue;
      case CONFIGURATION_REQUIRES_PERMISSION:
        if (request->was_permission_requested) {
          DVLOG(2) << "Rejecting requested configuration because "
                   << "permission was denied.";
          continue;
        }
        DVLOG(3) << "Request permission.";
        media_permission_->RequestPermission(
            media::MediaPermission::Type::kProtectedMediaIdentifier,
            base::BindOnce(&KeySystemConfigSelector::OnPermissionResult,
                           weak_factory_.GetWeakPtr(), std::move(request)));
        return;
      case CONFIGURATION_SUPPORTED:
        std::string key_system = request->key_system;
        if (key_systems_->ShouldUseBaseKeySystemName(key_system)) {
          key_system = key_systems_->GetBaseKeySystemName(key_system);
        }
        cdm_config.key_system = key_system;

        cdm_config.allow_distinctive_identifier =
            (accumulated_configuration.distinctive_identifier ==
             EmeFeatureRequirement::kRequired);
        cdm_config.allow_persistent_state =
            (accumulated_configuration.persistent_state ==
             EmeFeatureRequirement::kRequired);
        cdm_config.use_hw_secure_codecs =
            config_state.AreHwSecureCodecsRequired();
#if BUILDFLAG(IS_WIN)
        // Check whether hardware secure decryption CDM should be disabled.
        if (cdm_config.use_hw_secure_codecs &&
            base::FeatureList::IsEnabled(
                media::kHardwareSecureDecryptionFallback) &&
            media::kHardwareSecureDecryptionFallbackPerSite.Get()) {
          if (!request->was_hardware_secure_decryption_preferences_requested) {
            media_permission_->IsHardwareSecureDecryptionAllowed(
                base::BindOnce(&KeySystemConfigSelector::
                                   OnHardwareSecureDecryptionAllowedResult,
                               weak_factory_.GetWeakPtr(), std::move(request)));
            return;
          }

          if (!config_state.IsHardwareSecureDecryptionAllowed()) {
            DVLOG(2) << "Rejecting requested configuration because "
                     << "Hardware secure decryption is not allowed.";
            continue;
          }
        }
#endif  // BUILDFLAG(IS_WIN)

        std::move(request->cb)
            .Run(Status::kSupported, &accumulated_configuration, &cdm_config);
        return;
    }
  }

  // 6.4. Reject promise with a NotSupportedError.
  std::move(request->cb).Run(Status::kUnsupportedConfigs, nullptr, nullptr);
}

void KeySystemConfigSelector::OnPermissionResult(
    std::unique_ptr<SelectionRequest> request,
    bool is_permission_granted) {
  DVLOG(3) << __func__;

  request->was_permission_requested = true;
  request->is_permission_granted = is_permission_granted;
  SelectConfigInternal(std::move(request));
}

#if BUILDFLAG(IS_WIN)
void KeySystemConfigSelector::OnHardwareSecureDecryptionAllowedResult(
    std::unique_ptr<SelectionRequest> request,
    bool is_hardware_secure_decryption_allowed) {
  DVLOG(3) << __func__;

  request->was_hardware_secure_decryption_preferences_requested = true;
  request->is_hardware_secure_decryption_allowed =
      is_hardware_secure_decryption_allowed;
  SelectConfigInternal(std::move(request));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace blink
