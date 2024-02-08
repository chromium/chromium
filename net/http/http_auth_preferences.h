// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_PREFERENCES_H_
#define NET_HTTP_HTTP_AUTH_PREFERENCES_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"

namespace url {
class SchemeHostPort;
}

namespace net {

class URLSecurityManager;

// Manage the preferences needed for authentication, and provide a cache of
// them accessible from the IO thread.
class NET_EXPORT HttpAuthPreferences {
 public:
  // |DefaultCredentials| influences the behavior of codepaths that use
  // IdentitySource::IDENT_SRC_DEFAULT_CREDENTIALS in |HttpAuthController|
  enum DefaultCredentials {
    DISALLOW_DEFAULT_CREDENTIALS = 0,
    ALLOW_DEFAULT_CREDENTIALS = 1,
  };

  HttpAuthPreferences();

  HttpAuthPreferences(const HttpAuthPreferences&) = delete;
  HttpAuthPreferences& operator=(const HttpAuthPreferences&) = delete;

  virtual ~HttpAuthPreferences();

  virtual bool NegotiateDisableCnameLookup() const;
  virtual bool NegotiateEnablePort() const;
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  virtual bool NtlmV2Enabled() const;
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_ANDROID)
  virtual std::string AuthAndroidNegotiateAccountType() const;
#endif
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  virtual bool AllowGssapiLibraryLoad() const;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  virtual bool CanUseDefaultCredentials(
      const url::SchemeHostPort& auth_scheme_host_port) const;
  virtual HttpAuth::DelegationType GetDelegationType(
      const url::SchemeHostPort& auth_scheme_host_port) const;

  void set_delegate_by_kdc_policy(bool delegate_by_kdc_policy) {
    delegate_by_kdc_policy_ = delegate_by_kdc_policy;
  }

  bool delegate_by_kdc_policy() const { return delegate_by_kdc_policy_; }

  void set_negotiate_disable_cname_lookup(bool negotiate_disable_cname_lookup) {
    negotiate_disable_cname_lookup_ = negotiate_disable_cname_lookup;
  }

  void set_negotiate_enable_port(bool negotiate_enable_port) {
    negotiate_enable_port_ = negotiate_enable_port;
  }

  // Return |true| if the browser should allow attempts to use HTTP Basic auth
  // on non-secure HTTP connections.
  bool basic_over_http_enabled() const { return basic_over_http_enabled_; }

  void set_basic_over_http_enabled(bool allow_http) {
    basic_over_http_enabled_ = allow_http;
  }

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  void set_ntlm_v2_enabled(bool ntlm_v2_enabled) {
    ntlm_v2_enabled_ = ntlm_v2_enabled;
  }
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  void set_allow_gssapi_library_load(bool allow_gssapi_library_load) {
    allow_gssapi_library_load_ = allow_gssapi_library_load;
  }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

  const std::optional<std::set<std::string>>& allowed_schemes() const {
    return allowed_schemes_;
  }

  void set_allowed_schemes(
      const std::optional<std::set<std::string>>& allowed_schemes) {
    allowed_schemes_ = allowed_schemes;
  }

  void set_http_auth_scheme_filter(
      base::RepeatingCallback<bool(const url::SchemeHostPort&)>&& filter) {
    http_auth_scheme_filter_ = std::move(filter);
  }

  bool IsAllowedToUseAllHttpAuthSchemes(const url::SchemeHostPort& url) const;

  void SetServerAllowlist(const std::string& server_allowlist);

  void SetDelegateAllowlist(const std::string& delegate_allowlist);

  void SetAllowDefaultCredentials(DefaultCredentials creds);

#if BUILDFLAG(IS_ANDROID)
  void set_auth_android_negotiate_account_type(
      const std::string& account_type) {
    auth_android_negotiate_account_type_ = account_type;
  }
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  bool delegate_by_kdc_policy_ = false;
  bool negotiate_disable_cname_lookup_ = false;
  bool negotiate_enable_port_ = false;
  bool basic_over_http_enabled_ = true;

  DefaultCredentials allow_default_credentials_ = ALLOW_DEFAULT_CREDENTIALS;

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  bool ntlm_v2_enabled_ = true;
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
  std::string auth_android_negotiate_account_type_;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  bool allow_gssapi_library_load_ = true;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

  std::optional<std::set<std::string>> allowed_schemes_;
  std::unique_ptr<URLSecurityManager> security_manager_;
  base::RepeatingCallback<bool(const url::SchemeHostPort&)>
      http_auth_scheme_filter_ =
          base::RepeatingCallback<bool(const url::SchemeHostPort&)>();
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_PREFERENCES_H_
