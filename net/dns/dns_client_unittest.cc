// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/opt_record_rdata.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/resolve_context.h"
#include "net/socket/socket_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

namespace net {

class ClientSocketFactory;

namespace {

class AlwaysFailSocketFactory : public MockClientSocketFactory {
 public:
  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    return std::make_unique<MockUDPClientSocket>();
  }
};

class DnsClientTest : public TestWithTaskEnvironment {
 protected:
  DnsClientTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    client_ = DnsClient::CreateClient(/*net_log=*/NetLog::Get());
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(&socket_factory_);
    request_context_ = context_builder->Build();
    resolve_context_ = std::make_unique<ResolveContext>(
        request_context_.get(), false /* enable_caching */);
  }

  DnsConfig BasicValidConfig() {
    DnsConfig config;
    config.nameservers = {IPEndPoint(IPAddress(2, 3, 4, 5), 123)};
    return config;
  }

  DnsConfig ValidConfigWithDoh(bool doh_only) {
    DnsConfig config;
    if (!doh_only) {
      config = BasicValidConfig();
    }
    config.doh_config =
        *net::DnsOverHttpsConfig::FromString("https://www.doh.com/");
    return config;
  }

  DnsConfigOverrides BasicValidOverrides() {
    DnsConfigOverrides config;
    config.nameservers.emplace({IPEndPoint(IPAddress(1, 2, 3, 4), 123)});
    return config;
  }

  IPEndPoint Loopbackv4() { return IPEndPoint(IPAddress::IPv4Localhost(), 53); }

  IPEndPoint Loopbackv6() { return IPEndPoint(IPAddress::IPv6Localhost(), 53); }

  IPAddress PublicDnsIp() { return IPAddress(1, 2, 3, 4); }

  IPAddress GooglePublicDnsIp() { return IPAddress(8, 8, 8, 8); }

  IPAddress PrivateDnsIp() { return IPAddress(192, 168, 1, 1); }

  std::unique_ptr<URLRequestContext> request_context_;
  std::unique_ptr<ResolveContext> resolve_context_;
  std::unique_ptr<DnsClient> client_;
  AlwaysFailSocketFactory socket_factory_;
};

TEST_F(DnsClientTest, NoConfig) {
  client_->SetInsecureEnabled(/*enabled=*/true,
                              /*additional_types_enabled=*/true);

  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_FALSE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_FALSE(client_->GetEffectiveConfig());
  EXPECT_FALSE(client_->GetHosts());
  EXPECT_FALSE(client_->GetTransactionFactory());
  EXPECT_FALSE(client_->GetCurrentSession());
}

TEST_F(DnsClientTest, InvalidConfig) {
  client_->SetInsecureEnabled(/*enabled=*/true,
                              /*additional_types_enabled=*/true);
  client_->SetSystemConfig(DnsConfig());

  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_FALSE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_FALSE(client_->GetEffectiveConfig());
  EXPECT_FALSE(client_->GetHosts());
  EXPECT_FALSE(client_->GetTransactionFactory());
  EXPECT_FALSE(client_->GetCurrentSession());
}

TEST_F(DnsClientTest, CanUseSecureDnsTransactions_NoDohServers) {
  client_->SetInsecureEnabled(/*enabled=*/true,
                              /*additional_types_enabled=*/true);
  client_->SetSystemConfig(BasicValidConfig());

  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->CanQueryAdditionalTypesViaInsecureDns());
  EXPECT_FALSE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(BasicValidConfig()));
  EXPECT_TRUE(client_->GetHosts());
  EXPECT_TRUE(client_->GetTransactionFactory());
  EXPECT_EQ(client_->GetCurrentSession()->config(), BasicValidConfig());
}

TEST_F(DnsClientTest, InsecureNotEnabled) {
  client_->SetInsecureEnabled(/*enabled=*/false,
                              /*additional_types_enabled=*/false);
  client_->SetSystemConfig(ValidConfigWithDoh(false /* doh_only */));

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_FALSE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(ValidConfigWithDoh(false /* doh_only */)));
  EXPECT_TRUE(client_->GetHosts());
  EXPECT_TRUE(client_->GetTransactionFactory());
  EXPECT_EQ(client_->GetCurrentSession()->config(),
            ValidConfigWithDoh(false /* doh_only */));
}

TEST_F(DnsClientTest, RespectsAdditionalTypesDisabled) {
  client_->SetInsecureEnabled(/*enabled=*/true,
                              /*additional_types_enabled=*/false);
  client_->SetSystemConfig(BasicValidConfig());

  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
  EXPECT_FALSE(client_->CanQueryAdditionalTypesViaInsecureDns());
  EXPECT_FALSE(client_->FallbackFromInsecureTransactionPreferred());
}

TEST_F(DnsClientTest, UnhandledOptions) {
  client_->SetInsecureEnabled(/*enabled=*/true,
                              /*additional_types_enabled=*/true);
  DnsConfig config = ValidConfigWithDoh(false /* doh_only */);
  config.unhandled_options = true;
  client_->SetSystemConfig(config);

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_FALSE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  DnsConfig expected_config = config;
  expected_config.nameservers.clear();
  EXPECT_THAT(client_->GetEffectiveConfig(), testing::Pointee(expected_config));
  EXPECT_TRUE(client_->GetHosts());
  EXPECT_TRUE(client_->GetTransactionFactory());
  EXPECT_EQ(client_->GetCurrentSession()->config(), expected_config);
}

TEST_F(DnsClientTest, CanUseSecureDnsTransactions_ProbeSuccess) {
  client_->SetSystemConfig(ValidConfigWithDoh(true /* doh_only */));
  resolve_context_->InvalidateCachesAndPerSessionData(
      client_->GetCurrentSession(), true /* network_change */);

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));

  resolve_context_->RecordServerSuccess(0u /* server_index */,
                                        true /* is_doh_server */,
                                        client_->GetCurrentSession());
  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_FALSE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
}

TEST_F(DnsClientTest, DnsOverTlsActive) {
  client_->SetInsecureEnabled(/*enabled=*/true,
                              /*additional_types_enabled=*/true);
  DnsConfig config = ValidConfigWithDoh(false /* doh_only */);
  config.dns_over_tls_active = true;
  client_->SetSystemConfig(config);

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_FALSE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_THAT(client_->GetEffectiveConfig(), testing::Pointee(config));
  EXPECT_TRUE(client_->GetHosts());
  EXPECT_TRUE(client_->GetTransactionFactory());
  EXPECT_EQ(client_->GetCurrentSession()->config(), config);
}

TEST_F(DnsClientTest, AllAllowed) {
  client_->SetInsecureEnabled(/*enabled=*/true,
                              /*additional_types_enabled=*/true);
  client_->SetSystemConfig(ValidConfigWithDoh(false /* doh_only */));
  resolve_context_->InvalidateCachesAndPerSessionData(
      client_->GetCurrentSession(), false /* network_change */);
  resolve_context_->RecordServerSuccess(0u /* server_index */,
                                        true /* is_doh_server */,
                                        client_->GetCurrentSession());

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_FALSE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->CanQueryAdditionalTypesViaInsecureDns());
  EXPECT_FALSE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(ValidConfigWithDoh(false /* doh_only */)));
  EXPECT_TRUE(client_->GetHosts());
  EXPECT_TRUE(client_->GetTransactionFactory());
  EXPECT_EQ(client_->GetCurrentSession()->config(),
            ValidConfigWithDoh(false /* doh_only */));
}

TEST_F(DnsClientTest, FallbackFromInsecureTransactionPreferred_Failures) {
  client_->SetInsecureEnabled(/*enabled=*/true,
                              /*additional_types_enabled=*/true);
  client_->SetSystemConfig(ValidConfigWithDoh(false /* doh_only */));

  for (int i = 0; i < DnsClient::kMaxInsecureFallbackFailures; ++i) {
    EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
    EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred(
        resolve_context_.get()));
    EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
    EXPECT_TRUE(client_->CanQueryAdditionalTypesViaInsecureDns());
    EXPECT_FALSE(client_->FallbackFromInsecureTransactionPreferred());

    client_->IncrementInsecureFallbackFailures();
  }

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->CanQueryAdditionalTypesViaInsecureDns());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  client_->ClearInsecureFallbackFailures();

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(
      client_->FallbackFromSecureTransactionPreferred(resolve_context_.get()));
  EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->CanQueryAdditionalTypesViaInsecureDns());
  EXPECT_FALSE(client_->FallbackFromInsecureTransactionPreferred());
}

TEST_F(DnsClientTest, GetPresetAddrs) {
  DnsConfig config;
  config.doh_config = *net::DnsOverHttpsConfig::FromString(R"(
    {
      "servers": [{
        "template": "https://www.doh.com/",
        "endpoints": [{
          "ips": ["4.3.2.1"]
        }, {
          "ips": ["4.3.2.2"]
        }]
      }]
    }
  )");
  client_->SetSystemConfig(config);

  EXPECT_FALSE(client_->GetPresetAddrs(
      url::SchemeHostPort("https", "otherdomain.com", 443)));
  EXPECT_FALSE(
      client_->GetPresetAddrs(url::SchemeHostPort("http", "www.doh.com", 443)));
  EXPECT_FALSE(client_->GetPresetAddrs(
      url::SchemeHostPort("https", "www.doh.com", 9999)));

  std::vector<IPEndPoint> expected({{{4, 3, 2, 1}, 443}, {{4, 3, 2, 2}, 443}});

  EXPECT_THAT(
      client_->GetPresetAddrs(url::SchemeHostPort("https", "www.doh.com", 443)),
      testing::Optional(expected));
}

TEST_F(DnsClientTest,
       SetSystemConfig_AutomaticModeWithDohFallback_AddsFallback) {
  // The DoH config is replaced with a fallback server if:
  // - Secure DNS is used in Automatic mode
  // - The DNS Config has no DoH servers and no local nameservers set
  // - The kAddAutomaticWithDohFallbackMode feature flag is enabled
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kAddAutomaticWithDohFallbackMode);
  base::HistogramTester histogram_tester;

  DnsConfig initial_config = BasicValidConfig();
  initial_config.secure_dns_mode = SecureDnsMode::kAutomatic;
  initial_config.allow_dns_over_https_upgrade = true;
  client_->SetSystemConfig(initial_config);

  // Check that kAutomatic doesn't change the config without a
  // fallback server set.
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(std::move(initial_config)));
  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeWithFallbackSucceeded", false, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded", false, 1);

  DnsConfigOverrides overrides = BasicValidOverrides();
  // Use well-known nameserver that is supported for DoH upgrade.
  std::vector<net::IPEndPoint> fallback_doh_nameservers = {net::IPEndPoint(
      net::IPAddress(8, 8, 8, 8), net::dns_protocol::kDefaultPort)};
  std::vector<DnsOverHttpsServerConfig> fallback_doh_configs =
      net::GetDohUpgradeServersFromNameservers(fallback_doh_nameservers);
  ASSERT_GT(fallback_doh_configs.size(), 0u);
  overrides.fallback_doh_nameservers = fallback_doh_nameservers;
  client_->SetConfigOverrides(std::move(overrides));

  // The DNS config now has the fallback nameservers which are used to set
  // the DoH config, enabling Secure DNS.
  EXPECT_THAT(client_->GetEffectiveConfig()->doh_config,
              DnsOverHttpsConfig(fallback_doh_configs));
  EXPECT_THAT(client_->GetEffectiveConfig()->fallback_doh_nameservers,
              fallback_doh_nameservers);
  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeWithFallbackSucceeded", true, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded", true, 1);
}

TEST_F(
    DnsClientTest,
    SetSystemConfig_AutomaticModeWithDohFallback_WithIpv4Loopback_DoesntAddFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kAddAutomaticWithDohFallbackMode);
  base::HistogramTester histogram_tester;

  DnsConfig initial_config = BasicValidConfig();
  initial_config.secure_dns_mode = SecureDnsMode::kAutomatic;
  initial_config.allow_dns_over_https_upgrade = true;
  initial_config.nameservers.emplace_back(IPAddress::IPv4Localhost(),
                                          dns_protocol::kDefaultPort);
  client_->SetSystemConfig(std::move(initial_config));
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeWithFallbackSucceeded", false, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded", false, 1);

  DnsConfigOverrides overrides;
  std::vector<net::IPEndPoint> fallback_doh_nameservers = {net::IPEndPoint(
      net::IPAddress(8, 8, 8, 8), net::dns_protocol::kDefaultPort)};
  overrides.fallback_doh_nameservers = std::move(fallback_doh_nameservers);
  client_->SetConfigOverrides(std::move(overrides));

  // Check that the fallback DoH nameservers aren't applied to the DoH config.
  EXPECT_EQ(client_->GetEffectiveConfig()->doh_config, DnsOverHttpsConfig());
  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeWithFallbackSucceeded", false, 2);
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded", false, 2);
}

TEST_F(
    DnsClientTest,
    SetSystemConfig_AutomaticModeWithDohFallback_WithIpv6Loopback_DoesntAddFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kAddAutomaticWithDohFallbackMode);
  base::HistogramTester histogram_tester;

  DnsConfig initial_config = BasicValidConfig();
  initial_config.secure_dns_mode = SecureDnsMode::kAutomatic;
  initial_config.allow_dns_over_https_upgrade = true;
  initial_config.nameservers.emplace_back(IPAddress::IPv6Localhost(),
                                          dns_protocol::kDefaultPort);
  client_->SetSystemConfig(std::move(initial_config));
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeWithFallbackSucceeded", false, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded", false, 1);

  DnsConfigOverrides overrides;
  std::vector<net::IPEndPoint> fallback_doh_nameservers = {net::IPEndPoint(
      net::IPAddress(8, 8, 8, 8), net::dns_protocol::kDefaultPort)};
  overrides.fallback_doh_nameservers = std::move(fallback_doh_nameservers);
  client_->SetConfigOverrides(std::move(overrides));

  // Check that the fallback DoH nameservers aren't applied to the DoH config.
  EXPECT_EQ(client_->GetEffectiveConfig()->doh_config, DnsOverHttpsConfig());
  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeWithFallbackSucceeded", false, 2);
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded", false, 2);
}

TEST_F(
    DnsClientTest,
    SetSystemConfig_AutomaticModeWithDohFallback_WithLocalAddress_DoesntAddFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kAddAutomaticWithDohFallbackMode);
  base::HistogramTester histogram_tester;

  DnsConfig initial_config = BasicValidConfig();
  initial_config.secure_dns_mode = SecureDnsMode::kAutomatic;
  initial_config.allow_dns_over_https_upgrade = true;
  initial_config.nameservers.emplace_back(IPAddress(192, 168, 1, 1),
                                          dns_protocol::kDefaultPort);
  client_->SetSystemConfig(std::move(initial_config));
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeWithFallbackSucceeded", false, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded", false, 1);

  DnsConfigOverrides overrides;
  std::vector<net::IPEndPoint> fallback_doh_nameservers = {net::IPEndPoint(
      net::IPAddress(8, 8, 8, 8), net::dns_protocol::kDefaultPort)};
  overrides.fallback_doh_nameservers = std::move(fallback_doh_nameservers);
  client_->SetConfigOverrides(std::move(overrides));

  // Check that the fallback DoH nameservers aren't applied to the DoH config.
  EXPECT_EQ(client_->GetEffectiveConfig()->doh_config, DnsOverHttpsConfig());
  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeWithFallbackSucceeded", false, 2);
  histogram_tester.ExpectBucketCount(
      "Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded", false, 2);
}

TEST_F(
    DnsClientTest,
    SetSystemConfig_AutomaticModeWithDohFallback_AutoUpgradeSucceeds_DoesntAddFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kAddAutomaticWithDohFallbackMode);

  DnsConfig initial_config;
  initial_config.secure_dns_mode = SecureDnsMode::kAutomatic;
  initial_config.allow_dns_over_https_upgrade = true;
  // Use well-known nameserver that is supported for DoH upgrade.
  initial_config.nameservers.emplace_back(IPAddress(8, 8, 8, 8),
                                          dns_protocol::kDefaultPort);
  client_->SetSystemConfig(initial_config);

  // Set a different server for fallback DoH to differentiate which one was used
  // for the upgrade.
  DnsConfigOverrides overrides;
  std::vector<net::IPEndPoint> fallback_doh_nameservers = {net::IPEndPoint(
      net::IPAddress(1, 1, 1, 1), net::dns_protocol::kDefaultPort)};
  overrides.fallback_doh_nameservers = std::move(fallback_doh_nameservers);
  client_->SetConfigOverrides(std::move(overrides));

  // The DoH config should be from the standard autoupgrade, not the fallback.
  std::vector<DnsOverHttpsServerConfig> expected_doh_configs =
      net::GetDohUpgradeServersFromNameservers(initial_config.nameservers);
  EXPECT_THAT(client_->GetEffectiveConfig()->doh_config,
              DnsOverHttpsConfig(expected_doh_configs));
  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
}

TEST_F(DnsClientTest, Override) {
  client_->SetSystemConfig(BasicValidConfig());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(BasicValidConfig()));
  EXPECT_EQ(client_->GetCurrentSession()->config(), BasicValidConfig());

  client_->SetConfigOverrides(BasicValidOverrides());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(
                  BasicValidOverrides().ApplyOverrides(BasicValidConfig())));
  EXPECT_EQ(client_->GetCurrentSession()->config(),
            BasicValidOverrides().ApplyOverrides(BasicValidConfig()));

  client_->SetConfigOverrides(DnsConfigOverrides());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(BasicValidConfig()));
  EXPECT_EQ(client_->GetCurrentSession()->config(), BasicValidConfig());
}

// Cannot apply overrides without a system config unless everything is
// overridden
TEST_F(DnsClientTest, OverrideNoConfig) {
  client_->SetConfigOverrides(BasicValidOverrides());
  EXPECT_FALSE(client_->GetEffectiveConfig());
  EXPECT_FALSE(client_->GetCurrentSession());

  auto override_everything =
      DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
  override_everything.nameservers.emplace(
      {IPEndPoint(IPAddress(1, 2, 3, 4), 123)});
  client_->SetConfigOverrides(override_everything);
  EXPECT_THAT(
      client_->GetEffectiveConfig(),
      testing::Pointee(override_everything.ApplyOverrides(DnsConfig())));
  EXPECT_EQ(client_->GetCurrentSession()->config(),
            override_everything.ApplyOverrides(DnsConfig()));
}

TEST_F(DnsClientTest, OverrideInvalidConfig) {
  client_->SetSystemConfig(DnsConfig());
  EXPECT_FALSE(client_->GetEffectiveConfig());
  EXPECT_FALSE(client_->GetCurrentSession());

  client_->SetConfigOverrides(BasicValidOverrides());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(
                  BasicValidOverrides().ApplyOverrides(BasicValidConfig())));
  EXPECT_EQ(client_->GetCurrentSession()->config(),
            BasicValidOverrides().ApplyOverrides(DnsConfig()));
}

TEST_F(DnsClientTest, OverrideToInvalid) {
  client_->SetSystemConfig(BasicValidConfig());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(BasicValidConfig()));
  EXPECT_EQ(client_->GetCurrentSession()->config(), BasicValidConfig());

  DnsConfigOverrides overrides;
  overrides.nameservers.emplace();
  client_->SetConfigOverrides(std::move(overrides));

  EXPECT_FALSE(client_->GetEffectiveConfig());
  EXPECT_FALSE(client_->GetCurrentSession());
}

TEST_F(DnsClientTest, ReplaceCurrentSession) {
  client_->SetSystemConfig(BasicValidConfig());

  base::WeakPtr<DnsSession> session_before =
      client_->GetCurrentSession()->GetWeakPtr();
  ASSERT_TRUE(session_before);

  client_->ReplaceCurrentSession();

  EXPECT_FALSE(session_before);
  EXPECT_TRUE(client_->GetCurrentSession());
}

TEST_F(DnsClientTest, ReplaceCurrentSession_NoSession) {
  ASSERT_FALSE(client_->GetCurrentSession());

  client_->ReplaceCurrentSession();

  EXPECT_FALSE(client_->GetCurrentSession());
}

TEST_F(DnsClientTest, AutoUpgradeSucceeds) {
  base::HistogramTester histogram_tester;
  DnsConfig config;
  config.nameservers = {IPEndPoint(GooglePublicDnsIp(), 53)};
  config.secure_dns_mode = SecureDnsMode::kAutomatic;
  config.allow_dns_over_https_upgrade = true;

  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  client_->SetSystemConfig(std::move(config));
  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());

  histogram_tester.ExpectUniqueSample(
      "Net.DNS.UpgradeConfig.InsecureUpgradeSucceeded", true, 1);
  histogram_tester.ExpectTotalCount(
      "Net.DNS.UpgradeConfigFailed.LocalNameserverState", 0);
}

TEST_F(DnsClientTest, AutoUpgradeFails_NoLocalNameservers) {
  base::HistogramTester histogram_tester;
  DnsConfig config;
  config.nameservers = {IPEndPoint(PublicDnsIp(), 53)};
  config.secure_dns_mode = SecureDnsMode::kAutomatic;
  config.allow_dns_over_https_upgrade = true;
  client_->SetSystemConfig(std::move(config));

  histogram_tester.ExpectUniqueSample(
      "Net.DNS.UpgradeConfigFailed.LocalNameserverState",
      net::DnsConfigLocalNameserverState::kNoLocal, 1);
}

TEST_F(DnsClientTest, AutoUpgradeFails_OnlyLoopbackNameservers) {
  base::HistogramTester histogram_tester;
  DnsConfig config;
  config.nameservers = {Loopbackv4(), Loopbackv6(),
                        IPEndPoint(PublicDnsIp(), 53)};
  config.secure_dns_mode = SecureDnsMode::kAutomatic;
  config.allow_dns_over_https_upgrade = true;
  client_->SetSystemConfig(std::move(config));

  histogram_tester.ExpectUniqueSample(
      "Net.DNS.UpgradeConfigFailed.LocalNameserverState",
      net::DnsConfigLocalNameserverState::kOnlyLoopback, 1);
}

TEST_F(DnsClientTest, AutoUpgradeFails_OnlyNonLoopbackLocalNameservers) {
  base::HistogramTester histogram_tester;
  DnsConfig config;
  config.nameservers = {IPEndPoint(PrivateDnsIp(), 53)};
  config.secure_dns_mode = SecureDnsMode::kAutomatic;
  config.allow_dns_over_https_upgrade = true;
  client_->SetSystemConfig(std::move(config));

  histogram_tester.ExpectUniqueSample(
      "Net.DNS.UpgradeConfigFailed.LocalNameserverState",
      net::DnsConfigLocalNameserverState::kOnlyNonLoopbackLocal, 1);
}

TEST_F(DnsClientTest, AutoUpgradeFails_LoopbackAndNonLoopbackLocalNameservers) {
  base::HistogramTester histogram_tester;
  DnsConfig config;
  config.nameservers = {Loopbackv4(), IPEndPoint(PrivateDnsIp(), 53)};
  config.secure_dns_mode = SecureDnsMode::kAutomatic;
  config.allow_dns_over_https_upgrade = true;
  client_->SetSystemConfig(std::move(config));

  histogram_tester.ExpectUniqueSample(
      "Net.DNS.UpgradeConfigFailed.LocalNameserverState",
      net::DnsConfigLocalNameserverState::kLoopbackAndNonLoopback, 1);
}

class DnsClientFeatureTest : public base::test::WithFeatureOverride,
                             public DnsClientTest {
 public:
  DnsClientFeatureTest()
      : base::test::WithFeatureOverride(features::kUseStructuredDnsErrors) {}
};

TEST_P(DnsClientFeatureTest, EdnsOptions) {
  // This forces a call to UpdateSession, which constructs a new factory, which
  // should have the EDE option added when the feature is enabled.
  client_->SetSystemConfig(BasicValidConfig());
  OptRecordRdata* rdata =
      client_->GetTransactionFactory()->GetOptRdataForTest();
  if (IsParamFeatureEnabled()) {
    ASSERT_THAT(rdata, testing::NotNull());
    const auto& ede = rdata->GetEdeOpts();
    ASSERT_EQ(1u, ede.size());
    EXPECT_EQ(*OptRecordRdata::EdeOpt::CreateStructuredErrorsRequest(),
              *ede.front());
  } else {
    ASSERT_THAT(rdata, testing::IsNull());
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(DnsClientFeatureTest);

}  // namespace

}  // namespace net
