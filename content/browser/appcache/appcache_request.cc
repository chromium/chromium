// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_request.h"

#include "content/common/appcache_interfaces.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"

namespace content {

AppCacheRequest::AppCacheRequest(const network::ResourceRequest& request)
    : request_(request) {}

AppCacheRequest::~AppCacheRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool AppCacheRequest::IsSuccess() const {
  if (response_.headers)
    return true;
  return false;
}

int AppCacheRequest::GetResponseCode() const {
  if (response_.headers)
    return response_.headers->response_code();
  return 0;
}

std::string AppCacheRequest::GetResponseHeaderByName(
    const std::string& name) const {
  std::string header;
  if (response_.headers)
    response_.headers->GetNormalizedHeader(name, &header);
  return header;
}

void AppCacheRequest::UpdateWithRedirectInfo(
    const net::RedirectInfo& redirect_info) {
  bool not_used_clear_body;
  net::RedirectUtil::UpdateHttpRequest(
      request_.url, request_.method, redirect_info,
      base::nullopt /* removed_request_headers */,
      base::nullopt /* modified_request_headers */, &request_.headers,
      &not_used_clear_body);
  request_.url = redirect_info.new_url;
  request_.method = redirect_info.new_method;
  request_.referrer = GURL(redirect_info.new_referrer);
  request_.referrer_policy = redirect_info.new_referrer_policy;
  request_.site_for_cookies = redirect_info.new_site_for_cookies;
}

void AppCacheRequest::set_request(const network::ResourceRequest& request) {
  request_ = request;
}

void AppCacheRequest::set_response(
    const network::ResourceResponseHead& response) {
  response_ = response;
}

base::WeakPtr<AppCacheRequest> AppCacheRequest::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
bool AppCacheRequest::IsSchemeAndMethodSupportedForAppCache(
    const AppCacheRequest* request) {
  return IsSchemeSupportedForAppCache(request->GetURL()) &&
         IsMethodSupportedForAppCache(request->GetMethod());
}

}  // namespace content
