// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_UNEXPORTABLE_KEY_METRICS_H_
#define CRYPTO_UNEXPORTABLE_KEY_METRICS_H_

#include "crypto/crypto_export.h"

namespace crypto {

// Records UMA metrics of TPM availability, latency and successful usage.
// Does the work on a new background task.
CRYPTO_EXPORT void MaybeMeasureTpmOperations();

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

// Exported for testing
CRYPTO_EXPORT void MeasureTpmOperationsInternalForTesting();
}  // namespace internal

}  // namespace crypto

#endif  // CRYPTO_UNEXPORTABLE_KEY_METRICS_H_
