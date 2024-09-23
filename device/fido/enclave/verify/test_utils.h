// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_TEST_UTILS_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_TEST_UTILS_H_

#include <string_view>

#include "base/time/time.h"
#include "device/fido/enclave/verify/claim.h"

namespace device::enclave {

EndorsementStatement MakeEndorsementStatement(
    std::string_view statement_type,
    std::string_view predicate_type,
    base::Time issued_on,
    base::Time not_before,
    base::Time not_after,
    std::string_view endorsement_type = kEndorsementV2);

EndorsementStatement MakeValidEndorsementStatement();

std::string GetContentsFromFile(std::string_view file_name);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_TEST_UTILS_H_
