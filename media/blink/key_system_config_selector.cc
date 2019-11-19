// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/key_system_config_selector.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/cdm_config.h"
#include "media/base/key_system_names.h"
#include "media/base/key_systems.h"
#include "media/base/logging_override_if_enabled.h"
#include "media/base/media_permission.h"
#include "media/base/mime_util.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/media/webmediaplayer_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

using EmeFeatureRequirement =
    blink::WebMediaKeySystemConfiguration::Requirement;
using EmeEncryptionScheme =
    blink::WebMediaKeySystemMediaCapability::EncryptionScheme;

namespace {

EmeConfigRule GetSessionTypeConfigRule(EmeSessionTypeSupport support) {
  switch (support) {
    case EmeSessionTypeSupport::INVALID:
      NOTREACHED();
      return EmeConfigRule::NOT_SUPPORTED;
    case EmeSessionTypeSupport::NOT_SUPPORTED:
      return EmeConfigRule::NOT_SUPPORTED;
    case EmeSessionTypeSupport::SUPPORTED_WITH_IDENTIFIER:
      return EmeConfigRule::IDENTIFIER_AND_PERSISTENCE_REQUIRED;
    case EmeSessionTypeSupport::SUPPORTED:
      return EmeConfigRule::PERSISTENCE_REQUIRED;
  }
  NOTREACHED();
  return EmeConfigRule::NOT_SUPPORTED;
}

EmeConfigRule GetDistinctiveIdentifierConfigRule(
    EmeFeatureSupport support,
    EmeFeatureRequirement requirement) {
  if (support == EmeFeatureSupport::INVALID) {
    NOTREACHED();
    return EmeConfigRule::NOT_SUPPORTED;
  }

  // For NOT_ALLOWED and REQUIRED, the result is as expected. For OPTIONAL, we
  // return the most restrictive rule that is not more restrictive than for
  // NOT_ALLOWED or REQUIRED. Those values will be checked individually when
  // the option is resolved.
  //
  //                   NOT_ALLOWED    OPTIONAL       REQUIRED
  //    NOT_SUPPORTED  I_NOT_ALLOWED  I_NOT_ALLOWED  NOT_SUPPORTED
  //      REQUESTABLE  I_NOT_ALLOWED  SUPPORTED      I_REQUIRED
  //   ALWAYS_ENABLED  NOT_SUPPORTED  I_REQUIRED     I_REQUIRED
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
    return EmeConfigRule::NOT_SUPPORTED;
  }
  if (support == EmeFeatureSupport::REQUESTABLE &&
      requirement == EmeFeatureRequirement::kOptional) {
    return EmeConfigRule::SUPPORTED;
  }
  if (support == EmeFeatureSupport::NOT_SUPPORTED ||
      requirement == EmeFeatureRequirement::kNotAllowed) {
    return EmeConfigRule::IDENTIFIER_NOT_ALLOWED;
  }
  return EmeConfigRule::IDENTIFIER_REQUIRED;
}

EmeConfigRule GetPersistentStateConfigRule(EmeFeatureSupport support,
                                           EmeFeatureRequirement requirement) {
  if (support == EmeFeatureSupport::INVALID) {
    NOTREACHED();
    return EmeConfigRule::NOT_SUPPORTED;
  }

  // For NOT_ALLOWED and REQUIRED, the result is as expected. For OPTIONAL, we
  // return the most restrictive rule that is not more restrictive than for
  // NOT_ALLOWED or REQUIRED. Those values will be checked individually when
  // the option is resolved.
  //
  // Note that even though a distinctive identifier can not be required for
  // persistent state, it may still be required for persistent sessions.
  //
  //                   NOT_ALLOWED    OPTIONAL       REQUIRED
  //    NOT_SUPPORTED  P_NOT_ALLOWED  P_NOT_ALLOWED  NOT_SUPPORTED
  //      REQUESTABLE  P_NOT_ALLOWED  SUPPORTED      P_REQUIRED
  //   ALWAYS_ENABLED  NOT_SUPPORTED  P_REQUIRED     P_REQUIRED
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
    return EmeConfigRule::NOT_SUPPORTED;
  }
  if (support == EmeFeatureSupport::REQUESTABLE &&
      requirement == EmeFeatureRequirement::kOptional) {
    return EmeConfigRule::SUPPORTED;
  }
  if (support == EmeFeatureSupport::NOT_SUPPORTED ||
      requirement == EmeFeatureRequirement::kNotAllowed) {
    return EmeConfigRule::PERSISTENCE_NOT_ALLOWED;
  }
  return EmeConfigRule::PERSISTENCE_REQUIRED;
}

bool IsPersistentSessionType(blink::WebEncryptedMediaSessionType sessionType) {
  switch (sessionType) {
    case blink::WebEncryptedMediaSessionType::kTemporary:
      return false;
    case blink::WebEncryptedMediaSessionType::kPersistentLicense:
      return true;
    case blink::WebEncryptedMediaSessionType::kPersistentUsageRecord:
      return true;
    case blink::WebEncryptedMediaSessionType::kUnknown:
      break;
  }

  NOTREACHED();
  return false;
}

bool IsSupportedMediaType(const std::string& container_mime_type,
                          const std::string& codecs,
                          bool use_aes_decryptor) {
  DVLOG(3) << __func__ << ": container_mime_type=" << container_mime_type
           << ", codecs=" << codecs
           << ", use_aes_decryptor=" << use_aes_decryptor;

  std::vector<std::string> codec_vector;
  SplitCodecs(codecs, &codec_vector);

  // AesDecryptor decrypts the stream in the demuxer before it reaches the
  // decoder so check whether the media format is supported when clear.
  SupportsType support_result =
      use_aes_decryptor
          ? IsSupportedMediaFormat(container_mime_type, codec_vector)
          : IsSupportedEncryptedMediaFormat(container_mime_type, codec_vector);
  return (support_result == IsSupported);
}

}  // namespace

struct KeySystemConfigSelector::SelectionRequest {
  std::string key_system;
  blink::WebVector<blink::WebMediaKeySystemConfiguration>
      candidate_configurations;
  base::Callback<void(const blink::WebMediaKeySystemConfiguration&,
                      const CdmConfig&)> succeeded_cb;
  base::Closure not_supported_cb;
  bool was_permission_requested = false;
  bool is_permission_granted = false;
};

// Accumulates configuration rules to determine if a feature (additional
// configuration rule) can be added to an accumulated configuration.
class KeySystemConfigSelector::ConfigState {
 public:
  ConfigState(bool was_permission_requested, bool is_permission_granted)
      : was_permission_requested_(was_permission_requested),
        is_permission_granted_(is_permission_granted) {}

  bool IsPermissionGranted() const { return is_permission_granted_; }

  // Permission is possible if it has not been denied.
  bool IsPermissionPossible() const {
    return is_permission_granted_ || !was_permission_requested_;
  }

  bool IsIdentifierRequired() const { return is_identifier_required_; }

  bool IsIdentifierRecommended() const { return is_identifier_recommended_; }

  bool AreHwSecureCodecsRequired() const {
    return are_hw_secure_codecs_required_;
  }

  // Checks whether a rule is compatible with all previously added rules.
  bool IsRuleSupported(EmeConfigRule rule) const {
    switch (rule) {
      case EmeConfigRule::NOT_SUPPORTED:
        return false;
      case EmeConfigRule::IDENTIFIER_NOT_ALLOWED:
        return !is_identifier_required_;
      case EmeConfigRule::IDENTIFIER_REQUIRED:
        // TODO(sandersd): Confirm if we should be refusing these rules when
        // permission has been denied (as the spec currently says).
        return !is_identifier_not_allowed_ && IsPermissionPossible();
      case EmeConfigRule::IDENTIFIER_RECOMMENDED:
        return true;
      case EmeConfigRule::PERSISTENCE_NOT_ALLOWED:
        return !is_persistence_required_;
      case EmeConfigRule::PERSISTENCE_REQUIRED:
        return !is_persistence_not_allowed_;
      case EmeConfigRule::IDENTIFIER_AND_PERSISTENCE_REQUIRED:
        return (!is_identifier_not_allowed_ && IsPermissionPossible() &&
                !is_persistence_not_allowed_);
      case EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED:
        return !are_hw_secure_codecs_required_;
      case EmeConfigRule::HW_SECURE_CODECS_REQUIRED:
        return !are_hw_secure_codecs_not_allowed_;
      case EmeConfigRule::SUPPORTED:
        return true;
    }
    NOTREACHED();
    return false;
  }

  // Add a rule to the accumulated configuration state.
  void AddRule(EmeConfigRule rule) {
    DCHECK(IsRuleSupported(rule));
    switch (rule) {
      case EmeConfigRule::NOT_SUPPORTED:
        NOTREACHED();
        return;
      case EmeConfigRule::IDENTIFIER_NOT_ALLOWED:
        is_identifier_not_allowed_ = true;
        return;
      case EmeConfigRule::IDENTIFIER_REQUIRED:
        is_identifier_required_ = true;
        return;
      case EmeConfigRule::IDENTIFIER_RECOMMENDED:
        is_identifier_recommended_ = true;
        return;
      case EmeConfigRule::PERSISTENCE_NOT_ALLOWED:
        is_persistence_not_allowed_ = true;
        return;
      case EmeConfigRule::PERSISTENCE_REQUIRED:
        is_persistence_required_ = true;
        return;
      case EmeConfigRule::IDENTIFIER_AND_PERSISTENCE_REQUIRED:
        is_identifier_required_ = true;
        is_persistence_required_ = true;
        return;
      case EmeConfigRule::HW_SECURE_CODECS_NOT_ALLOWED:
        are_hw_secure_codecs_not_allowed_ = true;
        return;
      case EmeConfigRule::HW_SECURE_CODECS_REQUIRED:
        are_hw_secure_codecs_required_ = true;
        return;
      case EmeConfigRule::SUPPORTED:
        return;
    }
    NOTREACHED();
  }

 private:
  // Whether permission to use a distinctive identifier was requested. If set,
  // |is_permission_granted_| represents the final decision.
  // (Not changed by adding rules.)
  bool was_permission_requested_;

  // Whether permission to use a distinctive identifier has been granted.
  // (Not changed by adding rules.)
  bool is_permission_granted_;

  // Whether a rule has been added that requires or blocks a distinctive
  // identifier.
  bool is_identifier_required_ = false;
  bool is_identifier_not_allowed_ = false;

  // Whether a rule has been added that recommends a distinctive identifier.
  bool is_identifier_recommended_ = false;

  // Whether a rule has been added that requires or blocks persistent state.
  bool is_persistence_required_ = false;
  bool is_persistence_not_allowed_ = false;

  // Whether a rule has been added that requires or blocks hardware-secure
  // codecs.
  bool are_hw_secure_codecs_required_ = false;
  bool are_hw_secure_codecs_not_allowed_ = false;
};

KeySystemConfigSelector::KeySystemConfigSelector(
    const KeySystems* key_systems,
    MediaPermission* media_permission)
    : key_systems_(key_systems),
      media_permission_(media_permission),
      is_supported_media_type_cb_(base::BindRepeating(&IsSupportedMediaType)) {
  DCHECK(key_systems_);
  DCHECK(media_permission_);
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
  SplitCodecs(codecs, &codec_vector);

  // Check that |container_lower| and |codec_vector| are supported by the CDM.
  EmeConfigRule codecs_rule = key_systems_->GetContentTypeConfigRule(
      key_system, media_type, container_lower, codec_vector);
  if (!config_state->IsRuleSupported(codecs_rule)) {
    DVLOG(3) << "Container mime type and codecs are not supported by CDM";
    return false;
  }
  config_state->AddRule(codecs_rule);

  return true;
}

EmeConfigRule KeySystemConfigSelector::GetEncryptionSchemeConfigRule(
    const std::string& key_system,
    const EmeEncryptionScheme encryption_scheme) {
  switch (encryption_scheme) {
    // https://github.com/WICG/encrypted-media-encryption-scheme/blob/master/explainer.md
    // "A missing or null value indicates that any encryption scheme is
    // acceptable."
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
      return key_systems_->GetEncryptionSchemeConfigRule(
          key_system, EncryptionScheme::kCbcs);
  }

  NOTREACHED();
  return EmeConfigRule::NOT_SUPPORTED;
}

bool KeySystemConfigSelector::GetSupportedCapabilities(
    const std::string& key_system,
    EmeMediaType media_type,
    const blink::WebVector<blink::WebMediaKeySystemMediaCapability>&
        requested_media_capabilities,
    // Corresponds to the partial configuration, plus restrictions.
    KeySystemConfigSelector::ConfigState* config_state,
    std::vector<blink::WebMediaKeySystemMediaCapability>*
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
    const blink::WebMediaKeySystemMediaCapability& capability =
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
    EmeConfigRule robustness_rule = key_systems_->GetRobustnessConfigRule(
        key_system, media_type, requested_robustness_ascii);

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
    EmeConfigRule encryption_scheme_rule =
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
    // "3.1.1.2. Get Supported Configuration and Consent"
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
    const blink::WebMediaKeySystemConfiguration& candidate,
    ConfigState* config_state,
    blink::WebMediaKeySystemConfiguration* accumulated_configuration) {
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
    std::vector<EmeInitDataType> supported_types;

    // 3.2. For each value in candidate configuration's initDataTypes member:
    for (size_t i = 0; i < candidate.init_data_types.size(); i++) {
      // 3.2.1. Let initDataType be the value.
      EmeInitDataType init_data_type = candidate.init_data_types[i];

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
  EmeFeatureSupport distinctive_identifier_support =
      key_systems_->GetDistinctiveIdentifierSupport(key_system);
  if (distinctive_identifier == EmeFeatureRequirement::kOptional) {
    if (distinctive_identifier_support == EmeFeatureSupport::INVALID ||
        distinctive_identifier_support == EmeFeatureSupport::NOT_SUPPORTED) {
      distinctive_identifier = EmeFeatureRequirement::kNotAllowed;
    }
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
  EmeConfigRule di_rule = GetDistinctiveIdentifierConfigRule(
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
  EmeFeatureSupport persistent_state_support =
      key_systems_->GetPersistentStateSupport(key_system);
  if (persistent_state == EmeFeatureRequirement::kOptional) {
    if (persistent_state_support == EmeFeatureSupport::INVALID ||
        persistent_state_support == EmeFeatureSupport::NOT_SUPPORTED) {
      persistent_state = EmeFeatureRequirement::kNotAllowed;
    }
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
  EmeConfigRule ps_rule =
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
  blink::WebVector<blink::WebEncryptedMediaSessionType> session_types =
      candidate.session_types;

  // 13. For each value in session types:
  for (size_t i = 0; i < session_types.size(); i++) {
    // 13.1. Let session type be the value.
    blink::WebEncryptedMediaSessionType session_type = session_types[i];
    if (session_type == blink::WebEncryptedMediaSessionType::kUnknown) {
      DVLOG(2) << "Rejecting requested configuration because "
               << "session type was not recognized.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 13.2. If accumulated configuration's persistentState value is
    //       "not-allowed" and the Is persistent session type? algorithm
    //       returns true for session type return NotSupported.
    if (accumulated_configuration->persistent_state ==
            EmeFeatureRequirement::kNotAllowed &&
        IsPersistentSessionType(session_type)) {
      DVLOG(2) << "Rejecting requested configuration because persistent "
                  "sessions are not allowed.";
      return CONFIGURATION_NOT_SUPPORTED;
    }

    // 13.3. If the implementation does not support session type in combination
    //       with accumulated configuration and restrictions for other reasons,
    //       return NotSupported.
    EmeConfigRule session_type_rule = EmeConfigRule::NOT_SUPPORTED;
    switch (session_type) {
      case blink::WebEncryptedMediaSessionType::kUnknown:
        NOTREACHED();
        return CONFIGURATION_NOT_SUPPORTED;
      case blink::WebEncryptedMediaSessionType::kTemporary:
        session_type_rule = EmeConfigRule::SUPPORTED;
        break;
      case blink::WebEncryptedMediaSessionType::kPersistentLicense:
        session_type_rule = GetSessionTypeConfigRule(
            key_systems_->GetPersistentLicenseSessionSupport(key_system));
        break;
      case blink::WebEncryptedMediaSessionType::kPersistentUsageRecord:
        session_type_rule = GetSessionTypeConfigRule(
            key_systems_->GetPersistentUsageRecordSessionSupport(key_system));
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
  std::vector<blink::WebMediaKeySystemMediaCapability> video_capabilities;
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
  std::vector<blink::WebMediaKeySystemMediaCapability> audio_capabilities;
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
    EmeConfigRule not_allowed_rule = GetDistinctiveIdentifierConfigRule(
        key_systems_->GetDistinctiveIdentifierSupport(key_system),
        EmeFeatureRequirement::kNotAllowed);
    EmeConfigRule required_rule = GetDistinctiveIdentifierConfigRule(
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
      NOTREACHED();
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
    EmeConfigRule not_allowed_rule = GetPersistentStateConfigRule(
        key_systems_->GetPersistentStateSupport(key_system),
        EmeFeatureRequirement::kNotAllowed);
    EmeConfigRule required_rule = GetPersistentStateConfigRule(
        key_systems_->GetPersistentStateSupport(key_system),
        EmeFeatureRequirement::kRequired);
    // |distinctiveIdentifier| should not be affected after it is decided.
    DCHECK(not_allowed_rule == EmeConfigRule::NOT_SUPPORTED ||
           not_allowed_rule == EmeConfigRule::PERSISTENCE_NOT_ALLOWED);
    DCHECK(required_rule == EmeConfigRule::NOT_SUPPORTED ||
           required_rule == EmeConfigRule::PERSISTENCE_REQUIRED);
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
      NOTREACHED();
      return CONFIGURATION_NOT_SUPPORTED;
    }
  }

  // 20. If implementation in the configuration specified by the combination of
  //     the values in accumulated configuration is not supported or not allowed
  //     in the origin, return NotSupported.
  // TODO(jrummell): can we check that the CDM can't be loaded by the origin?

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
    const blink::WebString& key_system,
    const blink::WebVector<blink::WebMediaKeySystemConfiguration>&
        candidate_configurations,
    base::Callback<void(const blink::WebMediaKeySystemConfiguration&,
                        const CdmConfig&)> succeeded_cb,
    base::Closure not_supported_cb) {
  // Continued from requestMediaKeySystemAccess(), step 6, from
  // https://w3c.github.io/encrypted-media/#requestmediakeysystemaccess
  //
  // 6.1 If keySystem is not one of the Key Systems supported by the user
  //     agent, reject promise with a NotSupportedError. String comparison
  //     is case-sensitive.
  if (!key_system.ContainsOnlyASCII()) {
    not_supported_cb.Run();
    return;
  }

  std::string key_system_ascii = key_system.Ascii();
  if (!key_systems_->IsSupportedKeySystem(key_system_ascii)) {
    not_supported_cb.Run();
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
  if (!is_encrypted_media_enabled && !IsClearKey(key_system_ascii)) {
    not_supported_cb.Run();
    return;
  }

  // 6.2-6.4. Implemented by OnSelectConfig().
  // TODO(sandersd): This should be async, ideally not on the main thread.
  std::unique_ptr<SelectionRequest> request(new SelectionRequest());
  request->key_system = key_system_ascii;
  request->candidate_configurations = candidate_configurations;
  request->succeeded_cb = succeeded_cb;
  request->not_supported_cb = not_supported_cb;
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
    ConfigState config_state(request->was_permission_requested,
                             request->is_permission_granted);
    blink::WebMediaKeySystemConfiguration accumulated_configuration;
    CdmConfig cdm_config;
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
            MediaPermission::PROTECTED_MEDIA_IDENTIFIER,
            base::Bind(&KeySystemConfigSelector::OnPermissionResult,
                       weak_factory_.GetWeakPtr(), base::Passed(&request)));
        return;
      case CONFIGURATION_SUPPORTED:
        cdm_config.allow_distinctive_identifier =
            (accumulated_configuration.distinctive_identifier ==
             EmeFeatureRequirement::kRequired);
        cdm_config.allow_persistent_state =
            (accumulated_configuration.persistent_state ==
             EmeFeatureRequirement::kRequired);
        cdm_config.use_hw_secure_codecs =
            config_state.AreHwSecureCodecsRequired();
        request->succeeded_cb.Run(accumulated_configuration, cdm_config);
        return;
    }
  }

  // 6.4. Reject promise with a NotSupportedError.
  request->not_supported_cb.Run();
}

void KeySystemConfigSelector::OnPermissionResult(
    std::unique_ptr<SelectionRequest> request,
    bool is_permission_granted) {
  DVLOG(3) << __func__;

  request->was_permission_requested = true;
  request->is_permission_granted = is_permission_granted;
  SelectConfigInternal(std::move(request));
}

}  // namespace media
