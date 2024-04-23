// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/endorsement.h"

#include "base/time/time.h"
#include "device/fido/enclave/verify/claim.h"
#include "device/fido/enclave/verify/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::enclave {

TEST(
    EndorsementTest,
    VerifyEndorsementStatement_WithEndorsementHasBadStatementType_ReturnsFalse) {
  EXPECT_FALSE(VerifyEndorsementStatement(
      base::Time::FromTimeT(17),
      MakeEndorsementStatement("bad statement type",
                               /*predicate_type=*/kPredicateV2,
                               /*issued_on=*/base::Time::FromTimeT(10),
                               /*not_before=*/base::Time::FromTimeT(15),
                               /*not_after=*/base::Time::FromTimeT(20))));
}

TEST(EndorsementTest,
     VerifyEndorsementStatement_WithInvalidValidityDuration_ReturnsFalse) {
  EXPECT_FALSE(VerifyEndorsementStatement(base::Time::FromTimeT(10),
                                          MakeValidEndorsementStatement()));
}

TEST(
    EndorsementTest,
    VerifyEndorsementStatement_WithValidEndorsementAndValidityDuration_ReturnsTrue) {
  EXPECT_TRUE(VerifyEndorsementStatement(base::Time::FromTimeT(17),
                                         MakeValidEndorsementStatement()));
}

}  // namespace device::enclave
