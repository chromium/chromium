// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/claim.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::enclave {

EndorsementStatement MakeEndorsementStatement(
    std::string_view statement_type,
    std::string_view predicate_type,
    base::Time issued_on,
    base::Time not_before,
    base::Time not_after,
    std::string_view endorsement_type = kEndorsementV2) {
  EndorsementStatement endorsement_statement;
  endorsement_statement.predicate.issued_on = issued_on;
  endorsement_statement.type = statement_type;
  endorsement_statement.predicate_type = predicate_type;
  ClaimValidity claim_validity;
  claim_validity.not_after = not_after;
  claim_validity.not_before = not_before;
  endorsement_statement.predicate.validity = claim_validity;
  endorsement_statement.predicate.claim_type = endorsement_type;
  return endorsement_statement;
}

EndorsementStatement MakeValidEndorsementStatement() {
  return MakeEndorsementStatement(
      kStatementV1, kPredicateV2, base::Time::FromTimeT(10),
      base::Time::FromTimeT(15), base::Time::FromTimeT(20));
}

TEST(ClaimTest, ValidateClaim_WithAllValidFields_ReturnsTrue) {
  EXPECT_TRUE(ValidateClaim(MakeValidEndorsementStatement()));
}

TEST(ClaimTest, ValidateClaim_WithInvalidStatementType_ReturnsFalse) {
  EXPECT_FALSE(ValidateClaim(MakeEndorsementStatement(
      "bad statement type", /*predicate_type=*/kPredicateV2,
      /*issued_on=*/base::Time::FromTimeT(10),
      /*not_before=*/base::Time::FromTimeT(15),
      /*not_after=*/base::Time::FromTimeT(20))));
}

TEST(ClaimTest, ValidateClaim_WithInvalidPredicateType_ReturnsFalse) {
  EXPECT_FALSE(ValidateClaim(MakeEndorsementStatement(
      /*statement_type=*/kStatementV1, "bad predicate type",
      /*issued_on=*/base::Time::FromTimeT(10),
      /*not_before=*/base::Time::FromTimeT(15),
      /*not_after=*/base::Time::FromTimeT(20))));
}

TEST(ClaimTest, ValidateClaim_WithInvalidNotBeforeTime_ReturnsFalse) {
  EXPECT_FALSE(ValidateClaim(MakeEndorsementStatement(
      /*statement_type=*/kStatementV1, /*predicate_type=*/kPredicateV2,
      /*issued_on=*/base::Time::FromTimeT(10),
      /*not_before=*/base::Time::FromTimeT(9),
      /*not_after=*/base::Time::FromTimeT(20))));
}

TEST(ClaimTest, ValidateClaim_WithInvalidNotAfterTime_ReturnsFalse) {
  EXPECT_FALSE(device::enclave::ValidateClaim(MakeEndorsementStatement(
      /*statement_type=*/kStatementV1, /*predicate_type=*/kPredicateV2,
      /*issued_on=*/base::Time::FromTimeT(10),
      /*not_before=*/base::Time::FromTimeT(21),
      /*not_after=*/base::Time::FromTimeT(20))));
}

TEST(ClaimTest, ValidateEndorsement_WithInvalidClaim_ReturnsFalse) {
  EXPECT_FALSE(ValidateEndorsement(MakeEndorsementStatement(
      "bad statement type", /*predicate_type=*/kPredicateV2,
      /*issued_on=*/base::Time::FromTimeT(10),
      /*not_before=*/base::Time::FromTimeT(15),
      /*not_after=*/base::Time::FromTimeT(20))));
}

TEST(ClaimTest, ValidateEndorsement_WithInvalidEndorsementType_ReturnsFalse) {
  EXPECT_FALSE(ValidateEndorsement(MakeEndorsementStatement(
      /*statement_type=*/kStatementV1, /*predicate_type=*/kPredicateV2,
      /*issued_on=*/base::Time::FromTimeT(10),
      /*not_before=*/base::Time::FromTimeT(15),
      /*not_after=*/base::Time::FromTimeT(20), "bad endorsement type")));
}

TEST(ClaimTest, ValidateEndorsement_WithAllValidFields_ReturnsTrue) {
  EXPECT_TRUE(ValidateEndorsement(MakeValidEndorsementStatement()));
}

}  // namespace device::enclave
