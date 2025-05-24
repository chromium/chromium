// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_READINESS_LIST_COOKIE_READINESS_LIST_PARSER_H_
#define CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_READINESS_LIST_COOKIE_READINESS_LIST_PARSER_H_

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "content/browser/cookie_insight_list/cookie_insight_list.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT CookieReadinessListParser {
 public:
  CookieReadinessListParser() = delete;
  ~CookieReadinessListParser() = delete;

  CookieReadinessListParser(const CookieReadinessListParser&) = delete;
  CookieReadinessListParser& operator=(const CookieReadinessListParser&) =
      delete;

  // Parses Cookie Readiness List (as a stringified JSON dictionary) from
  // the third-party cookie migration readiness list:
  // https://github.com/privacysandbox/privacy-sandbox-dev-support/blob/main/3pc-migration-readiness.md
  static CookieInsightList ParseReadinessList(std::string_view json_content);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_READINESS_LIST_COOKIE_READINESS_LIST_PARSER_H_
