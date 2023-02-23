// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/common_source_info.h"

#include "base/time/time.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
namespace {

using ::attribution_reporting::mojom::SourceType;

TEST(CommonSourceInfoTest, NoExpiryForImpression_DefaultUsed) {
  const base::Time source_time = base::Time::Now();

  for (auto source_type : kSourceTypes) {
    EXPECT_EQ(source_time + base::Days(30),
              CommonSourceInfo::GetExpiryTime(
                  /*declared_expiry=*/absl::nullopt, source_time, source_type));
  }
}

TEST(CommonSourceInfoTest, NoReportWindowForImpression_NullOptReturned) {
  EXPECT_EQ(absl::nullopt, CommonSourceInfo::GetReportWindowTime(
                               /*declared_window=*/absl::nullopt,
                               /*source_time=*/base::Time::Now()));
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

TEST(CommonSourceInfoTest, LargeReportWindowSpecified_ClampedTo30Days) {
  constexpr base::TimeDelta declared_report_window = base::Days(60);
  const base::Time source_time = base::Time::Now();

  EXPECT_EQ(source_time + base::Days(30),
            CommonSourceInfo::GetReportWindowTime(declared_report_window,
                                                  source_time));
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

TEST(CommonSourceInfoTest, SmallReportWindowSpecified_ClampedTo1Day) {
  const struct {
    base::TimeDelta declared_report_window;
    base::TimeDelta want_report_window;
  } kTestCases[] = {
      {base::Days(-1), base::Days(1)},
      {base::Days(0), base::Days(1)},
      {base::Days(1) - base::Milliseconds(1), base::Days(1)},
  };

  const base::Time source_time = base::Time::Now();

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(source_time + test_case.want_report_window,
              CommonSourceInfo::GetReportWindowTime(
                  test_case.declared_report_window, source_time));
  }
}

TEST(CommonSourceInfoTest, NonWholeDayImpressionExpirySpecified_Rounded) {
  const struct {
    SourceType source_type;
    base::TimeDelta declared_expiry;
    base::TimeDelta want_expiry;
  } kTestCases[] = {
      {SourceType::kNavigation, base::Hours(36), base::Hours(36)},
      {SourceType::kEvent, base::Hours(36), base::Days(2)},

      {SourceType::kNavigation, base::Days(1) + base::Milliseconds(1),
       base::Days(1) + base::Milliseconds(1)},
      {SourceType::kEvent, base::Days(1) + base::Milliseconds(1),
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

TEST(CommonSourceInfoTest, ReportWindowSpecified_WindowOverrideDefault) {
  constexpr base::TimeDelta declared_expiry =
      base::Days(10) + base::Milliseconds(1);
  const base::Time source_time = base::Time::Now();

  // Verify no rounding occurs.
  EXPECT_EQ(
      source_time + declared_expiry,
      CommonSourceInfo::GetReportWindowTime(declared_expiry, source_time));
}

}  // namespace
}  // namespace content
