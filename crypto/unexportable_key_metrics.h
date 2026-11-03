// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_UNEXPORTABLE_KEY_METRICS_H_
#define CRYPTO_UNEXPORTABLE_KEY_METRICS_H_

#include <string>

#include "crypto/crypto_export.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"

namespace crypto {

// LINT.IfChange(TPMOperation)
enum class TPMOperation {
  // An operation to sign data with a TPM key.
  kMessageSigning,
  // An operation to verify a TPM signature.
  kMessageVerify,
  // An operation to create a TPM key from a wrapped key or a similar
  // representation identifying a TPM key.
  kWrappedKeyCreation,
  // An operation to create a new TPM-protected key.
  kNewKeyCreation,
  // An operation to export a wrapped key (or a similar representation
  // identifying a TPM key) from an existing TPM key.
  kWrappedKeyExport,
  // An operation to select a signature algorithm supported by the TPM.
  kSelectAlgorithm,
  // An operation to delete a TPM-protected key.
  kKeyDeletion,
  // An operation to certify a key with an attestation key.
  kKeyCertification,
  // An operation to create a new TPM-protected attestation key.
  kNewAttestationKeyCreation,
  // An operation to create an attestation key from a wrapped key.
  kWrappedAttestationKeyCreation,
  // An operation to export a wrapped attestation key.
  kWrappedAttestationKeyExport,
  // An operation to hash data using the TPM.
  kMessageHashing,
  // An operation to sign data with a restricted signing key (such as a TPM 2.0
  // Attestation Identity Key) using restricted credentials.
  kRestrictedMessageSigning,
  // An operation to verify the signature of a restricted signing key.
  kRestrictedMessageVerify,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/crypto/histograms.xml:TpmOperation)

// Converts the given `operation` to a string representation.
CRYPTO_EXPORT std::string OperationToString(TPMOperation operation);

// Converts the given `algorithm` to a string representation.
CRYPTO_EXPORT std::string AlgorithmToString(sign::SignatureKind algorithm);

// Records UMA metrics of TPM availability, latency and successful usage.
// Runs slow, blocking TPM operations on the calling sequence, which must allow
// blocking.
CRYPTO_EXPORT void MaybeMeasureTpmOperations(
    UnexportableKeyProvider::Config config);

// internal namespace to be used by tests only
namespace internal {

// Note that values here are used in a recorded histogram. Don't change
// the values of existing members.
enum class TPMSupport {
  kNone = 0,
  kRSA = 1,
  kECDSA = 2,
  kMaxValue = 2,
};

// Whether a Windows machine has TPM hardware for unexportable keys and, if not,
// why. Recorded as Crypto.KeyHardwareSupport.Win.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WinKeyHardwareSupport)
enum class WinKeyHardwareSupport {
  // tbs.dll could not be loaded.
  kTbsUnavailable = 0,
  // `Tbsi_GetDeviceInfo()` returned `TBS_E_TPM_NOT_FOUND`. There is no TPM, it
  // is disabled in firmware, or no driver claimed it.
  kTpmNotFound = 1,
  // `Tbsi_GetDeviceInfo()` failed with any other error.
  kGetDeviceInfoFailed = 2,
  kTpm12 = 3,
  kTpm20 = 4,
  // TBS reported a TPM version other than 1.2 or 2.0.
  kUnknownTpmVersion = 5,
  kMaxValue = kUnknownTpmVersion,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/crypto/enums.xml:WinKeyHardwareSupport)

// Whether a Mac has a Secure Enclave and the executable has the
// keychain-access-groups entitlement that unexportable keys need, or which of
// the two is missing. Recorded as Crypto.KeyHardwareSupport.Mac.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(MacKeyHardwareSupport)
enum class MacKeyHardwareSupport {
  // There is no Secure Enclave.
  kNoSecureEnclave = 0,
  // There is a Secure Enclave, but the executable lacks the
  // keychain-access-groups entitlement for the configured access group.
  kMissingEntitlement = 1,
  // There is a Secure Enclave and the executable has the entitlement.
  kSecureEnclave = 2,
  kMaxValue = kSecureEnclave,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/crypto/enums.xml:MacKeyHardwareSupport)

// Exported for testing
CRYPTO_EXPORT void MeasureTpmOperationsInternalForTesting();
}  // namespace internal

}  // namespace crypto

#endif  // CRYPTO_UNEXPORTABLE_KEY_METRICS_H_
