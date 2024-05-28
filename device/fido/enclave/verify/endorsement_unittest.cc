// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/endorsement.h"

#include "base/time/time.h"
#include "device/fido/enclave/verify/claim.h"
#include "device/fido/enclave/verify/test_utils.h"
#include "device/fido/enclave/verify/utils.h"
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

TEST(EndorsementTest,
     VerifyEndorserPublicKey_WithValidLogEntryAndKey_ReturnsTrue) {
  auto endorser = ConvertPemToRaw(GetContentsFromFile("endorser.pem"));
  ASSERT_TRUE(endorser.has_value());
  std::string log_entry_str = GetContentsFromFile("logentry.json");
  base::span<const uint8_t> log_entry =
      base::make_span(static_cast<uint8_t*>((uint8_t*)log_entry_str.data()),
                      log_entry_str.size());
  EXPECT_TRUE(VerifyEndorserPublicKey(log_entry, *endorser));
}

TEST(EndorsementTest,
     VerifyEndorserPublicKey_WithInvalidLogEntry_ReturnsFalse) {
  auto endorser = ConvertPemToRaw(GetContentsFromFile("endorser.pem"));
  ASSERT_TRUE(endorser.has_value());
  std::string log_entry_str = GetContentsFromFile("logentry_backslash.json");
  base::span<const uint8_t> log_entry =
      base::make_span(static_cast<uint8_t*>((uint8_t*)log_entry_str.data()),
                      log_entry_str.size());
  EXPECT_FALSE(VerifyEndorserPublicKey(log_entry, *endorser));
}

TEST(EndorsementTest, VerifyEndorserPublicKey_WithInvalidKey_ReturnsFalse) {
  auto endorser = ConvertPemToRaw(GetContentsFromFile("rekor_pub_key.pem"));
  ASSERT_TRUE(endorser.has_value());
  std::string log_entry_str = GetContentsFromFile("logentry.json");
  base::span<const uint8_t> log_entry =
      base::make_span(static_cast<uint8_t*>((uint8_t*)log_entry_str.data()),
                      log_entry_str.size());
  EXPECT_FALSE(VerifyEndorserPublicKey(log_entry, *endorser));
}

}  // namespace device::enclave
