// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_COMMON_UNITTEST_H_
#define NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_COMMON_UNITTEST_H_

#include "net/proxy_resolution/proxy_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// Helper functions to describe the expected value of a
// ProxyConfig::ProxyRules, and to check for a match.

namespace net {

// This structure contains our expectations on what values the ProxyRules
// should have.
struct ProxyRulesExpectation {
  ProxyRulesExpectation(ProxyConfig::ProxyRules::Type type,
                        const char* single_proxy,
                        const char* proxy_for_http,
                        const char* proxy_for_https,
                        const char* proxy_for_ftp,
                        const char* fallback_proxy,
                        const char* flattened_bypass_rules,
                        bool reverse_bypass);

  // Call this within an EXPECT_TRUE(), to assert that |rules| matches
  // our expected values |*this|.
  ::testing::AssertionResult Matches(
      const ProxyConfig::ProxyRules& rules) const;

  // Creates an expectation that the ProxyRules has no rules.
  static ProxyRulesExpectation Empty();

  // Creates an expectation that the ProxyRules has nothing other than
  // the specified bypass rules.
  static ProxyRulesExpectation EmptyWithBypass(
      const char* flattened_bypass_rules);

  // Creates an expectation that the ProxyRules is for a single proxy
  // server for all schemes.
  static ProxyRulesExpectation Single(const char* single_proxy,
                                      const char* flattened_bypass_rules);

  // Creates an expectation that the ProxyRules specifies a different
  // proxy server for each URL scheme.
  static ProxyRulesExpectation PerScheme(const char* proxy_http,
                                         const char* proxy_https,
                                         const char* proxy_ftp,
                                         const char* flattened_bypass_rules);

  // Same as above, but additionally with a SOCKS fallback.
  static ProxyRulesExpectation PerSchemeWithSocks(
      const char* proxy_http,
      const char* proxy_https,
      const char* proxy_ftp,
      const char* fallback_proxy,
      const char* flattened_bypass_rules);

  // Same as PerScheme, but with the bypass rules reversed
  static ProxyRulesExpectation PerSchemeWithBypassReversed(
      const char* proxy_http,
      const char* proxy_https,
      const char* proxy_ftp,
      const char* flattened_bypass_rules);

  ProxyConfig::ProxyRules::Type type;
  const char* single_proxy;
  const char* proxy_for_http;
  const char* proxy_for_https;
  const char* proxy_for_ftp;
  const char* fallback_proxy;
  const char* flattened_bypass_rules;
  bool reverse_bypass;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_COMMON_UNITTEST_H_
