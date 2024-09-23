// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/enclave/verify/claim.h"

#include "base/time/time.h"
#include "device/fido/enclave/verify/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::enclave {

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

TEST(ClaimTest,
     VerifyValidityDuration_EndorsementStatementHasNoValidity_ReturnsFalse) {
  EndorsementStatement endorsement_statement;
  EXPECT_FALSE(
      VerifyValidityDuration(base::Time::Now(), endorsement_statement));
}

TEST(ClaimTest,
     VerifyValidityDuration_ValidityNotBeforeHasNotPassed_ReturnsFalse) {
  EXPECT_FALSE(VerifyValidityDuration(base::Time::FromTimeT(10),
                                      MakeValidEndorsementStatement()));
}

TEST(ClaimTest, VerifyValidityDuration_ValidityNotAfterPassed_ReturnsFalse) {
  EXPECT_FALSE(VerifyValidityDuration(base::Time::FromTimeT(25),
                                      MakeValidEndorsementStatement()));
}

TEST(ClaimTest, VerifyValidityDuration_ValidityIsCurrent_ReturnsTrue) {
  EXPECT_TRUE(VerifyValidityDuration(base::Time::FromTimeT(17),
                                     MakeValidEndorsementStatement()));
}

TEST(ClaimTest, ParseEndorsement_ValidJsonFile_SuccessfullyReturnsValue) {
  std::string endorsement = GetContentsFromFile("endorsement.json");
  auto endorsement_statement = ParseEndorsementStatement(
      base::make_span(reinterpret_cast<const uint8_t*>(endorsement.data()),
                      endorsement.size()));
  ASSERT_TRUE(endorsement_statement.has_value());
  ASSERT_TRUE(ValidateEndorsement(*endorsement_statement));
}

TEST(ClaimTest, ParseEndorsement_InvalidJsonFile_ReturnsErrorMessage) {
  std::string endorsement = GetContentsFromFile("endorsement_novalidity.json");
  auto endorsement_statement = ParseEndorsementStatement(
      base::make_span(reinterpret_cast<const uint8_t*>(endorsement.data()),
                      endorsement.size()));
  ASSERT_FALSE(endorsement_statement.has_value());
  ASSERT_EQ(endorsement_statement.error(),
            "can't parse predicate from endorsement.");
}

}  // namespace device::enclave
