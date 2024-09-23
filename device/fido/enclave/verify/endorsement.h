// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_ENDORSEMENT_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_ENDORSEMENT_H_

#include <cstdint>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "device/fido/enclave/verify/claim.h"

namespace device::enclave {

// Verifies the binary endorsement against log entry and public keys.
bool COMPONENT_EXPORT(DEVICE_FIDO)
    VerifyBinaryEndorsement(base::Time now,
                            base::span<const uint8_t> endorsement,
                            base::span<const uint8_t> signature,
                            base::span<const uint8_t> log_entry,
                            base::span<const uint8_t> endorser_public_key,
                            base::span<const uint8_t> rekor_public_key);

// Verifies endorsement against the given reference values.
bool COMPONENT_EXPORT(DEVICE_FIDO)
    VerifyEndorsementStatement(base::Time now,
                               const EndorsementStatement& statement);

// Verifies that the endorser public key coincides with the one contained in
// the attestation.
bool COMPONENT_EXPORT(DEVICE_FIDO)
    VerifyEndorserPublicKey(base::span<const uint8_t> log_entry,
                            base::span<const uint8_t> endorser_public_key);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_ENDORSEMENT_H_
