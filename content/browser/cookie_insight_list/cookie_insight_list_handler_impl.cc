// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_insight_list/cookie_insight_list_handler_impl.h"

#include <optional>
#include <string_view>
#include <utility>

#include "content/browser/cookie_insight_list/cookie_insight_list.h"
#include "content/browser/cookie_insight_list/cookie_readiness_list/cookie_readiness_list_parser.h"
#include "content/public/browser/cookie_insight_list_data.h"
#include "content/public/browser/cookie_insight_list_handler.h"

namespace content {

CookieInsightListHandlerImpl::CookieInsightListHandlerImpl() = default;
//  Since CookieInsightListHandler is a singleton,
//  the destructor will never run.
CookieInsightListHandlerImpl::~CookieInsightListHandlerImpl() = default;

CookieInsightListHandler& CookieInsightListHandler::GetInstance() {
  static base::NoDestructor<CookieInsightListHandlerImpl> instance;
  return *instance.get();
}

void CookieInsightListHandlerImpl::set_insight_list(
    std::string_view json_content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  insight_list_ =
      std::move(CookieReadinessListParser::ParseReadinessList(json_content));
}

std::optional<CookieIssueInsight> CookieInsightListHandlerImpl::GetInsight(
    std::string_view cookie_domain,
    const net::CookieInclusionStatus& status) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return insight_list_.GetInsight(cookie_domain, status);
}

}  // namespace content
