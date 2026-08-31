// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_PROXY_PROXY_CONFIG_H_
#define IOS_WEB_PUBLIC_PROXY_PROXY_CONFIG_H_

#import <optional>
#import <string>
#import <vector>

#import "net/base/proxy_server.h"

namespace web {

// Represents a single proxy rule, specifying an optional proxy server and the
// domain patterns or IP addresses to which it applies.
struct ProxyRule {
  ProxyRule();
  ProxyRule(const ProxyRule&);
  ProxyRule& operator=(const ProxyRule&);
  ProxyRule(ProxyRule&&);
  ProxyRule& operator=(ProxyRule&&);
  ~ProxyRule();

  bool operator==(const ProxyRule&) const = default;

  // The proxy server endpoint. If `std::nullopt`, this rule represents a direct
  // connection (bypass).
  std::optional<net::ProxyServer> proxy_server;

  // Domain matchers indicating which destinations should use this proxy rule.
  // Supports wildcards (e.g., "*.example.com"), IPv4, and IPv6 addresses.
  // Note: Apple's native API (`nw_proxy_config_add_match_domain`) ignores
  // port numbers in domain matchers.
  std::vector<std::string> match_domains;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_PROXY_PROXY_CONFIG_H_
