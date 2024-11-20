// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_INSIGHT_LIST_HANDLER_H_
#define CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_INSIGHT_LIST_HANDLER_H_

#include <optional>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "content/browser/cookie_insight_list/cookie_insight_list.h"
#include "content/common/content_export.h"
#include "net/cookies/cookie_inclusion_status.h"

namespace content {

// Singleton class that stores a CookieInsightList, which can be queried to
// provide third-party cookie migration insights about a cookie.
class CONTENT_EXPORT CookieInsightListHandler final {
 public:
  CookieInsightListHandler(const CookieInsightListHandler&) = delete;
  CookieInsightListHandler& operator=(const CookieInsightListHandler&) = delete;
  CookieInsightListHandler(const CookieInsightListHandler&&) = delete;
  CookieInsightListHandler& operator=(const CookieInsightListHandler&&) =
      delete;

  ~CookieInsightListHandler();

  static CookieInsightListHandler& GetInstance();  // Singleton

  // Sets the handler's CookieInsightList.
  void set_insight_list(CookieInsightList new_insight_list);

  // Returns a CookieIssueInsight based on the data in the handler's
  // CookieInsightList.
  //
  // Returns nullopt if no CookieIssueInsight could be retrieved.
  std::optional<CookieInsightList::CookieIssueInsight> GetInsight(
      std::string_view cookie_domain,
      const net::CookieInclusionStatus& status) const;

 private:
  CookieInsightListHandler();

  friend class base::NoDestructor<CookieInsightListHandler>;

  SEQUENCE_CHECKER(sequence_checker_);

  CookieInsightList insight_list_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_INSIGHT_LIST_HANDLER_H_
