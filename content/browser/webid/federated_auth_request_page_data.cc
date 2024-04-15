// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_page_data.h"

namespace content {

FederatedAuthRequestPageData::FederatedAuthRequestPageData(Page& page)
    : PageUserData<FederatedAuthRequestPageData>(page) {}

FederatedAuthRequestPageData::~FederatedAuthRequestPageData() = default;

PAGE_USER_DATA_KEY_IMPL(FederatedAuthRequestPageData);

FederatedAuthRequestImpl*
FederatedAuthRequestPageData::PendingWebIdentityRequest() {
  return pending_web_identity_request_;
}

void FederatedAuthRequestPageData::SetPendingWebIdentityRequest(
    FederatedAuthRequestImpl* request) {
  pending_web_identity_request_ = request;
}

void FederatedAuthRequestPageData::SetUserInfoAccountsResponseTime(
    const GURL& idp_url,
    const base::TimeTicks& time) {
  user_info_accounts_response_time_[idp_url] = time;
}

std::optional<base::TimeTicks>
FederatedAuthRequestPageData::ConsumeUserInfoAccountsResponseTime(
    const GURL& idp_url) {
  const auto& it = user_info_accounts_response_time_.find(idp_url);
  if (it != user_info_accounts_response_time_.end()) {
    base::TimeTicks time = it->second;
    user_info_accounts_response_time_.erase(it);
    return time;
  }
  return std::nullopt;
}

}  // namespace content
