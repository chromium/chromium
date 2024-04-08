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
#include "device/fido/enclave/types.h"
#include "device/fido/network_context_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace cbor {
class Value;
}

namespace device::enclave {

// Perform a transaction with the enclave.
//
// Serialises and sends `request` and calls `callback` with the response, or
// else `nullopt` if there was an error.
COMPONENT_EXPORT(DEVICE_FIDO)
void Transact(NetworkContextFactory network_context_factory,
              const EnclaveIdentity& enclave,
              std::string access_token,
              std::optional<std::string> reauthentication_token,
              cbor::Value request,
              SigningCallback signing_callback,
              base::OnceCallback<void(std::optional<cbor::Value>)> callback);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_TRANSACT_H_
