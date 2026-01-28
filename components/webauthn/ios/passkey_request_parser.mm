// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_request_parser.h"

#import "base/base64url.h"
#import "device/fido/fido_user_verification_requirement.h"
#import "device/fido/public/fido_constants.h"

namespace webauthn {

namespace {

// Message event.
constexpr char kEvent[] = "event";

// Message event types.
constexpr char kHandleGetRequest[] = "handleGetRequest";
constexpr char kHandleCreateRequest[] = "handleCreateRequest";
constexpr char kLogGetRequest[] = "logGetRequest";
constexpr char kLogCreateRequest[] = "logCreateRequest";
constexpr char kLogGetResolved[] = "logGetResolved";
constexpr char kLogCreateResolved[] = "logCreateResolved";

// Parameters for logging events.
constexpr char kCredentialId[] = "credentialId";
constexpr char kRpId[] = "rpId";
constexpr char kIsGpm[] = "isGpm";

// Request ID associated with deferred promises.
constexpr char kRequestId[] = "requestId";

// Frame ID for handle* events.
constexpr char kFrameId[] = "frameId";

// Common parameters of "handleGetRequest" and "handleCreateRequest" events.
constexpr char kRequest[] = "request";
constexpr char kRpEntity[] = "rpEntity";

// Parameters exclusive to the "handleCreateRequest" event.
constexpr char kUserEntity[] = "userEntity";
constexpr char kExcludeCredentials[] = "excludeCredentials";

// Parameter exclusive to the "handleGetRequest" event.
constexpr char kAllowCredentials[] = "allowCredentials";

// Members of the "request" dictionary.
constexpr char kChallenge[] = "challenge";
constexpr char kUserVerification[] = "userVerification";
constexpr char kIsConditional[] = "isConditional";

// Common members of the "rpEntity" and "userEntity" dictionaries.
constexpr char kId[] = "id";
constexpr char kName[] = "name";

// Member exclusive to the "userEntity" dictionary.
constexpr char kDisplayName[] = "displayName";

// Member of the credential descriptors array.
constexpr char kType[] = "type";
constexpr char kTransports[] = "transports";

// JSON formatted extension input data.
constexpr char kExtensions[] = "extensions";

// Returns the string if valid, otherwise returns the error.
base::expected<const std::string, PasskeysParsingError> ValidateString(
    const std::string* str,
    PasskeysParsingError missing_code,
    PasskeysParsingError empty_code) {
  if (!str) {
    return base::unexpected(missing_code);
  }
  if (str->empty()) {
    return base::unexpected(empty_code);
  }
  return *str;
}

// Encodes a byte vector to base 64 URL encoded string.
std::string Base64UrlEncode(base::span<const uint8_t> input) {
  std::string output;
  // Omit padding, according to the spec. See:
  // https://w3c.github.io/webauthn/#base64url-encoding
  base::Base64UrlEncode(input, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &output);
  return output;
}

// Decodes a base 64 URL encoded string into a data vector.
// Returns std::nullopt on failure.
std::optional<std::vector<uint8_t>> Base64UrlDecode(
    const std::string& base_64_url_string) {
  std::vector<uint8_t> decoded_data;
  if (base_64_url_string.empty()) {
    return decoded_data;
  }

  std::string decoded_string;
  if (!base::Base64UrlDecode(base_64_url_string,
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &decoded_string)) {
    return std::nullopt;
  }

  decoded_data.assign(decoded_string.begin(), decoded_string.end());
  return decoded_data;
}

// Returns the decoded data if valid, otherwise returns the error.
base::expected<std::vector<uint8_t>, PasskeysParsingError>
ValidateBase64URLString(const std::string* str,
                        PasskeysParsingError missing_code,
                        PasskeysParsingError empty_code,
                        PasskeysParsingError malformed_code) {
  auto validated_str = ValidateString(str, missing_code, empty_code);
  if (!validated_str.has_value()) {
    return base::unexpected(validated_str.error());
  }

  std::optional<std::vector<uint8_t>> decoded_data =
      Base64UrlDecode(*validated_str);
  if (!decoded_data.has_value()) {
    return base::unexpected(malformed_code);
  }

  return *decoded_data;
}

// Builds a PublicKeyCredentialUserEntity object from the parameters contained
// in the provided dictionary.
base::expected<device::PublicKeyCredentialUserEntity, PasskeysParsingError>
BuildUserEntity(const base::DictValue& dict) {
  auto decoded_id = ValidateBase64URLString(
      dict.FindString(kId), PasskeysParsingError::kMissingUserId,
      PasskeysParsingError::kEmptyUserId,
      PasskeysParsingError::kMalformedUserId);
  if (!decoded_id.has_value()) {
    return base::unexpected(decoded_id.error());
  }

  device::PublicKeyCredentialUserEntity user_entity(*decoded_id);

  const std::string* name = dict.FindString(kName);
  if (name && !name->empty()) {
    user_entity.name = *name;
  }

  const std::string* display_name = dict.FindString(kDisplayName);
  if (display_name && !display_name->empty()) {
    user_entity.display_name = *display_name;
  }

  return user_entity;
}

// Builds a PublicKeyCredentialRpEntity object from the parameters contained in
// the provided dictionary.
base::expected<device::PublicKeyCredentialRpEntity, PasskeysParsingError>
BuildRpEntity(const base::DictValue& dict) {
  auto id_str =
      ValidateString(dict.FindString(kId), PasskeysParsingError::kMissingRpId,
                     PasskeysParsingError::kEmptyRpId);
  if (!id_str.has_value()) {
    return base::unexpected(id_str.error());
  }

  device::PublicKeyCredentialRpEntity rp_entity(*id_str);

  const std::string* name = dict.FindString(kName);
  if (name && !name->empty()) {
    rp_entity.name = *name;
  }

  return rp_entity;
}

// Converts the provided string to a UserVerificationRequirement enum.
device::UserVerificationRequirement ToUserVerificationRequirement(
    const std::string* user_verification) {
  if (!user_verification) {
    // Fall back to the `kPreferred` UV requirement as per the WebAuthn spec.
    return device::UserVerificationRequirement::kPreferred;
  }

  // TODO(crbug.com/460484682): Merge this code with
  // ShouldPerformUserVerificationForPreference().
  return device::ConvertToUserVerificationRequirement(*user_verification)
      .value_or(device::UserVerificationRequirement::kPreferred);
}

// Converts the provided booleans to a PasskeyRequestParams::RequestType enum.
PasskeyRequestParams::RequestType ToRequestType(bool is_conditional,
                                                bool for_create_request) {
  if (!is_conditional) {
    return PasskeyRequestParams::RequestType::kModal;
  }

  return for_create_request
             ? PasskeyRequestParams::RequestType::kConditionalCreate
             : PasskeyRequestParams::RequestType::kConditionalGet;
}

// Reads a list of PublicKeyCredentialDescriptor from the provided list.
base::expected<std::vector<device::PublicKeyCredentialDescriptor>,
               PasskeysParsingError>
ReadCredentials(const base::ListValue* serialized_descriptors) {
  std::vector<device::PublicKeyCredentialDescriptor> credential_descriptors;
  if (!serialized_descriptors) {
    return credential_descriptors;
  }

  for (const auto& serialized_descriptor : *serialized_descriptors) {
    const base::DictValue& dict = serialized_descriptor.GetDict();

    const std::string* type = dict.FindString(kType);
    if (!type) {
      return base::unexpected(PasskeysParsingError::kMissingCredentialType);
    }

    // Only the "public-key" type is supported.
    if (*type != device::kPublicKey) {
      continue;
    }

    auto decoded_id = ValidateBase64URLString(
        dict.FindString(kId), PasskeysParsingError::kMissingCredentialId,
        PasskeysParsingError::kEmptyCredentialId,
        PasskeysParsingError::kMalformedCredentialId);
    if (!decoded_id.has_value()) {
      return base::unexpected(decoded_id.error());
    }

    device::PublicKeyCredentialDescriptor credential_descriptor(
        device::CredentialType::kPublicKey, std::move(*decoded_id));

    // Read transport protocols.
    const base::ListValue* transports = dict.FindList(kTransports);
    if (transports) {
      for (const auto& transport : *transports) {
        std::optional<device::FidoTransportProtocol> fidoTransportProtocol =
            device::ConvertToFidoTransportProtocol(transport.GetString());
        if (fidoTransportProtocol.has_value()) {
          credential_descriptor.transports.insert(*fidoTransportProtocol);
        }
      }
    }

    credential_descriptors.emplace_back(credential_descriptor);
  }

  return credential_descriptors;
}

// Builds a PRFInputData object from a dictionary.
base::expected<passkey_model_utils::PRFInputData, PasskeysParsingError>
BuildPRFInputData(const base::DictValue& dict) {
  // Get base 64 encoded strings.
  const std::string* first64 = dict.FindString(device::kExtensionPRFFirst);
  const std::string* second64 = dict.FindString(device::kExtensionPRFSecond);

  // The first input is mandatory.
  if (!first64) {
    return base::unexpected(PasskeysParsingError::kMissingFirstPRFInput);
  }

  // Decode base 64 strings into byte arrays.
  std::optional<std::vector<uint8_t>> first = Base64UrlDecode(*first64);
  if (!first.has_value()) {
    // Base 64 decode failure.
    return base::unexpected(PasskeysParsingError::kMalformedFirstPRFInput);
  }

  std::optional<std::vector<uint8_t>> second = std::nullopt;
  if (second64) {
    second = Base64UrlDecode(*second64);
    if (!second.has_value()) {
      // Base 64 decode failure.
      return base::unexpected(PasskeysParsingError::kMalformedSecondPRFInput);
    }
  }

  // No input should be larger than the maximum allowed input size.
  if (first->size() > device::kMaxPRFInputSize ||
      (second.has_value() && second->size() > device::kMaxPRFInputSize)) {
    return base::unexpected(PasskeysParsingError::kPRFInputTooLarge);
  }

  return passkey_model_utils::PRFInputData(*first, second);
}

// Reads all extension data from the extensions dictionary.
base::expected<PasskeyExtensionData, PasskeysParsingError> BuildExtensionData(
    const base::DictValue& extensions,
    const std::optional<std::vector<device::PublicKeyCredentialDescriptor>>&
        allow_credentials,
    bool for_create_request) {
  const base::DictValue* prf = extensions.FindDict(device::kExtensionPRF);
  PasskeyExtensionData extension_data;
  if (!prf) {
    return extension_data;
  }

  const base::DictValue* prf_eval = prf->FindDict(device::kExtensionPRFEval);
  if (prf_eval) {
    auto prf_input_data = BuildPRFInputData(*prf_eval);
    if (!prf_input_data.has_value()) {
      return base::unexpected(prf_input_data.error());
    }
    extension_data.prf_eval = std::move(*prf_input_data);
  }

  const base::DictValue* eval_by_credential =
      prf->FindDict(device::kExtensionPRFEvalByCredential);
  if (eval_by_credential) {
    if (for_create_request) {
      // eval_by_credential is disallowed on create requests.
      return base::unexpected(PasskeysParsingError::kEvalByCredentialOnCreate);
    }

    for (auto per_credential_data : *eval_by_credential) {
      // Note that `&per_credential_data.first` can't be null, so
      // kMissingEvalByCredential is used for both the empty and missing cases.
      auto credential_id = ValidateBase64URLString(
          &per_credential_data.first,
          PasskeysParsingError::kMissingEvalByCredential,
          PasskeysParsingError::kMissingEvalByCredential,
          PasskeysParsingError::kMalformedEvalByCredential);

      if (!credential_id.has_value()) {
        return base::unexpected(credential_id.error());
      }

      // Every credential id must appear in the allow_credentials list.
      if (allow_credentials.has_value() &&
          !std::any_of(allow_credentials->begin(), allow_credentials->end(),
                       [&credential_id](
                           const device::PublicKeyCredentialDescriptor& desc) {
                         return desc.id == *credential_id;
                       })) {
        return base::unexpected(
            PasskeysParsingError::kEvalByCredentialNotAllowed);
      }

      auto prf_input_data =
          BuildPRFInputData(per_credential_data.second.GetDict());
      if (!prf_input_data.has_value()) {
        return base::unexpected(prf_input_data.error());
      }
      extension_data.prf_eval_by_credential.emplace(*credential_id,
                                                    std::move(*prf_input_data));
    }
  }

  return extension_data;
}

// Builds a PasskeyRequestParams object from the parameters contained in the
// provided dictionary.
base::expected<PasskeyRequestParams, PasskeysParsingError> BuildRequestParams(
    IOSPasskeyClient::RequestInfo request_info,
    const base::DictValue& dict,
    const std::optional<std::vector<device::PublicKeyCredentialDescriptor>>&
        allow_credentials,
    bool for_create_request) {
  const base::DictValue* request_dict = dict.FindDict(kRequest);
  if (!request_dict) {
    return base::unexpected(PasskeysParsingError::kMissingRequest);
  }

  auto challenge =
      ValidateBase64URLString(request_dict->FindString(kChallenge),
                              PasskeysParsingError::kMissingChallenge,
                              PasskeysParsingError::kEmptyChallenge,
                              PasskeysParsingError::kMalformedChallenge);
  if (!challenge.has_value()) {
    return base::unexpected(challenge.error());
  }

  std::optional<bool> isConditional = request_dict->FindBool(kIsConditional);
  if (!isConditional.has_value()) {
    return base::unexpected(PasskeysParsingError::kMissingConditional);
  }

  const base::DictValue* rp_entity_dict = dict.FindDict(kRpEntity);
  if (!rp_entity_dict) {
    return base::unexpected(PasskeysParsingError::kMissingRpEntity);
  }

  auto rp_entity = BuildRpEntity(*rp_entity_dict);
  if (!rp_entity.has_value()) {
    return base::unexpected(rp_entity.error());
  }

  const base::DictValue* extensions = dict.FindDict(kExtensions);
  if (!extensions) {
    return base::unexpected(PasskeysParsingError::kMissingExtensions);
  }

  auto extension_data =
      BuildExtensionData(*extensions, allow_credentials, for_create_request);
  if (!extension_data.has_value()) {
    return base::unexpected(extension_data.error());
  }

  return PasskeyRequestParams(std::move(request_info), std::move(*rp_entity),
                              std::move(*challenge),
                              ToUserVerificationRequirement(
                                  request_dict->FindString(kUserVerification)),
                              ToRequestType(*isConditional, for_create_request),
                              std::move(*extension_data));
}

}  // namespace

base::expected<IOSPasskeyClient::RequestInfo, PasskeysParsingError>
BuildRequestInfo(const base::DictValue& dict) {
  auto frame_id = ValidateString(dict.FindString(kFrameId),
                                 PasskeysParsingError::kMissingFrameId,
                                 PasskeysParsingError::kEmptyFrameId);
  if (!frame_id.has_value()) {
    return base::unexpected(frame_id.error());
  }

  auto request_id = ValidateString(dict.FindString(kRequestId),
                                   PasskeysParsingError::kMissingRequestId,
                                   PasskeysParsingError::kEmptyRequestId);
  if (!request_id.has_value()) {
    return base::unexpected(request_id.error());
  }

  return IOSPasskeyClient::RequestInfo(*frame_id, *request_id);
}

base::expected<AssertionRequestParams, PasskeysParsingError>
BuildAssertionRequestParams(IOSPasskeyClient::RequestInfo request_info,
                            const base::DictValue& dict) {
  auto credentials = ReadCredentials(dict.FindList(kAllowCredentials));
  if (!credentials.has_value()) {
    return base::unexpected(credentials.error());
  }

  auto request_params =
      BuildRequestParams(std::move(request_info), dict, *credentials,
                         /*for_create_request=*/false);
  if (!request_params.has_value()) {
    return base::unexpected(request_params.error());
  }

  return AssertionRequestParams(std::move(*request_params),
                                std::move(*credentials));
}

base::expected<RegistrationRequestParams, PasskeysParsingError>
BuildRegistrationRequestParams(IOSPasskeyClient::RequestInfo request_info,
                               const base::DictValue& dict) {
  auto credentials = ReadCredentials(dict.FindList(kExcludeCredentials));
  if (!credentials.has_value()) {
    return base::unexpected(credentials.error());
  }

  auto request_params = BuildRequestParams(
      std::move(request_info), dict, std::nullopt, /*for_create_request=*/true);
  if (!request_params.has_value()) {
    return base::unexpected(request_params.error());
  }

  const base::DictValue* user_entity_dict = dict.FindDict(kUserEntity);
  if (!user_entity_dict) {
    return base::unexpected(PasskeysParsingError::kMissingUserEntity);
  }

  auto user_entity = BuildUserEntity(*user_entity_dict);
  if (!user_entity.has_value()) {
    return base::unexpected(user_entity.error());
  }

  return RegistrationRequestParams(std::move(*request_params),
                                   std::move(*user_entity),
                                   std::move(*credentials));
}

base::DictValue ToAuthenticationExtensionsClientOutputsJSON(
    passkey_model_utils::ExtensionOutputData extension_output_data) {
  base::DictValue extensions_dict;

  if (!extension_output_data.prf_result.empty()) {
    static constexpr size_t kPRFOutputSize = 32u;
    size_t output_size = extension_output_data.prf_result.size();

    // PRF extension dictionary.
    // Contains `enabled` value and `results` dictionary.
    base::DictValue prf_dict;

    // PRF extension's `enabled` value.
    prf_dict.Set(device::kExtensionPRFEnabled, true);

    // PRF extension's `result` dictionary.
    // Contains `first` value as a base 64 encoded string.
    // May contain `second` value as a base 64 encoded string.
    base::DictValue prf_results_dict;
    if (output_size == kPRFOutputSize) {
      prf_results_dict.Set(device::kExtensionPRFFirst,
                           Base64UrlEncode(extension_output_data.prf_result));
    } else {
      // When non empty, the PRF result can have exactly 1 output or exactly 2
      // outputs.
      CHECK_EQ(output_size, kPRFOutputSize * 2u);

      auto span = base::span(extension_output_data.prf_result);
      auto [first, second] = span.split_at<kPRFOutputSize>();

      prf_results_dict.Set(device::kExtensionPRFFirst, Base64UrlEncode(first));
      prf_results_dict.Set(device::kExtensionPRFSecond,
                           Base64UrlEncode(second));
    }
    prf_dict.Set(device::kExtensionPRFResults, std::move(prf_results_dict));
    extensions_dict.Set(device::kExtensionPRF, std::move(prf_dict));
  }

  return extensions_dict;
}

std::optional<PasskeyScriptEvent> ParsePasskeyScriptEvent(
    const base::DictValue& dict,
    IsGpmPasskeyFunc is_gpm_passkey_func) {
  const std::string* event_string = dict.FindString(kEvent);
  if (!event_string || event_string->empty()) {
    return std::nullopt;
  }

  if (*event_string == kHandleGetRequest) {
    return PasskeyScriptEvent::kHandleGetRequest;
  }
  if (*event_string == kHandleCreateRequest) {
    return PasskeyScriptEvent::kHandleCreateRequest;
  }
  if (*event_string == kLogGetRequest) {
    return PasskeyScriptEvent::kLogGetRequest;
  }
  if (*event_string == kLogCreateRequest) {
    return PasskeyScriptEvent::kLogCreateRequest;
  }

  if (*event_string == kLogGetResolved) {
    const std::string* credential_id = dict.FindString(kCredentialId);
    const std::string* rp_id = dict.FindString(kRpId);
    if (!credential_id || credential_id->empty() || !rp_id || rp_id->empty()) {
      return std::nullopt;
    }
    // Checks whether a passkey matching the provided rp ID and credential ID is
    // present in the currently logged in user's GPM passkeys.
    bool is_gpm = is_gpm_passkey_func(*rp_id, *credential_id);
    return is_gpm ? PasskeyScriptEvent::kLogGetResolvedGpm
                  : PasskeyScriptEvent::kLogGetResolvedNonGpm;
  }

  if (*event_string == kLogCreateResolved) {
    std::optional<bool> is_gpm = dict.FindBool(kIsGpm);
    if (!is_gpm.has_value()) {
      return std::nullopt;
    }
    return *is_gpm ? PasskeyScriptEvent::kLogCreateResolvedGpm
                   : PasskeyScriptEvent::kLogCreateResolvedNonGpm;
  }

  return std::nullopt;
}

}  // namespace webauthn
