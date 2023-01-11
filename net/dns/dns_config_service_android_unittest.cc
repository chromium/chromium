// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_android.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/build_info.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "net/android/network_library.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_config.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::internal {
namespace {

const IPEndPoint kNameserver1(IPAddress(1, 2, 3, 4), 53);
const IPEndPoint kNameserver2(IPAddress(1, 2, 3, 8), 53);

// DnsConfigServiceAndroid uses a simplified implementation for Android versions
// before relevant APIs were added in Android M. Most of these tests are
// targeting the logic used in M and beyond.
#define SKIP_ANDROID_VERSIONS_BEFORE_M()                              \
  {                                                                   \
    if (base::android::BuildInfo::GetInstance()->sdk_int() <          \
        base::android::SDK_VERSION_MARSHMALLOW) {                     \
      GTEST_SKIP() << "Test not necessary or compatible with pre-M."; \
    }                                                                 \
  }

// RefCountedThreadSafe to allow safe usage and reference storage in
// DnsConfigServiceAndroid's off-sequence utility classes.
class MockDnsServerGetter
    : public base::RefCountedThreadSafe<MockDnsServerGetter> {
 public:
  void set_retval(bool retval) { retval_ = retval; }

  void set_dns_servers(std::vector<IPEndPoint> dns_servers) {
    dns_servers_ = std::move(dns_servers);
  }

  void set_dns_over_tls_active(bool dns_over_tls_active) {
    dns_over_tls_active_ = dns_over_tls_active;
  }

  void set_dns_over_tls_hostname(std::string dns_over_tls_hostname) {
    dns_over_tls_hostname_ = std::move(dns_over_tls_hostname);
  }

  void set_search_suffixes(std::vector<std::string> search_suffixes) {
    search_suffixes_ = std::move(search_suffixes);
  }

  android::DnsServerGetter ConstructGetter() {
    return base::BindRepeating(&MockDnsServerGetter::GetDnsServers, this);
  }

 private:
  friend base::RefCountedThreadSafe<MockDnsServerGetter>;
  ~MockDnsServerGetter() = default;

  bool GetDnsServers(std::vector<IPEndPoint>* dns_servers,
                     bool* dns_over_tls_active,
                     std::string* dns_over_tls_hostname,
                     std::vector<std::string>* search_suffixes) {
    if (retval_) {
      *dns_servers = dns_servers_;
      *dns_over_tls_active = dns_over_tls_active_;
      *dns_over_tls_hostname = dns_over_tls_hostname_;
      *search_suffixes = search_suffixes_;
    }
    return retval_;
  }

  bool retval_ = false;
  std::vector<IPEndPoint> dns_servers_;
  bool dns_over_tls_active_ = false;
  std::string dns_over_tls_hostname_;
  std::vector<std::string> search_suffixes_;
};

class DnsConfigServiceAndroidTest : public testing::Test,
                                    public WithTaskEnvironment {
 public:
  DnsConfigServiceAndroidTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    service_->set_dns_server_getter_for_testing(
        mock_dns_server_getter_->ConstructGetter());
  }
  ~DnsConfigServiceAndroidTest() override = default;

  void OnConfigChanged(const DnsConfig& config) {
    EXPECT_TRUE(config.IsValid());
    seen_config_ = true;
    real_config_ = config;
  }

 protected:
  bool seen_config_ = false;
  std::unique_ptr<DnsConfigServiceAndroid> service_ =
      std::make_unique<DnsConfigServiceAndroid>();
  DnsConfig real_config_;

  scoped_refptr<MockDnsServerGetter> mock_dns_server_getter_ =
      base::MakeRefCounted<MockDnsServerGetter>();
  test::ScopedMockNetworkChangeNotifier mock_notifier_;
};

TEST_F(DnsConfigServiceAndroidTest, HandlesNetworkChangeNotifications) {
  service_->WatchConfig(base::BindRepeating(
      &DnsConfigServiceAndroidTest::OnConfigChanged, base::Unretained(this)));
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();

  // Cannot validate any behavior other than not crashing because this test runs
  // on Android versions with unmocked behavior.
}

TEST_F(DnsConfigServiceAndroidTest, NewConfigReadOnNetworkChange) {
  SKIP_ANDROID_VERSIONS_BEFORE_M();

  mock_dns_server_getter_->set_retval(true);
  mock_dns_server_getter_->set_dns_servers({kNameserver1});

  service_->WatchConfig(base::BindRepeating(
      &DnsConfigServiceAndroidTest::OnConfigChanged, base::Unretained(this)));
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();
  ASSERT_TRUE(seen_config_);
  EXPECT_THAT(real_config_.nameservers, testing::ElementsAre(kNameserver1));

  mock_dns_server_getter_->set_dns_servers({kNameserver2});

  seen_config_ = false;
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
      NetworkChangeNotifier::CONNECTION_WIFI);
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();
  ASSERT_TRUE(seen_config_);
  EXPECT_THAT(real_config_.nameservers, testing::ElementsAre(kNameserver2));
}

TEST_F(DnsConfigServiceAndroidTest, NoConfigNotificationWhenUnchanged) {
  SKIP_ANDROID_VERSIONS_BEFORE_M();

  mock_dns_server_getter_->set_retval(true);
  mock_dns_server_getter_->set_dns_servers({kNameserver1});

  service_->WatchConfig(base::BindRepeating(
      &DnsConfigServiceAndroidTest::OnConfigChanged, base::Unretained(this)));
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();
  ASSERT_TRUE(seen_config_);
  EXPECT_THAT(real_config_.nameservers, testing::ElementsAre(kNameserver1));

  seen_config_ = false;
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
      NetworkChangeNotifier::CONNECTION_WIFI);
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();

  // Because the DNS config hasn't changed, no new config should be seen.
  EXPECT_FALSE(seen_config_);
}

TEST_F(DnsConfigServiceAndroidTest, IgnoresConnectionNoneChangeNotifications) {
  SKIP_ANDROID_VERSIONS_BEFORE_M();

  mock_dns_server_getter_->set_retval(true);
  mock_dns_server_getter_->set_dns_servers({kNameserver1});

  service_->WatchConfig(base::BindRepeating(
      &DnsConfigServiceAndroidTest::OnConfigChanged, base::Unretained(this)));
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();
  ASSERT_TRUE(seen_config_);
  EXPECT_THAT(real_config_.nameservers, testing::ElementsAre(kNameserver1));

  // Change the DNS config to ensure the lack of notification is due to not
  // being checked for.
  mock_dns_server_getter_->set_dns_servers({kNameserver2});

  seen_config_ = false;
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
      NetworkChangeNotifier::CONNECTION_NONE);
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();

  // Expect no new config read for network change to NONE.
  EXPECT_FALSE(seen_config_);
}

// Regression test for https://crbug.com/704662.
TEST_F(DnsConfigServiceAndroidTest, ChangeConfigMultipleTimes) {
  SKIP_ANDROID_VERSIONS_BEFORE_M();

  mock_dns_server_getter_->set_retval(true);
  mock_dns_server_getter_->set_dns_servers({kNameserver1});

  service_->WatchConfig(base::BindRepeating(
      &DnsConfigServiceAndroidTest::OnConfigChanged, base::Unretained(this)));
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();
  ASSERT_TRUE(seen_config_);
  EXPECT_THAT(real_config_.nameservers, testing::ElementsAre(kNameserver1));

  for (int i = 0; i < 5; i++) {
    mock_dns_server_getter_->set_dns_servers({kNameserver2});

    seen_config_ = false;
    NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        NetworkChangeNotifier::CONNECTION_WIFI);
    FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
    RunUntilIdle();
    ASSERT_TRUE(seen_config_);
    EXPECT_THAT(real_config_.nameservers, testing::ElementsAre(kNameserver2));

    mock_dns_server_getter_->set_dns_servers({kNameserver1});

    seen_config_ = false;
    NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        NetworkChangeNotifier::CONNECTION_WIFI);
    FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
    RunUntilIdle();
    ASSERT_TRUE(seen_config_);
    EXPECT_THAT(real_config_.nameservers, testing::ElementsAre(kNameserver1));
  }
}

TEST_F(DnsConfigServiceAndroidTest, ReadsSearchSuffixes) {
  SKIP_ANDROID_VERSIONS_BEFORE_M();

  const std::vector<std::string> kSuffixes{"name1.test", "name2.test"};

  mock_dns_server_getter_->set_retval(true);
  mock_dns_server_getter_->set_dns_servers({kNameserver1});
  mock_dns_server_getter_->set_search_suffixes(kSuffixes);

  service_->ReadConfig(base::BindRepeating(
      &DnsConfigServiceAndroidTest::OnConfigChanged, base::Unretained(this)));
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();
  ASSERT_TRUE(seen_config_);
  EXPECT_EQ(real_config_.search, kSuffixes);
}

TEST_F(DnsConfigServiceAndroidTest, ReadsEmptySearchSuffixes) {
  SKIP_ANDROID_VERSIONS_BEFORE_M();

  mock_dns_server_getter_->set_retval(true);
  mock_dns_server_getter_->set_dns_servers({kNameserver1});

  service_->ReadConfig(base::BindRepeating(
      &DnsConfigServiceAndroidTest::OnConfigChanged, base::Unretained(this)));
  FastForwardBy(DnsConfigServiceAndroid::kConfigChangeDelay);
  RunUntilIdle();
  ASSERT_TRUE(seen_config_);
  EXPECT_TRUE(real_config_.search.empty());
}

}  // namespace
}  // namespace net::internal
