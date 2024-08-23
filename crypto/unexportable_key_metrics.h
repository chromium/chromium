// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_UNEXPORTABLE_KEY_METRICS_H_
#define CRYPTO_UNEXPORTABLE_KEY_METRICS_H_

#include <string>

#include "crypto/crypto_export.h"
#include "crypto/unexportable_key.h"

namespace crypto {

enum class TPMOperation {
  kMessageSigning,
  kMessageVerify,
  kWrappedKeyCreation,
  kNewKeyCreation,
};

// Converts the given `operation` to a string representation.
CRYPTO_EXPORT std::string OperationToString(TPMOperation operation);

// Converts the given `algorithm` to a string representation.
CRYPTO_EXPORT std::string AlgorithmToString(
    SignatureVerifier::SignatureAlgorithm algorithm);

// Records UMA metrics of TPM availability, latency and successful usage.
// Does the work on a new background task.
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

// Note that values here are used in a recorded histogram. Don't change
// the values of existing members.
enum class TPMType {
  kNone = 0,
  kHW = 1,
  kVirtual = 2,
  kBoth = 3,
  kMaxValue = 3,
};

// Exported for testing
CRYPTO_EXPORT void MeasureTpmOperationsInternalForTesting();
}  // namespace internal

}  // namespace crypto

#endif  // CRYPTO_UNEXPORTABLE_KEY_METRICS_H_
