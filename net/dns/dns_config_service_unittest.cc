// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/immediate_crash.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/address_sorter.h"
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
using testing::InvokeWithoutArgs;
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
    config.nameservers.push_back(
        IPEndPoint(IPAddress(1, 2, 3, 4), seed & 0xFFFF));
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
    MockHostsParserFactory* factory_;
  };
};

HostsReadingTestDnsConfigService::HostsParserFactory
MockHostsParserFactory::GetFactory() {
  return base::BindLambdaForTesting(
      [this]() -> std::unique_ptr<DnsHostsParser> {
        return std::make_unique<Delegator>(this);
      });
}

DnsHosts::value_type CreateHostsEntry(base::StringPiece name,
                                      AddressFamily family,
                                      std::vector<IPAddress> addresses) {
  DnsHostsKey key = std::make_pair(std::string(name), family);
  return std::make_pair(std::move(key), std::move(addresses));
}

// `AddressSorter` that will always crash if invoked. Useful to validate cases
// where no sort is expected to be needed.
class CrashingAddressSorter : public AddressSorter {
 public:
  void Sort(const AddressList& list, CallbackType callback) const override {
    IMMEDIATE_CRASH();
  }

  static HostsReadingTestDnsConfigService::AddressSorterFactory GetFactory() {
    return base::BindRepeating([]() -> std::unique_ptr<AddressSorter> {
      return std::make_unique<CrashingAddressSorter>();
    });
  }
};

class MockAddressSorterFactory : public AddressSorter {
 public:
  HostsReadingTestDnsConfigService::AddressSorterFactory GetFactory();

  MOCK_METHOD(void,
              Sort,
              (const AddressList&, CallbackType),
              (const, override));

 private:
  class Delegator : public AddressSorter {
   public:
    explicit Delegator(MockAddressSorterFactory* factory) : factory_(factory) {}

    void Sort(const AddressList& list, CallbackType callback) const override {
      return factory_->Sort(list, std::move(callback));
    }

   private:
    MockAddressSorterFactory* factory_;
  };
};

HostsReadingTestDnsConfigService::AddressSorterFactory
MockAddressSorterFactory::GetFactory() {
  return base::BindLambdaForTesting([this]() -> std::unique_ptr<AddressSorter> {
    return std::make_unique<Delegator>(this);
  });
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

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), CrashingAddressSorter::GetFactory());
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

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), CrashingAddressSorter::GetFactory());
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

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), CrashingAddressSorter::GetFactory());
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

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), CrashingAddressSorter::GetFactory());
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

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), CrashingAddressSorter::GetFactory());
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

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), CrashingAddressSorter::GetFactory());
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

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), CrashingAddressSorter::GetFactory());
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

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), CrashingAddressSorter::GetFactory());
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

TEST_F(DnsConfigServiceTest, ReadMultiAddressHosts) {
  DnsHosts hosts = {
      CreateHostsEntry(
          "name1", ADDRESS_FAMILY_IPV4,
          {IPAddress(1, 1, 1, 1), IPAddress(2, 2, 2, 2), IPAddress(3, 3, 3, 3),
           IPAddress(4, 4, 4, 4), IPAddress(5, 5, 5, 5)}),
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(6, 6, 6, 6)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(hosts), Return(true)));

  // AddressSorter that reverses order and then removes element 3 (if 4 or more
  // elements).  Really just an arbitrary operation to prove the sort occurred.
  MockAddressSorterFactory sorter;
  EXPECT_CALL(sorter, Sort(_, _))
      .WillRepeatedly(
          [](const AddressList& list, AddressSorter::CallbackType callback) {
            std::vector<IPEndPoint> reversed = list.endpoints();
            base::ranges::reverse(reversed);

            AddressList sorted;
            for (size_t i = 0; i < reversed.size(); ++i) {
              if (i == 3)
                continue;
              sorted.push_back(reversed[i]);
            }

            std::move(callback).Run(/*success=*/true, sorted);
          });

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), sorter.GetFactory());
  SetUpService(*service);

  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);

  DnsHosts expected_hosts = {
      CreateHostsEntry("name1", ADDRESS_FAMILY_IPV4,
                       {IPAddress(5, 5, 5, 5), IPAddress(4, 4, 4, 4),
                        IPAddress(3, 3, 3, 3), IPAddress(1, 1, 1, 1)}),
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(6, 6, 6, 6)})};

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, expected_hosts);

  // No change from retriggering read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  ValidateNoNotification();
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, expected_hosts);
}

TEST_F(DnsConfigServiceTest, ReadMultipleMultiAddressHosts) {
  DnsHosts hosts = {
      CreateHostsEntry(
          "name1", ADDRESS_FAMILY_IPV4,
          {IPAddress(1, 1, 1, 1), IPAddress(2, 2, 2, 2), IPAddress(3, 3, 3, 3),
           IPAddress(4, 4, 4, 4), IPAddress(5, 5, 5, 5)}),
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(6, 6, 6, 6)}),
      CreateHostsEntry("name3", ADDRESS_FAMILY_IPV4,
                       {IPAddress(7, 7, 7, 7), IPAddress(8, 8, 8, 8)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(hosts), Return(true)));

  // AddressSorter that reverses order and then removes element 3 (if 4 or more
  // elements).  Really just an arbitrary operation to prove the sort occurred.
  MockAddressSorterFactory sorter;
  EXPECT_CALL(sorter, Sort(_, _))
      .WillRepeatedly(
          [](const AddressList& list, AddressSorter::CallbackType callback) {
            std::vector<IPEndPoint> reversed = list.endpoints();
            base::ranges::reverse(reversed);

            AddressList sorted;
            for (size_t i = 0; i < reversed.size(); ++i) {
              if (i == 3)
                continue;
              sorted.push_back(reversed[i]);
            }

            std::move(callback).Run(/*success=*/true, sorted);
          });

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), sorter.GetFactory());
  SetUpService(*service);

  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);

  DnsHosts expected_hosts = {
      CreateHostsEntry("name1", ADDRESS_FAMILY_IPV4,
                       {IPAddress(5, 5, 5, 5), IPAddress(4, 4, 4, 4),
                        IPAddress(3, 3, 3, 3), IPAddress(1, 1, 1, 1)}),
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(6, 6, 6, 6)}),
      CreateHostsEntry("name3", ADDRESS_FAMILY_IPV4,
                       {IPAddress(8, 8, 8, 8), IPAddress(7, 7, 7, 7)})};

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, expected_hosts);

  // No change from retriggering read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  ValidateNoNotification();
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, expected_hosts);
}

TEST_F(DnsConfigServiceTest, FailedSortResultsAreExcluded) {
  DnsHosts hosts = {
      CreateHostsEntry("name1", ADDRESS_FAMILY_IPV4,
                       {IPAddress(1, 1, 1, 1), IPAddress(2, 2, 2, 2)}),
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(3, 3, 3, 3)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(hosts), Return(true)));

  // AddressSorter that always fails.
  MockAddressSorterFactory sorter;
  EXPECT_CALL(sorter, Sort(_, _))
      .WillRepeatedly(
          [](const AddressList& list, AddressSorter::CallbackType callback) {
            std::move(callback).Run(/*success=*/false, AddressList());
          });

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), sorter.GetFactory());
  SetUpService(*service);

  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);

  // Expect only the unsorted single-address entry.
  DnsHosts expected_hosts = {
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(3, 3, 3, 3)})};

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, expected_hosts);

  // No change from retriggering read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  ValidateNoNotification();
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, expected_hosts);
}

TEST_F(DnsConfigServiceTest, HandlesAsyncSort) {
  DnsHosts hosts = {
      CreateHostsEntry("name1", ADDRESS_FAMILY_IPV4,
                       {IPAddress(1, 1, 1, 1), IPAddress(2, 2, 2, 2)}),
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(3, 3, 3, 3)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(hosts), Return(true)));

  // Noop sorter that always succeeds and doesn't change any ordering, but does
  // so asynchronously.
  MockAddressSorterFactory sorter;
  EXPECT_CALL(sorter, Sort(_, _))
      .WillRepeatedly(
          [](const AddressList& list, AddressSorter::CallbackType callback) {
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), /*success=*/true, list));
          });

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), sorter.GetFactory());
  SetUpService(*service);

  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, hosts);

  // No change from retriggering read.
  service->TriggerHostsChangeNotification(/*success=*/true);
  ValidateNoNotification();
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));
  EXPECT_EQ(last_config_.hosts, hosts);
}

TEST_F(DnsConfigServiceTest, DestroyServiceDuringSort) {
  DnsHosts hosts = {
      CreateHostsEntry("name1", ADDRESS_FAMILY_IPV4,
                       {IPAddress(1, 1, 1, 1), IPAddress(2, 2, 2, 2)}),
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(3, 3, 3, 3)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillOnce(DoAll(SetArgPointee<0>(hosts), Return(true)));

  // Sorter that saves out the callback and never invokes it itself.
  MockAddressSorterFactory sorter;
  AddressSorter::CallbackType saved_callback;
  EXPECT_CALL(sorter, Sort(_, _))
      .WillOnce([&saved_callback](const AddressList& list,
                                  AddressSorter::CallbackType callback) {
        saved_callback = std::move(callback);
      });

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(), sorter.GetFactory());
  SetUpService(*service);

  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);

  // Expect hung sorting.
  EXPECT_EQ(last_config_, DnsConfig());
  ASSERT_TRUE(saved_callback);

  // Destroy the service and then resume the sort.
  service.reset();
  std::move(saved_callback).Run(/*success=*/true, AddressList());
  ValidateNoNotification();
  EXPECT_EQ(last_config_, DnsConfig());
}

// Only test actual platform sort on platforms with a sort implementation.
#if !defined(OS_IOS)
TEST_F(DnsConfigServiceTest, ActualPlatformSort) {
  DnsHosts hosts = {
      CreateHostsEntry("name1", ADDRESS_FAMILY_IPV4,
                       {IPAddress(1, 1, 1, 1), IPAddress(2, 2, 2, 2)}),
      CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4, {IPAddress(3, 3, 3, 3)})};

  MockHostsParserFactory parser;
  EXPECT_CALL(parser, ParseHosts(_))
      .WillOnce(DoAll(SetArgPointee<0>(hosts), Return(true)));

  auto service = std::make_unique<HostsReadingTestDnsConfigService>(
      parser.GetFactory(),
      base::BindRepeating(&AddressSorter::CreateAddressSorter));
  SetUpService(*service);

  DnsConfig config = MakeConfig(1);
  service->OnConfigRead(config);

  // Expect immediate result on reading config because HOSTS should already have
  // been read on initting watch in `SetUpService()`.
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));

  // Expect results to at least contain the single-address result.
  EXPECT_LE(last_config_.hosts.size(), 2u);
  EXPECT_THAT(last_config_.hosts,
              testing::Contains(CreateHostsEntry("name2", ADDRESS_FAMILY_IPV4,
                                                 {IPAddress(3, 3, 3, 3)})));

  // Expect results to maybe contain the multi-address result with 1 or 2 of the
  // addresses. Specific addresses and order could depend on system
  // configuration.
  auto multi_address_entry =
      last_config_.hosts.find(DnsHostsKey("name1", ADDRESS_FAMILY_IPV4));
  if (multi_address_entry != last_config_.hosts.end()) {
    EXPECT_FALSE(multi_address_entry->second.empty());
    EXPECT_THAT(
        multi_address_entry->second,
        testing::IsSubsetOf({IPAddress(1, 1, 1, 1), IPAddress(2, 2, 2, 2)}));
  }
}
#endif  // !defined(OS_IOS)

}  // namespace net
