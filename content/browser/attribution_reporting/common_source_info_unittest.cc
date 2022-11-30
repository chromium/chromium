// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/common_source_info.h"

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

TEST(CommonSourceInfoTest, NoExpiryForImpression_DefaultUsed) {
  const base::Time source_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(source_time + base::Days(30),
              CommonSourceInfo::GetExpiryTime(
                  /*declared_expiry=*/absl::nullopt, source_time, source_type));
  }
}

TEST(CommonSourceInfoTest, LargeImpressionExpirySpecified_ClampedTo30Days) {
  constexpr base::TimeDelta declared_expiry = base::Days(60);
  const base::Time source_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(source_time + base::Days(30),
              CommonSourceInfo::GetExpiryTime(declared_expiry, source_time,
                                              source_type));
  }
}

TEST(CommonSourceInfoTest, SmallImpressionExpirySpecified_ClampedTo1Day) {
  const struct {
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {base::Days(-1), base::Days(1)},
      {base::Days(0), base::Days(1)},
      {base::Days(1) - base::Milliseconds(1), base::Days(1)},
  };

  const base::Time source_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    for (const auto& test_case : kTestCases) {
      EXPECT_EQ(source_time + test_case.want_expiry,
                CommonSourceInfo::GetExpiryTime(test_case.declared_expiry,
                                                source_time, source_type));
    }
  }
}

TEST(CommonSourceInfoTest, NonWholeDayImpressionExpirySpecified_Rounded) {
  const struct {
    AttributionSourceType source_type;
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {AttributionSourceType::kNavigation, base::Hours(36), base::Hours(36)},
      {AttributionSourceType::kEvent, base::Hours(36), base::Days(2)},

      {AttributionSourceType::kNavigation,
       base::Days(1) + base::Milliseconds(1),
       base::Days(1) + base::Milliseconds(1)},
      {AttributionSourceType::kEvent, base::Days(1) + base::Milliseconds(1),
       base::Days(1)},
  };

  const base::Time source_time = base::Time::Now();

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        source_time + test_case.want_expiry,
        CommonSourceInfo::GetExpiryTime(test_case.declared_expiry, source_time,
                                        test_case.source_type));
  }
}

TEST(CommonSourceInfoTest, ImpressionExpirySpecified_ExpiryOverrideDefault) {
  constexpr base::TimeDelta declared_expiry = base::Days(10);
  const base::Time source_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(source_time + base::Days(10),
              CommonSourceInfo::GetExpiryTime(declared_expiry, source_time,
                                              source_type));
  }
}

}  // namespace content
