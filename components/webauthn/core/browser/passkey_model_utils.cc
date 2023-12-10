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
#include "crypto/random.h"

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
    base::span<const uint8_t> key,
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
      if (!DecryptAes256Gcm(key, ciphertext, nonce,
                            kAadWebauthnCredentialSpecificsEncrypted,
                            &plaintext)) {
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
      if (!DecryptAes256Gcm(key, ciphertext, nonce,
                            kAadWebauthnCredentialSpecificsPrivateKey,
                            &plaintext)) {
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
  NOTREACHED_NORETURN();
}

bool EncryptWebauthnCredentialSpecificsData(
    base::span<const uint8_t> key,
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
  if (!EncryptAes256Gcm(key, plaintext, nonce,
                        kAadWebauthnCredentialSpecificsEncrypted,
                        &ciphertext)) {
    return false;
  }
  *out->mutable_encrypted() = base::StrCat({nonce, ciphertext});
  return true;
}

}  // namespace webauthn::passkey_model_utils
