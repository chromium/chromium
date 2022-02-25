// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_policy.h"

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(AttributionPolicyTest, NoExpiryForImpression_DefaultUsed) {
  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(
        impression_time + base::Days(30),
        GetExpiryTimeForImpression(
            /*declared_expiry=*/absl::nullopt, impression_time, source_type));
  }
}

TEST(AttributionPolicyTest, LargeImpressionExpirySpecified_ClampedTo30Days) {
  constexpr base::TimeDelta declared_expiry = base::Days(60);
  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(impression_time + base::Days(30),
              GetExpiryTimeForImpression(declared_expiry, impression_time,
                                         source_type));
  }
}

TEST(AttributionPolicyTest, SmallImpressionExpirySpecified_ClampedTo1Day) {
  const struct {
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {base::Days(-1), base::Days(1)},
      {base::Days(0), base::Days(1)},
      {base::Days(1) - base::Milliseconds(1), base::Days(1)},
  };

  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    for (const auto& test_case : kTestCases) {
      EXPECT_EQ(impression_time + test_case.want_expiry,
                GetExpiryTimeForImpression(test_case.declared_expiry,
                                           impression_time, source_type));
    }
  }
}

TEST(AttributionPolicyTest, NonWholeDayImpressionExpirySpecified_Rounded) {
  const struct {
    CommonSourceInfo::SourceType source_type;
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {CommonSourceInfo::SourceType::kNavigation, base::Hours(36),
       base::Hours(36)},
      {CommonSourceInfo::SourceType::kEvent, base::Hours(36), base::Days(2)},

      {CommonSourceInfo::SourceType::kNavigation,
       base::Days(1) + base::Milliseconds(1),
       base::Days(1) + base::Milliseconds(1)},
      {CommonSourceInfo::SourceType::kEvent,
       base::Days(1) + base::Milliseconds(1), base::Days(1)},
  };

  const base::Time impression_time = base::Time::Now();

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        impression_time + test_case.want_expiry,
        GetExpiryTimeForImpression(test_case.declared_expiry, impression_time,
                                   test_case.source_type));
  }
}

TEST(AttributionPolicyTest, ImpressionExpirySpecified_ExpiryOverrideDefault) {
  constexpr base::TimeDelta declared_expiry = base::Days(10);
  const base::Time impression_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(impression_time + base::Days(10),
              GetExpiryTimeForImpression(declared_expiry, impression_time,
                                         source_type));
  }
}

TEST(AttributionPolicyTest, GetFailedReportDelay) {
  const struct {
    int failed_send_attempts;
    absl::optional<base::TimeDelta> expected;
  } kTestCases[] = {
      {1, base::Minutes(5)},
      {2, base::Minutes(15)},
      {3, absl::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected,
              GetFailedReportDelay(test_case.failed_send_attempts))
        << "failed_send_attempts=" << test_case.failed_send_attempts;
  }
}

}  // namespace content
