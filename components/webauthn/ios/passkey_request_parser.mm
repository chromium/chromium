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

// Decodes a base 64 encoded string into a data vector.
// Returns an empty vector on failure.
std::vector<uint8_t> Base64Decode(const std::string* base_64_string) {
  std::vector<uint8_t> decoded_data;
  if (!base_64_string || base_64_string->empty()) {
    return decoded_data;
  }

  std::string decoded_string;
  if (base::Base64Decode(*base_64_string, &decoded_string,
                         base::Base64DecodePolicy::kStrict)) {
    decoded_data.assign(decoded_string.begin(), decoded_string.end());
  }

  return decoded_data;
}

// Extracts all parameters required to build a PublicKeyCredentialUserEntity
// object from the provided dictionary.
device::PublicKeyCredentialUserEntity ExtractUserEntity(
    const base::Value::Dict* dict) {
  device::PublicKeyCredentialUserEntity user_entity;
  if (!dict) {
    return user_entity;
  }

  const std::string* id_base_64 = dict->FindString(kId);
  std::vector<uint8_t> decoded_id = Base64Decode(id_base_64);
  if (!decoded_id.empty()) {
    user_entity.id = std::move(decoded_id);
  }

  const std::string* name = dict->FindString(kName);
  if (name && !name->empty()) {
    user_entity.name = *name;
  }

  const std::string* display_name = dict->FindString(kDisplayName);
  if (display_name && !display_name->empty()) {
    user_entity.display_name = *display_name;
  }

  return user_entity;
}

// Extracts all parameters required to build a PublicKeyCredentialRpEntity
// object from the provided dictionary.
device::PublicKeyCredentialRpEntity ExtractRpEntity(
    const base::Value::Dict* dict) {
  device::PublicKeyCredentialRpEntity rp_entity;
  if (!dict) {
    return rp_entity;
  }

  const std::string* id_str = dict->FindString(kId);
  if (id_str && !id_str->empty()) {
    rp_entity.id = *id_str;
  }

  const std::string* name = dict->FindString(kName);
  if (name && !name->empty()) {
    rp_entity.name = *name;
  }

  return rp_entity;
}

// Converts the provided string to a UserVerificationRequirement enum.
device::UserVerificationRequirement ExtractUserVerification(
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
std::vector<device::PublicKeyCredentialDescriptor> ExtractCredentials(
    const base::Value::List* serialized_descriptors) {
  std::vector<device::PublicKeyCredentialDescriptor> credential_descriptors;
  if (!serialized_descriptors) {
    return credential_descriptors;
  }

  for (const auto& serialized_descriptor : *serialized_descriptors) {
    const base::Value::Dict& dict = serialized_descriptor.GetDict();
    std::vector<uint8_t> decoded_id = Base64Decode(dict.FindString(kId));
    if (decoded_id.empty()) {
      continue;
    }

    // Only the public-key type is supported.
    const std::string* type = dict.FindString(kType);
    if (!type || *type != device::kPublicKey) {
      continue;
    }

    device::PublicKeyCredentialDescriptor credential_descriptor(
        device::CredentialType::kPublicKey, std::move(decoded_id));

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

// Extracts all parameters required to build a PasskeyRequestParams object from
// the provided dictionary.
PasskeyRequestParams ExtractRequestParams(const base::Value::Dict& dict) {
  const std::string* frame_id = dict.FindString(kFrameId);
  const std::string* request_id = dict.FindString(kRequestId);
  const base::Value::Dict* request_dict = dict.FindDict(kRequest);
  if (!frame_id || !request_id || !request_dict) {
    return PasskeyRequestParams();
  }

  return PasskeyRequestParams(
      *frame_id, *request_id, ExtractRpEntity(dict.FindDict(kRpEntity)),
      Base64Decode(request_dict->FindString(kChallenge)),
      ExtractUserVerification(request_dict->FindString(kUserVerification)));
}

}  // namespace

AssertionRequestParams ExtractAssertionRequestParams(
    const base::Value::Dict& dict) {
  return AssertionRequestParams(
      ExtractRequestParams(dict),
      ExtractCredentials(dict.FindList(kAllowCredentials)));
}

RegistrationRequestParams ExtractRegistrationRequestParams(
    const base::Value::Dict& dict) {
  return RegistrationRequestParams(
      ExtractRequestParams(dict), ExtractUserEntity(dict.FindDict(kUserEntity)),
      ExtractCredentials(dict.FindList(kExcludeCredentials)));
}

}  // namespace webauthn
