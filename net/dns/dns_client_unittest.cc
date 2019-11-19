// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"

#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_test_util.h"
#include "net/socket/socket_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    client_ = DnsClient::CreateClientForTesting(
        nullptr /* net_log */, &socket_factory_, base::Bind(&base::RandInt));
  }

  DnsConfig BasicValidConfig() {
    DnsConfig config;
    config.nameservers = {IPEndPoint(IPAddress(2, 3, 4, 5), 123)};
    return config;
  }

  DnsConfig ValidConfigWithDoh() {
    DnsConfig config = BasicValidConfig();
    config.dns_over_https_servers = {DnsConfig::DnsOverHttpsServerConfig(
        "www.doh.com", true /* use_post */)};
    return config;
  }

  DnsConfigOverrides BasicValidOverrides() {
    DnsConfigOverrides config;
    config.nameservers.emplace({IPEndPoint(IPAddress(1, 2, 3, 4), 123)});
    return config;
  }

  std::unique_ptr<DnsClient> client_;
  AlwaysFailSocketFactory socket_factory_;
};

TEST_F(DnsClientTest, NoConfig) {
  client_->SetInsecureEnabled(true);

  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred());
  EXPECT_FALSE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_FALSE(client_->GetEffectiveConfig());
  EXPECT_FALSE(client_->GetHosts());
  EXPECT_FALSE(client_->GetTransactionFactory());
}

TEST_F(DnsClientTest, InvalidConfig) {
  client_->SetInsecureEnabled(true);
  client_->SetSystemConfig(DnsConfig());

  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred());
  EXPECT_FALSE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_FALSE(client_->GetEffectiveConfig());
  EXPECT_FALSE(client_->GetHosts());
  EXPECT_FALSE(client_->GetTransactionFactory());
}

TEST_F(DnsClientTest, CanUseSecureDnsTransactions_NoDohServers) {
  client_->SetInsecureEnabled(true);
  client_->SetSystemConfig(BasicValidConfig());

  EXPECT_FALSE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred());
  EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
  EXPECT_FALSE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(BasicValidConfig()));
  EXPECT_TRUE(client_->GetHosts());
  EXPECT_TRUE(client_->GetTransactionFactory());
}

TEST_F(DnsClientTest, InsecureNotEnabled) {
  client_->SetInsecureEnabled(false);
  client_->SetSystemConfig(ValidConfigWithDoh());

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred());
  EXPECT_FALSE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(ValidConfigWithDoh()));
  EXPECT_TRUE(client_->GetHosts());
  EXPECT_TRUE(client_->GetTransactionFactory());
}

TEST_F(DnsClientTest, CanUseSecureDnsTransactions_ProbeSuccess) {
  client_->SetSystemConfig(ValidConfigWithDoh());
  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred());

  client_->SetProbeSuccessForTest(0, true /* success */);
  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_FALSE(client_->FallbackFromSecureTransactionPreferred());
}

TEST_F(DnsClientTest, DnsOverTlsActive) {
  client_->SetInsecureEnabled(true);
  DnsConfig config = ValidConfigWithDoh();
  config.dns_over_tls_active = true;
  client_->SetSystemConfig(config);

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred());
  EXPECT_FALSE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_THAT(client_->GetEffectiveConfig(), testing::Pointee(config));
  EXPECT_TRUE(client_->GetHosts());
  EXPECT_TRUE(client_->GetTransactionFactory());
}

TEST_F(DnsClientTest, AllAllowed) {
  client_->SetInsecureEnabled(true);
  client_->SetSystemConfig(ValidConfigWithDoh());
  client_->SetProbeSuccessForTest(0, true /* success */);

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_FALSE(client_->FallbackFromSecureTransactionPreferred());
  EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
  EXPECT_FALSE(client_->FallbackFromInsecureTransactionPreferred());

  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(ValidConfigWithDoh()));
  EXPECT_TRUE(client_->GetHosts());
  EXPECT_TRUE(client_->GetTransactionFactory());
}

TEST_F(DnsClientTest, FallbackFromInsecureTransactionPreferred_Failures) {
  client_->SetInsecureEnabled(true);
  client_->SetSystemConfig(ValidConfigWithDoh());

  for (int i = 0; i < DnsClient::kMaxInsecureFallbackFailures; ++i) {
    EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
    EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred());
    EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
    EXPECT_FALSE(client_->FallbackFromInsecureTransactionPreferred());

    client_->IncrementInsecureFallbackFailures();
  }

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred());
  EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromInsecureTransactionPreferred());

  client_->ClearInsecureFallbackFailures();

  EXPECT_TRUE(client_->CanUseSecureDnsTransactions());
  EXPECT_TRUE(client_->FallbackFromSecureTransactionPreferred());
  EXPECT_TRUE(client_->CanUseInsecureDnsTransactions());
  EXPECT_FALSE(client_->FallbackFromInsecureTransactionPreferred());
}

TEST_F(DnsClientTest, Override) {
  client_->SetSystemConfig(BasicValidConfig());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(BasicValidConfig()));

  client_->SetConfigOverrides(BasicValidOverrides());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(
                  BasicValidOverrides().ApplyOverrides(BasicValidConfig())));

  client_->SetConfigOverrides(DnsConfigOverrides());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(BasicValidConfig()));
}

// Cannot apply overrides without a system config unless everything is
// overridden
TEST_F(DnsClientTest, OverrideNoConfig) {
  client_->SetConfigOverrides(BasicValidOverrides());
  EXPECT_FALSE(client_->GetEffectiveConfig());

  auto override_everything =
      DnsConfigOverrides::CreateOverridingEverythingWithDefaults();
  override_everything.nameservers.emplace(
      {IPEndPoint(IPAddress(1, 2, 3, 4), 123)});
  client_->SetConfigOverrides(override_everything);
  EXPECT_THAT(
      client_->GetEffectiveConfig(),
      testing::Pointee(override_everything.ApplyOverrides(DnsConfig())));
}

TEST_F(DnsClientTest, OverrideInvalidConfig) {
  client_->SetSystemConfig(DnsConfig());
  EXPECT_FALSE(client_->GetEffectiveConfig());

  client_->SetConfigOverrides(BasicValidOverrides());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(
                  BasicValidOverrides().ApplyOverrides(BasicValidConfig())));
}

TEST_F(DnsClientTest, OverrideToInvalid) {
  client_->SetSystemConfig(BasicValidConfig());
  EXPECT_THAT(client_->GetEffectiveConfig(),
              testing::Pointee(BasicValidConfig()));

  DnsConfigOverrides overrides;
  overrides.nameservers.emplace();
  client_->SetConfigOverrides(std::move(overrides));

  EXPECT_FALSE(client_->GetEffectiveConfig());
}

TEST_F(DnsClientTest, ActivateDohProbes) {
  client_->SetSystemConfig(ValidConfigWithDoh());
  auto transaction_factory =
      std::make_unique<MockDnsTransactionFactory>(MockDnsClientRuleList());
  auto* transaction_factory_ptr = transaction_factory.get();
  client_->SetTransactionFactoryForTesting(std::move(transaction_factory));

  ASSERT_FALSE(transaction_factory_ptr->doh_probes_running());

  URLRequestContext context;
  client_->ActivateDohProbes(&context);
  EXPECT_TRUE(transaction_factory_ptr->doh_probes_running());
}

TEST_F(DnsClientTest, CancelDohProbes) {
  client_->SetSystemConfig(ValidConfigWithDoh());
  auto transaction_factory =
      std::make_unique<MockDnsTransactionFactory>(MockDnsClientRuleList());
  auto* transaction_factory_ptr = transaction_factory.get();
  client_->SetTransactionFactoryForTesting(std::move(transaction_factory));

  URLRequestContext context;
  client_->ActivateDohProbes(&context);

  ASSERT_TRUE(transaction_factory_ptr->doh_probes_running());

  client_->CancelDohProbes();
  EXPECT_FALSE(transaction_factory_ptr->doh_probes_running());
}

TEST_F(DnsClientTest, CancelDohProbes_BeforeConfig) {
  URLRequestContext context;
  client_->ActivateDohProbes(&context);
  client_->CancelDohProbes();

  client_->SetSystemConfig(ValidConfigWithDoh());
  auto transaction_factory =
      std::make_unique<MockDnsTransactionFactory>(MockDnsClientRuleList());
  auto* transaction_factory_ptr = transaction_factory.get();
  client_->SetTransactionFactoryForTesting(std::move(transaction_factory));

  EXPECT_FALSE(transaction_factory_ptr->doh_probes_running());
}

}  // namespace

}  // namespace net
