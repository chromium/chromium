// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_inclusion_status.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(CookieInclusionStatusTest, IncludeStatus) {
  int num_exclusion_reasons =
      static_cast<int>(CookieInclusionStatus::NUM_EXCLUSION_REASONS);
  int num_warning_reasons =
      static_cast<int>(CookieInclusionStatus::NUM_WARNING_REASONS);
  // Zero-argument constructor
  CookieInclusionStatus status;
  EXPECT_TRUE(status.IsInclude());
  for (int i = 0; i < num_exclusion_reasons; ++i) {
    EXPECT_FALSE(status.HasExclusionReason(
        static_cast<CookieInclusionStatus::ExclusionReason>(i)));
  }
  for (int i = 0; i < num_warning_reasons; ++i) {
    EXPECT_FALSE(status.HasWarningReason(
        static_cast<CookieInclusionStatus::WarningReason>(i)));
  }
  EXPECT_FALSE(
      status.HasExclusionReason(CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR));
}

TEST(CookieInclusionStatusTest, ExcludeStatus) {
  int num_exclusion_reasons =
      static_cast<int>(CookieInclusionStatus::NUM_EXCLUSION_REASONS);
  // Test exactly one exclusion reason and multiple (two) exclusion reasons.
  for (int i = 0; i < num_exclusion_reasons; ++i) {
    auto reason1 = static_cast<CookieInclusionStatus::ExclusionReason>(i);
    CookieInclusionStatus status_one_reason(reason1);
    EXPECT_FALSE(status_one_reason.IsInclude());
    EXPECT_TRUE(status_one_reason.HasExclusionReason(reason1));
    EXPECT_TRUE(status_one_reason.HasOnlyExclusionReason(reason1));

    for (int j = 0; j < num_exclusion_reasons; ++j) {
      if (i == j)
        continue;
      auto reason2 = static_cast<CookieInclusionStatus::ExclusionReason>(j);

      EXPECT_FALSE(status_one_reason.HasExclusionReason(reason2));
      EXPECT_FALSE(status_one_reason.HasOnlyExclusionReason(reason2));

      CookieInclusionStatus status_two_reasons = status_one_reason;
      status_two_reasons.AddExclusionReason(reason2);
      EXPECT_FALSE(status_two_reasons.IsInclude());
      EXPECT_TRUE(status_two_reasons.HasExclusionReason(reason1));
      EXPECT_TRUE(status_two_reasons.HasExclusionReason(reason2));
      EXPECT_FALSE(status_two_reasons.HasOnlyExclusionReason(reason1));
      EXPECT_FALSE(status_two_reasons.HasOnlyExclusionReason(reason2));
    }
  }
}

TEST(CookieInclusionStatusTest, AddExclusionReason) {
  CookieInclusionStatus status;
  status.AddWarningReason(
      CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE);
  status.AddExclusionReason(CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR}));
  // Adding an exclusion reason other than
  // EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX or
  // EXCLUDE_SAMESITE_NONE_INSECURE should clear any SameSite warning.
  EXPECT_FALSE(status.ShouldWarn());

  status = CookieInclusionStatus();
  status.AddWarningReason(
      CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
  status.AddExclusionReason(
      CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX);
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting(
      {CookieInclusionStatus::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}));
  EXPECT_TRUE(status.HasExactlyWarningReasonsForTesting(
      {CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT}));
}

TEST(CookieInclusionStatusTest, CheckEachWarningReason) {
  CookieInclusionStatus status;

  int num_warning_reasons =
      static_cast<int>(CookieInclusionStatus::NUM_WARNING_REASONS);
  EXPECT_FALSE(status.ShouldWarn());
  for (int i = 0; i < num_warning_reasons; ++i) {
    auto reason = static_cast<CookieInclusionStatus::WarningReason>(i);
    status.AddWarningReason(reason);
    EXPECT_TRUE(status.IsInclude());
    EXPECT_TRUE(status.ShouldWarn());
    EXPECT_TRUE(status.HasWarningReason(reason));
    for (int j = 0; j < num_warning_reasons; ++j) {
      if (i == j)
        continue;
      EXPECT_FALSE(status.HasWarningReason(
          static_cast<CookieInclusionStatus::WarningReason>(j)));
    }
    status.RemoveWarningReason(reason);
    EXPECT_FALSE(status.ShouldWarn());
  }
}

TEST(CookieInclusionStatusTest, RemoveExclusionReason) {
  CookieInclusionStatus status(CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
  ASSERT_TRUE(
      status.HasExclusionReason(CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR));

  status.RemoveExclusionReason(CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
  EXPECT_FALSE(
      status.HasExclusionReason(CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR));

  // Removing a nonexistent exclusion reason doesn't do anything.
  ASSERT_FALSE(
      status.HasExclusionReason(CookieInclusionStatus::NUM_EXCLUSION_REASONS));
  status.RemoveExclusionReason(CookieInclusionStatus::NUM_EXCLUSION_REASONS);
  EXPECT_FALSE(
      status.HasExclusionReason(CookieInclusionStatus::NUM_EXCLUSION_REASONS));
}

TEST(CookieInclusionStatusTest, RemoveWarningReason) {
  CookieInclusionStatus status(
      CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR,
      CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE);
  EXPECT_TRUE(status.ShouldWarn());
  ASSERT_TRUE(status.HasWarningReason(
      CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE));

  status.RemoveWarningReason(
      CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE);
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_FALSE(status.HasWarningReason(
      CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE));

  // Removing a nonexistent warning reason doesn't do anything.
  ASSERT_FALSE(status.HasWarningReason(
      CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT));
  status.RemoveWarningReason(
      CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
  EXPECT_FALSE(status.ShouldWarn());
  EXPECT_FALSE(status.HasWarningReason(
      CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT));
}

TEST(CookieInclusionStatusTest, HasDowngradeWarning) {
  std::vector<CookieInclusionStatus::WarningReason> downgrade_warnings = {
      CookieInclusionStatus::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE,
      CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE,
      CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE,
      CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE,
      CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE,
  };

  CookieInclusionStatus empty_status;
  EXPECT_FALSE(empty_status.HasDowngradeWarning());

  CookieInclusionStatus not_downgrade;
  not_downgrade.AddWarningReason(
      CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
  EXPECT_FALSE(not_downgrade.HasDowngradeWarning());

  for (auto warning : downgrade_warnings) {
    CookieInclusionStatus status;
    status.AddWarningReason(warning);
    CookieInclusionStatus::WarningReason reason;

    EXPECT_TRUE(status.HasDowngradeWarning(&reason));
    EXPECT_EQ(warning, reason);
  }
}

TEST(CookieInclusionStatusTest, ShouldRecordDowngradeMetrics) {
  EXPECT_TRUE(CookieInclusionStatus::MakeFromReasonsForTesting({})
                  .ShouldRecordDowngradeMetrics());

  EXPECT_TRUE(CookieInclusionStatus::MakeFromReasonsForTesting(
                  {
                      CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT,
                  })
                  .ShouldRecordDowngradeMetrics());

  EXPECT_TRUE(CookieInclusionStatus::MakeFromReasonsForTesting(
                  {
                      CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
                  })
                  .ShouldRecordDowngradeMetrics());

  EXPECT_TRUE(CookieInclusionStatus::MakeFromReasonsForTesting(
                  {
                      CookieInclusionStatus::
                          EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
                  })
                  .ShouldRecordDowngradeMetrics());

  // Note: the following cases cannot occur under normal circumstances.
  EXPECT_TRUE(CookieInclusionStatus::MakeFromReasonsForTesting(
                  {
                      CookieInclusionStatus::
                          EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
                      CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
                  })
                  .ShouldRecordDowngradeMetrics());
  EXPECT_FALSE(CookieInclusionStatus::MakeFromReasonsForTesting(
                   {
                       CookieInclusionStatus::EXCLUDE_SAMESITE_NONE_INSECURE,
                       CookieInclusionStatus::EXCLUDE_SAMESITE_LAX,
                   })
                   .ShouldRecordDowngradeMetrics());
}

TEST(CookieInclusionStatusTest, RemoveExclusionReasons) {
  CookieInclusionStatus status =
      CookieInclusionStatus::MakeFromReasonsForTesting({
          CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR,
          CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT,
          CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
      });
  ASSERT_TRUE(status.HasExactlyExclusionReasonsForTesting({
      CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR,
      CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT,
      CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
  }));

  status.RemoveExclusionReasons(
      {CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR,
       CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR,
       CookieInclusionStatus::EXCLUDE_SAMESITE_STRICT});
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting({
      CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
  }));

  // Removing a nonexistent exclusion reason doesn't do anything.
  ASSERT_FALSE(
      status.HasExclusionReason(CookieInclusionStatus::NUM_EXCLUSION_REASONS));
  status.RemoveExclusionReasons({CookieInclusionStatus::NUM_EXCLUSION_REASONS});
  EXPECT_TRUE(status.HasExactlyExclusionReasonsForTesting({
      CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
  }));
}

TEST(CookieInclusionStatusTest, ValidateExclusionAndWarningFromWire) {
  uint32_t exclusion_reasons = 0ul;
  uint32_t warning_reasons = 0ul;

  EXPECT_TRUE(CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
      exclusion_reasons, warning_reasons));

  exclusion_reasons = static_cast<uint32_t>(~0ul);
  warning_reasons = static_cast<uint32_t>(~0ul);
  EXPECT_FALSE(CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
      exclusion_reasons, warning_reasons));
  EXPECT_FALSE(CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
      exclusion_reasons, 0u));
  EXPECT_FALSE(CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
      0u, warning_reasons));

  exclusion_reasons = (1u << CookieInclusionStatus::EXCLUDE_DOMAIN_MISMATCH);
  warning_reasons = (1u << CookieInclusionStatus::WARN_TREATED_AS_SAMEPARTY);
  EXPECT_TRUE(CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
      exclusion_reasons, warning_reasons));

  exclusion_reasons = (1u << CookieInclusionStatus::NUM_EXCLUSION_REASONS);
  warning_reasons = (1u << CookieInclusionStatus::NUM_WARNING_REASONS);
  EXPECT_FALSE(CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
      exclusion_reasons, warning_reasons));

  exclusion_reasons =
      (1u << (CookieInclusionStatus::NUM_EXCLUSION_REASONS - 1));
  warning_reasons = (1u << (CookieInclusionStatus::NUM_WARNING_REASONS - 1));
  EXPECT_TRUE(CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
      exclusion_reasons, warning_reasons));
}

TEST(CookieInclusionStatusTest, ExcludedByUserPreferences) {
  CookieInclusionStatus status =
      CookieInclusionStatus::MakeFromReasonsForTesting(
          {CookieInclusionStatus::ExclusionReason::EXCLUDE_USER_PREFERENCES});
  EXPECT_TRUE(status.ExcludedByUserPreferences());

  status = CookieInclusionStatus::MakeFromReasonsForTesting({
      CookieInclusionStatus::ExclusionReason::EXCLUDE_USER_PREFERENCES,
      CookieInclusionStatus::ExclusionReason::EXCLUDE_FAILURE_TO_STORE,
  });
  EXPECT_FALSE(status.ExcludedByUserPreferences());

  status = CookieInclusionStatus::MakeFromReasonsForTesting({
      CookieInclusionStatus::ExclusionReason::EXCLUDE_USER_PREFERENCES,
      CookieInclusionStatus::ExclusionReason::
          EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
  });
  EXPECT_TRUE(status.ExcludedByUserPreferences());

  status = CookieInclusionStatus::MakeFromReasonsForTesting({
      CookieInclusionStatus::ExclusionReason::
          EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
  });
  EXPECT_FALSE(status.ExcludedByUserPreferences());

  status = CookieInclusionStatus::MakeFromReasonsForTesting({
      CookieInclusionStatus::ExclusionReason::
          EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
      CookieInclusionStatus::ExclusionReason::EXCLUDE_FAILURE_TO_STORE,
  });
  EXPECT_FALSE(status.ExcludedByUserPreferences());

  status = CookieInclusionStatus::MakeFromReasonsForTesting({
      CookieInclusionStatus::ExclusionReason::EXCLUDE_USER_PREFERENCES,
      CookieInclusionStatus::ExclusionReason::
          EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
      CookieInclusionStatus::ExclusionReason::EXCLUDE_FAILURE_TO_STORE,
  });
  EXPECT_FALSE(status.ExcludedByUserPreferences());
}

}  // namespace net
