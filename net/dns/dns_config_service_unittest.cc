// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/test_dns_config_service.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

class DnsConfigServiceTest : public TestWithTaskEnvironment {
 public:
  DnsConfigServiceTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void OnConfigChanged(const DnsConfig& config) {
    last_config_ = config;
    if (quit_on_config_)
      std::move(quit_on_config_).Run();
  }

 protected:
  void WaitForConfig() {
    base::RunLoop run_loop;
    quit_on_config_ = run_loop.QuitClosure();

    // Some work may be performed on `ThreadPool` and is not accounted for in a
    // `RunLoop::RunUntilIdle()` call.
    run_loop.RunUntilIdle();
    base::ThreadPoolInstance::Get()->FlushForTesting();
    if (!run_loop.AnyQuitCalled())
      run_loop.RunUntilIdle();

    // Validate a config notification was received.
    ASSERT_TRUE(run_loop.AnyQuitCalled());
  }

  void WaitForInvalidationTimeout() {
    base::RunLoop run_loop;
    quit_on_config_ = run_loop.QuitClosure();
    FastForwardBy(DnsConfigService::kInvalidationTimeout);

    // Validate a config notification was received, and that it was an empty
    // config (empty config always expected for invalidation).
    ASSERT_TRUE(run_loop.AnyQuitCalled());
    ASSERT_EQ(last_config_, DnsConfig());
  }

  void ValidateNoNotification() {
    base::RunLoop run_loop;
    quit_on_config_ = run_loop.QuitClosure();

    // Flush any potential work and wait for any potential invalidation timeout.
    run_loop.RunUntilIdle();
    base::ThreadPoolInstance::Get()->FlushForTesting();
    if (!run_loop.AnyQuitCalled())
      run_loop.RunUntilIdle();
    FastForwardBy(DnsConfigService::kInvalidationTimeout);

    // Validate no config notification was received.
    ASSERT_FALSE(run_loop.AnyQuitCalled());
    quit_on_config_.Reset();
  }

  // Generate a config using the given seed..
  DnsConfig MakeConfig(unsigned seed) {
    DnsConfig config;
    config.nameservers.emplace_back(IPAddress(1, 2, 3, 4), seed & 0xFFFF);
    EXPECT_TRUE(config.IsValid());
    return config;
  }

  // Generate hosts using the given seed.
  DnsHosts MakeHosts(unsigned seed) {
    DnsHosts hosts;
    std::string hosts_content = "127.0.0.1 localhost";
    hosts_content.append(seed, '1');
    ParseHosts(hosts_content, &hosts);
    EXPECT_FALSE(hosts.empty());
    return hosts;
  }

  void SetUpService(TestDnsConfigService& service) {
    service.WatchConfig(base::BindRepeating(
        &DnsConfigServiceTest::OnConfigChanged, base::Unretained(this)));

    // Run through any initial config notifications triggered by starting the
    // watch.
    base::RunLoop run_loop;
    quit_on_config_ = run_loop.QuitClosure();
    run_loop.RunUntilIdle();
    base::ThreadPoolInstance::Get()->FlushForTesting();
    run_loop.RunUntilIdle();
    FastForwardBy(DnsConfigService::kInvalidationTimeout);
    quit_on_config_.Reset();
  }

  void SetUp() override {
    service_ = std::make_unique<TestDnsConfigService>();
    SetUpService(*service_);
    EXPECT_FALSE(last_config_.IsValid());
  }

  void TearDown() override {
    // After test, expect no more config notifications.
    ValidateNoNotification();
  }

  DnsConfig last_config_;
  base::OnceClosure quit_on_config_;

  // Service under test.
  std::unique_ptr<TestDnsConfigService> service_;
};

class MockHostsParserFactory : public DnsHostsParser {
 public:
  HostsReadingTestDnsConfigService::HostsParserFactory GetFactory();

  MOCK_METHOD(bool, ParseHosts, (DnsHosts*), (const, override));

 private:
  class Delegator : public DnsHostsParser {
   public:
    explicit Delegator(MockHostsParserFactory* factory) : factory_(factory) {}

    bool ParseHosts(DnsHosts* hosts) const override {
      return factory_->ParseHosts(hosts);
    }

   private:
    raw_ptr<MockHostsParserFactory> factory_;
  };
};

HostsReadingTestDnsConfigService::HostsParserFactory
MockHostsParserFactory::GetFactory() {
  return base::BindLambdaForTesting(
      [this]() -> std::unique_ptr<DnsHostsParser> {
        return std::make_unique<Delegator>(this);
      });
}

DnsHosts::value_type CreateHostsEntry(std::string_view name,
                                      AddressFamily family,
                                      IPAddress address) {
  DnsHostsKey key = std::pair(std::string(name), family);
  return std::pair(std::move(key), address);
}

}  // namespace

TEST_F(DnsConfigServiceTest, FirstConfig) {
  DnsConfig config = MakeConfig(1);

  service_->OnConfigRead(config);
  // No hosts yet, so no config.
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.Equals(config));
}

TEST_F(DnsConfigServiceTest, Timeout) {
  DnsConfig config = MakeConfig(1);
  config.hosts = MakeHosts(1);
  ASSERT_TRUE(config.IsValid());

  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  service_->InvalidateConfig();
  WaitForInvalidationTimeout();
  EXPECT_FALSE(last_config_.Equals(config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  service_->OnConfigRead(config);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  service_->InvalidateHosts();
  WaitForInvalidationTimeout();
  EXPECT_FALSE(last_config_.Equals(config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  DnsConfig bad_config = last_config_ = MakeConfig(0xBAD);
  service_->InvalidateConfig();
  ValidateNoNotification();
  EXPECT_TRUE(last_config_.Equals(bad_config)) << "Unexpected change";

  last_config_ = DnsConfig();
  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));
}

TEST_F(DnsConfigServiceTest, SameConfig) {
  DnsConfig config = MakeConfig(1);
  config.hosts = MakeHosts(1);

  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  last_config_ = DnsConfig();
  service_->OnConfigRead(config);
  EXPECT_TRUE(last_config_.Equals(DnsConfig())) << "Unexpected change";

  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.Equals(DnsConfig())) << "Unexpected change";
}

TEST_F(DnsConfigServiceTest, DifferentConfig) {
  DnsConfig config1 = MakeConfig(1);
  DnsConfig config2 = MakeConfig(2);
  DnsConfig config3 = MakeConfig(1);
  config1.hosts = MakeHosts(1);
  config2.hosts = MakeHosts(1);
  config3.hosts = MakeHosts(2);
  ASSERT_TRUE(config1.EqualsIgnoreHosts(config3));
  ASSERT_FALSE(config1.Equals(config2));
  ASSERT_FALSE(config1.Equals(config3));
  ASSERT_FALSE(config2.Equals(config3));

  service_->OnConfigRead(config1);
  service_->OnHostsRead(config1.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config1));

  // It doesn't matter for this tests, but increases coverage.
  service_->InvalidateConfig();
  service_->InvalidateHosts();

  service_->OnConfigRead(config2);
  EXPECT_TRUE(last_config_.Equals(config1)) << "Unexpected change";
  service_->OnHostsRead(config2.hosts);  // Not an actual change.
  EXPECT_FALSE(last_config_.Equals(config1));
  EXPECT_TRUE(last_config_.Equals(config2));

  service_->OnConfigRead(config3);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config3));
  service_->OnHostsRead(config3.hosts);
  EXPECT_FALSE(last_config_.Equals(config2));
  EXPECT_TRUE(last_config_.Equals(config3));
}

TEST_F(DnsConfigServiceTest, WatchFailure) {
  DnsConfig config1 = MakeConfig(1);
  DnsConfig config2 = MakeConfig(2);
  config1.hosts = MakeHosts(1);
  config2.hosts = MakeHosts(2);

  service_->OnConfigRead(config1);
  service_->OnHostsRead(config1.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config1));

  // Simulate watch failure.
  service_->set_watch_failed_for_testing(true);
  service_->InvalidateConfig();
  WaitForInvalidationTimeout();
  EXPECT_FALSE(last_config_.Equals(config1));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  DnsConfig bad_config = last_config_ = MakeConfig(0xBAD);
  // Actual change in config, so expect an update, but it should be empty.
  service_->OnConfigRead(config1);
  EXPECT_FALSE(last_config_.Equals(bad_config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  last_config_ = bad_config;
  // Actual change in config, so expect an update, but it should be empty.
  service_->InvalidateConfig();
  service_->OnConfigRead(config2);
  EXPECT_FALSE(last_config_.Equals(bad_config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  last_config_ = bad_config;
  // No change, so no update.
  service_->InvalidateConfig();
  service_->OnConfigRead(config2);
  EXPECT_TRUE(last_config_.Equals(bad_config));
}

TEST_F(DnsConfigServiceTest, HostsReadFailure) {
  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(DnsHosts()), Return(false)));

  auto service =
      std::make_unique<HostsReadingTestDnsConfigService>(parser.GetFactory());
  SetUpService(*service);

  service->OnConfigRead(MakeConfig(1));
  // No successfully read hosts, so no config result.
  EXPECT_EQ(last_config_, DnsConfig());

  // No change from retriggering read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  ValidateNoNotification();
  EXPECT_EQ(last_config_, DnsConfig());
}

TEST_F(DnsConfigServiceTest, ReadEmptyHosts) {
  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(DnsHosts()), Return(true)));

  auto service =
      std::make_unique<HostsReadingTestDnsConfigService>(parser.GetFactory());
  SetUpService(*service);

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, DnsHosts());

  // No change from retriggering read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  ValidateNoNotification();
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, DnsHosts());
}

TEST_F(DnsConfigServiceTest, ReadSingleHosts) {
  DnsHosts hosts = {
      CreateHostsEntry("name", ADDRESS_FAMILY_IPV4, {IPAddress(1, 2, 3, 4)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(hosts), Return(true)));

  auto service =
      std::make_unique<HostsReadingTestDnsConfigService>(parser.GetFactory());
  SetUpService(*service);

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, hosts);

  // No change from retriggering read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  ValidateNoNotification();
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, hosts);
}

TEST_F(DnsConfigServiceTest, ReadMultipleHosts) {
  DnsHosts hosts = {
      CreateHostsEntry("name1", ADDRESS_FAMILY_IPV4, {IPAddress(1, 2, 3, 4)}),
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(1, 2, 3, 5)}),
      CreateHostsEntry(
          "name1", ADDRESS_FAMILY_IPV6,
          {IPAddress(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(hosts), Return(true)));

  auto service =
      std::make_unique<HostsReadingTestDnsConfigService>(parser.GetFactory());
  SetUpService(*service);

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, hosts);

  // No change from retriggering read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  ValidateNoNotification();
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, hosts);
}

TEST_F(DnsConfigServiceTest, HostsReadSubsequentFailure) {
  DnsHosts hosts = {
      CreateHostsEntry("name", ADDRESS_FAMILY_IPV4, {IPAddress(1, 2, 3, 4)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillOnce(DoAll(SetArgPointee<0>(hosts), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(DnsHosts()), Return(false)));

  auto service =
      std::make_unique<HostsReadingTestDnsConfigService>(parser.GetFactory());
  SetUpService(*service);

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, hosts);

  // Config cleared after subsequent read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  WaitForInvalidationTimeout();
  EXPECT_EQ(last_config_, DnsConfig());
}

TEST_F(DnsConfigServiceTest, HostsReadSubsequentSuccess) {
  DnsHosts hosts = {
      CreateHostsEntry("name", ADDRESS_FAMILY_IPV4, {IPAddress(1, 2, 3, 4)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillOnce(DoAll(SetArgPointee<0>(DnsHosts()), Return(false)))
      .WillOnce(DoAll(SetArgPointee<0>(hosts), Return(true)));

  auto service =
      std::make_unique<HostsReadingTestDnsConfigService>(parser.GetFactory());
  SetUpService(*service);

  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);
  // No successfully read hosts, so no config result.
  EXPECT_EQ(last_config_, DnsConfig());

  // Expect success after subsequent read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  WaitForConfig();
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, hosts);
}

TEST_F(DnsConfigServiceTest, ConfigReadDuringHostsReRead) {
  DnsHosts hosts = {
      CreateHostsEntry("name", ADDRESS_FAMILY_IPV4, {IPAddress(1, 2, 3, 4)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(hosts), Return(true)));

  auto service =
      std::make_unique<HostsReadingTestDnsConfigService>(parser.GetFactory());
  SetUpService(*service);

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  DnsConfig config1 = MakeConfig(1);
  service->OnConfigRead(config1);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config1));
  EXPECT_EQ(last_config_.hosts, hosts);

  // Trigger HOSTS read, and expect no new-config notification yet.
  service->TriggerHostsChangeNotification(/*success=*/true);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config1));
  EXPECT_EQ(last_config_.hosts, hosts);

  // Simulate completion of a Config read. Expect no new-config notification
  // while HOSTS read still in progress.
  DnsConfig config2 = MakeConfig(2);
  service->OnConfigRead(config2);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config1));
  EXPECT_EQ(last_config_.hosts, hosts);

  // Expect new config on completion of HOSTS read.
  WaitForConfig();
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config2));
  EXPECT_EQ(last_config_.hosts, hosts);
}

TEST_F(DnsConfigServiceTest, HostsWatcherFailure) {
  DnsHosts hosts = {
      CreateHostsEntry("name", ADDRESS_FAMILY_IPV4, {IPAddress(1, 2, 3, 4)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillOnce(DoAll(SetArgPointee<0>(hosts), Return(true)));

  auto service =
      std::make_unique<HostsReadingTestDnsConfigService>(parser.GetFactory());
  SetUpService(*service);

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, hosts);

  // Simulate watcher failure.
  service->TriggerHostsChangeNotification(/*success=*/false);
  WaitForInvalidationTimeout();
  EXPECT_EQ(last_config_, DnsConfig());
}

}  // namespace net
