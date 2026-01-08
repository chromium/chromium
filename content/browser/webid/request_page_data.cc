// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/request_page_data.h"

namespace content::webid {

RequestPageData::RequestPageData(Page& page)
    : PageUserData<RequestPageData>(page) {}

RequestPageData::~RequestPageData() = default;

PAGE_USER_DATA_KEY_IMPL(RequestPageData);

RequestService* RequestPageData::PendingWebIdentityRequest() {
  return pending_web_identity_request_;
}

void RequestPageData::SetPendingWebIdentityRequest(RequestService* request) {
  pending_web_identity_request_ = request;
}

void RequestPageData::SetUserInfoAccountsResponseTime(
    const GURL& idp_url,
    const base::TimeTicks& time) {
  user_info_accounts_response_time_[idp_url] = time;
}

std::optional<base::TimeTicks>
RequestPageData::ConsumeUserInfoAccountsResponseTime(const GURL& idp_url) {
  const auto& it = user_info_accounts_response_time_.find(idp_url);
  if (it != user_info_accounts_response_time_.end()) {
    base::TimeTicks time = it->second;
    user_info_accounts_response_time_.erase(it);
    return time;
  }
  return std::nullopt;
}

}  // namespace content::webid
