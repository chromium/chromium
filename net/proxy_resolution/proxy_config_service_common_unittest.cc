// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_service_common_unittest.h"

#include <string>
#include <vector>

#include "net/proxy_resolution/proxy_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Helper to verify that |expected_proxy| matches the first proxy conatined in
// |actual_proxies|, and that |actual_proxies| contains exactly one proxy. If
// either condition is untrue, then |*did_fail| is set to true, and
// |*failure_details| is filled with a description of the failure.
void MatchesProxyServerHelper(const char* failure_message,
                              const char* expected_proxy,
                              const ProxyList& actual_proxies,
                              ::testing::AssertionResult* failure_details,
                              bool* did_fail) {
  // If |expected_proxy| is empty, then we expect |actual_proxies| to be so as
  // well.
  if (strlen(expected_proxy) == 0) {
    if (!actual_proxies.IsEmpty()) {
      *did_fail = true;
      *failure_details
          << failure_message << ". Was expecting no proxies but got "
          << actual_proxies.size() << ".";
    }
    return;
  }

  // Otherwise we check that |actual_proxies| holds a single matching proxy.
  if (actual_proxies.size() != 1) {
    *did_fail = true;
    *failure_details
        << failure_message << ". Was expecting exactly one proxy but got "
        << actual_proxies.size() << ".";
    return;
  }

  ProxyServer actual_proxy = actual_proxies.Get();
  std::string actual_proxy_string;
  if (actual_proxy.is_valid())
    actual_proxy_string = actual_proxy.ToURI();

  if (std::string(expected_proxy) != actual_proxy_string) {
    *failure_details
        << failure_message << ". Was expecting: \"" << expected_proxy
        << "\" but got: \"" << actual_proxy_string << "\"";
    *did_fail = true;
  }
}

std::string FlattenProxyBypass(const ProxyBypassRules& bypass_rules) {
  std::string flattened_proxy_bypass;
  for (auto it = bypass_rules.rules().begin(); it != bypass_rules.rules().end();
       ++it) {
    if (!flattened_proxy_bypass.empty())
      flattened_proxy_bypass += ",";
    flattened_proxy_bypass += (*it)->ToString();
  }
  return flattened_proxy_bypass;
}

}  // namespace

ProxyRulesExpectation::ProxyRulesExpectation(
    ProxyConfig::ProxyRules::Type type,
    const char* single_proxy,
    const char* proxy_for_http,
    const char* proxy_for_https,
    const char* proxy_for_ftp,
    const char* fallback_proxy,
    const char* flattened_bypass_rules,
    bool reverse_bypass)
    : type(type),
      single_proxy(single_proxy),
      proxy_for_http(proxy_for_http),
      proxy_for_https(proxy_for_https),
      proxy_for_ftp(proxy_for_ftp),
      fallback_proxy(fallback_proxy),
      flattened_bypass_rules(flattened_bypass_rules),
      reverse_bypass(reverse_bypass) {
}


::testing::AssertionResult ProxyRulesExpectation::Matches(
    const ProxyConfig::ProxyRules& rules) const {
  ::testing::AssertionResult failure_details = ::testing::AssertionFailure();
  bool failed = false;

  if (rules.type != type) {
    failure_details << "Type mismatch. Expected: " << static_cast<int>(type)
                    << " but was: " << static_cast<int>(rules.type);
    failed = true;
  }

  MatchesProxyServerHelper("Bad single_proxy", single_proxy,
                           rules.single_proxies, &failure_details, &failed);
  MatchesProxyServerHelper("Bad proxy_for_http", proxy_for_http,
                           rules.proxies_for_http, &failure_details,
                           &failed);
  MatchesProxyServerHelper("Bad proxy_for_https", proxy_for_https,
                           rules.proxies_for_https, &failure_details,
                           &failed);
  MatchesProxyServerHelper("Bad fallback_proxy", fallback_proxy,
                           rules.fallback_proxies, &failure_details, &failed);

  std::string actual_flattened_bypass = FlattenProxyBypass(rules.bypass_rules);
  if (std::string(flattened_bypass_rules) != actual_flattened_bypass) {
    failure_details
        << "Bad bypass rules. Expected: \"" << flattened_bypass_rules
        << "\" but got: \"" << actual_flattened_bypass << "\"";
    failed = true;
  }

  if (rules.reverse_bypass != reverse_bypass) {
    failure_details << "Bad reverse_bypass. Expected: " << reverse_bypass
                    << " but got: " << rules.reverse_bypass;
    failed = true;
  }

  return failed ? failure_details : ::testing::AssertionSuccess();
}

// static
ProxyRulesExpectation ProxyRulesExpectation::Empty() {
  return ProxyRulesExpectation(ProxyConfig::ProxyRules::Type::EMPTY,
                               "", "", "", "", "", "", false);
}

// static
ProxyRulesExpectation ProxyRulesExpectation::EmptyWithBypass(
    const char* flattened_bypass_rules) {
  return ProxyRulesExpectation(ProxyConfig::ProxyRules::Type::EMPTY,
                               "", "", "", "", "", flattened_bypass_rules,
                               false);
}

// static
ProxyRulesExpectation ProxyRulesExpectation::Single(
    const char* single_proxy,
    const char* flattened_bypass_rules) {
  return ProxyRulesExpectation(ProxyConfig::ProxyRules::Type::PROXY_LIST,
                               single_proxy, "", "", "", "",
                               flattened_bypass_rules, false);
}

// static
ProxyRulesExpectation ProxyRulesExpectation::PerScheme(
    const char* proxy_http,
    const char* proxy_https,
    const char* proxy_ftp,
    const char* flattened_bypass_rules) {
  return ProxyRulesExpectation(ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
                               "", proxy_http, proxy_https, proxy_ftp, "",
                               flattened_bypass_rules, false);
}

// static
ProxyRulesExpectation ProxyRulesExpectation::PerSchemeWithSocks(
    const char* proxy_http,
    const char* proxy_https,
    const char* proxy_ftp,
    const char* socks_proxy,
    const char* flattened_bypass_rules) {
  return ProxyRulesExpectation(ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
                               "", proxy_http, proxy_https, proxy_ftp,
                               socks_proxy, flattened_bypass_rules, false);
}

// static
ProxyRulesExpectation ProxyRulesExpectation::PerSchemeWithBypassReversed(
    const char* proxy_http,
    const char* proxy_https,
    const char* proxy_ftp,
    const char* flattened_bypass_rules) {
  return ProxyRulesExpectation(ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
                               "", proxy_http, proxy_https, proxy_ftp, "",
                               flattened_bypass_rules, true);
}

}  // namespace net
