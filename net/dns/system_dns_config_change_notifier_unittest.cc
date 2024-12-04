// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/system_dns_config_change_notifier.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/test_dns_config_service.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
const std::vector<IPEndPoint> kNameservers = {
    IPEndPoint(IPAddress(1, 2, 3, 4), 95)};
const std::vector<IPEndPoint> kNameservers2 = {
    IPEndPoint(IPAddress(2, 3, 4, 5), 195)};
const DnsConfig kConfig(kNameservers);
const DnsConfig kConfig2(kNameservers2);
}  // namespace

class SystemDnsConfigChangeNotifierTest : public TestWithTaskEnvironment {
 public:
  // Set up a change notifier, owned on a dedicated blockable task runner, with
  // a faked underlying DnsConfigService.
  SystemDnsConfigChangeNotifierTest()
      : notifier_task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
    auto test_service = std::make_unique<TestDnsConfigService>();
    notifier_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TestDnsConfigService::OnHostsRead,
                       base::Unretained(test_service.get()), DnsHosts()));
    test_config_service_ = test_service.get();

    notifier_ = std::make_unique<SystemDnsConfigChangeNotifier>(
        notifier_task_runner_, std::move(test_service));
  }

 protected:
  // Test observer implementation that records all notifications received in a
  // vector, and also validates that all notifications are received on the
  // expected sequence.
  class TestObserver : public SystemDnsConfigChangeNotifier::Observer {
   public:
    void OnSystemDnsConfigChanged(std::optional<DnsConfig> config) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      configs_received_.push_back(std::move(config));

      DCHECK_GT(notifications_remaining_, 0);
      if (--notifications_remaining_ == 0)
        run_loop_->Quit();
    }

    void WaitForNotification() { WaitForNotifications(1); }
    void WaitForNotifications(int num_notifications) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

      notifications_remaining_ = num_notifications;
      run_loop_->Run();
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    void ExpectNoMoreNotifications() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      configs_received_.clear();
      base::RunLoop().RunUntilIdle();
      EXPECT_TRUE(configs_received_.empty());
    }

    std::vector<std::optional<DnsConfig>>& configs_received() {
      return configs_received_;
    }

   private:
    int notifications_remaining_ = 0;
    std::unique_ptr<base::RunLoop> run_loop_ =
        std::make_unique<base::RunLoop>();
    std::vector<std::optional<DnsConfig>> configs_received_;
    SEQUENCE_CHECKER(sequence_checker_);
  };

  // Load a config and wait for it to be received by the notifier.
  void LoadConfig(const DnsConfig& config, bool already_loaded = false) {
    TestObserver observer;
    notifier_->AddObserver(&observer);

    // If |notifier_| already has a config loaded, |observer| will first get a
    // notification for that initial config.
    if (already_loaded)
      observer.WaitForNotification();

    notifier_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TestDnsConfigService::OnConfigRead,
                       base::Unretained(test_config_service_), config));
    observer.WaitForNotification();

    notifier_->RemoveObserver(&observer);
  }

  scoped_refptr<base::SequencedTaskRunner> notifier_task_runner_;
  std::unique_ptr<SystemDnsConfigChangeNotifier> notifier_;
  // Owned by |notifier_|.
  raw_ptr<TestDnsConfigService> test_config_service_;
};

TEST_F(SystemDnsConfigChangeNotifierTest, ReceiveNotification) {
  TestObserver observer;

  notifier_->AddObserver(&observer);
  notifier_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDnsConfigService::OnConfigRead,
                     base::Unretained(test_config_service_), kConfig));
  observer.WaitForNotification();

  EXPECT_THAT(observer.configs_received(),
              testing::ElementsAre(testing::Optional(kConfig)));
  observer.ExpectNoMoreNotifications();

  notifier_->RemoveObserver(&observer);
}

TEST_F(SystemDnsConfigChangeNotifierTest, ReceiveNotification_Multiple) {
  TestObserver observer;

  notifier_->AddObserver(&observer);
  notifier_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDnsConfigService::OnConfigRead,
                     base::Unretained(test_config_service_), kConfig));
  notifier_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDnsConfigService::OnConfigRead,
                     base::Unretained(test_config_service_), kConfig2));
  observer.WaitForNotifications(2);

  EXPECT_THAT(observer.configs_received(),
              testing::ElementsAre(testing::Optional(kConfig),
                                   testing::Optional(kConfig2)));
  observer.ExpectNoMoreNotifications();

  notifier_->RemoveObserver(&observer);
}

// If the notifier already has a config loaded, a new observer should receive an
// initial notification for that config.
TEST_F(SystemDnsConfigChangeNotifierTest, ReceiveInitialNotification) {
  LoadConfig(kConfig);

  TestObserver observer;
  notifier_->AddObserver(&observer);
  observer.WaitForNotification();

  EXPECT_THAT(observer.configs_received(),
              testing::ElementsAre(testing::Optional(kConfig)));
  observer.ExpectNoMoreNotifications();

  notifier_->RemoveObserver(&observer);
}

// If multiple configs have been read before adding an Observer, should notify
// it only of the most recent.
TEST_F(SystemDnsConfigChangeNotifierTest, ReceiveInitialNotification_Multiple) {
  LoadConfig(kConfig);
  LoadConfig(kConfig2, true /* already_loaded */);

  TestObserver observer;
  notifier_->AddObserver(&observer);
  observer.WaitForNotification();

  EXPECT_THAT(observer.configs_received(),
              testing::ElementsAre(testing::Optional(kConfig2)));
  observer.ExpectNoMoreNotifications();

  notifier_->RemoveObserver(&observer);
}

TEST_F(SystemDnsConfigChangeNotifierTest, NotificationsStopAfterRemoval) {
  TestObserver observer;
  notifier_->AddObserver(&observer);
  notifier_->RemoveObserver(&observer);

  LoadConfig(kConfig);
  LoadConfig(kConfig2, true /* already_loaded */);

  EXPECT_TRUE(observer.configs_received().empty());
  observer.ExpectNoMoreNotifications();
}

TEST_F(SystemDnsConfigChangeNotifierTest, UnchangedConfigs) {
  LoadConfig(kConfig);

  TestObserver observer;
  notifier_->AddObserver(&observer);
  observer.WaitForNotification();

  // Expect no notifications from duplicate configs.
  notifier_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDnsConfigService::OnConfigRead,
                     base::Unretained(test_config_service_), kConfig));
  notifier_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDnsConfigService::OnConfigRead,
                     base::Unretained(test_config_service_), kConfig));
  observer.ExpectNoMoreNotifications();

  // Notification on new config.
  notifier_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDnsConfigService::OnConfigRead,
                     base::Unretained(test_config_service_), kConfig2));
  observer.WaitForNotification();
  EXPECT_THAT(observer.configs_received(),
              testing::ElementsAre(testing::Optional(kConfig2)));
  observer.ExpectNoMoreNotifications();

  notifier_->RemoveObserver(&observer);
}

TEST_F(SystemDnsConfigChangeNotifierTest, UnloadedConfig) {
  LoadConfig(kConfig);

  TestObserver observer;
  notifier_->AddObserver(&observer);
  // Initial config.
  observer.WaitForNotification();

  notifier_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestDnsConfigService::InvalidateConfig,
                                base::Unretained(test_config_service_)));
  observer.WaitForNotification();

  EXPECT_THAT(observer.configs_received(),
              testing::ElementsAre(testing::Optional(kConfig), std::nullopt));
  observer.ExpectNoMoreNotifications();

  notifier_->RemoveObserver(&observer);
}

// All invalid configs are considered the same for notifications, so only expect
// a single notification on multiple config invalidations.
TEST_F(SystemDnsConfigChangeNotifierTest, UnloadedConfig_Multiple) {
  LoadConfig(kConfig);

  TestObserver observer;
  notifier_->AddObserver(&observer);
  // Initial config.
  observer.WaitForNotification();

  notifier_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestDnsConfigService::InvalidateConfig,
                                base::Unretained(test_config_service_)));
  notifier_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestDnsConfigService::InvalidateConfig,
                                base::Unretained(test_config_service_)));
  observer.WaitForNotification();  // Only 1 notification expected.

  EXPECT_THAT(observer.configs_received(),
              testing::ElementsAre(testing::Optional(kConfig), std::nullopt));
  observer.ExpectNoMoreNotifications();

  notifier_->RemoveObserver(&observer);
}

TEST_F(SystemDnsConfigChangeNotifierTest, InitialConfigInvalid) {
  // Add and invalidate a config (using an extra observer to wait for
  // invalidation to complete).
  LoadConfig(kConfig);
  TestObserver setup_observer;
  notifier_->AddObserver(&setup_observer);
  setup_observer.WaitForNotification();
  notifier_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TestDnsConfigService::InvalidateConfig,
                                base::Unretained(test_config_service_)));
  setup_observer.WaitForNotification();
  notifier_->RemoveObserver(&setup_observer);

  TestObserver observer;
  notifier_->AddObserver(&observer);

  // No notification expected until first valid config.
  observer.ExpectNoMoreNotifications();

  // Notification on new config.
  notifier_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TestDnsConfigService::OnConfigRead,
                     base::Unretained(test_config_service_), kConfig));
  observer.WaitForNotification();
  EXPECT_THAT(observer.configs_received(),
              testing::ElementsAre(testing::Optional(kConfig)));
  observer.ExpectNoMoreNotifications();

  notifier_->RemoveObserver(&observer);
}

TEST_F(SystemDnsConfigChangeNotifierTest, RefreshConfig) {
  test_config_service_->SetConfigForRefresh(kConfig);

  TestObserver observer;
  notifier_->AddObserver(&observer);

  notifier_->RefreshConfig();
  observer.WaitForNotification();

  EXPECT_THAT(observer.configs_received(),
              testing::ElementsAre(testing::Optional(kConfig)));
  observer.ExpectNoMoreNotifications();

  notifier_->RemoveObserver(&observer);
}

}  // namespace net
