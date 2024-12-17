// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_insight_list/cookie_insight_list.h"

#include <optional>
#include <string>

#include "content/public/browser/cookie_insight_list_data.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using testing::Optional;

TEST(CookieInsightListTest, GetInsight_Null_NoEntry) {
  EXPECT_EQ(CookieInsightList().GetInsight("unknown", /*status=*/{}),
            std::nullopt);
}

TEST(CookieInsightListTest, GetInsight_GitHubResource) {
  base::flat_map<std::string, DomainInfo> domain_map = {
      {"example.com", {"url"}}};

  EXPECT_THAT(
      CookieInsightList(domain_map).GetInsight("example.com", /*status=*/{}),
      Optional(
          CookieIssueInsight{InsightType::kGitHubResource, DomainInfo{"url"}}));
}

TEST(CookieInsightListTest, GetInsight_Heuristics) {
  net::CookieInclusionStatus status;
  status.MaybeSetExemptionReason(
      net::CookieInclusionStatus::ExemptionReason::k3PCDHeuristics);

  EXPECT_THAT(CookieInsightList().GetInsight("unknown", status),
              Optional(CookieIssueInsight{InsightType::kHeuristics, {}}));
}

TEST(CookieInsightListTest, GetInsight_GracePeriod) {
  net::CookieInclusionStatus status;
  status.MaybeSetExemptionReason(
      net::CookieInclusionStatus::ExemptionReason::k3PCDMetadata);

  EXPECT_THAT(CookieInsightList().GetInsight("unknown", status),
              Optional(CookieIssueInsight{InsightType::kGracePeriod, {}}));
}

}  // namespace content
