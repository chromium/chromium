// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/ctap_make_credential_request.h"

#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "components/cbor/values.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {
bool IsMakeCredentialOptionMapFormatCorrect(
    const cbor::Value::MapValue& option_map) {
  return base::ranges::all_of(
      option_map, [](const auto& param) {
        return param.first.is_string() &&
               (param.first.GetString() == kResidentKeyMapKey ||
                param.first.GetString() == kUserVerificationMapKey) &&
               param.second.is_bool();
      });
}

bool AreMakeCredentialRequestMapKeysCorrect(
    const cbor::Value::MapValue& request_map) {
  return base::ranges::all_of(
      request_map, [](const auto& param) {
        return (param.first.is_integer() && 1u <= param.first.GetInteger() &&
                param.first.GetInteger() <= 10u);
      });
}

}  // namespace

// static
std::optional<CtapMakeCredentialRequest> CtapMakeCredentialRequest::Parse(
    const cbor::Value::MapValue& request_map,
    const ParseOpts& opts) {
  if (!AreMakeCredentialRequestMapKeysCorrect(request_map))
    return std::nullopt;

  const auto client_data_hash_it = request_map.find(cbor::Value(1));
  if (client_data_hash_it == request_map.end() ||
      !client_data_hash_it->second.is_bytestring() ||
      client_data_hash_it->second.GetBytestring().size() !=
          kClientDataHashLength) {
    return std::nullopt;
  }
  base::span<const uint8_t, kClientDataHashLength> client_data_hash(
      client_data_hash_it->second.GetBytestring().data(),
      kClientDataHashLength);

  const auto rp_entity_it = request_map.find(cbor::Value(2));
  if (rp_entity_it == request_map.end() || !rp_entity_it->second.is_map())
    return std::nullopt;

  auto rp_entity =
      PublicKeyCredentialRpEntity::CreateFromCBORValue(rp_entity_it->second);
  if (!rp_entity)
    return std::nullopt;

  const auto user_entity_it = request_map.find(cbor::Value(3));
  if (user_entity_it == request_map.end() || !user_entity_it->second.is_map())
    return std::nullopt;

  auto user_entity = PublicKeyCredentialUserEntity::CreateFromCBORValue(
      user_entity_it->second);
  if (!user_entity)
    return std::nullopt;

  const auto credential_params_it = request_map.find(cbor::Value(4));
  if (credential_params_it == request_map.end())
    return std::nullopt;

  auto credential_params = PublicKeyCredentialParams::CreateFromCBORValue(
      credential_params_it->second);
  if (!credential_params)
    return std::nullopt;

  CtapMakeCredentialRequest request(
      /*client_data_json=*/std::string(), std::move(*rp_entity),
      std::move(*user_entity), std::move(*credential_params));
  request.client_data_hash = fido_parsing_utils::Materialize(client_data_hash);

  const auto exclude_list_it = request_map.find(cbor::Value(5));
  if (exclude_list_it != request_map.end()) {
    if (!exclude_list_it->second.is_array())
      return std::nullopt;

    const auto& credential_descriptors = exclude_list_it->second.GetArray();
    std::vector<PublicKeyCredentialDescriptor> exclude_list;
    for (const auto& credential_descriptor : credential_descriptors) {
      auto excluded_credential =
          PublicKeyCredentialDescriptor::CreateFromCBORValue(
              credential_descriptor);
      if (!excluded_credential)
        return std::nullopt;

      exclude_list.push_back(std::move(*excluded_credential));
    }
    request.exclude_list = std::move(exclude_list);
  }

  const auto enterprise_attestation_it = request_map.find(cbor::Value(10));
  if (enterprise_attestation_it != request_map.end()) {
    if (!enterprise_attestation_it->second.is_unsigned()) {
      return std::nullopt;
    }
    switch (enterprise_attestation_it->second.GetUnsigned()) {
      case 1:
        request.attestation_preference = AttestationConveyancePreference::
            kEnterpriseIfRPListedOnAuthenticator;
        break;
      case 2:
        request.attestation_preference =
            AttestationConveyancePreference::kEnterpriseApprovedByBrowser;
        break;
      default:
        return std::nullopt;
    }
  }

  const auto extensions_it = request_map.find(cbor::Value(6));
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

      const std::string& extension_name = extension.first.GetString();

      if (extension_name == kExtensionCredProtect) {
        if (!extension.second.is_unsigned()) {
          return std::nullopt;
        }
        switch (extension.second.GetUnsigned()) {
          case 1:
            request.cred_protect = device::CredProtect::kUVOptional;
            break;
          case 2:
            request.cred_protect = device::CredProtect::kUVOrCredIDRequired;
            break;
          case 3:
            request.cred_protect = device::CredProtect::kUVRequired;
            break;
          default:
            return std::nullopt;
        }
      } else if (extension_name == kExtensionHmacSecret) {
        if (!extension.second.is_bool()) {
          return std::nullopt;
        }
        request.hmac_secret = extension.second.GetBool();
      } else if (extension_name == kExtensionPRF) {
        if (!extension.second.is_map()) {
          return std::nullopt;
        }
        const cbor::Value::MapValue& prf = extension.second.GetMap();
        const auto eval_it = prf.find(cbor::Value(kExtensionPRFEval));
        if (eval_it != prf.end()) {
          request.prf_input = PRFInput::FromCBOR(eval_it->second);
          if (!request.prf_input) {
            return std::nullopt;
          }
        }
        request.prf = true;
      } else if (extension_name == kExtensionLargeBlobKey) {
        if (!extension.second.is_bool() || !extension.second.GetBool()) {
          return std::nullopt;
        }
        request.large_blob_key = true;
      } else if (extension_name == kExtensionLargeBlob) {
        if (!extension.second.is_map()) {
          return std::nullopt;
        }
        const cbor::Value::MapValue& large_blob_ext = extension.second.GetMap();
        const auto support_it =
            large_blob_ext.find(cbor::Value(kExtensionLargeBlobSupport));
        if (support_it != large_blob_ext.end()) {
          if (!support_it->second.is_string()) {
            return std::nullopt;
          }
          const std::string& support = support_it->second.GetString();
          if (support == kExtensionLargeBlobSupportRequired) {
            request.large_blob_support = LargeBlobSupport::kRequired;
          } else if (support == kExtensionLargeBlobSupportPreferred) {
            request.large_blob_support = LargeBlobSupport::kPreferred;
          } else {
            return std::nullopt;
          }
        }
      } else if (extension_name == kExtensionCredBlob) {
        if (!extension.second.is_bytestring()) {
          return std::nullopt;
        }
        request.cred_blob = extension.second.GetBytestring();
      } else if (extension_name == kExtensionMinPINLength) {
        if (!extension.second.is_bool()) {
          return std::nullopt;
        }
        request.min_pin_length_requested = extension.second.GetBool();
      }
    }
  }

  const auto option_it = request_map.find(cbor::Value(7));
  if (option_it != request_map.end()) {
    if (!option_it->second.is_map())
      return std::nullopt;

    const auto& option_map = option_it->second.GetMap();
    if (!IsMakeCredentialOptionMapFormatCorrect(option_map))
      return std::nullopt;

    const auto resident_key_option =
        option_map.find(cbor::Value(kResidentKeyMapKey));
    if (resident_key_option != option_map.end()) {
      request.resident_key_required = resident_key_option->second.GetBool();
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

  const auto pin_auth_it = request_map.find(cbor::Value(8));
  if (pin_auth_it != request_map.end()) {
    if (!pin_auth_it->second.is_bytestring())
      return std::nullopt;

    request.pin_auth = pin_auth_it->second.GetBytestring();
  }

  const auto pin_protocol_it = request_map.find(cbor::Value(9));
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

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    std::string in_client_data_json,
    PublicKeyCredentialRpEntity in_rp,
    PublicKeyCredentialUserEntity in_user,
    PublicKeyCredentialParams in_public_key_credential_params)
    : client_data_json(std::move(in_client_data_json)),
      client_data_hash(fido_parsing_utils::CreateSHA256Hash(client_data_json)),
      rp(std::move(in_rp)),
      user(std::move(in_user)),
      public_key_credential_params(std::move(in_public_key_credential_params)) {
}

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest::CtapMakeCredentialRequest(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    const CtapMakeCredentialRequest& that) = default;

CtapMakeCredentialRequest& CtapMakeCredentialRequest::operator=(
    CtapMakeCredentialRequest&& that) = default;

CtapMakeCredentialRequest::~CtapMakeCredentialRequest() = default;

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapMakeCredentialRequest& request) {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(request.client_data_hash);
  cbor_map[cbor::Value(2)] = AsCBOR(request.rp);
  cbor_map[cbor::Value(3)] = AsCBOR(request.user);
  cbor_map[cbor::Value(4)] = AsCBOR(request.public_key_credential_params);
  if (!request.exclude_list.empty()) {
    cbor::Value::ArrayValue exclude_list_array;
    for (const auto& descriptor : request.exclude_list) {
      exclude_list_array.push_back(AsCBOR(descriptor));
    }
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(exclude_list_array));
  }

  cbor::Value::MapValue extensions;

  if (request.hmac_secret) {
    extensions[cbor::Value(kExtensionHmacSecret)] = cbor::Value(true);
  }

  if (request.prf) {
    cbor::Value::MapValue prf_ext;
    if (request.prf_input) {
      cbor::Value::MapValue eval;
      prf_ext.emplace(kExtensionPRFEval, request.prf_input->ToCBOR());
    }

    extensions.emplace(kExtensionPRF, std::move(prf_ext));
  }

  if (request.large_blob_support != LargeBlobSupport::kNotRequested) {
    cbor::Value::MapValue large_blob_ext;
    large_blob_ext.emplace(
        kExtensionLargeBlobSupport,
        request.large_blob_support == LargeBlobSupport::kRequired
            ? kExtensionLargeBlobSupportRequired
            : kExtensionLargeBlobSupportPreferred);
    extensions.emplace(kExtensionLargeBlob, std::move(large_blob_ext));
  }

  if (request.large_blob_key) {
    extensions[cbor::Value(kExtensionLargeBlobKey)] = cbor::Value(true);
  }

  if (request.cred_protect) {
    extensions.emplace(kExtensionCredProtect,
                       static_cast<int64_t>(*request.cred_protect));
  }

  if (request.cred_blob) {
    extensions.emplace(kExtensionCredBlob, *request.cred_blob);
  }

  if (request.min_pin_length_requested) {
    extensions.emplace(kExtensionMinPINLength, true);
  }

  if (!extensions.empty()) {
    cbor_map[cbor::Value(6)] = cbor::Value(std::move(extensions));
  }

  if (request.pin_auth) {
    cbor_map[cbor::Value(8)] = cbor::Value(*request.pin_auth);
  }

  if (request.pin_protocol) {
    cbor_map[cbor::Value(9)] =
        cbor::Value(static_cast<uint8_t>(*request.pin_protocol));
  }

  cbor::Value::MapValue option_map;

  // Resident keys are not required by default.
  if (request.resident_key_required) {
    option_map[cbor::Value(kResidentKeyMapKey)] =
        cbor::Value(request.resident_key_required);
  }

  // User verification is not required by default.
  if (request.user_verification == UserVerificationRequirement::kRequired) {
    option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(7)] = cbor::Value(std::move(option_map));
  }

  switch (request.attestation_preference) {
    case AttestationConveyancePreference::kEnterpriseIfRPListedOnAuthenticator:
      cbor_map.emplace(10, static_cast<int64_t>(1));
      break;
    case AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
      cbor_map.emplace(10, static_cast<int64_t>(2));
      break;
    default:
      break;
  }

  return std::make_pair(CtapRequestCommand::kAuthenticatorMakeCredential,
                        cbor::Value(std::move(cbor_map)));
}

MakeCredentialOptions::MakeCredentialOptions() = default;
MakeCredentialOptions::~MakeCredentialOptions() = default;
MakeCredentialOptions::MakeCredentialOptions(const MakeCredentialOptions&) =
    default;
MakeCredentialOptions::MakeCredentialOptions(
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria)
    : authenticator_attachment(
          authenticator_selection_criteria.authenticator_attachment),
      resident_key(authenticator_selection_criteria.resident_key),
      user_verification(
          authenticator_selection_criteria.user_verification_requirement) {}
MakeCredentialOptions::MakeCredentialOptions(MakeCredentialOptions&&) = default;
MakeCredentialOptions& MakeCredentialOptions::operator=(
    const MakeCredentialOptions&) = default;
MakeCredentialOptions& MakeCredentialOptions::operator=(
    MakeCredentialOptions&&) = default;

}  // namespace device
