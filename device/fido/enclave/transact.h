// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_TRANSACT_H_
#define DEVICE_FIDO_ENCLAVE_TRANSACT_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "device/fido/enclave/types.h"
#include "device/fido/network_context_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace cbor {
class Value;
}

namespace device::enclave {

// This is used for metrics and must be kept in sync with the corresponding
// entry in tools/metrics/histograms/metadata/webauthn/enums.xml.
// Entries should not be renumbered or reused.
enum class EnclaveTransactionResult {
  kSuccess = 0,
  kUnknownClient = 1,
  kMissingKey = 2,
  kSignatureVerificationFailed = 3,
  kHandshakeFailed = 4,
  kDecryptionFailed = 5,
  kParseFailure = 6,
  kOtherError = 7,

  kMaxValue = kOtherError,
};

enum class TransactError {
  // The first group of error codes represent values received from the service
  // for whole message failures, which means no individual requests within the
  // message were processed.
  // They need to match 'Error' in
  // //third_party/cloud_authenticator/processor/src/lib.rs.
  kUnknownClient = 0,
  kMissingKey = 1,
  kSignatureVerificationFailed = 2,

  // The following are internal codes that don't correspond to any specific
  // service response:
  kHandshakeFailed,
  kSigningFailed,
  kUnknownServiceError,
  // kOther mainly encompasses invalid responses received from the service.
  kOther,
};

// Perform a transaction with the enclave.
//
// Serialises and sends `request` and calls `callback` with the response, or
// else `nullopt` if there was an error.
COMPONENT_EXPORT(DEVICE_FIDO)
void Transact(
    NetworkContextFactory network_context_factory,
    const EnclaveIdentity& enclave,
    std::string access_token,
    std::optional<std::string> reauthentication_token,
    cbor::Value request,
    SigningCallback signing_callback,
    base::OnceCallback<void(base::expected<cbor::Value, TransactError>)>
        callback);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_TRANSACT_H_
