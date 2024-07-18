// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/ctap_get_assertion_request.h"

#include <limits>
#include <utility>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/features.h"
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
  return base::ranges::all_of(request_map, [](const auto& param) {
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

bool operator<(const PRFInput& a, const PRFInput& b) {
  if (!a.credential_id.has_value()) {
    return b.credential_id.has_value();
  }
  if (!b.credential_id.has_value()) {
    return false;
  }
  return a.credential_id.value() < b.credential_id.value();
}

CtapGetAssertionRequest::HMACSecret::HMACSecret(
    base::span<const uint8_t, kP256X962Length> in_public_key_x962,
    base::span<const uint8_t> in_encrypted_salts,
    base::span<const uint8_t> in_salts_auth,
    std::optional<PINUVAuthProtocol> in_pin_protocol)
    : public_key_x962(fido_parsing_utils::Materialize(in_public_key_x962)),
      encrypted_salts(fido_parsing_utils::Materialize(in_encrypted_salts)),
      salts_auth(fido_parsing_utils::Materialize(in_salts_auth)),
      pin_protocol(in_pin_protocol) {}

CtapGetAssertionRequest::HMACSecret::HMACSecret(const HMACSecret&) = default;
CtapGetAssertionRequest::HMACSecret::~HMACSecret() = default;
CtapGetAssertionRequest::HMACSecret&
CtapGetAssertionRequest::HMACSecret::operator=(const HMACSecret&) = default;

// static
std::optional<CtapGetAssertionRequest> CtapGetAssertionRequest::Parse(
    const cbor::Value::MapValue& request_map,
    const ParseOpts& opts) {
  if (!AreGetAssertionRequestMapKeysCorrect(request_map))
    return std::nullopt;

  const auto rp_id_it = request_map.find(cbor::Value(1));
  if (rp_id_it == request_map.end() || !rp_id_it->second.is_string())
    return std::nullopt;

  const auto client_data_hash_it = request_map.find(cbor::Value(2));
  if (client_data_hash_it == request_map.end() ||
      !client_data_hash_it->second.is_bytestring() ||
      client_data_hash_it->second.GetBytestring().size() !=
          kClientDataHashLength) {
    return std::nullopt;
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
      return std::nullopt;

    const auto& credential_descriptors = allow_list_it->second.GetArray();
    if (credential_descriptors.empty())
      return std::nullopt;

    std::vector<PublicKeyCredentialDescriptor> allow_list;
    for (const auto& credential_descriptor : credential_descriptors) {
      auto allowed_credential =
          PublicKeyCredentialDescriptor::CreateFromCBORValue(
              credential_descriptor);
      if (!allowed_credential)
        return std::nullopt;

      allow_list.push_back(std::move(*allowed_credential));
    }
    request.allow_list = std::move(allow_list);
  }

  const auto extensions_it = request_map.find(cbor::Value(4));
  if (extensions_it != request_map.end()) {
    if (!extensions_it->second.is_map()) {
      return std::nullopt;
    }

    const cbor::Value::MapValue& extensions = extensions_it->second.GetMap();
    if (opts.reject_all_extensions && !extensions.empty()) {
      return std::nullopt;
    }

    for (const auto& extension : extensions) {
      if (!extension.first.is_string()) {
        return std::nullopt;
      }

      const std::string& extension_id = extension.first.GetString();
      if (extension_id == kExtensionHmacSecret) {
        if (!extension.second.is_map()) {
          return std::nullopt;
        }
        const auto& hmac_extension = extension.second.GetMap();

        auto hmac_it = hmac_extension.find(cbor::Value(1));
        if (hmac_it == hmac_extension.end() || !hmac_it->second.is_map()) {
          return std::nullopt;
        }
        const std::optional<pin::KeyAgreementResponse> key(
            pin::KeyAgreementResponse::ParseFromCOSE(hmac_it->second.GetMap()));
        if (!key) {
          return std::nullopt;
        }

        hmac_it = hmac_extension.find(cbor::Value(2));
        if (hmac_it == hmac_extension.end() ||
            !hmac_it->second.is_bytestring()) {
          return std::nullopt;
        }
        const std::vector<uint8_t>& encrypted_salts =
            hmac_it->second.GetBytestring();

        hmac_it = hmac_extension.find(cbor::Value(3));
        if (hmac_it == hmac_extension.end() ||
            !hmac_it->second.is_bytestring()) {
          return std::nullopt;
        }
        const std::vector<uint8_t>& salts_auth =
            hmac_it->second.GetBytestring();

        std::optional<PINUVAuthProtocol> pin_protocol;
        const auto pin_protocol_it = hmac_extension.find(cbor::Value(4));
        if (pin_protocol_it != hmac_extension.end()) {
          if (!pin_protocol_it->second.is_unsigned() ||
              pin_protocol_it->second.GetUnsigned() >
                  std::numeric_limits<uint8_t>::max()) {
            return std::nullopt;
          }
          pin_protocol =
              ToPINUVAuthProtocol(pin_protocol_it->second.GetUnsigned());
          if (!pin_protocol) {
            return std::nullopt;
          }
        }

        request.hmac_secret.emplace(key->X962(), encrypted_salts, salts_auth,
                                    pin_protocol);
      } else if (extension_id == kExtensionLargeBlobKey) {
        if (!extension.second.is_bool() || !extension.second.GetBool()) {
          return std::nullopt;
        }
        request.large_blob_key = true;
      } else if (extension_id == kExtensionCredBlob) {
        if (!extension.second.is_bool() || !extension.second.GetBool()) {
          return std::nullopt;
        }
        request.get_cred_blob = true;
      } else if (extension_id == kExtensionPRF) {
        if (!extension.second.is_map()) {
          return std::nullopt;
        }
        const cbor::Value::MapValue& prf = extension.second.GetMap();
        const auto eval_it = prf.find(cbor::Value(kExtensionPRFEval));
        if (eval_it != prf.end()) {
          std::optional<PRFInput> input = PRFInput::FromCBOR(eval_it->second);
          if (!input) {
            return std::nullopt;
          }
          request.prf_inputs.emplace_back(std::move(*input));
        }
        const auto by_cred_it =
            prf.find(cbor::Value(kExtensionPRFEvalByCredential));
        if (by_cred_it != prf.end()) {
          if (!by_cred_it->second.is_map()) {
            return std::nullopt;
          }
          const cbor::Value::MapValue& by_cred = by_cred_it->second.GetMap();
          for (const auto& cred : by_cred) {
            std::optional<PRFInput> input = PRFInput::FromCBOR(cred.second);
            if (!input || !cred.first.is_bytestring()) {
              return std::nullopt;
            }
            input->credential_id = cred.first.GetBytestring();
            if (input->credential_id->empty()) {
              return std::nullopt;
            }
            request.prf_inputs.emplace_back(std::move(*input));
          }
        }
        std::sort(request.prf_inputs.begin(), request.prf_inputs.end());
      } else if (extension_id == kExtensionLargeBlob) {
        if (!extension.second.is_map()) {
          return std::nullopt;
        }
        const cbor::Value::MapValue& large_blob_ext = extension.second.GetMap();
        const auto read_it =
            large_blob_ext.find(cbor::Value(kExtensionLargeBlobRead));
        const bool has_read = read_it != large_blob_ext.end();

        const auto write_it =
            large_blob_ext.find(cbor::Value(kExtensionLargeBlobWrite));
        const bool has_write = write_it != large_blob_ext.end();

        const auto original_size_it =
            large_blob_ext.find(cbor::Value(kExtensionLargeBlobOriginalSize));
        const bool has_original_size = original_size_it != large_blob_ext.end();

        if ((has_read && !read_it->second.is_bool()) ||
            (has_write && !write_it->second.is_bytestring()) ||
            (has_original_size && !original_size_it->second.is_unsigned())) {
          return std::nullopt;
        }

        if (has_read && !has_write && !has_original_size) {
          request.large_blob_extension_read = read_it->second.GetBool();
        } else if (!has_read && has_write && has_original_size) {
          request.large_blob_extension_write.emplace(
              write_it->second.GetBytestring(),
              base::checked_cast<size_t>(
                  original_size_it->second.GetUnsigned()));
        } else {
          // No other combinations of keys are acceptable.
          return std::nullopt;
        }
      }
    }
  }

  const auto option_it = request_map.find(cbor::Value(5));
  if (option_it != request_map.end()) {
    if (!option_it->second.is_map()) {
      return std::nullopt;
    }

    const auto& option_map = option_it->second.GetMap();
    if (!IsGetAssertionOptionMapFormatCorrect(option_map)) {
      return std::nullopt;
    }

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
      return std::nullopt;

    request.pin_auth = pin_auth_it->second.GetBytestring();
  }

  const auto pin_protocol_it = request_map.find(cbor::Value(7));
  if (pin_protocol_it != request_map.end()) {
    if (!pin_protocol_it->second.is_unsigned() ||
        pin_protocol_it->second.GetUnsigned() >
            std::numeric_limits<uint8_t>::max()) {
      return std::nullopt;
    }
    std::optional<PINUVAuthProtocol> pin_protocol =
        ToPINUVAuthProtocol(pin_protocol_it->second.GetUnsigned());
    if (!pin_protocol) {
      return std::nullopt;
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

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
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

  if (request.large_blob_extension_read) {
    DCHECK(!request.large_blob_key);
    cbor::Value::MapValue large_blob_ext;
    large_blob_ext.emplace(kExtensionLargeBlobRead, true);
    extensions.emplace(kExtensionLargeBlob, std::move(large_blob_ext));
  }

  if (request.large_blob_extension_write) {
    DCHECK(!request.large_blob_key);
    const LargeBlob& large_blob = *request.large_blob_extension_write;
    cbor::Value::MapValue large_blob_ext;
    large_blob_ext.emplace(kExtensionLargeBlobWrite,
                           large_blob.compressed_data);
    large_blob_ext.emplace(
        kExtensionLargeBlobOriginalSize,
        base::checked_cast<int64_t>(large_blob.original_size));
    extensions.emplace(kExtensionLargeBlob, std::move(large_blob_ext));
  }

  if (request.hmac_secret) {
    const auto& hmac_secret = *request.hmac_secret;
    cbor::Value::MapValue hmac_extension;
    hmac_extension.emplace(
        1, pin::EncodeCOSEPublicKey(hmac_secret.public_key_x962));
    hmac_extension.emplace(2, hmac_secret.encrypted_salts);
    hmac_extension.emplace(3, hmac_secret.salts_auth);
    if (request.pin_protocol &&
        static_cast<unsigned>(*request.pin_protocol) >= 2) {
      // If the request is using a PIN protocol other than v1, it must be
      // specified here too:
      // https://fidoalliance.org/specs/fido-v2.2-rd-20230321/fido-client-to-authenticator-protocol-v2.2-rd-20230321.html#sctn-hmac-secret-extension:~:text=pinuvauthprotocol(0x04)%3A%20(optional)%20as%20selected%20when%20getting%20the%20shared%20secret.%20ctap2.1%20platforms%20must%20include%20this%20parameter%20if%20the%20value%20of%20pinuvauthprotocol%20is%20not%201
      hmac_extension.emplace(4, static_cast<int64_t>(*request.pin_protocol));
    }
    extensions.emplace(kExtensionHmacSecret, std::move(hmac_extension));
  }

  if (request.get_cred_blob) {
    extensions.emplace(kExtensionCredBlob, true);
  }

  if (!request.prf_inputs.empty()) {
    cbor::Value::MapValue prf;
    cbor::Value::MapValue by_cred;
    for (const auto& input : request.prf_inputs) {
      if (!input.credential_id.has_value()) {
        prf.emplace(kExtensionPRFEval, input.ToCBOR());
      } else {
        by_cred.emplace(*input.credential_id, input.ToCBOR());
      }
    }
    if (!by_cred.empty()) {
      prf.emplace(kExtensionPRFEvalByCredential, std::move(by_cred));
    }
    extensions.emplace(kExtensionPRF, std::move(prf));
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

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapGetNextAssertionRequest&) {
  return std::make_pair(CtapRequestCommand::kAuthenticatorGetNextAssertion,
                        std::nullopt);
}

}  // namespace device
