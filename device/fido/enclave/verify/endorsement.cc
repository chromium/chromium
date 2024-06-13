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

bool VerifyBinaryEndorsement(base::Time now,
                             base::span<const uint8_t> endorsement,
                             base::span<const uint8_t> signature,
                             base::span<const uint8_t> log_entry,
                             base::span<const uint8_t> endorser_public_key,
                             base::span<const uint8_t> rekor_public_key) {
  auto endorsement_statement = ParseEndorsementStatement(endorsement);
  if (!endorsement_statement.has_value()) {
    return false;
  }
  if (log_entry.size() != 0) {
    if (!VerifyEndorsementStatement(now, endorsement_statement.value()) ||
        !VerifyRekorLogEntry(log_entry, rekor_public_key, endorsement) ||
        !VerifyEndorserPublicKey(log_entry, endorser_public_key)) {
      return false;
    }
    return true;
  } else {
    if (rekor_public_key.size() != 0) {
      return false;
    }
    return VerifySignatureRaw(signature, endorsement, endorser_public_key)
        .has_value();
  }
}

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
