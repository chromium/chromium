// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_INSIGHT_LIST_H_
#define CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_INSIGHT_LIST_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "net/cookies/cookie_inclusion_status.h"

namespace content {

// CookieInsightList stores a parsed Cookie Readiness List map, and contains the
// logic for retrieving third-party cookie migration insights about a cookie.
class CONTENT_EXPORT CookieInsightList {
 public:
  CookieInsightList();
  ~CookieInsightList();

  CookieInsightList(const CookieInsightList&);
  CookieInsightList& operator=(const CookieInsightList&);

  bool operator==(content::CookieInsightList const&) const;

  // Contains information about a domain's third-party cookie use status
  // retrieved from the third-party cookie migration readiness list:
  // https://github.com/privacysandbox/privacy-sandbox-dev-support/blob/main/3pc-migration-readiness.md
  //
  // Defined as a struct for ease of extensibility in the future.
  struct DomainInfo {
    // Link to table entry in third-party cookie migration readiness list
    std::string entry_url;

    bool operator==(const DomainInfo&) const = default;
  };

  enum class InsightType {
    // Cookie domain has an entry in third-party cookie migration readiness
    // list:
    // https://github.com/privacysandbox/privacy-sandbox-dev-support/blob/main/3pc-migration-readiness.md
    kGitHubResource,
    // Cookie is exempted due to a grace period:
    // https://developers.google.com/privacy-sandbox/cookies/temporary-exceptions/grace-period
    kGracePeriod,
    // Cookie is exempted due a heuristics-based exemptiuon:
    // https://developers.google.com/privacy-sandbox/cookies/temporary-exceptions/heuristics-based-exception
    kHeuristics
  };
  struct CookieIssueInsight {
    InsightType type;
    // Link to table entry in third-party cookie migration readiness list
    DomainInfo domain_info;

    bool operator==(const CookieIssueInsight&) const = default;
  };

  // Maps cookie domains as strings to DomainInfo.
  using ReadinessListMap = base::flat_map<std::string, DomainInfo>;

  explicit CookieInsightList(ReadinessListMap readiness_list_map);

  std::optional<CookieIssueInsight> GetInsight(
      std::string_view cookie_domain,
      const net::CookieInclusionStatus& status) const;

 private:
  ReadinessListMap readiness_list_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_INSIGHT_LIST_H_
