// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/url_security_manager.h"

#include <utility>

#include "net/http/http_auth_filter.h"

namespace net {

URLSecurityManagerAllowlist::URLSecurityManagerAllowlist() = default;

URLSecurityManagerAllowlist::~URLSecurityManagerAllowlist() = default;

bool URLSecurityManagerAllowlist::CanUseDefaultCredentials(
    const GURL& auth_origin) const {
  if (allowlist_default_.get())
    return allowlist_default_->IsValid(auth_origin, HttpAuth::AUTH_SERVER);
  return false;
}

bool URLSecurityManagerAllowlist::CanDelegate(const GURL& auth_origin) const {
  if (allowlist_delegate_.get())
    return allowlist_delegate_->IsValid(auth_origin, HttpAuth::AUTH_SERVER);
  return false;
}

void URLSecurityManagerAllowlist::SetDefaultAllowlist(
    std::unique_ptr<HttpAuthFilter> allowlist_default) {
  allowlist_default_ = std::move(allowlist_default);
}

void URLSecurityManagerAllowlist::SetDelegateAllowlist(
    std::unique_ptr<HttpAuthFilter> allowlist_delegate) {
  allowlist_delegate_ = std::move(allowlist_delegate);
}

bool URLSecurityManagerAllowlist::HasDefaultAllowlist() const {
  return allowlist_default_.get() != nullptr;
}

}  //  namespace net
