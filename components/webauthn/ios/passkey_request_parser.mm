// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_request_parser.h"

#import "base/base64.h"
#import "device/fido/fido_user_verification_requirement.h"

namespace webauthn {

namespace {

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

// Common members of the "rpEntity" and "userEntity" dictionaries.
constexpr char kId[] = "id";
constexpr char kName[] = "name";

// Member exclusive to the "userEntity" dictionary.
constexpr char kDisplayName[] = "displayName";

// Member of the credential descriptors array.
constexpr char kType[] = "type";
constexpr char kTransports[] = "transports";

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

// Decodes a base 64 encoded string into a data vector.
// Returns std::nullopt on failure.
std::optional<std::vector<uint8_t>> Base64Decode(
    const std::string& base_64_string) {
  std::vector<uint8_t> decoded_data;
  if (base_64_string.empty()) {
    return decoded_data;
  }

  std::string decoded_string;
  if (!base::Base64Decode(base_64_string, &decoded_string,
                          base::Base64DecodePolicy::kStrict)) {
    return std::nullopt;
  }

  decoded_data.assign(decoded_string.begin(), decoded_string.end());
  return decoded_data;
}

// Builds a PublicKeyCredentialUserEntity object from the parameters contained
// in the provided dictionary.
base::expected<device::PublicKeyCredentialUserEntity, PasskeysParsingError>
BuildUserEntity(const base::Value::Dict& dict) {
  auto id_base_64 =
      ValidateString(dict.FindString(kId), PasskeysParsingError::kMissingUserId,
                     PasskeysParsingError::kEmptyUserId);
  if (!id_base_64.has_value()) {
    return base::unexpected(id_base_64.error());
  }

  std::optional<std::vector<uint8_t>> decoded_id = Base64Decode(*id_base_64);
  if (!decoded_id.has_value()) {
    return base::unexpected(PasskeysParsingError::kMalformedUserId);
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
BuildRpEntity(const base::Value::Dict& dict) {
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

// Reads a list of PublicKeyCredentialDescriptor from the provided list.
base::expected<std::vector<device::PublicKeyCredentialDescriptor>,
               PasskeysParsingError>
ReadCredentials(const base::Value::List* serialized_descriptors) {
  std::vector<device::PublicKeyCredentialDescriptor> credential_descriptors;
  if (!serialized_descriptors) {
    return credential_descriptors;
  }

  for (const auto& serialized_descriptor : *serialized_descriptors) {
    const base::Value::Dict& dict = serialized_descriptor.GetDict();

    const std::string* type = dict.FindString(kType);
    if (!type) {
      return base::unexpected(PasskeysParsingError::kMissingCredentialType);
    }

    // Only the "public-key" type is supported.
    if (*type != device::kPublicKey) {
      continue;
    }

    auto credential_id_base_64 = ValidateString(
        dict.FindString(kId), PasskeysParsingError::kMissingCredentialId,
        PasskeysParsingError::kEmptyCredentialId);
    if (!credential_id_base_64.has_value()) {
      return base::unexpected(credential_id_base_64.error());
    }

    std::optional<std::vector<uint8_t>> decoded_id =
        Base64Decode(*credential_id_base_64);
    if (!decoded_id.has_value()) {
      return base::unexpected(PasskeysParsingError::kMalformedCredentialId);
    }

    device::PublicKeyCredentialDescriptor credential_descriptor(
        device::CredentialType::kPublicKey, std::move(*decoded_id));

    // Read transport protocols.
    const base::Value::List* transports = dict.FindList(kTransports);
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

// Builds a PasskeyRequestParams object from the parameters contained in the
// provided dictionary.
base::expected<PasskeyRequestParams, PasskeysParsingError> BuildRequestParams(
    IOSPasskeyClient::RequestInfo request_info,
    const base::Value::Dict& dict) {
  const base::Value::Dict* request_dict = dict.FindDict(kRequest);
  if (!request_dict) {
    return base::unexpected(PasskeysParsingError::kMissingRequest);
  }

  auto challenge_base_64 =
      ValidateString(request_dict->FindString(kChallenge),
                     PasskeysParsingError::kMissingChallenge,
                     PasskeysParsingError::kEmptyChallenge);
  if (!challenge_base_64.has_value()) {
    return base::unexpected(challenge_base_64.error());
  }

  std::optional<std::vector<uint8_t>> challenge =
      Base64Decode(*challenge_base_64);
  if (!challenge.has_value()) {
    return base::unexpected(PasskeysParsingError::kMalformedChallenge);
  }

  const base::Value::Dict* rp_entity_dict = dict.FindDict(kRpEntity);
  if (!rp_entity_dict) {
    return base::unexpected(PasskeysParsingError::kMissingRpEntity);
  }

  auto rp_entity = BuildRpEntity(*rp_entity_dict);
  if (!rp_entity.has_value()) {
    return base::unexpected(rp_entity.error());
  }

  return PasskeyRequestParams(std::move(request_info), std::move(*rp_entity),
                              std::move(*challenge),
                              ToUserVerificationRequirement(
                                  request_dict->FindString(kUserVerification)));
}

}  // namespace

base::expected<IOSPasskeyClient::RequestInfo, PasskeysParsingError>
BuildRequestInfo(const base::Value::Dict& dict) {
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
                            const base::Value::Dict& dict) {
  auto request_params = BuildRequestParams(std::move(request_info), dict);
  if (!request_params.has_value()) {
    return base::unexpected(request_params.error());
  }

  auto credentials = ReadCredentials(dict.FindList(kAllowCredentials));
  if (!credentials.has_value()) {
    return base::unexpected(credentials.error());
  }

  return AssertionRequestParams(std::move(*request_params),
                                std::move(*credentials));
}

base::expected<RegistrationRequestParams, PasskeysParsingError>
BuildRegistrationRequestParams(IOSPasskeyClient::RequestInfo request_info,
                               const base::Value::Dict& dict) {
  auto request_params = BuildRequestParams(std::move(request_info), dict);
  if (!request_params.has_value()) {
    return base::unexpected(request_params.error());
  }

  const base::Value::Dict* user_entity_dict = dict.FindDict(kUserEntity);
  if (!user_entity_dict) {
    return base::unexpected(PasskeysParsingError::kMissingUserEntity);
  }

  auto user_entity = BuildUserEntity(*user_entity_dict);
  if (!user_entity.has_value()) {
    return base::unexpected(user_entity.error());
  }

  auto credentials = ReadCredentials(dict.FindList(kExcludeCredentials));
  if (!credentials.has_value()) {
    return base::unexpected(credentials.error());
  }

  return RegistrationRequestParams(std::move(*request_params),
                                   std::move(*user_entity),
                                   std::move(*credentials));
}

}  // namespace webauthn
