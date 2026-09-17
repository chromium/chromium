// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_PROXY_PVD_ROUTE_TRANSLATOR_H_
#define IOS_WEB_PUBLIC_PROXY_PVD_ROUTE_TRANSLATOR_H_

#import <vector>

#import "ios/web/public/proxy/proxy_config.h"
#import "net/proxy_resolution/proxy_config.h"

namespace web {

// Alias matching cross-platform Provisioning Domain routing config terminology.
using ProvisioningDomainRoutingConfig = net::ProxyConfig::DynamicRoutingConfig;

// Translates a cross-platform Provisioning Domain dynamic routing configuration
// (`net::ProxyConfig::DynamicRoutingConfig`) into a list of `ProxyRule`s
// suitable for WebKit proxy configuration.
//
// For each `DynamicRoutingRule`:
// - Extracts the first HTTP CONNECT proxy (`HTTP` or `HTTPS` scheme) or direct
//   connection per rule.
// - Maps each destination matcher directly to a plain string within
//   `match_domains`, preserving wildcards (e.g., `*.apple.com`), domain
//   suffixes, IPv4, and IPv6 addresses as accepted by Apple's native APIs.
// - Omits rules that do not specify any supported proxy schemes.
// - Strictly preserves the order of routing rules to maintain priority.
std::vector<ProxyRule> TranslateProvisioningDomainRoutingConfig(
    const ProvisioningDomainRoutingConfig& config);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_PROXY_PVD_ROUTE_TRANSLATOR_H_
