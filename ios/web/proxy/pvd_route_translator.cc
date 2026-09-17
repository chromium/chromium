// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/proxy/pvd_route_translator.h"

#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/logging.h"
#import "ios/web/public/proxy/proxy_config.h"
#import "net/base/proxy_chain.h"
#import "net/base/proxy_server.h"
#import "net/proxy_resolution/proxy_config.h"

namespace web {

namespace {

// Translates a single dynamic routing rule into a `ProxyRule`.
//
// Extracts the first HTTP CONNECT proxy (`HTTP` or `HTTPS` scheme) or direct
// connection. Maps destination matchers straight into `match_domains` without
// modification because Apple's native APIs accept domain suffixes, wildcards,
// IPv4, and IPv6 addresses directly.
//
// Returns `std::nullopt` if the rule specifies unsupported proxy schemes
// (e.g. SOCKS only) and lacks a direct connection or HTTP fallback.
std::optional<ProxyRule> TranslateDynamicRoutingRule(
    const net::ProxyConfig::DynamicRoutingRule& dynamic_rule) {
  std::optional<net::ProxyServer> selected_proxy;
  bool has_valid_target = false;

  for (const net::ProxyChain& chain : dynamic_rule.proxy_list.AllChains()) {
    if (chain.is_direct()) {
      selected_proxy = std::nullopt;
      has_valid_target = true;
      break;
    }
    if (chain.is_single_proxy()) {
      const net::ProxyServer& server = chain.GetProxyServer(0);
      // WebKit's native proxy configuration only supports HTTP CONNECT proxies
      // (HTTP and HTTPS). Unsupported schemes (e.g. SOCKS) are skipped.
      if (server.is_http() || server.is_https()) {
        selected_proxy = server;
        has_valid_target = true;
        break;
      }
    } else {
      LOG(WARNING) << "Skipping unsupported multi-hop proxy chain.";
    }
  }

  if (!has_valid_target) {
    LOG(WARNING) << "Ignoring dynamic routing rule: no supported HTTP/HTTPS "
                    "proxy or direct connection found.";
    return std::nullopt;
  }

  std::vector<std::string> match_domains;
  match_domains.reserve(dynamic_rule.destination_matchers.rules().size());
  for (const auto& matcher_rule : dynamic_rule.destination_matchers.rules()) {
    match_domains.push_back(matcher_rule->ToString());
  }

  ProxyRule web_rule;
  web_rule.proxy_server = selected_proxy;
  web_rule.match_domains = std::move(match_domains);
  return web_rule;
}

}  // namespace

std::vector<ProxyRule> TranslateProvisioningDomainRoutingConfig(
    const ProvisioningDomainRoutingConfig& config) {
  std::vector<ProxyRule> web_config;
  web_config.reserve(config.routing_rules.size());

  for (const net::ProxyConfig::DynamicRoutingRule& dynamic_rule :
       config.routing_rules) {
    std::optional<ProxyRule> web_rule =
        TranslateDynamicRoutingRule(dynamic_rule);
    if (web_rule.has_value()) {
      web_config.push_back(std::move(*web_rule));
    }
  }

  return web_config;
}

}  // namespace web
