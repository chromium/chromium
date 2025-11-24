// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/imported_passkey_checker.h"

#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"

namespace webauthn {

// TODO(crbug.com/458337350): Check things outside of the
// WebauthnCredentialSpecifics scope, e.g. unsupported private key algorithms.
ImportedPasskeyStatus CheckImportedPasskey(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  // Set in ios/chrome/browser/credential_exchange/model/credential_importer.mm.
  CHECK_EQ(passkey.sync_id().size(), passkey_model_utils::kSyncIdLength);

  if (passkey.credential_id().size() < kCredentialIdMinLength) {
    return ImportedPasskeyStatus::kCredentialIdTooShort;
  }

  if (passkey.credential_id().size() > kCredentialIdMaxLength) {
    return ImportedPasskeyStatus::kCredentialIdTooLong;
  }

  if (passkey.user_id().size() > passkey_model_utils::kUserIdMaxLength) {
    return ImportedPasskeyStatus::kUserIdTooLong;
  }

  if (!passkey.has_private_key() && !passkey.has_encrypted()) {
    return ImportedPasskeyStatus::kPrivateKeyMissing;
  }

  if (passkey.rp_id().empty()) {
    return ImportedPasskeyStatus::kRpIdMissing;
  }

  return ImportedPasskeyStatus::kOk;
}

}  // namespace webauthn
