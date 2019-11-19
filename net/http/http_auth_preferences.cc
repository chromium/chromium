// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_preferences.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "net/http/http_auth_filter.h"
#include "net/http/url_security_manager.h"

namespace net {

HttpAuthPreferences::HttpAuthPreferences()
    : security_manager_(URLSecurityManager::Create()) {}

HttpAuthPreferences::~HttpAuthPreferences() = default;

bool HttpAuthPreferences::NegotiateDisableCnameLookup() const {
  return negotiate_disable_cname_lookup_;
}

bool HttpAuthPreferences::NegotiateEnablePort() const {
  return negotiate_enable_port_;
}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
bool HttpAuthPreferences::NtlmV2Enabled() const {
  return ntlm_v2_enabled_;
}
#endif

#if defined(OS_ANDROID)
std::string HttpAuthPreferences::AuthAndroidNegotiateAccountType() const {
  return auth_android_negotiate_account_type_;
}
#endif

#if defined(OS_CHROMEOS)
bool HttpAuthPreferences::AllowGssapiLibraryLoad() const {
  return allow_gssapi_library_load_;
}
#endif

bool HttpAuthPreferences::CanUseDefaultCredentials(
    const GURL& auth_origin) const {
  return allow_default_credentials_ == ALLOW_DEFAULT_CREDENTIALS &&
         security_manager_->CanUseDefaultCredentials(auth_origin);
}

using DelegationType = HttpAuth::DelegationType;

DelegationType HttpAuthPreferences::GetDelegationType(
    const GURL& auth_origin) const {
  if (!security_manager_->CanDelegate(auth_origin))
    return DelegationType::kNone;

  if (delegate_by_kdc_policy())
    return DelegationType::kByKdcPolicy;

  return DelegationType::kUnconstrained;
}

void HttpAuthPreferences::SetAllowDefaultCredentials(DefaultCredentials creds) {
  allow_default_credentials_ = creds;
}

void HttpAuthPreferences::SetServerAllowlist(
    const std::string& server_allowlist) {
  std::unique_ptr<HttpAuthFilter> allowlist;
  if (!server_allowlist.empty())
    allowlist = std::make_unique<HttpAuthFilterAllowlist>(server_allowlist);
  security_manager_->SetDefaultAllowlist(std::move(allowlist));
}

void HttpAuthPreferences::SetDelegateAllowlist(
    const std::string& delegate_allowlist) {
  std::unique_ptr<HttpAuthFilter> allowlist;
  if (!delegate_allowlist.empty())
    allowlist = std::make_unique<HttpAuthFilterAllowlist>(delegate_allowlist);
  security_manager_->SetDelegateAllowlist(std::move(allowlist));
}

}  // namespace net
