// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_model_utils.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_features.h"
#include "crypto/aead.h"
#include "crypto/hash.h"
#include "crypto/kdf.h"
#include "crypto/keypair.h"
#include "crypto/random.h"
#include "crypto/sign.h"
#include "device/fido/attestation_object.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public_key.h"

namespace webauthn::passkey_model_utils {

namespace {

// The length of the nonce prefix used for AES-256-GCM encryption of
// `WebAuthnCredentialSpecifics.encrypted_data` (all oneof cases) and of
// `SecurityDomainEncrypted.device_authorization_encrypted`.
constexpr size_t kWebAuthnCredentialSpecificsEncryptedDataNonceLength = 12;

// The AAD parameter for the AES-256 encryption of
// `WebAuthnCredentialSpecifics.encrypted`.
constexpr std::string_view kAadWebauthnCredentialSpecificsEncrypted =
    "WebauthnCredentialSpecifics.Encrypted";

// The AAD parameter for the AES-256 encryption of
// `WebAuthnCredentialSpecifics.private_key` (empty).
constexpr std::string_view kAadWebauthnCredentialSpecificsPrivateKey = "";

// The AAD parameter for the AES-256 encryption of
// `WebAuthnCredentialSpecifics.security_domain_encrypted` with the trusted
// vault key.
constexpr std::string_view
    kAadWebauthnCredentialSpecificsSecurityDomainEncrypted =
        "WebauthnCredentialSpecifics.SecurityDomainEncrypted";

// The AAD parameter for the AES-256 encryption of
// `SecurityDomainEncrypted.device_authorization_encrypted` with the device
// authorization key.
constexpr std::string_view
    kAadWebauthnCredentialSpecificsDeviceAuthorizationEncrypted =
        "WebauthnCredentialSpecifics.DeviceAuthorizationEncrypted";

// Signature counter, as defined in the w3c spec here:
// https://www.w3.org/TR/webauthn-2/#signature-counter
constexpr uint8_t kSignatureCounter[4] = {0};

constexpr size_t kEncryptionSecretSize = 32;

struct PasskeyComparator {
  bool operator()(const sync_pb::WebauthnCredentialSpecifics& a,
                  const sync_pb::WebauthnCredentialSpecifics& b) const {
    return std::tie(a.rp_id(), a.user_id()) < std::tie(b.rp_id(), b.user_id());
  }
};

std::array<uint8_t, kEncryptionSecretSize> DerivePasskeyEncryptionSecret(
    base::span<const uint8_t> trusted_vault_key) {
  constexpr std::string_view kHkdfInfo =
      "KeychainApplicationKey:gmscore_module:com.google.android.gms.fido";
  return crypto::kdf::Hkdf<kEncryptionSecretSize>(
      crypto::hash::kSha256, trusted_vault_key,
      /*salt=*/base::span<const uint8_t>(),
      base::as_bytes(base::span(kHkdfInfo)));
}

std::array<uint8_t, kHmacSecretSize> DeriveHmacSecretFromPrivateKey(
    base::span<const uint8_t> private_key) {
  CHECK(!private_key.empty());
  constexpr std::string_view kHkdfInfo = "derived PRF HMAC secret";
  return crypto::kdf::Hkdf<kHmacSecretSize>(
      crypto::hash::kSha256, private_key,
      /*salt=*/base::span<const uint8_t>(),
      base::as_bytes(base::span(kHkdfInfo)));
}

std::array<uint8_t, kEncryptionSecretSize>
DeriveDeviceAuthorizationEncryptionSecret(
    base::span<const uint8_t> device_authorization_key) {
  constexpr std::string_view kHkdfInfo =
      "DeviceAuthorizationKey:WebauthnCredentialSpecifics";
  return crypto::kdf::Hkdf<kEncryptionSecretSize>(
      crypto::hash::kSha256, device_authorization_key,
      /*salt=*/base::span<const uint8_t>(),
      base::as_bytes(base::span(kHkdfInfo)));
}

enum class AesGcmDecryptionError {
  kInvalidLength,
  kDecryptionFailed,
};

// Returns a human-readable description of `error` for logging.
std::string_view ToString(AesGcmDecryptionError error) {
  switch (error) {
    case AesGcmDecryptionError::kInvalidLength:
      return "has invalid length";
    case AesGcmDecryptionError::kDecryptionFailed:
      return "decryption failed";
  }
}

// Decrypts `encrypted`, which is an AES-256-GCM nonce followed by the
// ciphertext.
base::expected<std::vector<uint8_t>, AesGcmDecryptionError> DecryptAesGcm(
    base::span<const uint8_t> key,
    std::string_view encrypted,
    std::string_view aad) {
  if (encrypted.size() < kWebAuthnCredentialSpecificsEncryptedDataNonceLength) {
    return base::unexpected(AesGcmDecryptionError::kInvalidLength);
  }
  const auto [nonce, ciphertext] = base::as_byte_span(encrypted).split_at(
      kWebAuthnCredentialSpecificsEncryptedDataNonceLength);
  std::optional<std::vector<uint8_t>> decrypted =
      crypto::aead::Open(crypto::aead::AES_256_GCM, key, ciphertext, nonce,
                         base::as_byte_span(aad));
  if (!decrypted) {
    return base::unexpected(AesGcmDecryptionError::kDecryptionFailed);
  }
  return std::move(*decrypted);
}

// Encrypts `plaintext` with AES-256-GCM and returns the randomly generated
// nonce followed by the ciphertext.
std::string EncryptAesGcm(base::span<const uint8_t> key,
                          std::string_view plaintext,
                          std::string_view aad) {
  std::array<uint8_t, kWebAuthnCredentialSpecificsEncryptedDataNonceLength>
      nonce;
  crypto::RandBytes(nonce);
  std::vector<uint8_t> ciphertext = crypto::aead::Seal(
      crypto::aead::AES_256_GCM, key, base::as_byte_span(plaintext), nonce,
      base::as_byte_span(aad));
  return base::StrCat(
      {base::as_string_view(nonce), base::as_string_view(ciphertext)});
}

}  // namespace

ExtensionOutputData::ExtensionOutputData() = default;
ExtensionOutputData::ExtensionOutputData(const ExtensionOutputData&) = default;
ExtensionOutputData::~ExtensionOutputData() = default;

ExtensionInputData::ExtensionInputData() = default;

ExtensionInputData::ExtensionInputData(
    std::optional<device::PRFInput> prf_input)
    : has_prf(prf_input.has_value()), prf_input(std::move(prf_input)) {}

ExtensionInputData::ExtensionInputData(
    bool has_prf,
    std::optional<device::PRFInput> prf_input)
    : has_prf(has_prf), prf_input(std::move(prf_input)) {
  CHECK(this->has_prf || !this->prf_input.has_value());
}

ExtensionInputData::ExtensionInputData(const ExtensionInputData&) = default;
ExtensionInputData::~ExtensionInputData() = default;

ExtensionOutputData ExtensionInputData::ToOutputData(
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& encrypted) const {
  if (!prf_input.has_value()) {
    return {};
  }

  ExtensionOutputData extension_output_data;
  extension_output_data.prf_result = EvaluateHMAC(encrypted, *prf_input);
  return extension_output_data;
}

std::vector<uint8_t> EvaluateHMAC(
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& encrypted,
    const device::PRFInput& prf_input) {
  const std::string& hmac_secret = encrypted.hmac_secret();
  return prf_input.EvaluateHMAC(
      hmac_secret.empty() ? DeriveHmacSecretFromPrivateKey(
                                base::as_byte_span(encrypted.private_key()))
                          : base::as_byte_span(hmac_secret));
}

std::vector<sync_pb::WebauthnCredentialSpecifics> FilterShadowedCredentials(
    base::span<const sync_pb::WebauthnCredentialSpecifics> passkeys) {
  // Collect all explicitly shadowed credentials.
  base::flat_set<std::string> shadowed_credential_ids;
  for (const sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    for (const std::string& id : passkey.newly_shadowed_credential_ids()) {
      shadowed_credential_ids.emplace(id);
    }
  }
  // For each (user id, rp id) group, keep the newest credential.
  base::flat_set<sync_pb::WebauthnCredentialSpecifics, PasskeyComparator>
      grouped;
  for (const sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    if (shadowed_credential_ids.contains(passkey.credential_id())) {
      continue;
    }
    const auto passkey_it = grouped.insert(passkey).first;
    if (passkey_it->creation_time() < passkey.creation_time()) {
      *passkey_it = passkey;
    }
  }
  return std::vector<sync_pb::WebauthnCredentialSpecifics>(
      std::make_move_iterator(grouped.begin()),
      std::make_move_iterator(grouped.end()));
}

bool IsPasskeyValid(const sync_pb::WebauthnCredentialSpecifics& passkey) {
  const size_t cred_id_size = passkey.credential_id().size();
  return passkey.sync_id().size() == kSyncIdLength &&
         !passkey.rp_id().empty() && cred_id_size >= kCredentialIdMinLength &&
         cred_id_size <= kCredentialIdMaxLength &&
         passkey.user_id().length() <= kUserIdMaxLength &&
         (passkey.has_private_key() || passkey.has_encrypted() ||
          (passkey.has_security_domain_encrypted() &&
           base::FeatureList::IsEnabled(
               features::kDeviceAuthorizationPasskeyDecryption)));
}

bool IsGpmPasskeyValid(const sync_pb::WebauthnCredentialSpecifics& passkey) {
  return IsPasskeyValid(passkey) &&
         passkey.credential_id().size() == kGpmCreatedCredentialIdLength;
}

std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
GeneratePasskeyAndEncryptSecrets(
    std::string_view rp_id,
    const PasskeyModel::UserEntity& user_entity,
    base::span<const uint8_t> trusted_vault_key,
    int32_t trusted_vault_key_version,
    base::optional_ref<const DeviceAuthorizationKey> device_authorization_key,
    const ExtensionInputData& extension_input_data,
    ExtensionOutputData* extension_output_data) {
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_sync_id(base::RandBytesAsString(kSyncIdLength));
  specifics.set_credential_id(
      base::RandBytesAsString(kGpmCreatedCredentialIdLength));
  specifics.set_rp_id(std::string(rp_id));
  specifics.set_user_id(user_entity.id.data(), user_entity.id.size());
  specifics.set_user_name(user_entity.name);
  specifics.set_user_display_name(user_entity.display_name);
  specifics.set_creation_time(base::Time::Now().InMillisecondsSinceUnixEpoch());

  sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted;
  auto ec_key = crypto::keypair::PrivateKey::GenerateEcP256();
  std::vector<uint8_t> private_key_pkcs8 = ec_key.ToPrivateKeyInfo();
  encrypted.set_private_key(
      {private_key_pkcs8.begin(), private_key_pkcs8.end()});
  if (extension_input_data.has_prf) {
    encrypted.set_hmac_secret(base::RandBytesAsString(kHmacSecretSize));
  }
  CHECK(EncryptWebauthnCredentialSpecificsData(
      trusted_vault_key, device_authorization_key, encrypted, &specifics));
  CHECK(base::FeatureList::IsEnabled(
            features::kDeviceAuthorizationPasskeyEncryption)
            ? specifics.has_security_domain_encrypted()
            : specifics.has_encrypted());
  specifics.set_key_version(trusted_vault_key_version);

  if (extension_output_data) {
    *extension_output_data = extension_input_data.ToOutputData(encrypted);
  }

  std::vector<uint8_t> public_key_spki = ec_key.ToSubjectPublicKeyInfo();
  return {std::move(specifics), std::move(public_key_spki)};
}

bool DecryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> trusted_vault_key,
    base::span<const uint8_t> device_authorization_key,
    const sync_pb::WebauthnCredentialSpecifics& in,
    sync_pb::WebauthnCredentialSpecifics_Encrypted* out) {
  switch (in.encrypted_data_case()) {
    case sync_pb::WebauthnCredentialSpecifics::kEncrypted: {
      base::expected<std::vector<uint8_t>, AesGcmDecryptionError> decrypted =
          DecryptAesGcm(DerivePasskeyEncryptionSecret(trusted_vault_key),
                        in.encrypted(),
                        kAadWebauthnCredentialSpecificsEncrypted);
      if (!decrypted.has_value()) {
        DVLOG(1) << "WebauthnCredentialSpecifics.encrypted "
                 << ToString(decrypted.error());
        return false;
      }
      sync_pb::WebauthnCredentialSpecifics_Encrypted msg;
      if (!msg.ParseFromString(base::as_string_view(*decrypted))) {
        DVLOG(1) << "Parsing WebauthnCredentialSpecifics.encrypted failed";
        return false;
      }
      *out = std::move(msg);
      return true;
    }
    case sync_pb::WebauthnCredentialSpecifics::kPrivateKey: {
      base::expected<std::vector<uint8_t>, AesGcmDecryptionError> decrypted =
          DecryptAesGcm(DerivePasskeyEncryptionSecret(trusted_vault_key),
                        in.private_key(),
                        kAadWebauthnCredentialSpecificsPrivateKey);
      if (!decrypted.has_value()) {
        DVLOG(1) << "WebauthnCredentialSpecifics.private_key "
                 << ToString(decrypted.error());
        return false;
      }
      *out = sync_pb::WebauthnCredentialSpecifics_Encrypted();
      out->set_private_key(base::as_string_view(*decrypted));
      return true;
    }
    case sync_pb::WebauthnCredentialSpecifics::kSecurityDomainEncrypted: {
      // TODO(crbug.com/405036154): Add metrics / crash dumps before rollout.
      if (!base::FeatureList::IsEnabled(
              features::kDeviceAuthorizationPasskeyDecryption)) {
        DVLOG(1) << "Decrypting "
                    "WebauthnCredentialSpecifics.security_domain_encrypted is "
                    "disabled";
        return false;
      }
      if (device_authorization_key.empty()) {
        DVLOG(1) << "Missing device authorization key";
        return false;
      }
      base::expected<std::vector<uint8_t>, AesGcmDecryptionError>
          security_domain_decrypted = DecryptAesGcm(
              DerivePasskeyEncryptionSecret(trusted_vault_key),
              in.security_domain_encrypted(),
              kAadWebauthnCredentialSpecificsSecurityDomainEncrypted);
      if (!security_domain_decrypted.has_value()) {
        DVLOG(1) << "WebauthnCredentialSpecifics.security_domain_encrypted "
                 << ToString(security_domain_decrypted.error());
        return false;
      }
      sync_pb::WebauthnCredentialSpecifics_SecurityDomainEncrypted
          security_domain_msg;
      if (!security_domain_msg.ParseFromString(
              base::as_string_view(*security_domain_decrypted))) {
        DVLOG(1) << "Parsing "
                    "WebauthnCredentialSpecifics.security_domain_encrypted "
                    "failed";
        return false;
      }
      base::expected<std::vector<uint8_t>, AesGcmDecryptionError> decrypted =
          DecryptAesGcm(
              DeriveDeviceAuthorizationEncryptionSecret(
                  device_authorization_key),
              security_domain_msg.device_authorization_encrypted(),
              kAadWebauthnCredentialSpecificsDeviceAuthorizationEncrypted);
      if (!decrypted.has_value()) {
        DVLOG(1) << "SecurityDomainEncrypted.device_authorization_encrypted "
                 << ToString(decrypted.error());
        return false;
      }
      sync_pb::WebauthnCredentialSpecifics_Encrypted msg;
      if (!msg.ParseFromString(base::as_string_view(*decrypted))) {
        DVLOG(1)
            << "Parsing SecurityDomainEncrypted.device_authorization_encrypted "
               "failed";
        return false;
      }
      *out = std::move(msg);
      return true;
    }
    case sync_pb::WebauthnCredentialSpecifics::ENCRYPTED_DATA_NOT_SET:
      DVLOG(1) << "WebauthnCredentialSpecifics.encrypted_data not set";
      return false;
  }
  NOTREACHED();
}

bool EncryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> trusted_vault_key,
    base::optional_ref<const DeviceAuthorizationKey> device_authorization_key,
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& in,
    sync_pb::WebauthnCredentialSpecifics* out) {
  CHECK_NE(out, nullptr);
  std::string plaintext;
  if (!in.SerializeToString(&plaintext)) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          features::kDeviceAuthorizationPasskeyEncryption)) {
    if (!device_authorization_key.has_value() ||
        device_authorization_key->key().empty()) {
      DVLOG(1) << "Missing device authorization key";
      return false;
    }
    sync_pb::WebauthnCredentialSpecifics_SecurityDomainEncrypted
        security_domain_msg;
    *security_domain_msg.mutable_device_authorization_encrypted() =
        EncryptAesGcm(
            DeriveDeviceAuthorizationEncryptionSecret(
                base::as_byte_span(device_authorization_key->key())),
            plaintext,
            kAadWebauthnCredentialSpecificsDeviceAuthorizationEncrypted);
    std::string security_domain_plaintext;
    if (!security_domain_msg.SerializeToString(&security_domain_plaintext)) {
      return false;
    }
    *out->mutable_security_domain_encrypted() =
        EncryptAesGcm(DerivePasskeyEncryptionSecret(trusted_vault_key),
                      security_domain_plaintext,
                      kAadWebauthnCredentialSpecificsSecurityDomainEncrypted);
    out->set_device_authorization_key_version(
        device_authorization_key->version());
    return true;
  }
  *out->mutable_encrypted() =
      EncryptAesGcm(DerivePasskeyEncryptionSecret(trusted_vault_key), plaintext,
                    kAadWebauthnCredentialSpecificsEncrypted);
  return true;
}

device::AuthenticatorData MakeAuthenticatorDataForAssertion(
    std::string_view rp_id,
    bool did_complete_uv) {
  using Flag = device::AuthenticatorData::Flag;
  uint8_t flags = base::strict_cast<uint8_t>(Flag::kTestOfUserPresence) |
                  base::strict_cast<uint8_t>(Flag::kBackupEligible) |
                  base::strict_cast<uint8_t>(Flag::kBackupState);
  if (did_complete_uv) {
    flags |= base::strict_cast<uint8_t>(Flag::kTestOfUserVerification);
  }
  return device::AuthenticatorData(crypto::hash::Sha256(rp_id), flags,
                                   kSignatureCounter, /*data=*/std::nullopt,
                                   /*extensions=*/std::nullopt);
}

device::AttestationObject MakeAttestationObjectForCreation(
    std::string_view rp_id,
    bool did_complete_uv,
    base::span<const uint8_t> credential_id,
    base::span<const uint8_t> public_key_spki_der) {
  static constexpr std::array<const uint8_t, 16> kGpmAaguid{
      0xea, 0x9b, 0x8d, 0x66, 0x4d, 0x01, 0x1d, 0x21,
      0x3c, 0xe4, 0xb6, 0xb4, 0x8c, 0xb5, 0x75, 0xd4};

  using Flag = device::AuthenticatorData::Flag;
  std::unique_ptr<device::PublicKey> public_key =
      device::PublicKey::FromSpkiDer(
          base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256),
          public_key_spki_der);
  device::AttestedCredentialData attested_credential_data(
      kGpmAaguid, credential_id, std::move(public_key));
  uint8_t flags = base::strict_cast<uint8_t>(Flag::kTestOfUserPresence) |
                  base::strict_cast<uint8_t>(Flag::kBackupEligible) |
                  base::strict_cast<uint8_t>(Flag::kBackupState) |
                  base::strict_cast<uint8_t>(Flag::kAttestation);
  if (did_complete_uv) {
    flags |= base::strict_cast<uint8_t>(Flag::kTestOfUserVerification);
  }
  device::AuthenticatorData authenticator_data(
      crypto::hash::Sha256(rp_id), flags, kSignatureCounter,
      std::move(attested_credential_data), /*extensions=*/std::nullopt);
  return device::AttestationObject(
      std::move(authenticator_data),
      std::make_unique<device::NoneAttestationStatement>());
}

std::optional<std::vector<uint8_t>> GenerateEcSignature(
    base::span<const uint8_t> pkcs8_ec_private_key,
    base::span<const uint8_t> signed_over_data) {
  auto ec_private_key =
      crypto::keypair::PrivateKey::FromPrivateKeyInfo(pkcs8_ec_private_key);
  if (!ec_private_key || !ec_private_key->IsEcP256()) {
    return std::nullopt;
  }

  return crypto::sign::Sign(crypto::sign::SignatureKind::ECDSA_SHA256,
                            *ec_private_key, signed_over_data);
}

bool IsSupportedAlgorithm(int32_t algorithm) {
  return algorithm ==
         base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256);
}

}  // namespace webauthn::passkey_model_utils
