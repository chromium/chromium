// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_REQUEST_OUTCOME_ENUMS_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_REQUEST_OUTCOME_ENUMS_H_

namespace content {

// GetAssertionOutcome corresponds to metrics enum
// WebAuthenticationGetAssertionOutcome, and must be kept in sync with the
// definition in tools/metrics/histograms/metadata/webauthn/enums.xml. These
// must not be reordered and numeric values must not be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webauthn
// GENERATED_JAVA_PREFIX_TO_STRIP: k
enum class GetAssertionOutcome {
  kSuccess = 0,
  kSecurityError = 1,
  kUserCancellation = 2,
  kCredentialNotRecognized = 3,
  kUnknownResponseFromAuthenticator = 4,
  kRkNotSupported = 5,
  kUvNotSupported = 6,
  kSoftPinBlock = 7,
  kHardPinBlock = 8,
  kPlatformNotAllowed = 9,
  kHybridTransportError = 10,
  kFilterBlock = 11,
  kEnclaveError = 12,
  kUiTimeout = 13,
  kOtherFailure = 14,
  kBlockedByEmbedder = 15,
};

// MakeCredentialOutcome corresponds to metrics enum
// WebAuthenticationMakeCredentialOutcome, and must be kept in sync with the
// definition in tools/metrics/histograms/metadata/webauthn/enums.xml. These
// must not be reordered and numeric values must not be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webauthn
// GENERATED_JAVA_PREFIX_TO_STRIP: k
enum class MakeCredentialOutcome {
  kSuccess = 0,
  kSecurityError = 1,
  kUserCancellation = 2,
  kCredentialExcluded = 3,
  kUnknownResponseFromAuthenticator = 4,
  kRkNotSupported = 5,
  kUvNotSupported = 6,
  kLargeBlobNotSupported = 7,
  kAlgorithmNotSupported = 8,
  kSoftPinBlock = 9,
  kHardPinBlock = 10,
  kStorageFull = 11,
  kPlatformNotAllowed = 12,
  kHybridTransportError = 13,
  kFilterBlock = 14,
  kEnclaveError = 15,
  kUiTimeout = 16,
  kOtherFailure = 17,
  kConditionalCreateFailure = 18,
  kBlockedByEmbedder = 19,
};

// LINT.IfChange(AuthenticationRequestMode)

// This must match the `WebAuthenticationRequestMode` in
// tools/metrics/histograms/metadata/webauthn/enums.xml. These must not be
// reordered and numeric values must not be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webauthn
// GENERATED_JAVA_PREFIX_TO_STRIP: k
enum class AuthenticationRequestMode {
  kModalWebAuthn = 0,
  kConditional = 1,
  kPayment = 2,
  kPasskeyUpgrade = 3,
  kImmediate = 4,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:WebAuthenticationRequestMode)

// LINT.IfChange(CredentialRequestResult)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webauthn
// GENERATED_JAVA_PREFIX_TO_STRIP: k
enum class CredentialRequestResult {
  kTimeout = 0,
  kUserCancelled = 1,

  kWinNativeSuccess = 2,
  kWinNativeError = 3,

  kTouchIDSuccess = 4,
  kTouchIDError = 5,

  kChromeOSSuccess = 6,
  kChromeOSError = 7,

  kPhoneSuccess = 8,
  kPhoneError = 9,

  kICloudKeychainSuccess = 10,
  kICloudKeychainError = 11,

  kEnclaveSuccess = 12,
  kEnclaveError = 13,

  kOtherSuccess = 14,
  kOtherError = 15,

  kMaxValue = kOtherError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:WebAuthenticationCredentialRequestResult)

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_REQUEST_OUTCOME_ENUMS_H_
