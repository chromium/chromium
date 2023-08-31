// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_preferences.h"

#include <utility>

#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
bool HttpAuthPreferences::NtlmV2Enabled() const {
  return ntlm_v2_enabled_;
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
std::string HttpAuthPreferences::AuthAndroidNegotiateAccountType() const {
  return auth_android_negotiate_account_type_;
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
bool HttpAuthPreferences::AllowGssapiLibraryLoad() const {
  return allow_gssapi_library_load_;
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

bool HttpAuthPreferences::CanUseDefaultCredentials(
    const url::SchemeHostPort& auth_scheme_host_port) const {
  return allow_default_credentials_ == ALLOW_DEFAULT_CREDENTIALS &&
         security_manager_->CanUseDefaultCredentials(auth_scheme_host_port);
}

using DelegationType = HttpAuth::DelegationType;

DelegationType HttpAuthPreferences::GetDelegationType(
    const url::SchemeHostPort& auth_scheme_host_port) const {
  if (!security_manager_->CanDelegate(auth_scheme_host_port))
    return DelegationType::kNone;

  if (delegate_by_kdc_policy())
    return DelegationType::kByKdcPolicy;

  return DelegationType::kUnconstrained;
}

void HttpAuthPreferences::SetAllowDefaultCredentials(DefaultCredentials creds) {
  allow_default_credentials_ = creds;
}

bool HttpAuthPreferences::IsAllowedToUseAllHttpAuthSchemes(
    const url::SchemeHostPort& scheme_host_port) const {
  return !http_auth_scheme_filter_ ||
         http_auth_scheme_filter_.Run(scheme_host_port);
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
