// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/logging.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/network_service_test_helper.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_config_service_android.h"
#include "net/dns/system_dns_config_change_notifier.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/network/public/mojom/system_dns_config_observer.mojom.h"

namespace content {
namespace {

class MockSystemDnsConfigObserver
    : public network::mojom::SystemDnsConfigObserver {
 public:
  ~MockSystemDnsConfigObserver() override = default;

  net::DnsConfig WaitForDnsConfig(
      const std::string& wait_dns_over_tls_hostname) {
    wait_dns_over_tls_hostname_ = wait_dns_over_tls_hostname;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    EXPECT_TRUE(run_loop_->AnyQuitCalled()) << "Timed out waiting DnsConfig."
                                               "wait_dns_over_tls_hostname="
                                            << wait_dns_over_tls_hostname
                                            << ", the last "
                                               "config instead is "
                                            << dns_config_.ToDict();
    return dns_config_;
  }

 private:
  void OnConfigChanged(const net::DnsConfig& config) override {
    dns_config_ = config;
    if (config.dns_over_tls_hostname == wait_dns_over_tls_hostname_) {
      run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::string wait_dns_over_tls_hostname_;
  net::DnsConfig dns_config_;
};

class DnsConfigAndroidInProcessBrowserTest : public ContentBrowserTest {
 public:
  DnsConfigAndroidInProcessBrowserTest() { ForceInProcessNetworkService(); }
};

IN_PROC_BROWSER_TEST_F(DnsConfigAndroidInProcessBrowserTest,
                       DnsConfigListenAllowed) {
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;

  auto helper = NetworkServiceTestHelper::CreateInProcessReceiver(
      network_service_test().BindNewPipeAndPassReceiver());

  MockSystemDnsConfigObserver mock_observer;
  mojo::Receiver<network::mojom::SystemDnsConfigObserver> receiver(
      &mock_observer);
  network_service_test()->AddSystemDnsConfigObserver(
      receiver.BindNewPipeAndPassRemote());

  net::DnsConfig expect_config;
  expect_config.nameservers = {net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 80)};
  expect_config.dns_over_tls_active = true;
  expect_config.dns_over_tls_hostname = "https://example.com/";
  expect_config.search = {"foo"};
  auto* system_dns_config_notifier =
      net::NetworkChangeNotifier::GetSystemDnsConfigNotifier();
  CHECK(system_dns_config_notifier);
  system_dns_config_notifier->OnConfigChangedForTesting(expect_config);

  // `mock_observer` should observe DNS config change over mojo
  const net::DnsConfig return_config =
      mock_observer.WaitForDnsConfig(expect_config.dns_over_tls_hostname);
  EXPECT_EQ(expect_config.nameservers, return_config.nameservers);
  EXPECT_EQ(expect_config.dns_over_tls_active,
            return_config.dns_over_tls_active);
  EXPECT_EQ(expect_config.dns_over_tls_hostname,
            return_config.dns_over_tls_hostname);
  EXPECT_EQ(expect_config.search, return_config.search);
}

// TODO(yoichio): Add DnsConfigAndroidOutOfProcessSandboxedBrowserTest.

}  // namespace
}  // namespace content
