// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/for_debugging_only_report_util.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class ForDebuggingOnlyReportUtilTest : public testing::Test {
 public:
  ForDebuggingOnlyReportUtilTest() = default;

  ~ForDebuggingOnlyReportUtilTest() override = default;
};

TEST_F(ForDebuggingOnlyReportUtilTest, IsInDebugReportLockout) {
  base::Time now = base::Time::Now();
  EXPECT_FALSE(IsInDebugReportLockout(/*lockout=*/std::nullopt, now));

  EXPECT_FALSE(IsInDebugReportLockout(
      DebugReportLockout(now - base::Days(91), /*duration=*/base::Days(90)),
      now));
  EXPECT_TRUE(IsInDebugReportLockout(
      DebugReportLockout(now - base::Days(89), /*duration=*/base::Days(90)),
      now));
}

TEST_F(ForDebuggingOnlyReportUtilTest, EnableFilteringInFutureTime) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{blink::features::kFledgeSampleDebugReports,
        {{"fledge_enable_filtering_debug_report_starting_from",
          base::StrCat({base::NumberToString(base::Time::Max()
                                                 .ToDeltaSinceWindowsEpoch()
                                                 .InMilliseconds()),
                        "ms"})}}}},
      {});

  base::Time now = base::Time::Now();
  EXPECT_FALSE(IsInDebugReportLockout(
      DebugReportLockout(now, /*duration=*/base::Days(90)), now));
}

TEST_F(ForDebuggingOnlyReportUtilTest, ShouldSampleDebugReport) {
  EXPECT_FALSE(ShouldSampleDebugReport());
}

TEST_F(ForDebuggingOnlyReportUtilTest, DoSampleDebugReportForTesting) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kFledgeDoSampleDebugReportForTesting);
  EXPECT_TRUE(ShouldSampleDebugReport());
}

}  // namespace content
