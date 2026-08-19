// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/imported_passkey_checker.h"

#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/import/passkey_import_candidate.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "crypto/keypair.h"

namespace webauthn {

ImportedPasskeyStatus CheckImportedPasskey(
    const PasskeyImportCandidate& passkey) {
  if (passkey.credential_id.size() <
      passkey_model_utils::kCredentialIdMinLength) {
    return ImportedPasskeyStatus::kCredentialIdTooShort;
  }

  if (passkey.credential_id.size() >
      passkey_model_utils::kCredentialIdMaxLength) {
    return ImportedPasskeyStatus::kCredentialIdTooLong;
  }

  if (passkey.user_id.size() > passkey_model_utils::kUserIdMaxLength) {
    return ImportedPasskeyStatus::kUserIdTooLong;
  }

  if (passkey.private_key.empty()) {
    return ImportedPasskeyStatus::kPrivateKeyMissing;
  }

  if (passkey.rp_id.empty()) {
    return ImportedPasskeyStatus::kRpIdMissing;
  }

  auto private_key =
      crypto::keypair::PrivateKey::FromPrivateKeyInfo(passkey.private_key);
  if (!private_key) {
    return ImportedPasskeyStatus::kPrivateKeyInvalid;
  }
  if (!private_key->IsEcP256()) {
    return ImportedPasskeyStatus::kPrivateKeyUnsupportedAlgorithm;
  }

  return ImportedPasskeyStatus::kOk;
}

}  // namespace webauthn
