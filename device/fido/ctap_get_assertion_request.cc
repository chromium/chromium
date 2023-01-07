// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_get_assertion_request.h"

#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "components/cbor/writer.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/pin.h"

namespace device {

namespace {
bool IsGetAssertionOptionMapFormatCorrect(
    const cbor::Value::MapValue& option_map) {
  return base::ranges::all_of(
      option_map, [](const auto& param) {
        return param.first.is_string() &&
               (param.first.GetString() == kUserPresenceMapKey ||
                param.first.GetString() == kUserVerificationMapKey) &&
               param.second.is_bool();
      });
}

bool AreGetAssertionRequestMapKeysCorrect(
    const cbor::Value::MapValue& request_map) {
  return base::ranges::all_of(
      request_map, [](const auto& param) {
        return (param.first.is_integer() && 1u <= param.first.GetInteger() &&
                param.first.GetInteger() <= 7u);
      });
}
}  // namespace

CtapGetAssertionOptions::CtapGetAssertionOptions() = default;
CtapGetAssertionOptions::CtapGetAssertionOptions(
    const CtapGetAssertionOptions&) = default;
CtapGetAssertionOptions::CtapGetAssertionOptions(CtapGetAssertionOptions&&) =
    default;
CtapGetAssertionOptions::~CtapGetAssertionOptions() = default;

CtapGetAssertionOptions::PRFInput::PRFInput() = default;
CtapGetAssertionOptions::PRFInput::PRFInput(const PRFInput&) = default;
CtapGetAssertionOptions::PRFInput::PRFInput(PRFInput&&) = default;
CtapGetAssertionOptions::PRFInput::~PRFInput() = default;

CtapGetAssertionRequest::HMACSecret::HMACSecret(
    base::span<const uint8_t, kP256X962Length> in_public_key_x962,
    base::span<const uint8_t> in_encrypted_salts,
    base::span<const uint8_t> in_salts_auth)
    : public_key_x962(fido_parsing_utils::Materialize(in_public_key_x962)),
      encrypted_salts(fido_parsing_utils::Materialize(in_encrypted_salts)),
      salts_auth(fido_parsing_utils::Materialize(in_salts_auth)) {}

CtapGetAssertionRequest::HMACSecret::HMACSecret(const HMACSecret&) = default;
CtapGetAssertionRequest::HMACSecret::~HMACSecret() = default;
CtapGetAssertionRequest::HMACSecret&
CtapGetAssertionRequest::HMACSecret::operator=(const HMACSecret&) = default;

// static
absl::optional<CtapGetAssertionRequest> CtapGetAssertionRequest::Parse(
    const cbor::Value::MapValue& request_map,
    const ParseOpts& opts) {
  if (!AreGetAssertionRequestMapKeysCorrect(request_map))
    return absl::nullopt;

  const auto rp_id_it = request_map.find(cbor::Value(1));
  if (rp_id_it == request_map.end() || !rp_id_it->second.is_string())
    return absl::nullopt;

  const auto client_data_hash_it = request_map.find(cbor::Value(2));
  if (client_data_hash_it == request_map.end() ||
      !client_data_hash_it->second.is_bytestring() ||
      client_data_hash_it->second.GetBytestring().size() !=
          kClientDataHashLength) {
    return absl::nullopt;
  }
  base::span<const uint8_t, kClientDataHashLength> client_data_hash(
      client_data_hash_it->second.GetBytestring().data(),
      kClientDataHashLength);

  CtapGetAssertionRequest request(rp_id_it->second.GetString(),
                                  /*client_data_json=*/std::string());
  request.client_data_hash = fido_parsing_utils::Materialize(client_data_hash);

  const auto allow_list_it = request_map.find(cbor::Value(3));
  if (allow_list_it != request_map.end()) {
    if (!allow_list_it->second.is_array())
      return absl::nullopt;

    const auto& credential_descriptors = allow_list_it->second.GetArray();
    if (credential_descriptors.empty())
      return absl::nullopt;

    std::vector<PublicKeyCredentialDescriptor> allow_list;
    for (const auto& credential_descriptor : credential_descriptors) {
      auto allowed_credential =
          PublicKeyCredentialDescriptor::CreateFromCBORValue(
              credential_descriptor);
      if (!allowed_credential)
        return absl::nullopt;

      allow_list.push_back(std::move(*allowed_credential));
    }
    request.allow_list = std::move(allow_list);
  }

  const auto extensions_it = request_map.find(cbor::Value(4));
  if (extensions_it != request_map.end()) {
    if (!extensions_it->second.is_map()) {
      return absl::nullopt;
    }

    const cbor::Value::MapValue& extensions = extensions_it->second.GetMap();
    if (opts.reject_all_extensions && !extensions.empty()) {
      return absl::nullopt;
    }

    for (const auto& extension : extensions) {
      if (!extension.first.is_string()) {
        return absl::nullopt;
      }

      const std::string& extension_id = extension.first.GetString();
      if (extension_id == kExtensionHmacSecret) {
        if (!extension.second.is_map()) {
          return absl::nullopt;
        }
        const auto& hmac_extension = extension.second.GetMap();

        auto hmac_it = hmac_extension.find(cbor::Value(1));
        if (hmac_it == hmac_extension.end() || !hmac_it->second.is_map()) {
          return absl::nullopt;
        }
        const absl::optional<pin::KeyAgreementResponse> key(
            pin::KeyAgreementResponse::ParseFromCOSE(hmac_it->second.GetMap()));

        hmac_it = hmac_extension.find(cbor::Value(2));
        if (hmac_it == hmac_extension.end() ||
            !hmac_it->second.is_bytestring()) {
          return absl::nullopt;
        }
        const std::vector<uint8_t>& encrypted_salts =
            hmac_it->second.GetBytestring();

        hmac_it = hmac_extension.find(cbor::Value(3));
        if (hmac_it == hmac_extension.end() ||
            !hmac_it->second.is_bytestring()) {
          return absl::nullopt;
        }
        const std::vector<uint8_t>& salts_auth =
            hmac_it->second.GetBytestring();

        if (!key ||
            (encrypted_salts.size() != 32 && encrypted_salts.size() != 64) ||
            salts_auth.size() != 16) {
          return absl::nullopt;
        }

        request.hmac_secret.emplace(key->X962(), encrypted_salts, salts_auth);
      } else if (extension_id == kExtensionLargeBlobKey) {
        if (!extension.second.is_bool() || !extension.second.GetBool()) {
          return absl::nullopt;
        }
        request.large_blob_key = true;
      } else if (extension_id == kExtensionCredBlob) {
        if (!extension.second.is_bool() || !extension.second.GetBool()) {
          return absl::nullopt;
        }
        request.get_cred_blob = true;
      } else if (extension_id == kExtensionDevicePublicKey) {
        // There's not currently any support for the ep bit in assertion
        // requests so DPK requests are assumed to be ep=1 only.
        request.device_public_key = DevicePublicKeyRequest::FromCBOR(
            extension.second, /* ep_approved_by_browser= */ false);
        if (!request.device_public_key) {
          return absl::nullopt;
        }
      }
    }
  }

  const auto option_it = request_map.find(cbor::Value(5));
  if (option_it != request_map.end()) {
    if (!option_it->second.is_map())
      return absl::nullopt;

    const auto& option_map = option_it->second.GetMap();
    if (!IsGetAssertionOptionMapFormatCorrect(option_map))
      return absl::nullopt;

    const auto user_presence_option =
        option_map.find(cbor::Value(kUserPresenceMapKey));
    if (user_presence_option != option_map.end()) {
      request.user_presence_required = user_presence_option->second.GetBool();
    }

    const auto uv_option =
        option_map.find(cbor::Value(kUserVerificationMapKey));
    if (uv_option != option_map.end()) {
      request.user_verification =
          uv_option->second.GetBool()
              ? UserVerificationRequirement::kRequired
              : UserVerificationRequirement::kDiscouraged;
    }
  }

  const auto pin_auth_it = request_map.find(cbor::Value(6));
  if (pin_auth_it != request_map.end()) {
    if (!pin_auth_it->second.is_bytestring())
      return absl::nullopt;

    request.pin_auth = pin_auth_it->second.GetBytestring();
  }

  const auto pin_protocol_it = request_map.find(cbor::Value(7));
  if (pin_protocol_it != request_map.end()) {
    if (!pin_protocol_it->second.is_unsigned() ||
        pin_protocol_it->second.GetUnsigned() >
            std::numeric_limits<uint8_t>::max()) {
      return absl::nullopt;
    }
    absl::optional<PINUVAuthProtocol> pin_protocol =
        ToPINUVAuthProtocol(pin_protocol_it->second.GetUnsigned());
    if (!pin_protocol) {
      return absl::nullopt;
    }
    request.pin_protocol = *pin_protocol;
  }

  return request;
}

CtapGetAssertionRequest::CtapGetAssertionRequest(
    std::string in_rp_id,
    std::string in_client_data_json)
    : rp_id(std::move(in_rp_id)),
      client_data_json(std::move(in_client_data_json)),
      client_data_hash(fido_parsing_utils::CreateSHA256Hash(client_data_json)) {
}

CtapGetAssertionRequest::CtapGetAssertionRequest(
    const CtapGetAssertionRequest& that) = default;

CtapGetAssertionRequest::CtapGetAssertionRequest(
    CtapGetAssertionRequest&& that) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    const CtapGetAssertionRequest& other) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    CtapGetAssertionRequest&& other) = default;

CtapGetAssertionRequest::~CtapGetAssertionRequest() = default;

std::pair<CtapRequestCommand, absl::optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapGetAssertionRequest& request) {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(request.rp_id);
  cbor_map[cbor::Value(2)] = cbor::Value(request.client_data_hash);

  if (!request.allow_list.empty()) {
    cbor::Value::ArrayValue allow_list_array;
    for (const auto& descriptor : request.allow_list) {
      allow_list_array.push_back(AsCBOR(descriptor));
    }
    cbor_map[cbor::Value(3)] = cbor::Value(std::move(allow_list_array));
  }

  cbor::Value::MapValue extensions;

  if (request.large_blob_key) {
    extensions.emplace(kExtensionLargeBlobKey, cbor::Value(true));
  }

  if (request.hmac_secret) {
    const auto& hmac_secret = *request.hmac_secret;
    cbor::Value::MapValue hmac_extension;
    hmac_extension.emplace(
        1, pin::EncodeCOSEPublicKey(hmac_secret.public_key_x962));
    hmac_extension.emplace(2, hmac_secret.encrypted_salts);
    hmac_extension.emplace(3, hmac_secret.salts_auth);
    extensions.emplace(kExtensionHmacSecret, std::move(hmac_extension));
  }

  if (request.get_cred_blob) {
    extensions.emplace(kExtensionCredBlob, true);
  }

  if (request.device_public_key) {
    extensions.emplace(kExtensionDevicePublicKey,
                       request.device_public_key->ToCBOR());
  }

  if (!extensions.empty()) {
    cbor_map[cbor::Value(4)] = cbor::Value(std::move(extensions));
  }

  if (request.pin_auth) {
    cbor_map[cbor::Value(6)] = cbor::Value(*request.pin_auth);
  }

  if (request.pin_protocol) {
    cbor_map[cbor::Value(7)] =
        cbor::Value(static_cast<uint8_t>(*request.pin_protocol));
  }

  cbor::Value::MapValue option_map;

  // User presence is required by default.
  if (!request.user_presence_required) {
    option_map[cbor::Value(kUserPresenceMapKey)] =
        cbor::Value(request.user_presence_required);
  }

  // User verification is not required by default.
  if (request.user_verification == UserVerificationRequirement::kRequired) {
    option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(option_map));
  }

  return std::make_pair(CtapRequestCommand::kAuthenticatorGetAssertion,
                        cbor::Value(std::move(cbor_map)));
}

std::pair<CtapRequestCommand, absl::optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapGetNextAssertionRequest&) {
  return std::make_pair(CtapRequestCommand::kAuthenticatorGetNextAssertion,
                        absl::nullopt);
}

}  // namespace device
