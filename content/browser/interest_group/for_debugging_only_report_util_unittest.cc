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
#include "url/gurl.h"
#include "url/origin.h"

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

TEST_F(ForDebuggingOnlyReportUtilTest, IsInDebugReportCooldown) {
  base::Time now = base::Time::Now();
  url::Origin origin_a = url::Origin::Create(GURL("https://example-a.com"));
  url::Origin origin_b = url::Origin::Create(GURL("https://example-b.com"));
  url::Origin origin_c = url::Origin::Create(GURL("https://example-c.com"));

  std::map<url::Origin, DebugReportCooldown> cooldowns_map;
  cooldowns_map.emplace(
      origin_a,
      DebugReportCooldown(now, DebugReportCooldownType::kShortCooldown));
  cooldowns_map.emplace(
      origin_b,
      DebugReportCooldown(
          now - blink::features::kFledgeDebugReportShortCooldown.Get() -
              base::Days(1),
          DebugReportCooldownType::kShortCooldown));

  // origin_a is in cooldown.
  EXPECT_TRUE(IsInDebugReportCooldown(origin_a, cooldowns_map, now));
  // origin_b's cooldown expired.
  EXPECT_FALSE(IsInDebugReportCooldown(origin_b, cooldowns_map, now));
  // No cooldown entry for origin_c.
  EXPECT_FALSE(IsInDebugReportCooldown(origin_c, cooldowns_map, now));
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

  url::Origin origin_a = url::Origin::Create(GURL("https://example-a.com"));
  std::map<url::Origin, DebugReportCooldown> cooldowns_map;
  cooldowns_map.emplace(
      origin_a,
      DebugReportCooldown(now, DebugReportCooldownType::kShortCooldown));
  EXPECT_FALSE(IsInDebugReportCooldown(origin_a, cooldowns_map, now));
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
