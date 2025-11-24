// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_model_utils.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/cbor/writer.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "crypto/aead.h"
#include "crypto/hash.h"
#include "crypto/hkdf.h"
#include "crypto/keypair.h"
#include "crypto/random.h"
#include "crypto/sign.h"
#include "device/fido/attestation_object.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"

namespace webauthn::passkey_model_utils {

namespace {

// The byte length of the WebauthnCredentialSpecifics `credential_id` field.
constexpr size_t kCredentialIdLength = 16u;

// The length of the nonce prefix used for AES-256-GCM encryption of
// `WebAuthnCredentialSpecifics.encrypted_data` (both `private_key` and
// `encrypted` oneof cases).
constexpr size_t kWebAuthnCredentialSpecificsEncryptedDataNonceLength = 12;

// The AAD parameter for the AES-256 encryption of
// `WebAuthnCredentialSpecifics.encrypted`.
constexpr std::string_view kAadWebauthnCredentialSpecificsEncrypted =
    "WebauthnCredentialSpecifics.Encrypted";

// The AAD parameter for the AES-256 encryption of
// `WebAuthnCredentialSpecifics.private_key` (empty).
constexpr std::string_view kAadWebauthnCredentialSpecificsPrivateKey = "";

// Signature counter, as defined in the w3c spec here:
// https://www.w3.org/TR/webauthn-2/#signature-counter
constexpr uint8_t kSignatureCounter[4] = {0};

constexpr size_t kEncryptionSecretSize = 32;

constexpr size_t kHmacSecretSize = 32;

struct PasskeyComparator {
  bool operator()(const sync_pb::WebauthnCredentialSpecifics& a,
                  const sync_pb::WebauthnCredentialSpecifics& b) const {
    return std::tie(a.rp_id(), a.user_id()) < std::tie(b.rp_id(), b.user_id());
  }
};

bool DecryptAes256Gcm(base::span<const uint8_t> key,
                      std::string_view ciphertext,
                      std::string_view nonce,
                      std::string_view aad,
                      std::string* plaintext) {
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(key);
  return aead.Open(ciphertext, nonce, aad, plaintext);
}

bool EncryptAes256Gcm(base::span<const uint8_t> key,
                      std::string_view plaintext,
                      std::string_view nonce,
                      std::string_view aad,
                      std::string* ciphertext) {
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(key);
  return aead.Seal(plaintext, nonce, aad, ciphertext);
}

std::array<uint8_t, kEncryptionSecretSize> DerivePasskeyEncryptionSecret(
    base::span<const uint8_t> trusted_vault_key) {
  constexpr std::string_view kHkdfInfo =
      "KeychainApplicationKey:gmscore_module:com.google.android.gms.fido";
  return crypto::HkdfSha256<kEncryptionSecretSize>(
      trusted_vault_key,
      /*salt=*/base::span<const uint8_t>(),
      base::as_bytes(base::span(kHkdfInfo)));
}

std::array<uint8_t, kHmacSecretSize> DeriveHmacSecretFromPrivateKey(
    base::span<const uint8_t> private_key) {
  CHECK(!private_key.empty());
  constexpr std::string_view kHkdfInfo = "derived PRF HMAC secret";
  return crypto::HkdfSha256<kEncryptionSecretSize>(
      private_key,
      /*salt=*/base::span<const uint8_t>(),
      base::as_bytes(base::span(kHkdfInfo)));
}

}  // namespace

ExtensionOutputData::ExtensionOutputData() = default;
ExtensionOutputData::ExtensionOutputData(const ExtensionOutputData&) = default;
ExtensionOutputData::~ExtensionOutputData() = default;

ExtensionInputData::ExtensionInputData(base::span<const uint8_t> prf_input1,
                                       base::span<const uint8_t> prf_input2) {
  // prf_input must be created even if prf_input1 is empty, as it is an
  // indication the the PRF extension is requested.
  prf_input = device::PRFInput();
  if (!prf_input1.empty()) {
    prf_input->input1.insert(prf_input->input1.end(), prf_input1.begin(),
                             prf_input1.end());
    if (!prf_input2.empty()) {
      std::vector<uint8_t> input2;
      input2.insert(input2.end(), prf_input2.begin(), prf_input2.end());
      prf_input->input2 = input2;
    }
  }
  prf_input->HashInputsIntoSalts();
}

ExtensionInputData::ExtensionInputData() = default;
ExtensionInputData::ExtensionInputData(const ExtensionInputData&) = default;
ExtensionInputData::~ExtensionInputData() = default;

bool ExtensionInputData::hasPRF() const {
  return prf_input.has_value();
}

ExtensionOutputData ExtensionInputData::ToOutputData(
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& encrypted) const {
  if (!hasPRF() || prf_input->input1.empty()) {
    return {};
  }

  ExtensionOutputData extension_output_data;
  extension_output_data.prf_result = EvaluateHMAC(encrypted);
  return extension_output_data;
}

std::vector<uint8_t> ExtensionInputData::EvaluateHMAC(
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& encrypted) const {
  const std::string& hmac_secret = encrypted.hmac_secret();
  return prf_input->EvaluateHMAC(
      hmac_secret.empty() ? DeriveHmacSecretFromPrivateKey(
                                base::as_byte_span(encrypted.private_key()))
                          : base::as_byte_span(hmac_secret));
}

SerializedAttestationObject::SerializedAttestationObject() = default;
SerializedAttestationObject::SerializedAttestationObject(
    SerializedAttestationObject&& other) = default;
SerializedAttestationObject::~SerializedAttestationObject() = default;

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

bool IsGpmPasskeyValid(const sync_pb::WebauthnCredentialSpecifics& passkey) {
  return passkey.sync_id().size() == kSyncIdLength &&
         passkey.credential_id().size() == kCredentialIdLength &&
         !passkey.rp_id().empty() &&
         passkey.user_id().length() <= kUserIdMaxLength &&
         (passkey.has_private_key() || passkey.has_encrypted());
}

std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
GeneratePasskeyAndEncryptSecrets(std::string_view rp_id,
                                 const PasskeyModel::UserEntity& user_entity,
                                 base::span<const uint8_t> trusted_vault_key,
                                 int32_t trusted_vault_key_version,
                                 const ExtensionInputData& extension_input_data,
                                 ExtensionOutputData* extension_output_data) {
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_sync_id(base::RandBytesAsString(kSyncIdLength));
  specifics.set_credential_id(base::RandBytesAsString(kCredentialIdLength));
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
  if (extension_input_data.hasPRF()) {
    encrypted.set_hmac_secret(base::RandBytesAsString(kHmacSecretSize));
  }
  CHECK(EncryptWebauthnCredentialSpecificsData(trusted_vault_key, encrypted,
                                               &specifics));
  CHECK(specifics.has_encrypted());
  specifics.set_key_version(trusted_vault_key_version);

  if (extension_output_data) {
    *extension_output_data = extension_input_data.ToOutputData(encrypted);
  }

  std::vector<uint8_t> public_key_spki = ec_key.ToSubjectPublicKeyInfo();
  return {std::move(specifics), std::move(public_key_spki)};
}

bool DecryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> trusted_vault_key,
    const sync_pb::WebauthnCredentialSpecifics& in,
    sync_pb::WebauthnCredentialSpecifics_Encrypted* out) {
  switch (in.encrypted_data_case()) {
    case sync_pb::WebauthnCredentialSpecifics::kEncrypted: {
      if (in.encrypted().size() <
          kWebAuthnCredentialSpecificsEncryptedDataNonceLength) {
        DVLOG(1) << "WebauthnCredentialSpecifics.encrypted has invalid length";
        return false;
      }
      std::string_view nonce =
          std::string_view(in.encrypted())
              .substr(0, kWebAuthnCredentialSpecificsEncryptedDataNonceLength);
      std::string_view ciphertext =
          std::string_view(in.encrypted())
              .substr(kWebAuthnCredentialSpecificsEncryptedDataNonceLength);
      std::string plaintext;
      if (!DecryptAes256Gcm(
              DerivePasskeyEncryptionSecret(trusted_vault_key), ciphertext,
              nonce, kAadWebauthnCredentialSpecificsEncrypted, &plaintext)) {
        DVLOG(1) << "Decrypting WebauthnCredentialSpecifics.encrypted failed";
        return false;
      }
      sync_pb::WebauthnCredentialSpecifics_Encrypted msg;
      if (!msg.ParseFromString(plaintext)) {
        DVLOG(1) << "Parsing WebauthnCredentialSpecifics.encrypted failed";
        return false;
      }
      *out = std::move(msg);
      return true;
    }
    case sync_pb::WebauthnCredentialSpecifics::kPrivateKey: {
      if (in.private_key().size() <
          kWebAuthnCredentialSpecificsEncryptedDataNonceLength) {
        DVLOG(1)
            << "WebauthnCredentialSpecifics.private_key has invalid length";
        return false;
      }
      std::string_view nonce =
          std::string_view(in.private_key())
              .substr(0, kWebAuthnCredentialSpecificsEncryptedDataNonceLength);
      std::string_view ciphertext =
          std::string_view(in.private_key())
              .substr(kWebAuthnCredentialSpecificsEncryptedDataNonceLength);
      std::string plaintext;
      if (!DecryptAes256Gcm(
              DerivePasskeyEncryptionSecret(trusted_vault_key), ciphertext,
              nonce, kAadWebauthnCredentialSpecificsPrivateKey, &plaintext)) {
        DVLOG(1) << "Decrypting WebauthnCredentialSpecifics.private_key failed";
        return false;
      }
      *out = sync_pb::WebauthnCredentialSpecifics_Encrypted();
      out->set_private_key(plaintext);
      return true;
    }
    case sync_pb::WebauthnCredentialSpecifics::kSecurityDomainEncrypted: {
      // TODO(crbug.com/405036010): Implement handling of the new encryption
      // scheme.
      NOTIMPLEMENTED();
      return false;
    }
    case sync_pb::WebauthnCredentialSpecifics::ENCRYPTED_DATA_NOT_SET:
      DVLOG(1) << "WebauthnCredentialSpecifics.encrypted_data not set";
      return false;
  }
  NOTREACHED();
}

bool EncryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> trusted_vault_key,
    const sync_pb::WebauthnCredentialSpecifics_Encrypted& in,
    sync_pb::WebauthnCredentialSpecifics* out) {
  CHECK_NE(out, nullptr);
  std::string plaintext;
  if (!in.SerializeToString(&plaintext)) {
    return false;
  }
  const std::string nonce = base::RandBytesAsString(
      kWebAuthnCredentialSpecificsEncryptedDataNonceLength);
  std::string ciphertext;
  if (!EncryptAes256Gcm(
          DerivePasskeyEncryptionSecret(trusted_vault_key), plaintext, nonce,
          kAadWebauthnCredentialSpecificsEncrypted, &ciphertext)) {
    return false;
  }
  // TODO(crbug.com/405036010): Implement encrypting with the new encryption
  // scheme.
  *out->mutable_encrypted() = base::StrCat({nonce, ciphertext});
  return true;
}

std::vector<uint8_t> MakeAuthenticatorDataForAssertion(std::string_view rp_id,
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
                                   /*extensions=*/std::nullopt)
      .SerializeToByteArray();
}

SerializedAttestationObject MakeAttestationObjectForCreation(
    std::string_view rp_id,
    bool did_complete_uv,
    base::span<const uint8_t> credential_id,
    base::span<const uint8_t> public_key_spki_der) {
  static constexpr std::array<const uint8_t, 16> kGpmAaguid{
      0xea, 0x9b, 0x8d, 0x66, 0x4d, 0x01, 0x1d, 0x21,
      0x3c, 0xe4, 0xb6, 0xb4, 0x8c, 0xb5, 0x75, 0xd4};

  using Flag = device::AuthenticatorData::Flag;
  std::unique_ptr<device::PublicKey> public_key =
      device::P256PublicKey::ParseSpkiDer(
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
  SerializedAttestationObject serialized_attestation_object;
  serialized_attestation_object.authenticator_data =
      authenticator_data.SerializeToByteArray();

  device::AttestationObject attestationObject(
      std::move(authenticator_data),
      std::make_unique<device::NoneAttestationStatement>());
  serialized_attestation_object.attestation_object =
      cbor::Writer::Write(device::AsCBOR(attestationObject)).value();

  return serialized_attestation_object;
}

std::optional<std::vector<uint8_t>> GenerateEcSignature(
    base::span<const uint8_t> pkcs8_ec_private_key,
    base::span<const uint8_t> signed_over_data) {
  auto ec_private_key =
      crypto::keypair::PrivateKey::FromPrivateKeyInfo(pkcs8_ec_private_key);
  if (!ec_private_key || !ec_private_key->IsEc()) {
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
