// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_insight_list/cookie_insight_list.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "content/public/browser/cookie_insight_list_data.h"

namespace content {

CookieInsightList::CookieInsightList(
    base::flat_map<std::string, DomainInfo> readiness_list_map) {
  readiness_list_map_ = std::move(readiness_list_map);
}

CookieInsightList::CookieInsightList() = default;
CookieInsightList::~CookieInsightList() = default;

CookieInsightList::CookieInsightList(const CookieInsightList&) = default;
CookieInsightList& CookieInsightList::operator=(const CookieInsightList&) =
    default;

bool CookieInsightList::operator==(content::CookieInsightList const&) const =
    default;

std::optional<CookieIssueInsight> CookieInsightList::GetInsight(
    std::string_view cookie_domain,
    const net::CookieInclusionStatus& status) const {
  // If a cookie domain has an entry in the GitHub, we opt to use
  // kGitHubResource and populate entry_url. Otherwise, we check
  // if a cookie domain has a Heuristics or Grace Period exception
  auto it = readiness_list_map_.find(cookie_domain);
  if (it == readiness_list_map_.end()) {
    switch (status.exemption_reason()) {
      case net::CookieInclusionStatus::ExemptionReason::k3PCDMetadata:
        return CookieIssueInsight{InsightType::kGracePeriod, {}};
      case net::CookieInclusionStatus::ExemptionReason::k3PCDHeuristics:
        return CookieIssueInsight{InsightType::kHeuristics, {}};
      default:
        return std::nullopt;
    }
  }
  return CookieIssueInsight{InsightType::kGitHubResource,
                            {it->second.entry_url}};
}

}  // namespace content
