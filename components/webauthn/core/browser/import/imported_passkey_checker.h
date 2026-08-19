// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_IMPORTED_PASSKEY_CHECKER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_IMPORTED_PASSKEY_CHECKER_H_

namespace webauthn {

struct PasskeyImportCandidate;

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
  // Private key cannot be parsed as a valid PKCS#8 block.
  kPrivateKeyInvalid = 6,
  // Private key uses an algorithm not supported by GPM.
  kPrivateKeyUnsupportedAlgorithm = 7,
  // Failed to encrypt the passkey data.
  kEncryptionFailed = 8,
  kMaxValue = kEncryptionFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:ImportedPasskeyStatus)

// Checks the validity of a passkey that is about to be imported.
// This includes WebAuthn spec conformance (credential ID and user ID bounds,
// required fields) as well as private key validity and algorithm support.
ImportedPasskeyStatus CheckImportedPasskey(
    const PasskeyImportCandidate& passkey);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_IMPORTED_PASSKEY_CHECKER_H_
