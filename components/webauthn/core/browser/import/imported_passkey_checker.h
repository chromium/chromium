// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_IMPORTED_PASSKEY_CHECKER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_IMPORTED_PASSKEY_CHECKER_H_

#include <cstddef>

namespace sync_pb {
class WebauthnCredentialSpecifics;
}  // namespace sync_pb

namespace webauthn {

// Represents status of a validity check for an about to be imported passkey.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ImportedPasskeyStatus)
enum class ImportedPasskeyStatus {
  // All required fields are present and conform to WebAuthn spec
  // (https://www.w3.org/TR/webauthn-2).
  kOk = 0,
  // Credential ID does not conform to the spec-defined bounds
  // (https://www.w3.org/TR/webauthn-2/#credential-id).
  kCredentialIdTooShort = 1,
  kCredentialIdTooLong = 2,
  // User ID exceeds the spec-defined upper bound
  // (https://www.w3.org/TR/webauthn-2/#user-handle).
  kUserIdTooLong = 3,
  // Private key is a required field
  // (https://www.w3.org/TR/webauthn-2/#credential-private-key).
  kPrivateKeyMissing = 4,
  // Relying Party Identifier is a required field
  // (https://www.w3.org/TR/webauthn-2/#relying-party-identifier).
  kRpIdMissing = 5,
  kMaxValue = kRpIdMissing,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/webauthn/enums.xml:PasskeyImportStatus)

// Checks the validity of a passkey that is about to be imported. This mostly
// includes conformance to the WebAuthn spec (more details in possible statuses
// above).
ImportedPasskeyStatus CheckImportedPasskey(
    const sync_pb::WebauthnCredentialSpecifics& passkey);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_IMPORTED_PASSKEY_CHECKER_H_
