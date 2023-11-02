// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_URL_SECURITY_MANAGER_H_
#define NET_HTTP_URL_SECURITY_MANAGER_H_

#include <memory>

#include "net/base/net_export.h"

namespace url {
class SchemeHostPort;
}

namespace net {

class HttpAuthFilter;

// The URL security manager controls the policies (allow, deny, prompt user)
// regarding URL actions (e.g., sending the default credentials to a server).
class NET_EXPORT_PRIVATE URLSecurityManager {
 public:
  URLSecurityManager() = default;

  URLSecurityManager(const URLSecurityManager&) = delete;
  URLSecurityManager& operator=(const URLSecurityManager&) = delete;

  virtual ~URLSecurityManager() = default;

  // Creates a platform-dependent instance of URLSecurityManager.
  //
  // A security manager has two allowlists, a "default allowlist" that is a
  // allowlist of servers with which default credentials can be used, and a
  // "delegate allowlist" that is the allowlist of servers that are allowed to
  // have delegated Kerberos tickets.
  //
  // On creation both allowlists are empty.
  //
  // If the default allowlist is empty and the platform is Windows, it indicates
  // that security zone mapping should be used to determine whether default
  // credentials should be used. If the default allowlist is empty and the
  // platform is non-Windows, it indicates that no servers should be
  // allowlisted.
  //
  // If the delegate allowlist is empty no servers can have delegated Kerberos
  // tickets.
  //
  static std::unique_ptr<URLSecurityManager> Create();

  // Returns true if we can send the default credentials to the server at
  // |auth_scheme_host_port| for HTTP NTLM or Negotiate authentication.
  virtual bool CanUseDefaultCredentials(
      const url::SchemeHostPort& auth_scheme_host_port) const = 0;

  // Returns true if Kerberos delegation is allowed for the server at
  // |auth_scheme_host_port| for HTTP Negotiate authentication.
  virtual bool CanDelegate(
      const url::SchemeHostPort& auth_scheme_host_port) const = 0;

  virtual void SetDefaultAllowlist(
      std::unique_ptr<HttpAuthFilter> allowlist_default) = 0;
  virtual void SetDelegateAllowlist(
      std::unique_ptr<HttpAuthFilter> allowlist_delegate) = 0;
};

class URLSecurityManagerAllowlist : public URLSecurityManager {
 public:
  URLSecurityManagerAllowlist();

  URLSecurityManagerAllowlist(const URLSecurityManagerAllowlist&) = delete;
  URLSecurityManagerAllowlist& operator=(const URLSecurityManagerAllowlist&) =
      delete;

  ~URLSecurityManagerAllowlist() override;

  // URLSecurityManager methods.
  bool CanUseDefaultCredentials(
      const url::SchemeHostPort& auth_scheme_host_port) const override;
  bool CanDelegate(
      const url::SchemeHostPort& auth_scheme_host_port) const override;
  void SetDefaultAllowlist(
      std::unique_ptr<HttpAuthFilter> allowlist_default) override;
  void SetDelegateAllowlist(
      std::unique_ptr<HttpAuthFilter> allowlist_delegate) override;

 protected:
  bool HasDefaultAllowlist() const;

 private:
  std::unique_ptr<const HttpAuthFilter> allowlist_default_;
  std::unique_ptr<const HttpAuthFilter> allowlist_delegate_;
};

}  // namespace net

#endif  // NET_HTTP_URL_SECURITY_MANAGER_H_
