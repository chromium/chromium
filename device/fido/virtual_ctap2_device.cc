// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/virtual_ctap2_device.h"

#include <array>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/apdu/apdu_response.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "crypto/ec_private_key.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/credential_management.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_types.h"
#include "device/fido/large_blob.h"
#include "device/fido/opaque_attestation_statement.h"
#include "device/fido/pin.h"
#include "device/fido/pin_internal.h"
#include "device/fido/public_key.h"
#include "device/fido/virtual_u2f_device.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {

namespace {

constexpr std::array<uint8_t, kAaguidLength> kDeviceAaguid = {
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x01, 0x02, 0x03, 0x04,
     0x05, 0x06, 0x07, 0x08}};
constexpr size_t kMaxCredBlob = 32;
static_assert(
    kMaxCredBlob >= 32,
    "CTAP 2.1 requires at least 32 bytes of credBlob storage if supported");

struct PinUvAuthTokenPermissions {
  uint8_t permissions;
  std::optional<std::string> rp_id;
};

uint8_t GetSupportedPermissionsMask(const VirtualCtap2Device::Config& config) {
  uint8_t permissions =
      static_cast<uint8_t>(pin::Permissions::kMakeCredential) |
      static_cast<uint8_t>(pin::Permissions::kGetAssertion);
  if (config.credential_management_support) {
    permissions |=
        static_cast<uint8_t>(pin::Permissions::kCredentialManagement);
  }
  if (config.bio_enrollment_support) {
    permissions |= static_cast<uint8_t>(pin::Permissions::kBioEnrollment);
  }
  if (config.large_blob_support) {
    permissions |= static_cast<uint8_t>(pin::Permissions::kLargeBlobWrite);
  }
  return permissions;
}

std::vector<uint8_t> ConstructResponse(CtapDeviceResponseCode response_code,
                                       base::span<const uint8_t> data) {
  std::vector<uint8_t> response{base::strict_cast<uint8_t>(response_code)};
  fido_parsing_utils::Append(&response, data);
  return response;
}

// Returns true if the |permissions| parameter requires an explicit permissions
// RPID.
bool PermissionsRequireRPID(uint8_t permissions) {
  return permissions &
             static_cast<uint8_t>(pin::Permissions::kMakeCredential) ||
         permissions & static_cast<uint8_t>(pin::Permissions::kGetAssertion);
}

CtapDeviceResponseCode ExtractPermissions(
    const cbor::Value::MapValue& request_map,
    const VirtualCtap2Device::Config& config,
    PinUvAuthTokenPermissions& out_permissions) {
  const auto permissions_it = request_map.find(
      cbor::Value(static_cast<int>(pin::RequestKey::kPermissions)));
  if (permissions_it == request_map.end() ||
      !permissions_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
  }
  out_permissions.permissions = permissions_it->second.GetUnsigned();
  if (out_permissions.permissions == 0) {
    return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
  }

  DCHECK_EQ(out_permissions.permissions & ~GetSupportedPermissionsMask(config),
            0);

  const auto permissions_rpid_it = request_map.find(
      cbor::Value(static_cast<int>(pin::RequestKey::kPermissionsRPID)));
  if (permissions_rpid_it == request_map.end() &&
      PermissionsRequireRPID(out_permissions.permissions)) {
    return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
  }
  if (permissions_rpid_it != request_map.end()) {
    if (!permissions_rpid_it->second.is_string()) {
      return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
    }
    out_permissions.rp_id = permissions_rpid_it->second.GetString();
  }
  return CtapDeviceResponseCode::kSuccess;
}

void ReturnCtap2Response(
    FidoDevice::DeviceCallback cb,
    CtapDeviceResponseCode response_code,
    std::optional<base::span<const uint8_t>> data = std::nullopt) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(cb),
                     ConstructResponse(response_code,
                                       data.value_or(std::vector<uint8_t>{}))));
}

std::vector<uint8_t> ConstructSignatureBuffer(
    const AuthenticatorData& authenticator_data,
    base::span<const uint8_t, kClientDataHashLength> client_data_hash) {
  std::vector<uint8_t> signature_buffer;
  fido_parsing_utils::Append(&signature_buffer,
                             authenticator_data.SerializeToByteArray());
  fido_parsing_utils::Append(&signature_buffer, client_data_hash);
  return signature_buffer;
}

std::vector<uint8_t> ConstructMakeCredentialResponse(
    std::optional<std::vector<uint8_t>> attestation_certificate,
    base::span<const uint8_t> signature,
    AuthenticatorData authenticator_data,
    bool enterprise_attestation_requested,
    std::optional<LargeBlobSupportType> large_blob_type,
    bool prf_enabled,
    std::optional<std::vector<uint8_t>> prf_results) {
  std::unique_ptr<OpaqueAttestationStatement> attestation_statement;
  if (!signature.empty()) {
    cbor::Value::MapValue attestation_map;
    attestation_map.emplace("alg", -7);
    attestation_map.emplace("sig", fido_parsing_utils::Materialize(signature));

    if (attestation_certificate) {
      cbor::Value::ArrayValue certificate_chain;
      certificate_chain.emplace_back(std::move(*attestation_certificate));
      attestation_map.emplace("x5c", std::move(certificate_chain));
    }

    attestation_statement = std::make_unique<OpaqueAttestationStatement>(
        "packed", cbor::Value(std::move(attestation_map)));
  } else {
    attestation_statement = std::make_unique<OpaqueAttestationStatement>(
        "none", cbor::Value(cbor::Value::MapValue()));
  }

  AuthenticatorMakeCredentialResponse make_credential_response(
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      AttestationObject(std::move(authenticator_data),
                        std::move(attestation_statement)));
  make_credential_response.enterprise_attestation_returned =
      enterprise_attestation_requested;
  make_credential_response.large_blob_type = large_blob_type;
  make_credential_response.prf_enabled = prf_enabled;
  make_credential_response.prf_results = std::move(prf_results);
  return AsCTAPStyleCBORBytes(make_credential_response);
}

std::optional<std::vector<uint8_t>> GetPINBytestring(
    const cbor::Value::MapValue& request,
    pin::RequestKey key) {
  const auto it = request.find(cbor::Value(static_cast<int>(key)));
  if (it == request.end() || !it->second.is_bytestring()) {
    return std::nullopt;
  }
  return it->second.GetBytestring();
}

std::optional<bssl::UniquePtr<EC_POINT>> GetPINKey(
    const cbor::Value::MapValue& request,
    pin::RequestKey map_key) {
  const auto it = request.find(cbor::Value(static_cast<int>(map_key)));
  if (it == request.end() || !it->second.is_map()) {
    return std::nullopt;
  }
  const auto& cose_key = it->second.GetMap();
  auto response = pin::KeyAgreementResponse::ParseFromCOSE(cose_key);
  if (!response) {
    return std::nullopt;
  }

  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  return pin::PointFromKeyAgreementResponse(group.get(), *response).value();
}

// ConfirmPresentedPIN checks whether |encrypted_pin_hash| is a valid proof-of-
// possession of the PIN, given that |shared_key| is the result of the ECDH key
// agreement.
CtapDeviceResponseCode ConfirmPresentedPIN(
    PINUVAuthProtocol pin_protocol,
    VirtualCtap2Device::State* state,
    const std::vector<uint8_t>& shared_key,
    const std::vector<uint8_t>& encrypted_pin_hash) {
  constexpr size_t kPinHashSize = AES_BLOCK_SIZE;
  if (encrypted_pin_hash.empty() ||
      encrypted_pin_hash.size() % kPinHashSize != 0u) {
    return CtapDeviceResponseCode::kCtap2ErrPinInvalid;
  }

  if (state->pin_retries == 0) {
    return CtapDeviceResponseCode::kCtap2ErrPinBlocked;
  }
  if (state->soft_locked) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked;
  }

  state->pin_retries--;
  state->pin_retries_since_insertion++;

  std::vector<uint8_t> pin_hash = pin::ProtocolVersion(pin_protocol)
                                      .Decrypt(shared_key, encrypted_pin_hash);

  uint8_t calculated_pin_hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(state->pin.data()), state->pin.size(),
         calculated_pin_hash);
  static_assert(sizeof(calculated_pin_hash) >= kPinHashSize, "");

  if (state->pin.empty() || pin_hash.size() != kPinHashSize ||
      CRYPTO_memcmp(pin_hash.data(), calculated_pin_hash, kPinHashSize) != 0) {
    if (state->pin_retries == 0) {
      return CtapDeviceResponseCode::kCtap2ErrPinBlocked;
    }
    if (state->pin_retries_since_insertion == 3) {
      state->soft_locked = true;
      return CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked;
    }
    return CtapDeviceResponseCode::kCtap2ErrPinInvalid;
  }

  state->pin_retries = kMaxPinRetries;
  state->uv_retries = kMaxUvRetries;
  state->pin_retries_since_insertion = 0;

  return CtapDeviceResponseCode::kSuccess;
}

// SetPIN sets the current PIN based on the ciphertext in |encrypted_pin|, given
// that |shared_key| is the result of the ECDH key agreement.
CtapDeviceResponseCode SetPIN(
    PINUVAuthProtocol protocol,
    VirtualCtap2Device::State* state,
    const std::vector<uint8_t>& shared_key,
    const std::vector<uint8_t>& encrypted_pin,
    const std::vector<uint8_t>& pin_auth,
    std::optional<base::span<const uint8_t>> current_encrypted_pin_hash) {
  const pin::Protocol& pin_protocol = pin::ProtocolVersion(protocol);
  std::vector<uint8_t> pin_auth_bytes;
  pin_auth_bytes.insert(pin_auth_bytes.begin(), encrypted_pin.begin(),
                        encrypted_pin.end());
  if (current_encrypted_pin_hash) {
    pin_auth_bytes.insert(pin_auth_bytes.end(),
                          current_encrypted_pin_hash->begin(),
                          current_encrypted_pin_hash->end());
  }
  if (!pin_protocol.Verify(shared_key, pin_auth_bytes, pin_auth)) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
  }

  if (encrypted_pin.size() < 64) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
  }

  std::vector<uint8_t> plaintext_pin =
      pin_protocol.Decrypt(shared_key, encrypted_pin);

  size_t padding_len = 0;
  while (padding_len < plaintext_pin.size() &&
         plaintext_pin[plaintext_pin.size() - padding_len - 1] == 0) {
    padding_len++;
  }

  plaintext_pin.resize(plaintext_pin.size() - padding_len);
  if (padding_len == 0 || plaintext_pin.size() < state->min_pin_length ||
      plaintext_pin.size() > 63) {
    return CtapDeviceResponseCode::kCtap2ErrPinPolicyViolation;
  }

  state->pin = std::string(reinterpret_cast<const char*>(plaintext_pin.data()),
                           plaintext_pin.size());
  state->pin_retries = kMaxPinRetries;
  state->uv_retries = kMaxUvRetries;
  state->force_pin_change = false;

  return CtapDeviceResponseCode::kSuccess;
}

// VerifyPINUVAuthToken returns whether |request_map| contains a pinAuth
// parameter mapped to |pin_auth_map_key| that is a valid PIN/UV Auth Protocol
// authentication of |pinauth_bytes|. |pin_protocol_map_key| is the
// |request_map| index for the selected PIN/UV protocol version, which is
// checked against the supported versions from |authenticator_info|.
CtapDeviceResponseCode VerifyPINUVAuthToken(
    const AuthenticatorGetInfoResponse& authenticator_info,
    base::span<const uint8_t> pin_token,
    const cbor::Value::MapValue& request_map,
    const cbor::Value& pin_protocol_map_key,
    const cbor::Value& pin_auth_map_key,
    base::span<const uint8_t> pinauth_bytes) {
  DCHECK(
      authenticator_info.options.client_pin_availability !=
          AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported ||
      (authenticator_info.options.user_verification_availability !=
       AuthenticatorSupportedOptions::UserVerificationAvailability::
           kNotSupported));
  DCHECK(authenticator_info.pin_protocols &&
         !authenticator_info.pin_protocols->empty());

  const auto pin_protocol_it = request_map.find(pin_protocol_map_key);
  if (pin_protocol_it == request_map.end() ||
      !pin_protocol_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  std::optional<PINUVAuthProtocol> protocol =
      ToPINUVAuthProtocol(pin_protocol_it->second.GetUnsigned());
  if (!protocol ||
      !base::Contains(*authenticator_info.pin_protocols, *protocol)) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
  }
  const auto pinauth_it = request_map.find(pin_auth_map_key);
  if (pinauth_it == request_map.end() || !pinauth_it->second.is_bytestring()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  if (!pin::ProtocolVersion(*protocol).Verify(
          pin_token, pinauth_bytes, pinauth_it->second.GetBytestring())) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
  }
  return CtapDeviceResponseCode::kSuccess;
}

// Like AsCBOR(const PublicKeyCredentialRpEntity&), but optionally allows name
// to be INVALID_UTF8.
std::optional<cbor::Value> RpEntityAsCBOR(const PublicKeyCredentialRpEntity& rp,
                                          bool allow_invalid_utf8) {
  if (!allow_invalid_utf8) {
    return AsCBOR(rp);
  }

  cbor::Value::MapValue rp_map;
  rp_map.emplace(kEntityIdMapKey, rp.id);
  if (rp.name) {
    rp_map.emplace(kEntityNameMapKey,
                   cbor::Value::InvalidUTF8StringValueForTesting(*rp.name));
  }
  return cbor::Value(std::move(rp_map));
}

// Like AsCBOR(const PublicKeyCredentialUserEntity&), but optionally allows name
// or displayName to be INVALID_UTF8.
std::optional<cbor::Value> UserEntityAsCBOR(
    const PublicKeyCredentialUserEntity& user,
    bool user_verification,
    bool allow_invalid_utf8) {
  cbor::Value::MapValue user_map;
  user_map.emplace(kEntityIdMapKey, user.id);

  if (user_verification) {
    if (user.name) {
      user_map.emplace(
          kEntityNameMapKey,
          allow_invalid_utf8
              ? cbor::Value::InvalidUTF8StringValueForTesting(*user.name)
              : cbor::Value(*user.name));
    }
    if (user.display_name) {
      user_map.emplace(kDisplayNameMapKey,
                       allow_invalid_utf8
                           ? cbor::Value::InvalidUTF8StringValueForTesting(
                                 *user.display_name)
                           : cbor::Value(*user.display_name));
    }
  }

  return cbor::Value(std::move(user_map));
}

std::vector<uint8_t> WriteCBOR(cbor::Value value,
                               bool allow_invalid_utf8 = false) {
  cbor::Writer::Config config;
  config.allow_invalid_utf8_for_testing = allow_invalid_utf8;
  return *cbor::Writer::Write(std::move(value), std::move(config));
}

std::vector<uint8_t> EncodeGetAssertionResponse(
    const AuthenticatorGetAssertionResponse& response,
    bool allow_invalid_utf8) {
  cbor::Value::MapValue response_map;
  if (response.credential) {
    response_map.emplace(1, AsCBOR(*response.credential));
  }

  response_map.emplace(2, response.authenticator_data.SerializeToByteArray());
  response_map.emplace(3, response.signature);

  if (response.user_entity) {
    response_map.emplace(
        4, *UserEntityAsCBOR(
               *response.user_entity,
               response.authenticator_data.obtained_user_verification(),
               allow_invalid_utf8));
  }
  if (response.num_credentials) {
    response_map.emplace(5, response.num_credentials.value());
  }
  if (response.user_selected) {
    response_map.emplace(6, true);
  }
  if (response.large_blob_key) {
    response_map.emplace(7, cbor::Value(*response.large_blob_key));
  }

  cbor::Value::MapValue unsigned_extension_outputs;
  if (response.hmac_secret) {
    // This is actually the output of the PRF extension because the hmac-secret
    // output is carried in the authenticator data.
    const std::vector<uint8_t>& outputs = *response.hmac_secret;
    cbor::Value::MapValue prf_results;
    if (outputs.size() == 32) {
      prf_results.emplace(kExtensionPRFFirst, std::move(outputs));
    } else {
      CHECK_EQ(outputs.size(), 64u);
      prf_results.emplace(kExtensionPRFFirst,
                          std::vector<uint8_t>(&outputs[0], &outputs[32]));
      prf_results.emplace(
          kExtensionPRFSecond,
          std::vector<uint8_t>(outputs.begin() + 32, outputs.end()));
    }

    cbor::Value::MapValue prf;
    prf.emplace(kExtensionPRFResults, std::move(prf_results));
    unsigned_extension_outputs.emplace(kExtensionPRF, std::move(prf));
  }
  if (response.large_blob_extension) {
    DCHECK(!response.large_blob_written);
    cbor::Value::MapValue large_blob_ext;
    large_blob_ext.emplace(kExtensionLargeBlobBlob,
                           response.large_blob_extension->compressed_data);
    large_blob_ext.emplace(kExtensionLargeBlobOriginalSize,
                           base::checked_cast<int64_t>(
                               response.large_blob_extension->original_size));
    unsigned_extension_outputs.emplace(kExtensionLargeBlob,
                                       std::move(large_blob_ext));
  }
  if (response.large_blob_written) {
    DCHECK(!response.large_blob_extension);
    cbor::Value::MapValue large_blob_ext;
    large_blob_ext.emplace(kExtensionLargeBlobWritten, true);
    unsigned_extension_outputs.emplace(kExtensionLargeBlob,
                                       std::move(large_blob_ext));
  }
  if (!unsigned_extension_outputs.empty()) {
    response_map.emplace(8, cbor::Value(std::move(unsigned_extension_outputs)));
  }

  return WriteCBOR(cbor::Value(std::move(response_map)), allow_invalid_utf8);
}

std::vector<uint8_t> GenerateAndEncryptToken(
    PINUVAuthProtocol pin_protocol,
    base::span<const uint8_t> shared_key,
    base::span<uint8_t, 32> pin_token) {
  RAND_bytes(pin_token.data(), pin_token.size());
  return pin::ProtocolVersion(pin_protocol).Encrypt(shared_key, pin_token);
}

bool CheckCredentialListForExtraKeys(
    base::span<const PublicKeyCredentialDescriptor> creds) {
  if (base::ranges::any_of(
          creds, [](const auto& cred) { return cred.had_other_keys; })) {
    LOG(ERROR) << "A PublicKeyCredentialDescriptor contained unexpected CBOR "
                  "keys. This is believed to trigger bugs in some security "
                  "keys. See crbug.com/1270757.";
    return false;
  }

  return true;
}

std::vector<uint8_t> EvaluateHMAC(
    base::span<const uint8_t> hmac_key,
    const std::array<uint8_t, 32>& hmac_salt1,
    const std::optional<std::array<uint8_t, 32>>& hmac_salt2) {
  uint8_t hmac_result[SHA256_DIGEST_LENGTH];
  unsigned hmac_out_length;
  HMAC(EVP_sha256(), hmac_key.data(), hmac_key.size(), hmac_salt1.data(),
       hmac_salt1.size(), hmac_result, &hmac_out_length);
  CHECK_EQ(hmac_out_length, sizeof(hmac_result));

  std::vector<uint8_t> outputs;
  outputs.insert(outputs.end(), std::begin(hmac_result), std::end(hmac_result));

  if (hmac_salt2) {
    HMAC(EVP_sha256(), hmac_key.data(), hmac_key.size(), hmac_salt2->data(),
         hmac_salt2->size(), hmac_result, &hmac_out_length);
    CHECK_EQ(hmac_out_length, sizeof(hmac_result));
    outputs.insert(outputs.end(), std::begin(hmac_result),
                   std::end(hmac_result));
  }
  return outputs;
}

}  // namespace

VirtualCtap2Device::Config::Config() = default;
VirtualCtap2Device::Config::Config(const Config&) = default;
VirtualCtap2Device::Config& VirtualCtap2Device::Config::operator=(
    const Config&) = default;
VirtualCtap2Device::Config::~Config() = default;

VirtualCtap2Device::RequestState::RequestState() = default;
VirtualCtap2Device::RequestState::~RequestState() = default;

VirtualCtap2Device::VirtualCtap2Device() {
  RegenerateKeyAgreementKey();
  Init({ProtocolVersion::kCtap2});
}

VirtualCtap2Device::VirtualCtap2Device(scoped_refptr<State> state,
                                       const Config& config)
    : VirtualFidoDevice(std::move(state)), config_(config) {
  RegenerateKeyAgreementKey();

  std::vector<ProtocolVersion> versions = {ProtocolVersion::kCtap2};
  if (config.u2f_support) {
    versions.emplace_back(ProtocolVersion::kU2f);
    u2f_device_ = std::make_unique<VirtualU2fDevice>(NewReferenceToState());
  }
  Init(std::move(versions));

  AuthenticatorSupportedOptions options;
  bool options_updated = false;
  if (config.pin_support) {
    options_updated = true;

    if (mutable_state()->pin.empty()) {
      options.client_pin_availability = AuthenticatorSupportedOptions::
          ClientPinAvailability::kSupportedButPinNotSet;
    } else {
      options.client_pin_availability = AuthenticatorSupportedOptions::
          ClientPinAvailability::kSupportedAndPinSet;
    }
  }

  if (config.internal_uv_support) {
    options_updated = true;
    if (mutable_state()->fingerprints_enrolled) {
      options.user_verification_availability = AuthenticatorSupportedOptions::
          UserVerificationAvailability::kSupportedAndConfigured;
    } else {
      options.user_verification_availability = AuthenticatorSupportedOptions::
          UserVerificationAvailability::kSupportedButNotConfigured;
    }
  }

  options.supports_pin_uv_auth_token = config.pin_uv_auth_token_support;
  DCHECK(!options.supports_pin_uv_auth_token ||
         SupportsAtLeast(Ctap2Version::kCtap2_1));

  if (config.resident_key_support) {
    options_updated = true;
    options.supports_resident_key = true;
  }

  if (config.credential_management_support) {
    options_updated = true;
    options.supports_credential_management = true;
    options.supports_credential_management_preview = true;
  }

  if (config.bio_enrollment_support) {
    options_updated = true;
    if (mutable_state()->bio_enrollment_provisioned) {
      options.bio_enrollment_availability = AuthenticatorSupportedOptions::
          BioEnrollmentAvailability::kSupportedAndProvisioned;
    } else {
      options.bio_enrollment_availability = AuthenticatorSupportedOptions::
          BioEnrollmentAvailability::kSupportedButUnprovisioned;
    }
  }

  if (config.bio_enrollment_preview_support) {
    options_updated = true;
    if (mutable_state()->bio_enrollment_provisioned) {
      options.bio_enrollment_availability_preview =
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kSupportedAndProvisioned;
    } else {
      options.bio_enrollment_availability_preview =
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kSupportedButUnprovisioned;
    }
  }

  if (config.is_platform_authenticator) {
    options_updated = true;
    options.is_platform_device =
        AuthenticatorSupportedOptions::PlatformDevice::kYes;
  }

  if (config.cred_protect_support) {
    options_updated = true;
    options.default_cred_protect = config.default_cred_protect;
  }

  if (config.support_enterprise_attestation) {
    options_updated = true;
    options.enterprise_attestation = true;
  }

  if (config.large_blob_support) {
    DCHECK(config.resident_key_support);
    DCHECK(SupportsAtLeast(Ctap2Version::kCtap2_1));
    DCHECK(!config.large_blob_extension_support);
    DCHECK((!config.pin_support && !config.internal_uv_support) ||
           config.pin_uv_auth_token_support)
        << "PinUvAuthToken support is required to write large blobs for "
           "uv-enabled authenticators";
    options_updated = true;
    options.large_blob_type = LargeBlobSupportType::kKey;
    device_info_->max_serialized_large_blob_array =
        config.available_large_blob_storage;
  }

  if (config.always_uv) {
    DCHECK(config.pin_support || config.internal_uv_support);
    options_updated = true;
    options.always_uv = true;
  }

  if (config.allow_non_resident_credential_creation_without_uv) {
    options.make_cred_uv_not_required = true;
    options_updated = true;
  }

  if (options_updated) {
    device_info_->options = std::move(options);
  }

  std::vector<std::string> extensions;

  if (config.cred_protect_support) {
    extensions.emplace_back(device::kExtensionCredProtect);
  }

  if (config.hmac_secret_support) {
    extensions.emplace_back(device::kExtensionHmacSecret);
  }

  if (config.prf_support) {
    DCHECK(!config.hmac_secret_support);
    DCHECK(config.internal_account_chooser);
    extensions.emplace_back(device::kExtensionPRF);
  }

  if (config.cred_blob_support) {
    extensions.emplace_back(device::kExtensionCredBlob);
    device_info_->options.max_cred_blob_length = kMaxCredBlob;
  }

  if (config.large_blob_support) {
    DCHECK(!config.large_blob_extension_support);
    extensions.emplace_back(device::kExtensionLargeBlobKey);
  }

  if (config.large_blob_extension_support) {
    DCHECK(!config.large_blob_support);
    extensions.emplace_back(device::kExtensionLargeBlob);
  }

  if (config.min_pin_length_extension_support) {
    extensions.emplace_back(device::kExtensionMinPINLength);
  }

  if (!extensions.empty()) {
    device_info_->extensions.emplace(std::move(extensions));
  }

  if (config.max_credential_count_in_list > 0) {
    device_info_->max_credential_count_in_list =
        config.max_credential_count_in_list;
  }

  if (config.max_credential_id_length > 0) {
    device_info_->max_credential_id_length = config.max_credential_id_length;
  }

  if (!config.advertised_algorithms.empty()) {
    device_info_->algorithms.emplace();
    base::ranges::transform(
        config.advertised_algorithms,
        std::back_inserter(device_info_->algorithms.value()),
        [](auto algo) { return static_cast<int32_t>(algo); });
  }

  if (config.pin_support || config.pin_uv_auth_token_support) {
    device_info_->pin_protocols =
        base::flat_set<PINUVAuthProtocol>{config.pin_protocol};
  }

  if (config.resident_key_support && SupportsAtLeast(Ctap2Version::kCtap2_1)) {
    device_info_->remaining_discoverable_credentials =
        remaining_resident_credentials();
  }

  if (config.min_pin_length_support) {
    DCHECK(config.pin_support);
    DCHECK(config.pin_uv_auth_token_support);
    device_info_->min_pin_length = mutable_state()->min_pin_length;
    device_info_->force_pin_change = mutable_state()->force_pin_change;
  }

  if (!config.transports_in_get_info.empty()) {
    device_info_->transports = config.transports_in_get_info;
  }
}

VirtualCtap2Device::~VirtualCtap2Device() = default;

void VirtualCtap2Device::SetPin(std::string pin) {
  DCHECK_NE(
      device_info_->options.client_pin_availability,
      AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported);
  DCHECK_GE(pin.size(), mutable_state()->min_pin_length);
  mutable_state()->pin = std::move(pin);
  mutable_state()->pin_retries = device::kMaxPinRetries;
  device_info_->options.client_pin_availability =
      AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet;
}

void VirtualCtap2Device::SetForcePinChange(bool force_pin_change) {
  DCHECK(config_.min_pin_length_support);
  mutable_state()->force_pin_change = force_pin_change;
  device_info_->force_pin_change = force_pin_change;
}

void VirtualCtap2Device::SetMinPinLength(uint32_t min_pin_length) {
  DCHECK(config_.min_pin_length_support);
  mutable_state()->min_pin_length = min_pin_length;
  device_info_->min_pin_length = min_pin_length;
}

// If there is a pending operation, resolve it with a cancel status. Operations
// can be left pending if |SimulatePress()| returns false.
void VirtualCtap2Device::Cancel(CancelToken) {
  if (mutable_state()->transact_callback) {
    ReturnCtap2Response(std::move(mutable_state()->transact_callback),
                        mutable_state()->cancel_response_code);
  }
}

FidoDevice::CancelToken VirtualCtap2Device::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback cb) {
  if (command.empty()) {
    ReturnCtap2Response(std::move(cb), CtapDeviceResponseCode::kCtap2ErrOther);
    return 0;
  }

  auto cmd_type = command[0];
  // The CTAP2 commands start at one, so a "command" of zero indicates that this
  // is a U2F message.
  if (cmd_type == 0 && config_.u2f_support) {
    if (config_.always_uv && !mutable_state()->fingerprints_enrolled) {
      // The U2F_REGISTER and U2F_AUTHENTICATE commands MUST immediately fail
      // and return SW_COMMAND_NOT_ALLOWED if the alwaysUv option is true and
      // the device is not protected by a built-in user verification method.
      // Have the authenticator will just fail all u2f requests for simplicity.
      NOTREACHED();
    }
    u2f_device_->DeviceTransact(std::move(command), std::move(cb));
    return 0;
  }

  const CtapRequestCommand ctap_command =
      static_cast<CtapRequestCommand>(cmd_type);
  if (config_.override_response_map.contains(ctap_command)) {
    ReturnCtap2Response(std::move(cb),
                        config_.override_response_map.at(ctap_command), {});
    return 0;
  }

  const auto request_bytes = base::make_span(command).subspan(1);
  CtapDeviceResponseCode response_code =
      CtapDeviceResponseCode::kCtap1ErrInvalidCommand;
  std::vector<uint8_t> response_data;

  mutable_state()->transact_callback = std::move(cb);

  switch (ctap_command) {
    case CtapRequestCommand::kAuthenticatorGetInfo:
      if (!request_bytes.empty()) {
        ReturnCtap2Response(std::move(mutable_state()->transact_callback),
                            CtapDeviceResponseCode::kCtap2ErrOther);
        return 0;
      }

      response_code = OnAuthenticatorGetInfo(&response_data);
      break;
    case CtapRequestCommand::kAuthenticatorMakeCredential: {
      auto opt_response_code = OnMakeCredential(request_bytes, &response_data);
      if (!opt_response_code) {
        // Simulate timeout due to unresponded User Presence check.
        return 0;
      }
      response_code = *opt_response_code;
      break;
    }
    case CtapRequestCommand::kAuthenticatorGetAssertion: {
      auto opt_response_code = OnGetAssertion(request_bytes, &response_data);
      if (!opt_response_code) {
        // Simulate timeout due to unresponded User Presence check.
        return 0;
      }
      response_code = *opt_response_code;
      break;
    }
    case CtapRequestCommand::kAuthenticatorGetNextAssertion:
      response_code = OnGetNextAssertion(request_bytes, &response_data);
      break;
    case CtapRequestCommand::kAuthenticatorClientPin: {
      auto opt_response_code = OnPINCommand(request_bytes, &response_data);
      if (!opt_response_code) {
        // Simulate timeout due to unresponded User Presence check.
        return 0;
      }
      response_code = *opt_response_code;
      break;
    }
    case CtapRequestCommand::kAuthenticatorCredentialManagement:
    case CtapRequestCommand::kAuthenticatorCredentialManagementPreview:
      response_code = OnCredentialManagement(request_bytes, &response_data);
      break;
    case CtapRequestCommand::kAuthenticatorBioEnrollment:
    case CtapRequestCommand::kAuthenticatorBioEnrollmentPreview:
      response_code = OnBioEnrollment(request_bytes, &response_data);
      break;
    case CtapRequestCommand::kAuthenticatorSelection:
      DCHECK(SupportsAtLeast(Ctap2Version::kCtap2_1));
      if (!SimulatePress()) {
        // Simulate timeout due to unresponded User Presence check.
        return 0;
      }
      response_code = CtapDeviceResponseCode::kSuccess;
      break;
    case CtapRequestCommand::kAuthenticatorLargeBlobs:
      response_code = OnLargeBlobs(request_bytes, &response_data);
      break;
    default:
      break;
  }

  // Call |callback| via the |MessageLoop| because |AuthenticatorImpl| doesn't
  // support callback hairpinning.
  ReturnCtap2Response(std::move(mutable_state()->transact_callback),
                      response_code, std::move(response_data));
  return 0;
}

base::WeakPtr<FidoDevice> VirtualCtap2Device::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void VirtualCtap2Device::Init(std::vector<ProtocolVersion> versions) {
  device_info_ = AuthenticatorGetInfoResponse(
      std::move(versions), config_.ctap2_versions, kDeviceAaguid);
}

std::optional<CtapDeviceResponseCode> VirtualCtap2Device::CheckUserVerification(
    CheckUserVerificationMode mode,
    const AuthenticatorGetInfoResponse& authenticator_info,
    const std::string& rp_id,
    const std::optional<std::vector<uint8_t>>& pin_auth,
    const std::optional<PINUVAuthProtocol>& pin_protocol,
    base::span<const uint8_t> client_data_hash,
    UserVerificationRequirement user_verification,
    bool user_presence_required,
    bool* out_user_verified) {
  const AuthenticatorSupportedOptions& options = authenticator_info.options;

  // crbug.com/1216155#4 asserts that once an authenticator sees a request with
  // an RP ID that differs from the PUAT, it can assume that the transaction is
  // complete and discard the PUAT.
  if (mutable_state()->pin_uv_token_rpid &&
      rp_id != mutable_state()->pin_uv_token_rpid) {
    // Invalidate the PIN token.
    memset(mutable_state()->pin_token, 0xff,
           sizeof(mutable_state()->pin_token));
    mutable_state()->pin_uv_token_permissions = 0;
    mutable_state()->pin_uv_token_rpid.reset();
  }

  // The following quotes are from the CTAP2 spec:

  // 1. "If authenticator supports clientPin and platform sends a zero length
  // pinAuth, wait for user touch and then return either CTAP2_ERR_PIN_NOT_SET
  // if pin is not set or CTAP2_ERR_PIN_INVALID if pin has been set."
  const bool supports_pin =
      options.client_pin_availability !=
      AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported;
  if (supports_pin && pin_auth && pin_auth->empty()) {
    if (!SimulatePress()) {
      return std::nullopt;
    }

    switch (options.client_pin_availability) {
      case AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedAndPinSet:
        return CtapDeviceResponseCode::kCtap2ErrPinInvalid;
      case AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedButPinNotSet:
        return CtapDeviceResponseCode::kCtap2ErrPinNotSet;
      case AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported:
        NOTREACHED();
    }
  }
  const std::optional<base::flat_set<PINUVAuthProtocol>>&
      supported_pin_protocols = authenticator_info.pin_protocols;
  DCHECK(!supports_pin ||
         (supported_pin_protocols && !supported_pin_protocols->empty()));

  // 2. "If authenticator supports clientPin and pinAuth parameter is present
  // and the pinProtocol is not supported, return CTAP2_ERR_PIN_AUTH_INVALID
  // error."
  if (supports_pin && pin_auth &&
      (!pin_protocol ||
       !base::Contains(*supported_pin_protocols, *pin_protocol))) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
  }

  const bool can_do_uv =
      options.user_verification_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedAndConfigured ||
      options.client_pin_availability ==
          AuthenticatorSupportedOptions::ClientPinAvailability::
              kSupportedAndPinSet;

  // (CTAP2.1) 5. "If the alwaysUv option ID is present and true"
  if (options.always_uv &&
      (user_presence_required || config_.always_uv_for_up_false)) {
    // 5.1 "If the authenticator is not protected by some form of user
    // verification:"
    if (!can_do_uv) {
      // 5.1.1 "If the clientPin option ID is present: (clientPin is supported)"
      if (options.client_pin_availability !=
          AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported) {
        return CtapDeviceResponseCode::kCtap2ErrPinRequired;
      } else {
        return CtapDeviceResponseCode::kCtap2ErrOperationDenied;
      }
    }
    // 5.4 "If the "uv" option is false and the authenticator supports a
    // built-in user verification method, and the user verification method is
    // enabled then:"
    if (user_verification == UserVerificationRequirement::kDiscouraged &&
        options.user_verification_availability ==
            AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedAndConfigured) {
      user_verification = UserVerificationRequirement::kRequired;
    }
    // 5.5 "If the clientPin option ID is present and the pinUvAuthParam
    // parameter is not present, then end the operation by returning
    // CTAP2_ERR_PIN_REQUIRED."
    if (options.client_pin_availability !=
            AuthenticatorSupportedOptions::ClientPinAvailability::
                kNotSupported &&
        !pin_auth) {
      return CtapDeviceResponseCode::kCtap2ErrPinRequired;
    }
  }

  // 3. "If authenticator is not protected by some form of user verification and
  // platform has set "uv" or pinAuth to get the user verification, return
  // CTAP2_ERR_INVALID_OPTION."
  if (!can_do_uv &&
      (user_verification == UserVerificationRequirement::kRequired ||
       pin_auth)) {
    return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
  }

  // "If authenticator is protected by some form of user verification:"
  bool uv = false;
  if (can_do_uv) {
    // "If the request is passed with "uv" option, use built-in user
    // verification method and verify the user."
    if (user_verification == UserVerificationRequirement::kRequired) {
      if (options.user_verification_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedAndConfigured) {
        if (!SimulatePress()) {
          return std::nullopt;
        }

        if (!config_.user_verification_succeeds) {
          if (mode != CheckUserVerificationMode::kGetAssertion) {
            return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
          }
          return CtapDeviceResponseCode::kCtap2ErrOperationDenied;
        }
        uv = true;
      } else {
        // UV was requested, but either not supported or not configured.
        return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
      }
    }

    // "If pinUvAuthParam parameter is present and pinUvAuthProtocol is 1".
    if (pin_auth && (options.client_pin_availability ==
                         AuthenticatorSupportedOptions::ClientPinAvailability::
                             kSupportedAndPinSet ||
                     options.supports_pin_uv_auth_token)) {
      DCHECK(pin_protocol);

      if (options.supports_pin_uv_auth_token) {
        // "Verify that the pinUvAuthToken has the {mc,ga} permission, if not,
        // return CTAP2_ERR_PIN_AUTH_INVALID."
        auto permission = mode == CheckUserVerificationMode::kGetAssertion
                              ? pin::Permissions::kGetAssertion
                              : pin::Permissions::kMakeCredential;
        if (!(mutable_state()->pin_uv_token_permissions &
              static_cast<uint8_t>(permission))) {
          NOTREACHED() << "PIN missing mc / ga permission";
        }

        // "If the pinUvAuthToken has a permissions RPID associated and it
        // does not match the RPID in this request, return
        // CTAP2_ERR_PIN_AUTH_INVALID."
        if (mutable_state()->pin_uv_token_rpid &&
            mutable_state()->pin_uv_token_rpid != rp_id) {
          return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
        }

        // "If the pinUvAuthToken does not have a permissions RPID associated,
        // associate the request RPID as permissions RPID."
        if (!mutable_state()->pin_uv_token_rpid) {
          mutable_state()->pin_uv_token_rpid = rp_id;
        }
      }

      // Verify pinUvAuthParam.
      if (!pin::ProtocolVersion(*pin_protocol)
               .Verify(mutable_state()->pin_token, client_data_hash,
                       *pin_auth)) {
        return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
      }

      uv = true;
    }

    if (mode == CheckUserVerificationMode::kMakeCredential && !uv) {
      return CtapDeviceResponseCode::kCtap2ErrPinRequired;
    }
  }

  *out_user_verified = uv;
  return CtapDeviceResponseCode::kSuccess;
}

std::optional<CtapDeviceResponseCode> VirtualCtap2Device::OnMakeCredential(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  request_state_.Reset();

  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    DLOG(ERROR) << "Incorrectly formatted MakeCredential request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }

  CtapMakeCredentialRequest::ParseOpts parse_opts;
  parse_opts.reject_all_extensions = config_.reject_all_extensions;
  auto opt_request =
      CtapMakeCredentialRequest::Parse(cbor_request->GetMap(), parse_opts);
  if (!opt_request) {
    DLOG(ERROR) << "Incorrectly formatted MakeCredential request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }
  CtapMakeCredentialRequest request = std::move(*opt_request);

  mutable_state()->exclude_list_history.push_back(request.exclude_list);

  bool user_verified = false;
  const CheckUserVerificationMode check_uv_mode =
      (!request.resident_key_required && !request.pin_auth &&
       request.user_verification == UserVerificationRequirement::kDiscouraged &&
       device_info_->options.make_cred_uv_not_required)
          ? CheckUserVerificationMode::kMakeCredentialUvNotRequired
          : CheckUserVerificationMode::kMakeCredential;
  const std::optional<CtapDeviceResponseCode> uv_error = CheckUserVerification(
      check_uv_mode, *device_info_, request.rp.id, request.pin_auth,
      request.pin_protocol, request.client_data_hash, request.user_verification,
      /*user_presence_required=*/true, &user_verified);
  if (uv_error != CtapDeviceResponseCode::kSuccess) {
    return uv_error;
  }

  // 6. Check for already registered credentials.
  const auto rp_id_hash = fido_parsing_utils::CreateSHA256Hash(request.rp.id);
  if ((config_.reject_large_allow_and_exclude_lists &&
       request.exclude_list.size() > 1) ||
      (config_.max_credential_count_in_list &&
       request.exclude_list.size() > config_.max_credential_count_in_list)) {
    return CtapDeviceResponseCode::kCtap2ErrLimitExceeded;
  }

  if (!CheckCredentialListForExtraKeys(request.exclude_list)) {
    return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
  }

  for (const auto& excluded_credential : request.exclude_list) {
    if (0 < config_.max_credential_id_length &&
        config_.max_credential_id_length < excluded_credential.id.size()) {
      return CtapDeviceResponseCode::kCtap2ErrLimitExceeded;
    }
    const RegistrationData* found =
        FindRegistrationData(excluded_credential.id, rp_id_hash);
    if (found) {
      if (found->protection == device::CredProtect::kUVRequired &&
          !user_verified) {
        // Cannot disclose the existence of this credential without UV. If
        // a credentials ends up being created it'll overwrite this one.
        continue;
      }
      if (!SimulatePress()) {
        return std::nullopt;
      }
      return CtapDeviceResponseCode::kCtap2ErrCredentialExcluded;
    }
  }

  // Step 7.
  std::unique_ptr<PrivateKey> private_key;
  for (const auto& param :
       request.public_key_credential_params.public_key_credential_params()) {
    const bool advertised =
        base::Contains(config_.advertised_algorithms, param.algorithm,
                       [](auto algo) { return static_cast<int32_t>(algo); });
    if (!advertised && !config_.advertised_algorithms.empty()) {
      continue;
    }

    switch (param.algorithm) {
      case static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256):
        private_key = PrivateKey::FreshP256Key();
        break;
      case static_cast<int32_t>(CoseAlgorithmIdentifier::kRs256):
        private_key = PrivateKey::FreshRSAKey();
        break;
      case static_cast<int32_t>(CoseAlgorithmIdentifier::kEdDSA):
        private_key = PrivateKey::FreshEd25519Key();
        break;
      case static_cast<int32_t>(CoseAlgorithmIdentifier::kInvalidForTesting):
        if (!advertised) {
          // Uniquely, the kInvalidForTesting algorithm has to be explicitly
          // enabled. Setting an empty |advertised_algorithms| doesn't do it.
          continue;
        }
        private_key = PrivateKey::FreshInvalidForTestingKey();
        break;
    }
    break;
  }

  if (!private_key) {
    DLOG(ERROR) << "Virtual CTAP2 device does not support any public-key "
                   "algorithm listed in the request";
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithm;
  }
  std::unique_ptr<PublicKey> public_key(private_key->GetPublicKey());

  // Step 8.
  if ((request.resident_key_required &&
       !device_info_->options.supports_resident_key) ||
      !device_info_->options.supports_user_presence) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  // Step 10. Simulate a press unless the user has been verified by internal
  // user verification.
  if ((!user_verified || request.user_verification ==
                             UserVerificationRequirement::kDiscouraged) &&
      !SimulatePress()) {
    return std::nullopt;
  }

  // Our key handles are simple hashes of the public key.
  const auto key_handle = crypto::SHA256Hash(public_key->cose_key_bytes);

  std::optional<cbor::Value> extensions;
  cbor::Value::MapValue extensions_map;
  if (request.hmac_secret) {
    if (!config_.hmac_secret_support) {
      // Should not have been sent. Authenticators will normally ignore unknown
      // extensions but Chromium should not make this mistake.
      DLOG(ERROR)
          << "Rejecting makeCredential due to unexpected hmac_secret extension";
      return CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension;
    }
    extensions_map.emplace(cbor::Value(kExtensionHmacSecret),
                           cbor::Value(true));
  }

  const bool prf_enabled = request.prf;
  CHECK(!prf_enabled || config_.prf_support);

  CredProtect cred_protect = config_.default_cred_protect;
  if (request.cred_protect) {
    cred_protect = *request.cred_protect;
  }
  if (config_.force_cred_protect) {
    cred_protect = *config_.force_cred_protect;
  }

  if (request.cred_protect ||
      cred_protect != device::CredProtect::kUVOptional) {
    extensions_map.emplace(cbor::Value(kExtensionCredProtect),
                           cbor::Value(static_cast<int64_t>(cred_protect)));
  }

  std::optional<LargeBlobSupportType> supports_large_blob;
  if (request.large_blob_key) {
    if (!config_.large_blob_support) {
      DLOG(ERROR) << "Rejecting makeCredential due to unexpected largeBlobKey "
                     "extension";
      return CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension;
    }
    if (!request.resident_key_required) {
      DLOG(ERROR)
          << "largeBlobKey is not supported for non resident credentials";
      return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
    }
    supports_large_blob = LargeBlobSupportType::kKey;
  }

  if (request.large_blob_support != LargeBlobSupport::kNotRequested) {
    DCHECK(!request.large_blob_key);
    DCHECK(config_.large_blob_extension_support);
    DCHECK(!supports_large_blob.has_value());
    if (*config_.large_blob_extension_support) {
      supports_large_blob = LargeBlobSupportType::kExtension;
    } else if (request.large_blob_support == LargeBlobSupport::kRequired) {
      return CtapDeviceResponseCode::kCtap2ErrLargeBlobStorageFull;
    }
  }

  if (request.cred_blob) {
    if (!config_.cred_blob_support) {
      DLOG(ERROR) << "Rejecting makeCredential due to unexpected credBlob "
                     "extension";
      return CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension;
    }
    if (request.cred_blob->size() > kMaxCredBlob) {
      DLOG(ERROR) << "Rejecting makeCredential because credBlob is too large: "
                  << request.cred_blob->size();
      // This is stricter than the spec requires because Chromium should not
      // send credBlob requests that will be rejected. But the spec says that
      // an authenticator should report credBlob=false in this case.
      return CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension;
    }
    extensions_map.emplace(kExtensionCredBlob, true);
  }

  if (request.min_pin_length_requested) {
    if (!config_.min_pin_length_extension_support) {
      DLOG(ERROR) << "Rejecting makeCredential due to unexpected minPinLength "
                     "extension";
      return CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension;
    }
    extensions_map.emplace(kExtensionMinPINLength,
                           static_cast<int>(mutable_state()->min_pin_length));
  }

  if (config_.add_extra_extension) {
    extensions_map.emplace(cbor::Value("unsolicited"), cbor::Value(42));
  }

  if (!extensions_map.empty()) {
    extensions = cbor::Value(std::move(extensions_map));
  }

  AuthenticatorData authenticator_data(
      rp_id_hash, !mutable_state()->unset_up_bit,
      mutable_state()->unset_uv_bit ? false : user_verified,
      mutable_state()->default_backup_eligibility,
      mutable_state()->default_backup_state,
      /*sign_counter=*/01ul,
      ConstructAttestedCredentialData(key_handle, std::move(public_key)),
      std::move(extensions));

  std::vector<uint8_t> sign_buffer =
      ConstructSignatureBuffer(authenticator_data, request.client_data_hash);

  // Sign with attestation key.
  // Note: Non-deterministic, you need to mock this out if you rely on
  // deterministic behavior.
  std::vector<uint8_t> sig;
  if (!config_.none_attestation) {
    std::unique_ptr<crypto::ECPrivateKey> attestation_private_key =
        crypto::ECPrivateKey::CreateFromPrivateKeyInfo(GetAttestationKey());
    if (mutable_state()->ctap2_invalid_signature) {
      sig = {0x00};
    } else {
      bool status =
          Sign(attestation_private_key.get(), std::move(sign_buffer), &sig);
      DCHECK(status);
    }
  }

  std::optional<std::vector<uint8_t>> attestation_cert;
  bool enterprise_attestation_requested = false;
  if (!config_.none_attestation && !mutable_state()->self_attestation) {
    if (config_.support_enterprise_attestation) {
      switch (request.attestation_preference) {
        case AttestationConveyancePreference::
            kEnterpriseIfRPListedOnAuthenticator:
          if (base::Contains(config_.enterprise_attestation_rps,
                             request.rp.id)) {
            enterprise_attestation_requested = true;
          }
          break;
        case AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
          enterprise_attestation_requested = true;
          break;
        default:
          enterprise_attestation_requested = false;
      }
    }
    if (config_.always_return_enterprise_attestation) {
      enterprise_attestation_requested = true;
    }
    attestation_cert = GenerateAttestationCertificate(
        enterprise_attestation_requested,
        config_.include_transports_in_attestation_certificate);
    if (!attestation_cert) {
      DLOG(ERROR) << "Failed to generate attestation certificate.";
      return CtapDeviceResponseCode::kCtap2ErrOther;
    }
  }

  RegistrationData registration(std::move(private_key), rp_id_hash,
                                /*counter=*/1);
  if (request.resident_key_required) {
    // If there's already a registration for this RP and user ID, delete it.
    for (const auto& reg : mutable_state()->registrations) {
      if (reg.second.is_resident &&
          rp_id_hash == reg.second.application_parameter &&
          reg.second.user->id == request.user.id) {
        std::vector<uint8_t> cred_id = reg.first;
        mutable_state()->registrations.erase(reg.first);
        mutable_state()->NotifyCredentialDeleted(cred_id);
        break;
      }
    }

    if (remaining_resident_credentials() == 0) {
      return CtapDeviceResponseCode::kCtap2ErrKeyStoreFull;
    }

    // Simulate some security keys that return an error if user.displayName is
    // empty.
    if (request.user.display_name && request.user.display_name->empty() &&
        config_.reject_empty_display_name) {
      return CtapDeviceResponseCode::kCtap1ErrInvalidLength;
    }

    // Simulate iPhones returning an error if user.displayName is missing.
    if (!request.user.display_name && config_.reject_missing_display_name) {
      return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
    }

    registration.is_resident = true;
  }
  registration.backup_eligible = mutable_state()->default_backup_eligibility;
  registration.backup_state = mutable_state()->default_backup_state;
  registration.user = request.user;
  registration.rp = request.rp;
  registration.protection = cred_protect;
  registration.cred_blob = std::move(request.cred_blob);

  std::optional<std::vector<uint8_t>> prf_results;
  if (request.hmac_secret || prf_enabled) {
    registration.hmac_key.emplace();
    RAND_bytes(registration.hmac_key->first.data(),
               registration.hmac_key->first.size());
    RAND_bytes(registration.hmac_key->second.data(),
               registration.hmac_key->second.size());
    if (request.prf_input) {
      const std::array<uint8_t, 32>& hmac_key =
          user_verified ? registration.hmac_key->second
                        : registration.hmac_key->first;
      prf_results = EvaluateHMAC(hmac_key, request.prf_input->salt1,
                                 request.prf_input->salt2);
    }
  }

  if (request.large_blob_key) {
    registration.large_blob_key.emplace();
    RAND_bytes(registration.large_blob_key->data(),
               registration.large_blob_key->size());
  }

  StoreNewKey(key_handle, std::move(registration));

  *response = ConstructMakeCredentialResponse(
      std::move(attestation_cert), sig, std::move(authenticator_data),
      enterprise_attestation_requested, supports_large_blob, prf_enabled,
      std::move(prf_results));
  return CtapDeviceResponseCode::kSuccess;
}

std::optional<CtapDeviceResponseCode> VirtualCtap2Device::OnGetAssertion(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  request_state_.Reset();

  // Step numbers in this function refer to
  // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#authenticatorGetAssertion
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    DLOG(ERROR) << "Incorrectly formatted MakeCredential request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }

  const auto& request_map = cbor_request->GetMap();
  CtapGetAssertionRequest::ParseOpts parse_opts;
  parse_opts.reject_all_extensions = config_.reject_all_extensions;
  auto opt_request = CtapGetAssertionRequest::Parse(request_map, parse_opts);
  if (!opt_request) {
    DLOG(ERROR) << "Incorrectly formatted GetAssertion request.";
    return CtapDeviceResponseCode::kCtap2ErrOther;
  }
  CtapGetAssertionRequest request = std::move(*opt_request);

  mutable_state()->allow_list_history.push_back(request.allow_list);

  bool user_verified;
  const std::optional<CtapDeviceResponseCode> uv_error = CheckUserVerification(
      CheckUserVerificationMode::kGetAssertion, *device_info_, request.rp_id,
      request.pin_auth, request.pin_protocol, request.client_data_hash,
      request.user_verification, request.user_presence_required,
      &user_verified);
  if (uv_error != CtapDeviceResponseCode::kSuccess) {
    return uv_error;
  }

  if (!config_.resident_key_support && request.allow_list.empty()) {
    return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
  }

  const auto rp_id_hash = fido_parsing_utils::CreateSHA256Hash(request.rp_id);

  std::vector<std::pair<base::span<const uint8_t>, RegistrationData*>>
      found_registrations;

  if (!request.user_presence_required &&
      config_.reject_silent_authentication_requests) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  if ((config_.reject_large_allow_and_exclude_lists &&
       request.allow_list.size() > 1) ||
      (config_.max_credential_count_in_list &&
       request.allow_list.size() > config_.max_credential_count_in_list)) {
    return CtapDeviceResponseCode::kCtap2ErrLimitExceeded;
  }

  if (!CheckCredentialListForExtraKeys(request.allow_list)) {
    return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
  }

  for (const auto& allowed_credential : request.allow_list) {
    if (0 < config_.max_credential_id_length &&
        config_.max_credential_id_length < allowed_credential.id.size()) {
      return CtapDeviceResponseCode::kCtap2ErrLimitExceeded;
    }
    RegistrationData* registration =
        FindRegistrationData(allowed_credential.id, rp_id_hash);
    if (registration &&
        !(registration->is_u2f && config_.ignore_u2f_credentials)) {
      found_registrations.emplace_back(allowed_credential.id, registration);
      break;
    }
  }

  // CTAP 2.1 prohibits an empty (but present) allow_list. In CTAP 2.0, it is
  // technically permissible to send an empty allow_list when asking for
  // discoverable credentials, but some authenticators in practice don't take it
  // that way. Thus this code mirrors that to better reflect reality.
  if (!base::Contains(request_map, cbor::Value(3))) {
    DCHECK(config_.resident_key_support);
    for (auto& registration : mutable_state()->registrations) {
      if (registration.second.is_resident &&
          registration.second.application_parameter == rp_id_hash) {
        DCHECK(!registration.second.is_u2f);
        found_registrations.emplace_back(registration.first,
                                         &registration.second);
      }
    }
  }

  // Enforce credProtect semantics.
  found_registrations.erase(
      std::remove_if(
          found_registrations.begin(), found_registrations.end(),
          [user_verified, &request](
              const std::pair<base::span<const uint8_t>, RegistrationData*>&
                  candidate) -> bool {
            switch (candidate.second->protection) {
              case CredProtect::kUVOptional:
                return false;
              case CredProtect::kUVOrCredIDRequired:
                return request.allow_list.empty() && !user_verified;
              case CredProtect::kUVRequired:
                return !user_verified;
            }
          }),
      found_registrations.end());

  if (config_.return_immediate_invalid_credential_error &&
      found_registrations.empty()) {
    return CtapDeviceResponseCode::kCtap2ErrInvalidCredential;
  }

  // Step 5.
  if (!device_info_->options.supports_user_presence &&
      request.user_presence_required) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  // Step 7.
  if (request.user_presence_required &&
      (!user_verified || request.user_verification ==
                             UserVerificationRequirement::kDiscouraged) &&
      !SimulatePress()) {
    return std::nullopt;
  }

  // Step 8.
  if (found_registrations.empty()) {
    return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
  }

  std::optional<std::vector<uint8_t>> hmac_shared_key;
  std::optional<std::array<uint8_t, 32>> hmac_salt1;
  std::optional<std::array<uint8_t, 32>> hmac_salt2;

  if (request.hmac_secret) {
    if (!config_.hmac_secret_support) {
      // Should not have been sent. Authenticators will normally ignore unknown
      // extensions but Chromium should not make this mistake.
      DLOG(ERROR)
          << "Rejecting getAssertion due to unexpected hmac_secret extension";
      return CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension;
    }
    if (!mutable_state()->ecdh_key) {
      // Platform did not fetch the authenticator ECDH key first.
      NOTREACHED();
    }
    if (!request.pin_protocol) {
      return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
    }
    if (static_cast<unsigned>(*request.pin_protocol) >= 2 &&
        !request.hmac_secret->pin_protocol.has_value()) {
      DLOG(ERROR) << "Rejecting request because PIN protocol v2 request with "
                     "hmac-secret didn't duplicate the PIN protocol in the "
                     "hmac-secret extension";
      return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
    }
    if (request.hmac_secret->pin_protocol.has_value() &&
        *request.hmac_secret->pin_protocol != *request.pin_protocol) {
      DLOG(ERROR) << "Rejecting request because PIN protocol in hmac-secret "
                     "extension didn't match the top-level value.";
      return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
    }
    const pin::Protocol& pin_protocol =
        pin::ProtocolVersion(*request.pin_protocol);

    const auto& x962 = request.hmac_secret->public_key_x962;
    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    bssl::UniquePtr<EC_POINT> platform_point(EC_POINT_new(p256.get()));
    if (!EC_POINT_oct2point(p256.get(), platform_point.get(), x962.data(),
                            x962.size(), /*ctx=*/nullptr)) {
      NOTREACHED();
    }

    std::vector<uint8_t> shared_key = pin_protocol.CalculateSharedKey(
        mutable_state()->ecdh_key.get(), platform_point.get());

    const auto& encrypted_salts = request.hmac_secret->encrypted_salts;
    std::vector<uint8_t> salts =
        pin_protocol.Decrypt(shared_key, encrypted_salts);
    if (salts.size() != 32 && salts.size() != 64) {
      NOTREACHED();
    }

    if (pin_protocol.Authenticate(shared_key, encrypted_salts) !=
        request.hmac_secret->salts_auth) {
      NOTREACHED();
    }

    hmac_salt1.emplace();
    memcpy(hmac_salt1->data(), salts.data(), hmac_salt1->size());
    if (salts.size() == 64) {
      hmac_salt2.emplace();
      memcpy(hmac_salt2->data(), salts.data() + hmac_salt1->size(),
             hmac_salt2->size());
    }

    hmac_shared_key = std::move(shared_key);
  }

  if (request.allow_list.empty() && found_registrations.size() > 1 &&
      config_.internal_account_chooser) {
    // Simulate a local account chooser by erasing all but the first result.
    found_registrations.erase(found_registrations.begin() + 1,
                              found_registrations.end());
  }

  // This implementation does not sort credentials by creation time as the spec
  // requires.
  bool done_first = false;
  for (const auto& registration : found_registrations) {
    registration.second->counter++;

    std::optional<AttestedCredentialData> opt_attested_cred_data;
    if (config_.return_attested_cred_data_in_get_assertion_response) {
      opt_attested_cred_data.emplace(ConstructAttestedCredentialData(
          registration.first,
          registration.second->private_key->GetPublicKey()));
    }

    cbor::Value::MapValue extensions_map;
    if (config_.add_extra_extension) {
      extensions_map.emplace(cbor::Value("unsolicited"), cbor::Value(42));
    }

    if (hmac_salt1 && registration.second->hmac_key) {
      const std::pair<std::array<uint8_t, 32>, std::array<uint8_t, 32>>&
          hmac_keys = *registration.second->hmac_key;
      const std::array<uint8_t, 32>& hmac_key =
          user_verified ? hmac_keys.second : hmac_keys.first;
      const std::vector<uint8_t> outputs =
          EvaluateHMAC(hmac_key, *hmac_salt1, hmac_salt2);

      std::vector<uint8_t> encrypted_outputs =
          pin::ProtocolVersion(*request.pin_protocol)
              .Encrypt(*hmac_shared_key, outputs);

      extensions_map.emplace(kExtensionHmacSecret,
                             std::move(encrypted_outputs));
    }

    if (request.get_cred_blob) {
      extensions_map.emplace(
          kExtensionCredBlob,
          registration.second->cred_blob.value_or(std::vector<uint8_t>()));
    }

    std::optional<cbor::Value> extensions;
    if (!extensions_map.empty()) {
      extensions.emplace(std::move(extensions_map));
    }

    AuthenticatorData authenticator_data(
        rp_id_hash,
        mutable_state()->unset_up_bit ? false : request.user_presence_required,
        mutable_state()->unset_uv_bit ? false : user_verified,
        registration.second->backup_eligible, registration.second->backup_state,
        registration.second->counter, std::move(opt_attested_cred_data),
        std::move(extensions));

    std::vector<uint8_t> signature_buffer;
    if (config_.always_uv && !user_verified) {
      // Requests without user presence and with up=0 produce bogus signatures.
      DCHECK(!request.user_presence_required);
      signature_buffer.push_back(0);
    } else {
      signature_buffer = ConstructSignatureBuffer(authenticator_data,
                                                  request.client_data_hash);
    }

    std::vector<uint8_t> signature;
    if (mutable_state()->ctap2_invalid_signature) {
      signature = {0x00};
    } else {
      signature = registration.second->private_key->Sign(signature_buffer);
    }
    AuthenticatorGetAssertionResponse assertion(
        std::move(authenticator_data), signature,
        FidoTransportProtocol::kUsbHumanInterfaceDevice);

    bool include_credential;
    switch (config_.include_credential_in_assertion_response) {
      case VirtualCtap2Device::Config::IncludeCredential::ONLY_IF_NEEDED:
        include_credential = request.allow_list.size() != 1;
        break;
      case VirtualCtap2Device::Config::IncludeCredential::ALWAYS:
        include_credential = true;
        break;
      case VirtualCtap2Device::Config::IncludeCredential::NEVER:
        include_credential = false;
        break;
    }

    if (include_credential) {
      assertion.credential = PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(registration.first));
    }

    if (registration.second->is_resident &&
        (request.allow_list.empty() ||
         !config_.omit_user_entity_on_allow_credentials_requests)) {
      assertion.user_entity = registration.second->user.value();
    }

    if (request.allow_list.empty() && config_.internal_account_chooser) {
      assertion.user_selected = true;
    }

    if (request.large_blob_key) {
      if (!config_.large_blob_support) {
        return CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension;
      }
      if (registration.second->large_blob_key) {
        assertion.large_blob_key = *registration.second->large_blob_key;
      }
    }

    if (request.large_blob_extension_read) {
      DCHECK(config_.large_blob_extension_support == true);
      DCHECK(!registration.second->large_blob_key);
      assertion.large_blob_extension = registration.second->large_blob;
    }

    if (request.large_blob_extension_write) {
      DCHECK(config_.large_blob_extension_support == true);
      const LargeBlob& large_blob = *request.large_blob_extension_write;
      registration.second->large_blob.emplace(large_blob.compressed_data,
                                              large_blob.original_size);
      assertion.large_blob_written = true;
    }

    if (!request.prf_inputs.empty() && user_verified &&
        registration.second->hmac_key) {
      DCHECK(!request.hmac_secret);
      const PRFInput* selected_input = nullptr;
      for (const auto& input : request.prf_inputs) {
        if (!input.credential_id) {
          selected_input = &input;
        } else if (std::equal(
                       input.credential_id->begin(), input.credential_id->end(),
                       registration.first.begin(), registration.first.end())) {
          selected_input = &input;
        }
      }

      if (selected_input) {
        assertion.hmac_secret =
            EvaluateHMAC(registration.second->hmac_key->second,
                         selected_input->salt1, selected_input->salt2);
      }
    }

    if (!done_first) {
      if (found_registrations.size() > 1) {
        DCHECK_LT(found_registrations.size(), 256u);
        assertion.num_credentials = found_registrations.size();
      }
      *response = EncodeGetAssertionResponse(
          assertion, config_.allow_invalid_utf8_in_credential_entities);
      done_first = true;
    } else {
      // These replies will be returned in response to a GetNextAssertion
      // request.
      request_state_.pending_assertions.emplace_back(EncodeGetAssertionResponse(
          assertion, config_.allow_invalid_utf8_in_credential_entities));
    }
    mutable_state()->NotifyAssertion(registration);
  }

  return CtapDeviceResponseCode::kSuccess;
}

CtapDeviceResponseCode VirtualCtap2Device::OnGetNextAssertion(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  if (!request_bytes.empty() && !cbor::Reader::Read(request_bytes)) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }

  if (request_state_.pending_assertions.empty()) {
    return CtapDeviceResponseCode::kCtap2ErrNotAllowed;
  }

  *response = std::move(request_state_.pending_assertions.back());
  request_state_.pending_assertions.pop_back();

  return CtapDeviceResponseCode::kSuccess;
}

std::optional<CtapDeviceResponseCode> VirtualCtap2Device::OnPINCommand(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  request_state_.Reset();

  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const auto& request_map = cbor_request->GetMap();

  const auto protocol_it = request_map.find(
      cbor::Value(static_cast<int>(pin::RequestKey::kProtocol)));
  if (protocol_it == request_map.end() || !protocol_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  std::optional<PINUVAuthProtocol> pin_protocol =
      ToPINUVAuthProtocol(protocol_it->second.GetUnsigned());
  if (!pin_protocol) {
    return CtapDeviceResponseCode::kCtap1ErrInvalidCommand;
  }
  if (*pin_protocol != config_.pin_protocol) {
    return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
  }

  const auto subcommand_it = request_map.find(
      cbor::Value(static_cast<int>(pin::RequestKey::kSubcommand)));
  if (subcommand_it == request_map.end() ||
      !subcommand_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const int64_t subcommand = subcommand_it->second.GetUnsigned();

  if (device_info_->options.client_pin_availability ==
          AuthenticatorSupportedOptions::ClientPinAvailability::kNotSupported &&
      !config_.pin_uv_auth_token_support &&
      // hmac_secret requires the platform to fetch the key-agreement key and
      // so, presumably, devices that support it must support at least that
      // subcommand of PIN support too.
      (!config_.hmac_secret_support ||
       subcommand !=
           static_cast<int>(device::pin::Subcommand::kGetKeyAgreement))) {
    return CtapDeviceResponseCode::kCtap1ErrInvalidCommand;
  }

  cbor::Value::MapValue response_map;
  switch (subcommand) {
    case static_cast<int>(device::pin::Subcommand::kGetRetries):
      response_map.emplace(static_cast<int>(pin::ResponseKey::kRetries),
                           mutable_state()->pin_retries);
      break;

    case static_cast<int>(device::pin::Subcommand::kGetUvRetries):
      response_map.emplace(static_cast<int>(pin::ResponseKey::kUvRetries),
                           mutable_state()->uv_retries);
      break;

    case static_cast<int>(device::pin::Subcommand::kGetKeyAgreement): {
      std::array<uint8_t, kP256X962Length> x962;
      CHECK_EQ(x962.size(),
               EC_POINT_point2oct(
                   EC_KEY_get0_group(mutable_state()->ecdh_key.get()),
                   EC_KEY_get0_public_key(mutable_state()->ecdh_key.get()),
                   POINT_CONVERSION_UNCOMPRESSED, x962.data(), x962.size(),
                   nullptr /* BN_CTX */));

      response_map.emplace(static_cast<int>(pin::ResponseKey::kKeyAgreement),
                           pin::EncodeCOSEPublicKey(x962));
      break;
    }

    case static_cast<int>(device::pin::Subcommand::kSetPIN): {
      const auto encrypted_pin =
          GetPINBytestring(request_map, pin::RequestKey::kNewPINEnc);
      const auto pin_auth =
          GetPINBytestring(request_map, pin::RequestKey::kPINAuth);
      const auto peer_key =
          GetPINKey(request_map, pin::RequestKey::kKeyAgreement);

      if (!encrypted_pin || !pin_auth || !peer_key) {
        return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
      }

      if (!mutable_state()->pin.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
      }

      if (!mutable_state()->ecdh_key) {
        // kGetKeyAgreement should have been called first.
        NOTREACHED();
      }
      std::vector<uint8_t> shared_key =
          pin::ProtocolVersion(*pin_protocol)
              .CalculateSharedKey(mutable_state()->ecdh_key.get(),
                                  peer_key->get());

      CtapDeviceResponseCode err =
          SetPIN(*pin_protocol, mutable_state(), shared_key, *encrypted_pin,
                 *pin_auth, /*current_encrypted_pin_hash=*/std::nullopt);
      if (err != CtapDeviceResponseCode::kSuccess) {
        return err;
      }

      AuthenticatorSupportedOptions options = device_info_->options;
      options.client_pin_availability = AuthenticatorSupportedOptions::
          ClientPinAvailability::kSupportedAndPinSet;
      device_info_->options = std::move(options);

      break;
    }

    case static_cast<int>(device::pin::Subcommand::kChangePIN): {
      const auto encrypted_new_pin =
          GetPINBytestring(request_map, pin::RequestKey::kNewPINEnc);
      const auto encrypted_pin_hash =
          GetPINBytestring(request_map, pin::RequestKey::kPINHashEnc);
      const auto pin_auth =
          GetPINBytestring(request_map, pin::RequestKey::kPINAuth);
      const auto peer_key =
          GetPINKey(request_map, pin::RequestKey::kKeyAgreement);

      if (!encrypted_pin_hash || !encrypted_new_pin || !pin_auth || !peer_key) {
        return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
      }

      if (!mutable_state()->ecdh_key) {
        // kGetKeyAgreement should have been called first.
        NOTREACHED();
      }
      std::vector<uint8_t> shared_key =
          pin::ProtocolVersion(*pin_protocol)
              .CalculateSharedKey(mutable_state()->ecdh_key.get(),
                                  peer_key->get());

      CtapDeviceResponseCode err = ConfirmPresentedPIN(
          *pin_protocol, mutable_state(), shared_key, *encrypted_pin_hash);
      if (err != CtapDeviceResponseCode::kSuccess) {
        RegenerateKeyAgreementKey();
        return err;
      }

      err = SetPIN(*pin_protocol, mutable_state(), shared_key,
                   *encrypted_new_pin, *pin_auth, encrypted_pin_hash);
      if (err != CtapDeviceResponseCode::kSuccess) {
        return err;
      }

      break;
    }

    case static_cast<int>(device::pin::Subcommand::kGetPINToken):
    case static_cast<int>(
        device::pin::Subcommand::kGetPinUvAuthTokenUsingPinWithPermissions): {
      if (subcommand ==
              static_cast<int>(device::pin::Subcommand::
                                   kGetPinUvAuthTokenUsingPinWithPermissions) &&
          !config_.pin_uv_auth_token_support) {
        return CtapDeviceResponseCode::kCtap1ErrInvalidCommand;
      }
      const auto encrypted_pin_hash =
          GetPINBytestring(request_map, pin::RequestKey::kPINHashEnc);
      const auto peer_key =
          GetPINKey(request_map, pin::RequestKey::kKeyAgreement);

      if (!encrypted_pin_hash || !peer_key) {
        return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
      }

      PinUvAuthTokenPermissions permissions;
      if (subcommand ==
          static_cast<int>(device::pin::Subcommand::kGetPINToken)) {
        if (base::Contains(request_map, cbor::Value(static_cast<int>(
                                            pin::RequestKey::kPermissions))) ||
            base::Contains(request_map,
                           cbor::Value(static_cast<int>(
                               pin::RequestKey::kPermissionsRPID)))) {
          return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
        }
        // Set default PinUvAuthToken permissions.
        permissions.permissions =
            static_cast<uint8_t>(pin::Permissions::kMakeCredential) |
            static_cast<uint8_t>(pin::Permissions::kGetAssertion);
      } else {
        DCHECK_EQ(
            subcommand,
            static_cast<int>(device::pin::Subcommand::
                                 kGetPinUvAuthTokenUsingPinWithPermissions));
        CtapDeviceResponseCode response_code =
            ExtractPermissions(request_map, config_, permissions);
        if (response_code != CtapDeviceResponseCode::kSuccess) {
          return response_code;
        }
      }

      if (!mutable_state()->ecdh_key) {
        // kGetKeyAgreement should have been called first.
        NOTREACHED();
      }
      std::vector<uint8_t> shared_key =
          pin::ProtocolVersion(*pin_protocol)
              .CalculateSharedKey(mutable_state()->ecdh_key.get(),
                                  peer_key->get());

      CtapDeviceResponseCode err = ConfirmPresentedPIN(
          *pin_protocol, mutable_state(), shared_key, *encrypted_pin_hash);
      if (err != CtapDeviceResponseCode::kSuccess) {
        RegenerateKeyAgreementKey();
        return err;
      }

      mutable_state()->pin_retries = kMaxPinRetries;

      if (mutable_state()->force_pin_change) {
        return subcommand ==
                       static_cast<int>(device::pin::Subcommand::kGetPINToken)
                   ? CtapDeviceResponseCode::kCtap2ErrPinInvalid
                   : CtapDeviceResponseCode::kCtap2ErrPinPolicyViolation;
      }

      mutable_state()->pin_uv_token_permissions = permissions.permissions;
      mutable_state()->pin_uv_token_rpid = permissions.rp_id;

      response_map.emplace(static_cast<int>(pin::ResponseKey::kPINToken),
                           GenerateAndEncryptToken(*pin_protocol, shared_key,
                                                   mutable_state()->pin_token));
      break;
    }

    case static_cast<int>(device::pin::Subcommand::kGetUvToken): {
      const auto peer_key =
          GetPINKey(request_map, pin::RequestKey::kKeyAgreement);
      if (!peer_key) {
        return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
      }

      PinUvAuthTokenPermissions permissions;
      CtapDeviceResponseCode response_code =
          ExtractPermissions(request_map, config_, permissions);
      if (response_code != CtapDeviceResponseCode::kSuccess) {
        return response_code;
      }

      if (device_info_->options.user_verification_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedButNotConfigured) {
        return CtapDeviceResponseCode::kCtap2ErrNotAllowed;
      }

      if (mutable_state()->uv_retries <= 0) {
        return CtapDeviceResponseCode::kCtap2ErrUvBlocked;
      }

      if (!mutable_state()->ecdh_key) {
        // kGetKeyAgreement should have been called first.
        NOTREACHED();
      }
      std::vector<uint8_t> shared_key =
          pin::ProtocolVersion(*pin_protocol)
              .CalculateSharedKey(mutable_state()->ecdh_key.get(),
                                  peer_key->get());

      --mutable_state()->uv_retries;

      // Simulate internal UV.
      if (!SimulatePress()) {
        return std::nullopt;
      }
      if (!config_.user_verification_succeeds) {
        return mutable_state()->uv_retries > 0
                   ? CtapDeviceResponseCode::kCtap2ErrUvInvalid
                   : CtapDeviceResponseCode::kCtap2ErrUvBlocked;
      }

      mutable_state()->pin_retries = kMaxPinRetries;
      mutable_state()->uv_retries = kMaxUvRetries;
      mutable_state()->pin_uv_token_permissions = permissions.permissions;
      mutable_state()->pin_uv_token_rpid = permissions.rp_id;

      response_map.emplace(static_cast<int>(pin::ResponseKey::kPINToken),
                           GenerateAndEncryptToken(*pin_protocol, shared_key,
                                                   mutable_state()->pin_token));
      break;
    }

    default:
      return CtapDeviceResponseCode::kCtap1ErrInvalidCommand;
  }

  *response = cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
  return CtapDeviceResponseCode::kSuccess;
}

CtapDeviceResponseCode VirtualCtap2Device::OnCredentialManagement(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  if (!device_info_->options.supports_credential_management) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const auto& request_map = cbor_request->GetMap();
  const auto subcommand_it = request_map.find(cbor::Value(
      static_cast<int>(CredentialManagementRequestKey::kSubCommand)));
  if (subcommand_it == request_map.end() ||
      !subcommand_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const int64_t subcommand = subcommand_it->second.GetUnsigned();

  cbor::Value::MapValue response_map;
  switch (static_cast<CredentialManagementSubCommand>(subcommand)) {
    case CredentialManagementSubCommand::kGetCredsMetadata: {
      request_state_.Reset();

      CtapDeviceResponseCode pin_status = VerifyPINUVAuthToken(
          *device_info_, mutable_state()->pin_token, request_map,
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinProtocol)),
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinAuth)),
          {{static_cast<uint8_t>(subcommand)}});
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      const size_t num_resident = base::ranges::count_if(
          mutable_state()->registrations,
          [](const auto& it) { return it.second.is_resident; });
      response_map.emplace(
          static_cast<int>(CredentialManagementResponseKey::
                               kExistingResidentCredentialsCount),
          static_cast<int64_t>(num_resident));

      const size_t num_remaining =
          config_.resident_credential_storage - num_resident;
      DCHECK_LE(0ul, num_remaining);
      response_map.emplace(
          static_cast<int>(CredentialManagementResponseKey::
                               kMaxPossibleRemainingResidentCredentialsCount),
          static_cast<int64_t>(num_remaining));

      *response =
          cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::kEnumerateRPsBegin: {
      CtapDeviceResponseCode pin_status = VerifyPINUVAuthToken(
          *device_info_, mutable_state()->pin_token, request_map,
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinProtocol)),
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinAuth)),
          {{static_cast<uint8_t>(subcommand)}});
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      InitPendingRPs();
      if (request_state_.pending_rps.empty() &&
          config_.return_err_no_credentials_on_empty_rp_enumeration) {
        return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
      }
      response_map.emplace(
          static_cast<int>(CredentialManagementResponseKey::kTotalRPs),
          static_cast<int>(request_state_.pending_rps.size()));
      if (!request_state_.pending_rps.empty()) {
        GetNextRP(&response_map);
      }

      *response = WriteCBOR(cbor::Value(std::move(response_map)),
                            config_.allow_invalid_utf8_in_credential_entities);
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::kEnumerateRPsGetNextRP: {
      if (request_state_.pending_rps.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrNotAllowed;
      }
      GetNextRP(&response_map);

      *response = WriteCBOR(cbor::Value(std::move(response_map)),
                            config_.allow_invalid_utf8_in_credential_entities);
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::kEnumerateCredentialsBegin: {
      const auto params_it = request_map.find(cbor::Value(
          static_cast<int>(CredentialManagementRequestKey::kSubCommandParams)));
      if (params_it == request_map.end() && !params_it->second.is_map()) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      const cbor::Value::MapValue& params = params_it->second.GetMap();

      // pinAuth = LEFT(HMAC-SHA-256(pinToken, enumerateCredentialsBegin (0x04)
      //                                       || subCommandParams), 16)
      std::vector<uint8_t> pinauth_bytes =
          cbor::Writer::Write(cbor::Value(params)).value();
      pinauth_bytes.insert(pinauth_bytes.begin(),
                           static_cast<uint8_t>(subcommand));
      CtapDeviceResponseCode pin_status = VerifyPINUVAuthToken(
          *device_info_, mutable_state()->pin_token, request_map,
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinProtocol)),
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinAuth)),
          pinauth_bytes);
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      const auto rp_id_hash_it = params.find(cbor::Value(
          static_cast<int>(CredentialManagementRequestParamKey::kRPIDHash)));
      if (rp_id_hash_it == params.end() ||
          !rp_id_hash_it->second.is_bytestring() ||
          rp_id_hash_it->second.GetBytestring().size() != kRpIdHashLength) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }

      InitPendingRegistrations(rp_id_hash_it->second.GetBytestring());
      if (request_state_.pending_registrations.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
      }
      response_map.swap(request_state_.pending_registrations.front());
      response_map.emplace(
          static_cast<int>(CredentialManagementResponseKey::kTotalCredentials),
          static_cast<int>(request_state_.pending_registrations.size()));
      request_state_.pending_registrations.pop_front();

      *response = WriteCBOR(cbor::Value(std::move(response_map)),
                            config_.allow_invalid_utf8_in_credential_entities);
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::
        kEnumerateCredentialsGetNextCredential: {
      if (request_state_.pending_registrations.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrNotAllowed;
      }
      response_map.swap(request_state_.pending_registrations.front());
      request_state_.pending_registrations.pop_front();

      *response = WriteCBOR(cbor::Value(std::move(response_map)),
                            config_.allow_invalid_utf8_in_credential_entities);
      return CtapDeviceResponseCode::kSuccess;
    }

    case CredentialManagementSubCommand::kDeleteCredential: {
      request_state_.Reset();

      const auto params_it = request_map.find(cbor::Value(
          static_cast<int>(CredentialManagementRequestKey::kSubCommandParams)));
      if (params_it == request_map.end() && !params_it->second.is_map()) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      const cbor::Value::MapValue& params = params_it->second.GetMap();
      // pinAuth = LEFT(HMAC-SHA-256(pinToken, enumerateCredentialsBegin (0x04)
      //                                       || subCommandParams), 16)
      std::vector<uint8_t> pinauth_bytes =
          cbor::Writer::Write(cbor::Value(params)).value();
      pinauth_bytes.insert(pinauth_bytes.begin(),
                           static_cast<uint8_t>(subcommand));
      CtapDeviceResponseCode pin_status = VerifyPINUVAuthToken(
          *device_info_, mutable_state()->pin_token, request_map,
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinProtocol)),
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinAuth)),
          pinauth_bytes);
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      const auto credential_id_it = params.find(cbor::Value(static_cast<int>(
          CredentialManagementRequestParamKey::kCredentialID)));
      if (credential_id_it == params.end() ||
          !credential_id_it->second.is_map()) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      auto credential_id = PublicKeyCredentialDescriptor::CreateFromCBORValue(
          cbor::Value(credential_id_it->second.GetMap()));
      if (!credential_id) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      if (!base::Contains(mutable_state()->registrations, credential_id->id)) {
        return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
      }
      mutable_state()->registrations.erase(credential_id->id);
      *response = {};
      return CtapDeviceResponseCode::kSuccess;
    }
    case CredentialManagementSubCommand::kUpdateUserInformation: {
      request_state_.Reset();

      const auto params_it = request_map.find(cbor::Value(
          static_cast<int>(CredentialManagementRequestKey::kSubCommandParams)));
      if (params_it == request_map.end() && !params_it->second.is_map()) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      const cbor::Value::MapValue& params = params_it->second.GetMap();
      std::vector<uint8_t> pinauth_bytes =
          cbor::Writer::Write(cbor::Value(params)).value();
      pinauth_bytes.insert(pinauth_bytes.begin(),
                           static_cast<uint8_t>(subcommand));
      CtapDeviceResponseCode pin_status = VerifyPINUVAuthToken(
          *device_info_, mutable_state()->pin_token, request_map,
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinProtocol)),
          cbor::Value(
              static_cast<int>(CredentialManagementRequestKey::kPinAuth)),
          pinauth_bytes);
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      const auto credential_id_it = params.find(cbor::Value(static_cast<int>(
          CredentialManagementRequestParamKey::kCredentialID)));
      if (credential_id_it == params.end() ||
          !credential_id_it->second.is_map()) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      auto credential_id = PublicKeyCredentialDescriptor::CreateFromCBORValue(
          cbor::Value(credential_id_it->second.GetMap()));
      if (!credential_id) {
        return CtapDeviceResponseCode::kCtap2ErrMissingParameter;
      }
      if (!base::Contains(mutable_state()->registrations, credential_id->id)) {
        return CtapDeviceResponseCode::kCtap2ErrNoCredentials;
      }

      const auto new_user_it = params.find(cbor::Value(
          static_cast<int>(CredentialManagementRequestParamKey::kUser)));
      if (new_user_it == params.end() || !new_user_it->second.is_map()) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }
      std::optional<PublicKeyCredentialUserEntity> new_user =
          PublicKeyCredentialUserEntity::CreateFromCBORValue(
              cbor::Value(new_user_it->second.GetMap()));
      if (!new_user) {
        return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
      }

      mutable_state()->registrations[credential_id->id].user = new_user;
      *response = {};
      return CtapDeviceResponseCode::kSuccess;
    }
  }
  NOTREACHED();
}

CtapDeviceResponseCode VirtualCtap2Device::OnBioEnrollment(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  request_state_.Reset();

  // TODO(martinkr): Verify PIN/UV Auth.
  // Check to ensure that device supports bio enrollment.
  if (device_info_->options.bio_enrollment_availability ==
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kNotSupported &&
      device_info_->options.bio_enrollment_availability_preview ==
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kNotSupported) {
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  // Read request bytes into |cbor::Value::MapValue|.
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const auto& request_map = cbor_request->GetMap();

  cbor::Value::MapValue response_map;

  // Check for the get-modality command.
  auto it = request_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentRequestKey::kGetModality)));
  if (it != request_map.end()) {
    if (!it->second.is_bool()) {
      return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
    }
    if (!it->second.GetBool()) {
      // This value is optional so sending |false| is prohibited by the spec.
      return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
    }
    response_map.emplace(static_cast<int>(BioEnrollmentResponseKey::kModality),
                         static_cast<int>(BioEnrollmentModality::kFingerprint));
    *response =
        cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
    return CtapDeviceResponseCode::kSuccess;
  }

  // Check for subcommands.
  it = request_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentRequestKey::kSubCommand)));
  if (it == request_map.end()) {
    // Could not find a valid command, so return an error.
    NOTREACHED();
  }

  if (!it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }

  // Template id from subcommand parameters, if it exists.
  std::optional<uint8_t> template_id;
  std::optional<std::string> name;
  auto params_it = request_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentRequestKey::kSubCommandParams)));
  if (params_it != request_map.end()) {
    const auto& params = params_it->second.GetMap();
    auto template_it = params.find(cbor::Value(
        static_cast<int>(BioEnrollmentSubCommandParam::kTemplateId)));
    if (template_it != params.end()) {
      if (!template_it->second.is_bytestring()) {
        NOTREACHED() << "Template ID parameter must be a CBOR bytestring.";
      }
      // Simplification: for unit tests, enforce one byte template IDs
      DCHECK_EQ(template_it->second.GetBytestring().size(), 1u);
      template_id = template_it->second.GetBytestring()[0];
    }
    auto name_it = params.find(cbor::Value(
        static_cast<int>(BioEnrollmentSubCommandParam::kTemplateFriendlyName)));
    if (name_it != params.end()) {
      if (!name_it->second.is_string()) {
        NOTREACHED() << "Name parameter must be a CBOR string.";
      }
      name = name_it->second.GetString();
    }
  }

  auto cmd =
      ToBioEnrollmentEnum<BioEnrollmentSubCommand>(it->second.GetUnsigned());
  if (!cmd) {
    // Invalid command is unsupported.
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }

  using SubCmd = BioEnrollmentSubCommand;
  switch (*cmd) {
    // TODO(crbug.com/40697161): some of these commands should be checking
    // PinUvAuthToken.
    case SubCmd::kGetFingerprintSensorInfo:
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kModality),
          static_cast<int>(BioEnrollmentModality::kFingerprint));
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kFingerprintKind),
          static_cast<int>(BioEnrollmentFingerprintKind::kTouch));
      response_map.emplace(
          static_cast<int>(
              BioEnrollmentResponseKey::kMaxCaptureSamplesRequiredForEnroll),
          config_.bio_enrollment_samples_required);
      break;
    case SubCmd::kEnrollBegin:
      if (mutable_state()->bio_templates.size() ==
          config_.bio_enrollment_capacity) {
        return CtapDeviceResponseCode::kCtap2ErrFpDatabaseFull;
      }
      mutable_state()->bio_current_template_id = 0;
      while (mutable_state()->bio_templates.find(
                 ++(*mutable_state()->bio_current_template_id)) !=
             mutable_state()->bio_templates.end()) {
        // Check for integer overflow (indicates full)
        DCHECK_LT(*mutable_state()->bio_current_template_id, 255);
      }
      mutable_state()->bio_remaining_samples =
          config_.bio_enrollment_samples_required;
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kTemplateId),
          std::vector<uint8_t>{*mutable_state()->bio_current_template_id});
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus),
          static_cast<int>(BioEnrollmentSampleStatus::kGood));
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples),
          --mutable_state()->bio_remaining_samples);
      break;
    case SubCmd::kEnrollCaptureNextSample:
      if (!mutable_state()->bio_current_template_id ||
          mutable_state()->bio_current_template_id != *template_id) {
        NOTREACHED() << "Invalid current enrollment or template id parameter.";
      }
      if (mutable_state()->bio_enrollment_next_sample_error) {
        response_map.emplace(
            static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus),
            static_cast<int>(BioEnrollmentSampleStatus::kTooHigh));
        response_map.emplace(
            static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples),
            mutable_state()->bio_remaining_samples);
        mutable_state()->bio_enrollment_next_sample_error = false;
        break;
      }
      if (mutable_state()->bio_enrollment_next_sample_timeout) {
        response_map.emplace(
            static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus),
            static_cast<int>(BioEnrollmentSampleStatus::kNoUserActivity));
        response_map.emplace(
            static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples),
            mutable_state()->bio_remaining_samples);
        mutable_state()->bio_enrollment_next_sample_timeout = false;
        break;
      }
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus),
          static_cast<int>(BioEnrollmentSampleStatus::kGood));
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples),
          --mutable_state()->bio_remaining_samples);

      if (mutable_state()->bio_remaining_samples == 0) {
        mutable_state()
            ->bio_templates[*mutable_state()->bio_current_template_id] =
            base::StrCat(
                {"Template", base::NumberToString(
                                 *mutable_state()->bio_current_template_id)});
        mutable_state()->bio_current_template_id = std::nullopt;
        mutable_state()->fingerprints_enrolled = true;
      }
      break;
    case SubCmd::kEnumerateEnrollments: {
      if (mutable_state()->bio_templates.empty()) {
        return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
      }
      cbor::Value::ArrayValue template_infos;
      for (const auto& enroll : mutable_state()->bio_templates) {
        cbor::Value::MapValue template_info;
        template_info.emplace(cbor::Value(static_cast<int>(
                                  BioEnrollmentTemplateInfoParam::kTemplateId)),
                              std::vector<uint8_t>{enroll.first});
        template_info.emplace(
            cbor::Value(static_cast<int>(
                BioEnrollmentTemplateInfoParam::kTemplateFriendlyName)),
            cbor::Value(enroll.second));
        template_infos.emplace_back(std::move(template_info));
      }
      response_map.emplace(
          static_cast<int>(BioEnrollmentResponseKey::kTemplateInfos),
          std::move(template_infos));
      break;
    }
    case SubCmd::kSetFriendlyName:
      if (!template_id || !name) {
        NOTREACHED() << "Could not parse template_id or name from parameters.";
      }

      // Template ID from parameter does not exist, cannot rename.
      if (mutable_state()->bio_templates.find(*template_id) ==
          mutable_state()->bio_templates.end()) {
        return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
      }

      mutable_state()->bio_templates[*template_id] = *name;
      return CtapDeviceResponseCode::kSuccess;
    case SubCmd::kRemoveEnrollment:
      if (!template_id) {
        NOTREACHED() << "Could not parse template_id or name from parameters.";
      }

      // Template ID from parameter does not exist, cannot remove.
      if (mutable_state()->bio_templates.find(*template_id) ==
          mutable_state()->bio_templates.end()) {
        return CtapDeviceResponseCode::kCtap2ErrInvalidOption;
      }

      mutable_state()->bio_templates.erase(*template_id);
      return CtapDeviceResponseCode::kSuccess;
    case SubCmd::kCancelCurrentEnrollment:
      mutable_state()->bio_current_template_id = std::nullopt;
      return CtapDeviceResponseCode::kSuccess;
    default:
      // Handle all other commands as if they were unsupported (will change
      // when support is added).
      return CtapDeviceResponseCode::kCtap2ErrUnsupportedOption;
  }
  *response = cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
  return CtapDeviceResponseCode::kSuccess;
}

CtapDeviceResponseCode VirtualCtap2Device::OnLargeBlobs(
    base::span<const uint8_t> request_bytes,
    std::vector<uint8_t>* response) {
  if (!config_.large_blob_support) {
    DLOG(ERROR) << "Large blob not supported";
    return CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension;
  }

  // Read request bytes into |cbor::Value::MapValue|.
  const auto& cbor_request = cbor::Reader::Read(request_bytes);
  if (!cbor_request || !cbor_request->is_map()) {
    return CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType;
  }
  const auto& request_map = cbor_request->GetMap();

  const auto offset_it = request_map.find(
      cbor::Value(static_cast<uint8_t>(LargeBlobsRequestKey::kOffset)));
  if (offset_it == request_map.end() || !offset_it->second.is_unsigned()) {
    return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
  }
  const uint64_t offset = offset_it->second.GetUnsigned();

  const auto get_it = request_map.find(
      cbor::Value(static_cast<uint8_t>(LargeBlobsRequestKey::kGet)));
  const auto set_it = request_map.find(
      cbor::Value(static_cast<uint8_t>(LargeBlobsRequestKey::kSet)));
  if ((get_it == request_map.end() && set_it == request_map.end()) ||
      (get_it != request_map.end() && set_it != request_map.end())) {
    return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
  }
  if ((get_it != request_map.end() && !get_it->second.is_unsigned()) ||
      (set_it != request_map.end() && !set_it->second.is_bytestring())) {
    return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
  }
  const auto length_it = request_map.find(
      cbor::Value(static_cast<uint8_t>(LargeBlobsRequestKey::kLength)));
  const size_t max_fragment_length = kLargeBlobDefaultMaxFragmentLength;

  if (get_it != request_map.end()) {
    request_state_.Reset();

    if (length_it != request_map.end() ||
        request_map.find(cbor::Value(static_cast<uint8_t>(
            LargeBlobsRequestKey::kPinUvAuthParam))) != request_map.end() ||
        request_map.find(cbor::Value(static_cast<uint8_t>(
            LargeBlobsRequestKey::kPinUvAuthProtocol))) != request_map.end()) {
      return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
    }
    const size_t get = base::checked_cast<size_t>(get_it->second.GetUnsigned());
    if (get > max_fragment_length) {
      return CtapDeviceResponseCode::kCtap1ErrInvalidLength;
    }
    if (offset > mutable_state()->large_blob.size()) {
      return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
    }
    cbor::Value::MapValue response_map;

    auto subspan = base::make_span(mutable_state()->large_blob).subspan(offset);
    response_map.emplace(static_cast<uint8_t>(LargeBlobsResponseKey::kConfig),
                         subspan.first(std::min<size_t>(get, subspan.size())));

    *response =
        cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
  } else {
    CHECK(set_it != request_map.end(), base::NotFatalUntil::M130);
    const std::vector<uint8_t>& set = set_it->second.GetBytestring();
    if (set.size() > max_fragment_length) {
      return CtapDeviceResponseCode::kCtap1ErrInvalidLength;
    }
    if (offset == 0) {
      request_state_.Reset();

      if (length_it == request_map.end() || !length_it->second.is_unsigned()) {
        return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
      }
      const uint64_t length = length_it->second.GetUnsigned();
      if (length > config_.available_large_blob_storage) {
        return CtapDeviceResponseCode::kCtap2ErrLargeBlobStorageFull;
      }
      constexpr size_t kMinBlobLength = 17;
      if (length < kMinBlobLength) {
        return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
      }
      request_state_.large_blob_expected_length = length;
      request_state_.large_blob_expected_next_offset = 0;
    } else {
      if (length_it != request_map.end()) {
        return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
      }
    }

    if (offset != request_state_.large_blob_expected_next_offset) {
      return CtapDeviceResponseCode::kCtap1ErrInvalidSeq;
    }

    // If the device is protected by some sort of user verification or alwaysUv
    // is true.
    if (device_info_->options.client_pin_availability ==
            AuthenticatorSupportedOptions::ClientPinAvailability::
                kSupportedAndPinSet ||
        device_info_->options.user_verification_availability ==
            AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedAndConfigured ||
        config_.always_uv) {
      // verify(pinUvAuthToken,
      //        32×0xff || h’0c00' || uint32LittleEndian(offset) || SHA-256(
      //          contents of set byte string, i.e. not including an outer CBOR
      //          tag with major type two),
      //        pinUvAuthParam)
      std::vector<uint8_t> pinauth_bytes;
      pinauth_bytes.insert(pinauth_bytes.begin(),
                           pin::kPinUvAuthTokenSafetyPadding.begin(),
                           pin::kPinUvAuthTokenSafetyPadding.end());
      pinauth_bytes.insert(pinauth_bytes.end(), kLargeBlobPinPrefix.begin(),
                           kLargeBlobPinPrefix.end());
      auto offset_vec = fido_parsing_utils::Uint32LittleEndian(offset);
      pinauth_bytes.insert(pinauth_bytes.end(), offset_vec.begin(),
                           offset_vec.end());
      std::array<uint8_t, crypto::kSHA256Length> set_hash =
          crypto::SHA256Hash(set);
      pinauth_bytes.insert(pinauth_bytes.end(), set_hash.begin(),
                           set_hash.end());
      CtapDeviceResponseCode pin_status = VerifyPINUVAuthToken(
          *device_info_, mutable_state()->pin_token, request_map,
          cbor::Value(
              static_cast<uint8_t>(LargeBlobsRequestKey::kPinUvAuthProtocol)),
          cbor::Value(
              static_cast<uint8_t>(LargeBlobsRequestKey::kPinUvAuthParam)),
          pinauth_bytes);
      if (pin_status != CtapDeviceResponseCode::kSuccess) {
        return pin_status;
      }

      if (!(mutable_state()->pin_uv_token_permissions &
            static_cast<uint8_t>(pin::Permissions::kLargeBlobWrite))) {
        return CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid;
      }
    }

    if (offset + set.size() > request_state_.large_blob_expected_length) {
      return CtapDeviceResponseCode::kCtap1ErrInvalidParameter;
    }

    request_state_.large_blob_buffer.insert(
        request_state_.large_blob_buffer.end(), set.begin(), set.end());
    request_state_.large_blob_expected_next_offset =
        request_state_.large_blob_buffer.size();
    if (request_state_.large_blob_buffer.size() ==
        request_state_.large_blob_expected_length) {
      if (!VerifyLargeBlobArrayIntegrity(request_state_.large_blob_buffer)) {
        return CtapDeviceResponseCode::kCtap2ErrIntegrityFailure;
      }
      mutable_state()->large_blob = request_state_.large_blob_buffer;
    }
  }
  return CtapDeviceResponseCode::kSuccess;
}

void VirtualCtap2Device::InitPendingRPs() {
  request_state_.Reset();
  std::set<std::string> rp_ids;
  for (const auto& registration : mutable_state()->registrations) {
    if (!registration.second.is_resident) {
      continue;
    }
    DCHECK(!registration.second.is_u2f);
    DCHECK(registration.second.user);
    DCHECK(registration.second.rp);
    if (!base::Contains(rp_ids, registration.second.rp->id)) {
      rp_ids.insert(registration.second.rp->id);
      request_state_.pending_rps.push_back(*registration.second.rp);
    }
  }
}

void VirtualCtap2Device::InitPendingRegistrations(
    base::span<const uint8_t> rp_id_hash) {
  DCHECK_EQ(rp_id_hash.size(), kRpIdHashLength);
  request_state_.Reset();
  for (const auto& registration : mutable_state()->registrations) {
    if (!registration.second.is_resident ||
        !base::ranges::equal(rp_id_hash,
                             registration.second.application_parameter)) {
      continue;
    }
    DCHECK(!registration.second.is_u2f && registration.second.user &&
           registration.second.rp);
    cbor::Value::MapValue response_map;
    response_map.emplace(
        static_cast<int>(CredentialManagementResponseKey::kUser),
        *UserEntityAsCBOR(*registration.second.user,
                          /* user_verification= */ true,
                          config_.allow_invalid_utf8_in_credential_entities));
    response_map.emplace(
        static_cast<int>(CredentialManagementResponseKey::kCredentialID),
        AsCBOR(PublicKeyCredentialDescriptor(CredentialType::kPublicKey,
                                             registration.first)));
    if (registration.second.large_blob_key) {
      response_map.emplace(
          static_cast<int>(CredentialManagementResponseKey::kLargeBlobKey),
          cbor::Value(cbor::Value(*registration.second.large_blob_key)));
    }

    std::optional<cbor::Value> cose_key = cbor::Reader::Read(
        registration.second.private_key->GetPublicKey()->cose_key_bytes);
    response_map.emplace(
        static_cast<int>(CredentialManagementResponseKey::kPublicKey),
        cose_key->GetMap());
    request_state_.pending_registrations.emplace_back(std::move(response_map));
  }
}

void VirtualCtap2Device::RegenerateKeyAgreementKey() {
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_generate_key(key.get()));
  mutable_state()->ecdh_key = std::move(key);
}

void VirtualCtap2Device::GetNextRP(cbor::Value::MapValue* response_map) {
  DCHECK(!request_state_.pending_rps.empty());
  response_map->emplace(
      static_cast<int>(CredentialManagementResponseKey::kRP),
      *RpEntityAsCBOR(request_state_.pending_rps.front(),
                      config_.allow_invalid_utf8_in_credential_entities));
  response_map->emplace(
      static_cast<int>(CredentialManagementResponseKey::kRPIDHash),
      fido_parsing_utils::CreateSHA256Hash(
          request_state_.pending_rps.front().id));
  request_state_.pending_rps.pop_front();
}

CtapDeviceResponseCode VirtualCtap2Device::OnAuthenticatorGetInfo(
    std::vector<uint8_t>* response) const {
  *response = AuthenticatorGetInfoResponse::EncodeToCBOR(*device_info_);
  return CtapDeviceResponseCode::kSuccess;
}

AttestedCredentialData VirtualCtap2Device::ConstructAttestedCredentialData(
    base::span<const uint8_t> key_handle,
    std::unique_ptr<PublicKey> public_key) {
  constexpr std::array<uint8_t, 2> sha256_length = {0, crypto::kSHA256Length};
  constexpr std::array<uint8_t, 16> kZeroAaguid = {0, 0, 0, 0, 0, 0, 0, 0,
                                                   0, 0, 0, 0, 0, 0, 0, 0};
  base::span<const uint8_t, 16> aaguid(kDeviceAaguid);
  if (config_.none_attestation ||
      (mutable_state()->self_attestation &&
       !mutable_state()->non_zero_aaguid_with_self_attestation)) {
    aaguid = kZeroAaguid;
  }
  return AttestedCredentialData(aaguid, sha256_length,
                                fido_parsing_utils::Materialize(key_handle),
                                std::move(public_key));
}

size_t VirtualCtap2Device::remaining_resident_credentials() const {
  size_t num_resident_keys = 0;
  for (const auto& registration : mutable_state()->registrations) {
    if (registration.second.is_resident) {
      num_resident_keys++;
    }
  }

  DCHECK_LE(num_resident_keys, config_.resident_credential_storage);
  return config_.resident_credential_storage - num_resident_keys;
}

bool VirtualCtap2Device::SupportsAtLeast(Ctap2Version ctap2_version) const {
  return base::ranges::any_of(config_.ctap2_versions,
                              [ctap2_version](const Ctap2Version& version) {
                                return version >= ctap2_version;
                              });
}

}  // namespace device
