// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller.h"

#import <memory>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/enterprise/net/core/features.h"
#import "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/proxy/proxy_config.h"
#import "ios/web/public/proxy/proxy_configuration_provider.h"
#import "net/base/proxy_server.h"
#import "net/proxy_resolution/proxy_config.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;

constexpr uint16_t kProxyPort = 8080;
constexpr std::string_view kProxyHost = "proxy.example.com";
constexpr std::string_view kFirstDestinationPattern = "corp1.example.com";
constexpr std::string_view kSecondDestinationPattern = "corp2.example.com";

class TestProxyConfigurationProvider : public web::ProxyConfigurationProvider {
 public:
  explicit TestProxyConfigurationProvider(web::BrowserState* browser_state)
      : web::ProxyConfigurationProvider(browser_state) {}

  void UpdateProxyConfiguration(std::vector<web::ProxyRule> rules) override {
    rules_ = std::move(rules);
  }

  const std::vector<web::ProxyRule>& rules() const { return rules_; }

 private:
  std::vector<web::ProxyRule> rules_;
};

net::ProxyConfig::DynamicRoutingConfig CreateSampleRoutingConfig(
    std::string_view destination_pattern) {
  net::ProxyConfig::DynamicRoutingConfig config;
  net::ProxyConfig::DynamicRoutingRule rule;
  rule.proxy_list.AddProxyServer(net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTP, std::string(kProxyHost), kProxyPort));
  rule.destination_matchers.AddRuleFromString(std::string(destination_pattern));
  config.routing_rules.push_back(std::move(rule));
  return config;
}

class ProxyServiceControllerTest : public PlatformTest {
 protected:
  ProxyServiceControllerTest() {
    feature_list_.InitAndEnableFeature(
        enterprise_net::kEnableDynamicRouteFetching);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        EnterpriseProxyServiceFactoryIOS::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  NiceMock<enterprise_net::MockEnterpriseProxyService>>();
            }));
    profile_ = std::move(builder).Build();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Test that initial routing rules are applied to the proxy provider when the
// controller is created.
TEST_F(ProxyServiceControllerTest, AppliesInitialRoutesOnCreation) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  EXPECT_CALL(mock_service, GetDynamicRoutingConfig())
      .WillOnce(Return(CreateSampleRoutingConfig(kFirstDestinationPattern)));

  TestProxyConfigurationProvider provider(profile_.get());
  ProxyServiceController controller(&mock_service, &provider);

  ASSERT_EQ(provider.rules().size(), 1u);
  EXPECT_EQ(provider.rules()[0].match_domains,
            std::vector<std::string>{std::string(kFirstDestinationPattern)});
}

// Test that route update notifications propagate from the enterprise service
// and update the rules in the proxy provider.
TEST_F(ProxyServiceControllerTest, DynamicRoutingUpdatesPropagate) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  EXPECT_CALL(mock_service, GetDynamicRoutingConfig())
      .WillOnce(Return(CreateSampleRoutingConfig(kFirstDestinationPattern)))
      .WillOnce(Return(CreateSampleRoutingConfig(kSecondDestinationPattern)));

  TestProxyConfigurationProvider provider(profile_.get());
  ProxyServiceController controller(&mock_service, &provider);

  mock_service.NotifyObservers();

  ASSERT_EQ(provider.rules().size(), 1u);
  EXPECT_EQ(provider.rules()[0].match_domains,
            std::vector<std::string>{std::string(kSecondDestinationPattern)});
}

// Test that when renewed routes are empty, the proxy provider configuration is
// cleared.
TEST_F(ProxyServiceControllerTest, ClearsRoutesWhenUpdatedWithEmptyConfig) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  EXPECT_CALL(mock_service, GetDynamicRoutingConfig())
      .WillOnce(Return(CreateSampleRoutingConfig(kFirstDestinationPattern)))
      .WillOnce(Return(net::ProxyConfig::DynamicRoutingConfig{}));

  TestProxyConfigurationProvider provider(profile_.get());
  ProxyServiceController controller(&mock_service, &provider);

  ASSERT_EQ(provider.rules().size(), 1u);

  mock_service.NotifyObservers();

  EXPECT_TRUE(provider.rules().empty());
}

// Test that ProxyServiceControllerFactory creates a controller for a regular
// profile and returns null for an off-the-record profile.
TEST_F(ProxyServiceControllerTest, FactoryCreatesControllerForProfile) {
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      EnterpriseProxyServiceFactoryIOS::GetInstance(),
      base::BindRepeating(
          [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
            return std::make_unique<
                NiceMock<enterprise_net::MockEnterpriseProxyService>>();
          }));
  builder.AddTestingFactory(ProxyServiceControllerFactory::GetInstance(),
                            ProxyServiceControllerFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  EXPECT_NE(ProxyServiceControllerFactory::GetForProfile(profile.get()),
            nullptr);

  ProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories();
  EXPECT_EQ(ProxyServiceControllerFactory::GetForProfile(otr_profile), nullptr);
}

}  // namespace
