// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include <memory>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/test_dns_config_service.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class DnsConfigServiceTest : public TestWithTaskEnvironment {
 public:
  void OnConfigChanged(const DnsConfig& config) {
    last_config_ = config;
    if (quit_on_config_)
      std::move(quit_on_config_).Run();
  }

 protected:
  void WaitForConfig(base::TimeDelta timeout) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), timeout);
    quit_on_config_ = run_loop.QuitClosure();
    run_loop.Run();
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

  void SetUp() override {
    service_.reset(new TestDnsConfigService());
    service_->WatchConfig(base::Bind(&DnsConfigServiceTest::OnConfigChanged,
                                     base::Unretained(this)));
    EXPECT_FALSE(last_config_.IsValid());
  }

  DnsConfig last_config_;
  base::OnceClosure quit_on_config_;

  // Service under test.
  std::unique_ptr<TestDnsConfigService> service_;
};

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
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.Equals(config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  service_->OnConfigRead(config);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  service_->InvalidateHosts();
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.Equals(config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  DnsConfig bad_config = last_config_ = MakeConfig(0xBAD);
  service_->InvalidateConfig();
  // We don't expect an update. This should time out.
  WaitForConfig(base::TimeDelta::FromMilliseconds(100) +
                TestTimeouts::tiny_timeout());
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
  service_->set_watch_failed(true);
  service_->InvalidateConfig();
  WaitForConfig(TestTimeouts::action_timeout());
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

}  // namespace net
