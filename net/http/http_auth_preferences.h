// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_PREFERENCES_H_
#define NET_HTTP_HTTP_AUTH_PREFERENCES_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "url/gurl.h"

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
  virtual ~HttpAuthPreferences();

  virtual bool NegotiateDisableCnameLookup() const;
  virtual bool NegotiateEnablePort() const;
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  virtual bool NtlmV2Enabled() const;
#endif
#if defined(OS_ANDROID)
  virtual std::string AuthAndroidNegotiateAccountType() const;
#endif
#if defined(OS_CHROMEOS)
  virtual bool AllowGssapiLibraryLoad() const;
#endif
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const;
  virtual HttpAuth::DelegationType GetDelegationType(
      const GURL& auth_origin) const;

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

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  void set_ntlm_v2_enabled(bool ntlm_v2_enabled) {
    ntlm_v2_enabled_ = ntlm_v2_enabled;
  }
#endif

#if defined(OS_CHROMEOS)
  void set_allow_gssapi_library_load(bool allow_gssapi_library_load) {
    allow_gssapi_library_load_ = allow_gssapi_library_load;
  }
#endif

  void SetServerAllowlist(const std::string& server_allowlist);

  void SetDelegateAllowlist(const std::string& delegate_allowlist);

  void SetAllowDefaultCredentials(DefaultCredentials creds);

#if defined(OS_ANDROID)
  void set_auth_android_negotiate_account_type(
      const std::string& account_type) {
    auth_android_negotiate_account_type_ = account_type;
  }
#endif

 private:
  bool delegate_by_kdc_policy_ = false;
  bool negotiate_disable_cname_lookup_ = false;
  bool negotiate_enable_port_ = false;

  DefaultCredentials allow_default_credentials_ = ALLOW_DEFAULT_CREDENTIALS;

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  bool ntlm_v2_enabled_ = true;
#endif

#if defined(OS_ANDROID)
  std::string auth_android_negotiate_account_type_;
#endif

#if defined(OS_CHROMEOS)
  bool allow_gssapi_library_load_ = true;
#endif

  std::unique_ptr<URLSecurityManager> security_manager_;
  DISALLOW_COPY_AND_ASSIGN(HttpAuthPreferences);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_PREFERENCES_H_
