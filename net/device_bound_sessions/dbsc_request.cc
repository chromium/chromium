// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/dbsc_request.h"

#include "net/base/url_util.h"
#include "net/url_request/url_request.h"

namespace net::device_bound_sessions {

namespace {

GURL NormalizeUrl(const GURL& url) {
  return url.SchemeIsWSOrWSS() ? ChangeWebSocketSchemeToHttpScheme(url) : url;
}

}  // namespace

DbscRequest::DbscRequest(URLRequest* request) : request_(request) {
  CHECK(request);
}

DbscRequest::DbscRequest(const DbscRequest&) = default;
DbscRequest& DbscRequest::operator=(const DbscRequest&) = default;
DbscRequest::DbscRequest(DbscRequest&&) = default;
DbscRequest& DbscRequest::operator=(DbscRequest&&) = default;
DbscRequest::~DbscRequest() = default;

SessionUsage DbscRequest::device_bound_session_usage() const {
  return request_->device_bound_session_usage();
}

void DbscRequest::set_device_bound_session_usage(
    device_bound_sessions::SessionUsage usage) {
  request_->set_device_bound_session_usage(usage);
}

const base::flat_map<SessionKey, RefreshResult>&
DbscRequest::device_bound_session_deferrals() const {
  return request_->device_bound_session_deferrals();
}

base::RepeatingCallback<void(const SessionAccess&)>
DbscRequest::device_bound_session_access_callback() {
  return request_->device_bound_session_access_callback();
}

base::WeakPtr<URLRequest> DbscRequest::GetWeakPtr() {
  return request_->GetWeakPtr();
}

const NetLogWithSource& DbscRequest::net_log() const {
  return request_->net_log();
}

const std::optional<url::Origin>& DbscRequest::initiator() const {
  return request_->initiator();
}

const URLRequestContext* DbscRequest::context() const {
  return request_->context();
}

bool DbscRequest::force_ignore_site_for_cookies() const {
  return request_->force_ignore_site_for_cookies();
}

const SiteForCookies& DbscRequest::site_for_cookies() const {
  return request_->site_for_cookies();
}

const IsolationInfo& DbscRequest::isolation_info() const {
  return request_->isolation_info();
}

bool DbscRequest::force_main_frame_for_same_site_cookies() const {
  return request_->force_main_frame_for_same_site_cookies();
}

const std::string& DbscRequest::method() const {
  return request_->method();
}

bool DbscRequest::ignore_unsafe_method_for_same_site_lax() const {
  return request_->ignore_unsafe_method_for_same_site_lax();
}

const CookieAccessResultList& DbscRequest::maybe_sent_cookies() const {
  return request_->maybe_sent_cookies();
}

NetworkDelegate* DbscRequest::network_delegate() const {
  return request_->network_delegate();
}

bool DbscRequest::allows_device_bound_session_registration() const {
  return request_->allows_device_bound_session_registration();
}

int DbscRequest::load_flags() const {
  return request_->load_flags();
}

const GURL& DbscRequest::url() const {
  if (!normalized_url_.has_value()) {
    normalized_url_ = NormalizeUrl(request_->url());
  }
  return *normalized_url_;
}

const std::vector<GURL>& DbscRequest::url_chain() const {
  if (!normalized_url_chain_.has_value()) {
    normalized_url_chain_ = std::vector<GURL>();
    for (const auto& url : request_->url_chain()) {
      normalized_url_chain_->push_back(NormalizeUrl(url));
    }
  }

  return *normalized_url_chain_;
}

}  // namespace net::device_bound_sessions
