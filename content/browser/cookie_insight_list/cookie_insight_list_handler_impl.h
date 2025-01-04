// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_INSIGHT_LIST_HANDLER_IMPL_H_
#define CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_INSIGHT_LIST_HANDLER_IMPL_H_

#include <optional>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "content/browser/cookie_insight_list/cookie_insight_list.h"
#include "content/common/content_export.h"
#include "content/public/browser/cookie_insight_list_data.h"
#include "content/public/browser/cookie_insight_list_handler.h"
#include "net/cookies/cookie_inclusion_status.h"

namespace content {

// Singleton class that stores a CookieInsightList, which can be queried to
// provide third-party cookie migration insights about a cookie.
class CONTENT_EXPORT CookieInsightListHandlerImpl
    : public CookieInsightListHandler {
 public:
  CookieInsightListHandlerImpl(const CookieInsightListHandlerImpl&) = delete;
  CookieInsightListHandlerImpl& operator=(const CookieInsightListHandlerImpl&) =
      delete;
  CookieInsightListHandlerImpl(const CookieInsightListHandlerImpl&&) = delete;
  CookieInsightListHandlerImpl& operator=(
      const CookieInsightListHandlerImpl&&) = delete;
  ~CookieInsightListHandlerImpl() override;

  static CookieInsightListHandlerImpl& GetInstance();

  // CoookieInsightListHandler:
  void set_insight_list(std::string_view json_content) override;
  std::optional<CookieIssueInsight> GetInsight(
      std::string_view cookie_domain,
      const net::CookieInclusionStatus& status) const override;

 private:
  CookieInsightListHandlerImpl();

  friend class base::NoDestructor<CookieInsightListHandlerImpl>;

  SEQUENCE_CHECKER(sequence_checker_);

  CookieInsightList insight_list_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_INSIGHT_LIST_COOKIE_INSIGHT_LIST_HANDLER_IMPL_H_
