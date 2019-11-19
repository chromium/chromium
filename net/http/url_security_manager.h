// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_URL_SECURITY_MANAGER_H_
#define NET_HTTP_URL_SECURITY_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

class HttpAuthFilter;

// The URL security manager controls the policies (allow, deny, prompt user)
// regarding URL actions (e.g., sending the default credentials to a server).
class NET_EXPORT_PRIVATE URLSecurityManager {
 public:
  URLSecurityManager() {}
  virtual ~URLSecurityManager() {}

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
  // |auth_origin| for HTTP NTLM or Negotiate authentication.
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const = 0;

  // Returns true if Kerberos delegation is allowed for the server at
  // |auth_origin| for HTTP Negotiate authentication.
  virtual bool CanDelegate(const GURL& auth_origin) const = 0;

  virtual void SetDefaultAllowlist(
      std::unique_ptr<HttpAuthFilter> allowlist_default) = 0;
  virtual void SetDelegateAllowlist(
      std::unique_ptr<HttpAuthFilter> allowlist_delegate) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLSecurityManager);
};

class URLSecurityManagerAllowlist : public URLSecurityManager {
 public:
  URLSecurityManagerAllowlist();
  ~URLSecurityManagerAllowlist() override;

  // URLSecurityManager methods.
  bool CanUseDefaultCredentials(const GURL& auth_origin) const override;
  bool CanDelegate(const GURL& auth_origin) const override;
  void SetDefaultAllowlist(
      std::unique_ptr<HttpAuthFilter> allowlist_default) override;
  void SetDelegateAllowlist(
      std::unique_ptr<HttpAuthFilter> allowlist_delegate) override;

 protected:
  bool HasDefaultAllowlist() const;

 private:
  std::unique_ptr<const HttpAuthFilter> allowlist_default_;
  std::unique_ptr<const HttpAuthFilter> allowlist_delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLSecurityManagerAllowlist);
};

}  // namespace net

#endif  // NET_HTTP_URL_SECURITY_MANAGER_H_
