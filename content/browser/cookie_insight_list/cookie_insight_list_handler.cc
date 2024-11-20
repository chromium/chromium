// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_insight_list/cookie_insight_list_handler.h"

#include <optional>
#include <string_view>
#include <utility>

#include "content/browser/cookie_insight_list/cookie_insight_list.h"

namespace content {

CookieInsightListHandler::CookieInsightListHandler() = default;
//  Since CookieInsightListHandler is a singleton,
//  the destructor will never run.
CookieInsightListHandler::~CookieInsightListHandler() = default;

CookieInsightListHandler& CookieInsightListHandler::GetInstance() {
  static base::NoDestructor<CookieInsightListHandler> instance;
  return *instance.get();
}

void CookieInsightListHandler::set_insight_list(
    CookieInsightList new_insight_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  insight_list_ = std::move(new_insight_list);
}

std::optional<CookieInsightList::CookieIssueInsight>
CookieInsightListHandler::GetInsight(
    std::string_view cookie_domain,
    const net::CookieInclusionStatus& status) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return insight_list_.GetInsight(cookie_domain, status);
}

}  // namespace content
