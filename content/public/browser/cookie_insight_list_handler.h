// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COOKIE_INSIGHT_LIST_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_COOKIE_INSIGHT_LIST_HANDLER_H_

#include <optional>

#include "content/common/content_export.h"
#include "content/public/browser/cookie_insight_list_data.h"
#include "net/cookies/cookie_inclusion_status.h"

namespace content {

// The CookieInsightListHandler class allows an embedder to provide
// a readiness list input from custom sources.
class CONTENT_EXPORT CookieInsightListHandler {
 public:
  virtual ~CookieInsightListHandler() = default;

  // Returns the singleton instance.
  static CookieInsightListHandler& GetInstance();

  // Sets the handler's CookieInsightList.
  virtual void set_insight_list(std::string_view json_content) = 0;

  // Returns a CookieIssueInsight based on the data in the handler's
  // CookieInsightList.
  //
  // Returns nullopt if no CookieIssueInsight could be retrieved.
  virtual std::optional<CookieIssueInsight> GetInsight(
      std::string_view cookie_domain,
      const net::CookieInclusionStatus& status) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_INSIGHT_LIST_HANDLER_H_
