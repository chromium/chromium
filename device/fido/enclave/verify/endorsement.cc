// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/endorsement.h"

#include "base/base64.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "device/fido/enclave/verify/claim.h"
#include "device/fido/enclave/verify/rekor.h"
#include "device/fido/enclave/verify/utils.h"

namespace device::enclave {

bool VerifyEndorsementStatement(base::Time now,
                                const EndorsementStatement& statement) {
  if (!ValidateEndorsement(statement) ||
      !VerifyValidityDuration(now, statement)) {
    return false;
  }
  return true;
}

bool VerifyEndorserPublicKey(base::span<const uint8_t> log_entry,
                             base::span<const uint8_t> endorser_public_key) {
  std::optional<Body> body = GetRekorLogEntryBody(log_entry);
  if (!body.has_value()) {
    return false;
  }
  std::string actual_pem;
  if (!base::Base64Decode(body->spec.generic_signature.public_key.content,
                          &actual_pem)) {
    return false;
  }
  auto actual = ConvertPemToRaw(actual_pem);
  if (!actual.has_value()) {
    return false;
  }
  return base::ranges::equal(*actual, endorser_public_key);
}

}  // namespace device::enclave
