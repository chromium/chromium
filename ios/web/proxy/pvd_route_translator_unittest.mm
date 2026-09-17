// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/proxy/pvd_route_translator.h"

#import <optional>
#import <string>
#import <vector>

#import "ios/web/public/proxy/proxy_config.h"
#import "net/base/proxy_chain.h"
#import "net/base/proxy_server.h"
#import "net/proxy_resolution/proxy_config.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace web {

namespace {

constexpr char kTestHttpHost[] = "http-proxy.example.com";
constexpr uint16_t kTestProxyPort = 8080;

constexpr char kTestHttpsHost[] = "secure-proxy.corp.com";
constexpr uint16_t kTestHttpsPort = 443;

constexpr char kTestSecondHopHost[] = "second-hop.corp.com";
constexpr uint16_t kTestSecondHopPort = 8443;

constexpr char kTestSocksHost[] = "socks-proxy.example.com";
constexpr uint16_t kTestSocksPort = 1080;

constexpr char kWildcardDomain[] = "*.apple.com";
constexpr char kPlainDomain[] = "apple.com";
constexpr char kIpv4Address[] = "192.168.1.1";
constexpr char kIpv6Address[] = "[2001:db8::1]";

class PvDRouteTranslatorTest : public PlatformTest {
 protected:
  // Helper to verify that a dynamic routing rule for an HTTP or HTTPS proxy
  // correctly translates to a `web::ProxyRule`.
  void CheckProxyRule(net::ProxyServer::Scheme scheme,
                      const std::string& host) {
    net::ProxyConfig::DynamicRoutingConfig dynamic_config;
    net::ProxyConfig::DynamicRoutingRule dynamic_rule;
    dynamic_rule.proxy_list.AddProxyServer(
        net::ProxyServer::FromSchemeHostAndPort(scheme, host, kTestProxyPort));
    dynamic_rule.destination_matchers.AddRuleFromString("*.example.com");
    dynamic_config.routing_rules.push_back(std::move(dynamic_rule));

    std::vector<ProxyRule> web_config =
        TranslateProvisioningDomainRoutingConfig(dynamic_config);

    ASSERT_EQ(web_config.size(), 1u);
    ASSERT_TRUE(web_config[0].proxy_server.has_value());
    EXPECT_EQ(web_config[0].proxy_server->scheme(), scheme);
    EXPECT_EQ(web_config[0].proxy_server->GetHost(), host);
    EXPECT_EQ(web_config[0].proxy_server->GetPort(), kTestProxyPort);
    ASSERT_EQ(web_config[0].match_domains.size(), 1u);
    EXPECT_EQ(web_config[0].match_domains[0], "*.example.com");
  }
};

// Test that translating an empty configuration yields an empty
// `std::vector<web::ProxyRule>`.
TEST_F(PvDRouteTranslatorTest, EmptyConfig) {
  net::ProxyConfig::DynamicRoutingConfig dynamic_config;

  std::vector<ProxyRule> web_config =
      TranslateProvisioningDomainRoutingConfig(dynamic_config);

  EXPECT_TRUE(web_config.empty());
}

// Test that a direct rule maps to a `web::ProxyRule` with no `proxy_server`.
TEST_F(PvDRouteTranslatorTest, DirectRule) {
  net::ProxyConfig::DynamicRoutingConfig dynamic_config;
  net::ProxyConfig::DynamicRoutingRule dynamic_rule;
  dynamic_rule.proxy_list.AddProxyChain(net::ProxyChain::Direct());
  dynamic_rule.destination_matchers.AddRuleFromString("direct.example.com");
  dynamic_config.routing_rules.push_back(std::move(dynamic_rule));

  std::vector<ProxyRule> web_config =
      TranslateProvisioningDomainRoutingConfig(dynamic_config);

  ASSERT_EQ(web_config.size(), 1u);
  EXPECT_FALSE(web_config[0].proxy_server.has_value());
  ASSERT_EQ(web_config[0].match_domains.size(), 1u);
  EXPECT_EQ(web_config[0].match_domains[0], "direct.example.com");
}

// Test that an HTTP proxy rule is correctly translated.
TEST_F(PvDRouteTranslatorTest, HttpProxyRule) {
  CheckProxyRule(net::ProxyServer::SCHEME_HTTP, kTestHttpHost);
}

// Test that an HTTPS proxy rule is correctly translated.
TEST_F(PvDRouteTranslatorTest, HttpsProxyRule) {
  CheckProxyRule(net::ProxyServer::SCHEME_HTTPS, kTestHttpsHost);
}

// Test that a rule with a multi-hop proxy chain is skipped (returns nullopt).
TEST_F(PvDRouteTranslatorTest, SkipsMultiHopProxyChain) {
  net::ProxyConfig::DynamicRoutingConfig dynamic_config;
  net::ProxyConfig::DynamicRoutingRule dynamic_rule;
  net::ProxyServer hop1 = net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, kTestHttpsHost, kTestHttpsPort);
  net::ProxyServer hop2 = net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, kTestSecondHopHost, kTestSecondHopPort);
  dynamic_rule.proxy_list.AddProxyChain(
      net::ProxyChain(std::vector<net::ProxyServer>{hop1, hop2}));
  dynamic_rule.destination_matchers.AddRuleFromString("corp.com");
  dynamic_config.routing_rules.push_back(std::move(dynamic_rule));

  std::vector<ProxyRule> web_config =
      TranslateProvisioningDomainRoutingConfig(dynamic_config);

  EXPECT_TRUE(web_config.empty());
}

// Test that when `proxy_list` contains multiple proxies, the first supported
// proxy is extracted.
TEST_F(PvDRouteTranslatorTest, ExtractsFirstProxyInProxyList) {
  net::ProxyConfig::DynamicRoutingConfig dynamic_config;
  net::ProxyConfig::DynamicRoutingRule dynamic_rule;
  dynamic_rule.proxy_list.AddProxyServer(
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              kTestHttpsHost, kTestHttpsPort));
  dynamic_rule.proxy_list.AddProxyServer(
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTP,
                                              kTestHttpHost, kTestProxyPort));
  dynamic_rule.destination_matchers.AddRuleFromString("corp.com");
  dynamic_config.routing_rules.push_back(std::move(dynamic_rule));

  std::vector<ProxyRule> web_config =
      TranslateProvisioningDomainRoutingConfig(dynamic_config);

  ASSERT_EQ(web_config.size(), 1u);
  ASSERT_TRUE(web_config[0].proxy_server.has_value());
  EXPECT_EQ(web_config[0].proxy_server->GetHost(), kTestHttpsHost);
  EXPECT_EQ(web_config[0].proxy_server->GetPort(), kTestHttpsPort);
}

// Test that rules with unsupported proxy schemes (e.g. SOCKS) are omitted,
// but fallback chains with supported schemes are picked.
TEST_F(PvDRouteTranslatorTest, SkipsUnsupportedProxySchemes) {
  net::ProxyConfig::DynamicRoutingConfig dynamic_config;

  // Rule 1: SOCKS5 only (should be omitted).
  net::ProxyConfig::DynamicRoutingRule socks_rule;
  socks_rule.proxy_list.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_SOCKS5, kTestSocksHost, kTestSocksPort));
  socks_rule.destination_matchers.AddRuleFromString("socks.example.com");
  dynamic_config.routing_rules.push_back(std::move(socks_rule));

  // Rule 2: SOCKS5 followed by HTTPS fallback (should pick HTTPS).
  net::ProxyConfig::DynamicRoutingRule fallback_rule;
  fallback_rule.proxy_list.AddProxyServer(
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_SOCKS5,
                                              kTestSocksHost, kTestSocksPort));
  fallback_rule.proxy_list.AddProxyServer(
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              kTestHttpsHost, kTestHttpsPort));
  fallback_rule.destination_matchers.AddRuleFromString("fallback.corp.com");
  dynamic_config.routing_rules.push_back(std::move(fallback_rule));

  std::vector<ProxyRule> web_config =
      TranslateProvisioningDomainRoutingConfig(dynamic_config);

  ASSERT_EQ(web_config.size(), 1u);
  ASSERT_TRUE(web_config[0].proxy_server.has_value());
  EXPECT_TRUE(web_config[0].proxy_server->is_https());
  EXPECT_EQ(web_config[0].proxy_server->GetHost(), kTestHttpsHost);
}

// Test that cross-platform destination matchers are mapped directly to plain
// strings without stripping wildcards, domain suffixes, or IP addresses.
//
// Undocumented Apple behavior rationale:
// Apple's `Network.framework` API (`nw_proxy_config_add_match_domain`) does not
// explicitly document support for wildcard prefixes (such as `*.apple.com`) or
// IP literals, but experimentation confirms that the underlying WebKit proxy
// configuration accepts wildcards, plain domain suffixes, IPv4, and IPv6
// strings.
//
// Meaning of test failure:
// If this test fails, `TranslateProvisioningDomainRoutingConfig` is mutating or
// dropping matchers unexpectedly. If the corresponding native test
// (`ProxyConfigurationProviderTest.UndocumentedNativeMatcherBehaviors`) fails,
// Apple has changed how `Network.framework` evaluates `match_domains` in a new
// iOS release, which would prevent Secure Gateway traffic steering from
// matching corporate subdomains or IP destinations.
TEST_F(PvDRouteTranslatorTest, StraightDestinationMatchersMapping) {
  net::ProxyConfig::DynamicRoutingConfig dynamic_config;
  net::ProxyConfig::DynamicRoutingRule dynamic_rule;
  dynamic_rule.proxy_list.AddProxyServer(
      net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                              kTestHttpsHost, kTestHttpsPort));
  dynamic_rule.destination_matchers.AddRuleFromString(kWildcardDomain);
  dynamic_rule.destination_matchers.AddRuleFromString(kPlainDomain);
  dynamic_rule.destination_matchers.AddRuleFromString(kIpv4Address);
  dynamic_rule.destination_matchers.AddRuleFromString(kIpv6Address);
  dynamic_config.routing_rules.push_back(std::move(dynamic_rule));

  std::vector<ProxyRule> web_config =
      TranslateProvisioningDomainRoutingConfig(dynamic_config);

  ASSERT_EQ(web_config.size(), 1u);
  const std::vector<std::string>& match_domains = web_config[0].match_domains;
  ASSERT_EQ(match_domains.size(), 4u);
  EXPECT_EQ(match_domains[0], kWildcardDomain);
  EXPECT_EQ(match_domains[1], kPlainDomain);
  EXPECT_EQ(match_domains[2], kIpv4Address);
  EXPECT_EQ(match_domains[3], kIpv6Address);
}

// Test that the ordering of routing rules is strictly maintained.
TEST_F(PvDRouteTranslatorTest, PreservesRuleOrder) {
  net::ProxyConfig::DynamicRoutingConfig dynamic_config;

  // Rule 0: HTTPS proxy for corp.com
  net::ProxyConfig::DynamicRoutingRule rule0;
  rule0.proxy_list.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, kTestHttpsHost, kTestHttpsPort));
  rule0.destination_matchers.AddRuleFromString("*.corp.com");
  dynamic_config.routing_rules.push_back(std::move(rule0));

  // Rule 1: HTTP proxy for internal.net
  net::ProxyConfig::DynamicRoutingRule rule1;
  rule1.proxy_list.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTP, kTestHttpHost, kTestProxyPort));
  rule1.destination_matchers.AddRuleFromString("*.internal.net");
  dynamic_config.routing_rules.push_back(std::move(rule1));

  // Rule 2: Direct for direct.example.com
  net::ProxyConfig::DynamicRoutingRule rule2;
  rule2.proxy_list.AddProxyChain(net::ProxyChain::Direct());
  rule2.destination_matchers.AddRuleFromString("direct.example.com");
  dynamic_config.routing_rules.push_back(std::move(rule2));

  std::vector<ProxyRule> web_config =
      TranslateProvisioningDomainRoutingConfig(dynamic_config);

  ASSERT_EQ(web_config.size(), 3u);

  // Rule 0 check
  ASSERT_TRUE(web_config[0].proxy_server.has_value());
  EXPECT_TRUE(web_config[0].proxy_server->is_https());
  ASSERT_EQ(web_config[0].match_domains.size(), 1u);
  EXPECT_EQ(web_config[0].match_domains[0], "*.corp.com");

  // Rule 1 check
  ASSERT_TRUE(web_config[1].proxy_server.has_value());
  EXPECT_TRUE(web_config[1].proxy_server->is_http());
  ASSERT_EQ(web_config[1].match_domains.size(), 1u);
  EXPECT_EQ(web_config[1].match_domains[0], "*.internal.net");

  // Rule 2 check
  EXPECT_FALSE(web_config[2].proxy_server.has_value());
  ASSERT_EQ(web_config[2].match_domains.size(), 1u);
  EXPECT_EQ(web_config[2].match_domains[0], "direct.example.com");
}

}  // namespace

}  // namespace web
