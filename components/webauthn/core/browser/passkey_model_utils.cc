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
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "crypto/aead.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"

namespace webauthn::passkey_model_utils {

namespace {

// The byte length of the WebauthnCredentialSpecifics `sync_id` field.
constexpr size_t kSyncIdLength = 16u;

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

std::vector<uint8_t> DerivePasskeyEncryptionSecret(
    base::span<const uint8_t> trusted_vault_key) {
  constexpr std::string_view kHkdfInfo =
      "KeychainApplicationKey:gmscore_module:com.google.android.gms.fido";
  constexpr size_t kEncryptionSecretSize = 32u;
  return crypto::HkdfSha256(trusted_vault_key,
                            /*salt=*/base::span<const uint8_t>(),
                            base::as_bytes(base::span(kHkdfInfo)),
                            kEncryptionSecretSize);
}

}  // namespace

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

std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
GeneratePasskeyAndEncryptSecrets(std::string_view rp_id,
                                 const PasskeyModel::UserEntity& user_entity,
                                 base::span<const uint8_t> trusted_vault_key,
                                 int32_t trusted_vault_key_version) {
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_sync_id(base::RandBytesAsString(kSyncIdLength));
  specifics.set_credential_id(base::RandBytesAsString(kCredentialIdLength));
  specifics.set_rp_id(std::string(rp_id));
  specifics.set_user_id(user_entity.id.data(), user_entity.id.size());
  specifics.set_user_name(user_entity.name);
  specifics.set_user_display_name(user_entity.display_name);
  specifics.set_creation_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted;
  auto ec_key = crypto::ECPrivateKey::Create();
  std::vector<uint8_t> private_key_pkcs8;
  CHECK(ec_key->ExportPrivateKey(&private_key_pkcs8));
  encrypted.set_private_key(
      {private_key_pkcs8.begin(), private_key_pkcs8.end()});
  CHECK(EncryptWebauthnCredentialSpecificsData(trusted_vault_key, encrypted,
                                               &specifics));
  CHECK(specifics.has_encrypted());
  specifics.set_key_version(trusted_vault_key_version);

  std::vector<uint8_t> public_key_spki;
  CHECK(ec_key->ExportPublicKey(&public_key_spki));
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
  *out->mutable_encrypted() = base::StrCat({nonce, ciphertext});
  return true;
}

std::vector<uint8_t> MakeAuthenticatorDataForAssertion(std::string_view rp_id) {
  using Flag = device::AuthenticatorData::Flag;
  return device::AuthenticatorData(
             crypto::SHA256Hash(base::as_byte_span(rp_id)),
             {Flag::kTestOfUserPresence, Flag::kTestOfUserVerification,
              Flag::kBackupEligible, Flag::kBackupState},
             /*sign_counter=*/0u,
             /*attested_credential_data=*/std::nullopt,
             /*extensions=*/std::nullopt)
      .SerializeToByteArray();
}

std::vector<uint8_t> MakeAuthenticatorDataForCreation(
    std::string_view rp_id,
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
  return device::AuthenticatorData(
             crypto::SHA256Hash(base::as_byte_span(rp_id)),
             {Flag::kTestOfUserPresence, Flag::kTestOfUserVerification,
              Flag::kBackupEligible, Flag::kBackupState, Flag::kAttestation},
             /*sign_counter=*/0u, std::move(attested_credential_data),
             /*extensions=*/std::nullopt)
      .SerializeToByteArray();
}

std::optional<std::vector<uint8_t>> GenerateEcSignature(
    base::span<const uint8_t> pkcs8_ec_private_key,
    base::span<const uint8_t> signed_over_data) {
  auto ec_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(pkcs8_ec_private_key);
  if (!ec_private_key) {
    return std::nullopt;
  }
  auto signer = crypto::ECSignatureCreator::Create(ec_private_key.get());
  std::vector<uint8_t> signature;
  if (!signer->Sign(signed_over_data, &signature)) {
    return std::nullopt;
  }
  return signature;
}

bool IsSupportedAlgorithm(int32_t algorithm) {
  return algorithm ==
         base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256);
}

}  // namespace webauthn::passkey_model_utils
