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
#include "content/public/browser/cookie_insight_list_data.h"
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
