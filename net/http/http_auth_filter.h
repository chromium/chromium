// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_FILTER_H_
#define NET_HTTP_HTTP_AUTH_FILTER_H_

#include <string>

#include "net/base/net_export.h"
#include "net/http/http_auth.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"

namespace url {
class SchemeHostPort;
}

namespace net {

// HttpAuthFilters determine whether an authentication scheme should be
// allowed for a particular peer.
class NET_EXPORT_PRIVATE HttpAuthFilter {
 public:
  virtual ~HttpAuthFilter() = default;

  // Checks if (`scheme_host_port`, `target`) is supported by the authentication
  // scheme. Only the host of `scheme_host_port` is examined.
  virtual bool IsValid(const url::SchemeHostPort& scheme_host_port,
                       HttpAuth::Target target) const = 0;
};

// Allowlist HTTP authentication filter.
// Explicit allowlists of domains are set via SetAllowlist().
//
// Uses the ProxyBypassRules class to do allowlisting for servers.
// All proxies are allowed.
class NET_EXPORT HttpAuthFilterAllowlist : public HttpAuthFilter {
 public:
  explicit HttpAuthFilterAllowlist(const std::string& server_allowlist);

  HttpAuthFilterAllowlist(const HttpAuthFilterAllowlist&) = delete;
  HttpAuthFilterAllowlist& operator=(const HttpAuthFilterAllowlist&) = delete;

  ~HttpAuthFilterAllowlist() override;

  // Adds an individual URL `filter` to the list, of the specified `target`.
  bool AddFilter(const std::string& filter, HttpAuth::Target target);

  const ProxyBypassRules& rules() const { return rules_; }

  // HttpAuthFilter methods:
  bool IsValid(const url::SchemeHostPort& scheme_host_port,
               HttpAuth::Target target) const override;

 private:
  // Installs the allowlist.
  // `server_allowlist` is parsed by ProxyBypassRules.
  void SetAllowlist(const std::string& server_allowlist);

  // We are using ProxyBypassRules because they have the functionality that we
  // want, but we are not using it for proxy bypass.
  ProxyBypassRules rules_;
};

}   // namespace net

#endif  // NET_HTTP_HTTP_AUTH_FILTER_H_
