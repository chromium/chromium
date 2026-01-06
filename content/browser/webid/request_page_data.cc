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

RequestPageData::EmbedderLoginRequest::EmbedderLoginRequest(
    const GURL& idp_url,
    const std::string& account_id,
    OnFederatedTokenReceivedCallback callback)
    : idp_url(idp_url),
      account_id(account_id),
      on_federated_token_received_callback(std::move(callback)) {}

RequestPageData::EmbedderLoginRequest::EmbedderLoginRequest(
    EmbedderLoginRequest&&) = default;

RequestPageData::EmbedderLoginRequest::~EmbedderLoginRequest() = default;

void RequestPageData::SetEmbedderLoginRequest(
    std::optional<EmbedderLoginRequest> request) {
  embedder_login_request_ = std::move(request);
}

const std::optional<RequestPageData::EmbedderLoginRequest>&
RequestPageData::GetEmbedderLoginRequest() {
  return embedder_login_request_;
}

}  // namespace content::webid
