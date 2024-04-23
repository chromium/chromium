// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/endorsement.h"

#include "base/time/time.h"
#include "device/fido/enclave/verify/claim.h"

namespace device::enclave {

bool VerifyEndorsementStatement(base::Time now,
                                const EndorsementStatement& statement) {
  if (!ValidateEndorsement(statement) ||
      !VerifyValidityDuration(now, statement)) {
    return false;
  }
  return true;
}

}  // namespace device::enclave
