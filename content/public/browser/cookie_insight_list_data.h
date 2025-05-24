// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COOKIE_INSIGHT_LIST_DATA_H_
#define CONTENT_PUBLIC_BROWSER_COOKIE_INSIGHT_LIST_DATA_H_

#include <string>

namespace content {

// Contains information about a domain's third-party cookie use status.
struct DomainInfo {
  // Link to table entry in third-party cookie migration readiness list.
  //
  // TODO(crbug.com/384720467): Use GURL for entry_url instead of string.
  std::string entry_url;

  bool operator==(const DomainInfo&) const = default;
};

// Represents the category of insight that a reported cookie issue
// falls under.
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

// Contains information about a reported cookie issue, categorizing the issue
// and providing information about the cookie's domain's third-party cookie use
// status.
struct CookieIssueInsight {
  // The insight type.
  InsightType type;
  // Information about the cookie's domain.
  DomainInfo domain_info;

  bool operator==(const CookieIssueInsight&) const = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_INSIGHT_LIST_DATA_H_
