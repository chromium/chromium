// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/proxy/proxy_configuration_provider.h"

#import <Foundation/Foundation.h>
#import <Network/Network.h>
#import <WebKit/WebKit.h>

#import <string>
#import <vector>

#import "base/test/run_until.h"
#import "base/test/task_environment.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "net/base/proxy_server.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace web {

namespace {

// Extracts all match domains enumerated from `config`.
std::vector<std::string> GetMatchDomains(nw_proxy_config_t config) {
  std::vector<std::string> domains;
  if (@available(iOS 17.0, *)) {
    std::vector<std::string>* domains_ptr = &domains;
    nw_proxy_config_enumerate_match_domains(config, ^(const char* domain) {
      domains_ptr->push_back(domain);
    });
  }
  return domains;
}

// Extracts all excluded domains enumerated from `config`.
std::vector<std::string> GetExcludedDomains(nw_proxy_config_t config) {
  std::vector<std::string> domains;
  if (@available(iOS 17.0, *)) {
    std::vector<std::string>* domains_ptr = &domains;
    nw_proxy_config_enumerate_excluded_domains(config, ^(const char* domain) {
      domains_ptr->push_back(domain);
    });
  }
  return domains;
}

// Expected domains for a single native proxy configuration.
struct ExpectedProxyConfig {
  std::vector<std::string> match_domains;
  std::vector<std::string> excluded_domains;
};

// Verifies that the native proxy configurations in `WKWebsiteDataStore` for
// `browser_state` match the list of `expected_configs`.
void VerifyNativeProxyConfigurations(
    BrowserState* browser_state,
    const std::vector<ExpectedProxyConfig>& expected_configs) {
  if (@available(iOS 17.0, *)) {
    WKWebViewConfigurationProvider& config_provider =
        WKWebViewConfigurationProvider::FromBrowserState(browser_state);
    WKWebsiteDataStore* data_store = config_provider.GetWebsiteDataStore();
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return [data_store.proxyConfigurations count] == expected_configs.size();
    }));

    NSArray<nw_proxy_config_t>* native_configs = data_store.proxyConfigurations;
    ASSERT_EQ([native_configs count], expected_configs.size());
    for (size_t i = 0; i < expected_configs.size(); ++i) {
      nw_proxy_config_t config = [native_configs objectAtIndex:i];
      ASSERT_NE(config, nil);
      EXPECT_EQ(GetMatchDomains(config), expected_configs[i].match_domains);
      EXPECT_EQ(GetExcludedDomains(config),
                expected_configs[i].excluded_domains);
      EXPECT_FALSE(nw_proxy_config_get_failover_allowed(config));
    }
  }
}

// Helper to construct a direct connection rule (bypass).
ProxyRule DirectRule(std::vector<std::string> match_domains) {
  ProxyRule rule;
  rule.proxy_server = std::nullopt;
  rule.match_domains = std::move(match_domains);
  return rule;
}

// Helper to construct a proxy rule.
ProxyRule ProxyRuleFor(
    std::string host,
    uint16_t port,
    std::vector<std::string> match_domains,
    net::ProxyServer::Scheme scheme = net::ProxyServer::SCHEME_HTTP) {
  ProxyRule rule;
  rule.proxy_server =
      net::ProxyServer(scheme, net::HostPortPair(std::move(host), port));
  rule.match_domains = std::move(match_domains);
  return rule;
}

}  // namespace

class ProxyConfigurationProviderTest : public PlatformTest {
 public:
  ProxyConfigurationProviderTest()
      : web_client_(std::make_unique<FakeWebClient>()) {
    browser_state_.SetWebKitStorageID(base::Uuid::GenerateRandomV4());
  }
  ~ProxyConfigurationProviderTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  FakeBrowserState browser_state_;
};

// Tests that direct connection rules accumulate bypass domains, non-direct
// rules generate native configuration objects with bypass exclusions, and
// configs are applied to the active `WKWebsiteDataStore`.
TEST_F(ProxyConfigurationProviderTest, MappingAndBypassPrecedence) {
  ProxyConfigurationProvider& provider =
      ProxyConfigurationProvider::FromBrowserState(&browser_state_);

  provider.UpdateProxyConfiguration({
      DirectRule({"direct.example.com", "internal.corp"}),
      ProxyRuleFor("proxy.example.com", 8080, {"*.example.com"}),
  });

  VerifyNativeProxyConfigurations(
      &browser_state_,
      {
          {.match_domains = {"*.example.com"},
           .excluded_domains = {"direct.example.com", "internal.corp"}},
      });
}

// Tests a complex scenario with multiple direct connection rules interspersed
// with proxy rules to verify bypass accumulation: direct rules do not produce
// native configs, while subsequent proxy rules inherit all accumulated bypass
// domains as exclusions.
TEST_F(ProxyConfigurationProviderTest, MultipleBypassDomainsAccumulation) {
  ProxyConfigurationProvider& provider =
      ProxyConfigurationProvider::FromBrowserState(&browser_state_);

  provider.UpdateProxyConfiguration({
      DirectRule({"bypass1.example.com", "internal.corp"}),
      ProxyRuleFor("proxy1.example.com", 8080, {"*.example.com"}),
      DirectRule({"bypass2.example.com", "secure.local"}),
      ProxyRuleFor("proxy2.example.com", 8443, {"*.partner.com"}),
      DirectRule({"trailing.bypass.com"}),
  });

  VerifyNativeProxyConfigurations(
      &browser_state_,
      {
          {.match_domains = {"*.example.com"},
           .excluded_domains = {"bypass1.example.com", "internal.corp"}},
          {.match_domains = {"*.partner.com"},
           .excluded_domains = {"bypass1.example.com", "internal.corp",
                                "bypass2.example.com", "secure.local"}},
      });
}

// Tests that rapid updates to proxy configurations cancel ongoing background
// mapping tasks and apply only the latest configuration generation.
TEST_F(ProxyConfigurationProviderTest, CancellationOnSubsequentUpdate) {
  ProxyConfigurationProvider& provider =
      ProxyConfigurationProvider::FromBrowserState(&browser_state_);

  // Update with initial rules, then immediately supersede before
  // background tasks complete.
  provider.UpdateProxyConfiguration({
      ProxyRuleFor("initial-proxy.example.com", 8081, {"*.initial.com"}),
  });
  provider.UpdateProxyConfiguration({
      ProxyRuleFor("primary-proxy.example.com", 8082, {"*.primary.com"}),
      ProxyRuleFor("secondary-proxy.example.com", 8083, {"*.secondary.com"}),
  });

  VerifyNativeProxyConfigurations(&browser_state_,
                                  {
                                      {.match_domains = {"*.primary.com"}},
                                      {.match_domains = {"*.secondary.com"}},
                                  });
}

// Tests that updating with empty proxy rules clears existing
// native proxy configurations from the `WKWebsiteDataStore`.
TEST_F(ProxyConfigurationProviderTest, EmptyConfigurationClearsNativeConfigs) {
  ProxyConfigurationProvider& provider =
      ProxyConfigurationProvider::FromBrowserState(&browser_state_);

  // Initial non-empty configuration.
  provider.UpdateProxyConfiguration({
      ProxyRuleFor("proxy.example.com", 8080, {"*.example.com"}),
  });
  VerifyNativeProxyConfigurations(&browser_state_,
                                  {{.match_domains = {"*.example.com"}}});

  // Update with empty configuration.
  provider.UpdateProxyConfiguration({});
  VerifyNativeProxyConfigurations(&browser_state_, {});
}

// Tests that unsupported proxy schemes (e.g. SOCKS5) are skipped.
// TODO(crbug.com/476405339): Validate that warning logs get exposed on
// diagnostic `chrome://` pages (e.g. `chrome://net-export` or
// `chrome://policy`).
TEST_F(ProxyConfigurationProviderTest, UnsupportedProxySchemeIsSkipped) {
  ProxyConfigurationProvider& provider =
      ProxyConfigurationProvider::FromBrowserState(&browser_state_);

  provider.UpdateProxyConfiguration({
      ProxyRuleFor("socks.example.com", 1080, {"*.socks.com"},
                   net::ProxyServer::SCHEME_SOCKS5),
      ProxyRuleFor("http.example.com", 8080, {"*.http.com"},
                   net::ProxyServer::SCHEME_HTTP),
  });

  // Only the HTTP rule should produce a native proxy configuration.
  VerifyNativeProxyConfigurations(&browser_state_,
                                  {{.match_domains = {"*.http.com"}}});
}

// Tests that HTTPS proxy configurations are supported.
TEST_F(ProxyConfigurationProviderTest, HttpsProxyConfigurationSupported) {
  ProxyConfigurationProvider& provider =
      ProxyConfigurationProvider::FromBrowserState(&browser_state_);

  provider.UpdateProxyConfiguration({
      ProxyRuleFor("secure-proxy.example.com", 8443, {"*.secure.com"},
                   net::ProxyServer::SCHEME_HTTPS),
  });

  VerifyNativeProxyConfigurations(&browser_state_,
                                  {{.match_domains = {"*.secure.com"}}});
}

}  // namespace web
