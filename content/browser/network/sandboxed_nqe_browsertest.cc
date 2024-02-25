// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/network/network_service_util_internal.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "sandbox/policy/features.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/android/network_library.h"
#endif

namespace content {
namespace {

std::ostream& operator<<(
    std::ostream& os,
    const std::vector<net::EffectiveConnectionType>& types) {
  os << "[";
  bool is_first = true;
  for (auto& type : types) {
    if (is_first)
      is_first = false;
    else
      os << ",";
    os << type;
  }
  return os << "]";
}

class TestNetworkQualityObserver
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  TestNetworkQualityObserver() = default;

  // NetworkQualityTracker::EffectiveConnectionTypeObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    received_types_.push_back(type);
    if (type != run_loop_wait_type_)
      return;
    run_loop_->Quit();
  }

  void WaitForNotification(net::EffectiveConnectionType run_loop_wait_type) {
    if (base::Contains(received_types_, run_loop_wait_type)) {
      received_types_.clear();
      return;
    }
    run_loop_wait_type_ = run_loop_wait_type;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    if (!run_loop_->AnyQuitCalled()) {
      LOG(ERROR) << "Timed out waiting run_loop_wait_type="
                 << run_loop_wait_type
                 << ", received_types_=" << received_types_;
    }
    received_types_.clear();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  net::EffectiveConnectionType run_loop_wait_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  std::vector<net::EffectiveConnectionType> received_types_;
};

class SandboxedNQEBrowserTest : public ContentBrowserTest {
 public:
  SandboxedNQEBrowserTest() {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      // Network Service Sandboxing is unconditionally enabled on these
      // platforms.
    scoped_feature_list_.InitAndEnableFeature(
        sandbox::policy::features::kNetworkServiceSandbox);
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
    ForceOutOfProcessNetworkService();
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_WIN)
    if (!sandbox::policy::features::IsNetworkSandboxSupported()) {
      // On *some* Windows, sandboxing cannot be enabled. We skip all the tests
      // on such platforms.
      GTEST_SKIP();
    }
#endif

    // These assertions need to precede ContentBrowserTest::SetUp to prevent the
    // test body from running when one of the assertions fails.
    ASSERT_TRUE(IsOutOfProcessNetworkService());
    ASSERT_TRUE(sandbox::policy::features::IsNetworkSandboxEnabled());
  }

  // Simulates a network quality change.
  void SimulateNetworkQualityChange(net::EffectiveConnectionType type) {
    DCHECK(content::GetNetworkService());

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    network_service_test->SimulateNetworkQualityChange(type,
                                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  void ForceNetworkQualityEstimatorReportWifiAsSlow2G() {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    DCHECK(content::GetNetworkService());

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    network_service_test->ForceNetworkQualityEstimatorReportWifiAsSlow2G(
        run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// NetworkQualityEstimater used to call android syscall to gather device info
// in the constructor, which has been removed. This test confirms that.
IN_PROC_BROWSER_TEST_F(SandboxedNQEBrowserTest, GetNetworkService) {
  EXPECT_TRUE(GetNetworkService());
}

#if BUILDFLAG(IS_WIN)
#define MAYBE_NetworkQualityTracker DISABLED_NetworkQualityTracker
#else
#define MAYBE_NetworkQualityTracker NetworkQualityTracker
#endif
// Simulate EffectiveConnectionType change in NetworkQualityEstimator and
// reports it to mojo client.
IN_PROC_BROWSER_TEST_F(SandboxedNQEBrowserTest, MAYBE_NetworkQualityTracker) {
  // Change the network quality to UNKNOWN to prevent any spurious
  // notifications.
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

  std::unique_ptr<network::NetworkQualityTracker> tracker =
      std::make_unique<network::NetworkQualityTracker>(
          base::BindRepeating(&GetNetworkService));
  TestNetworkQualityObserver network_quality_observer;
  tracker->AddEffectiveConnectionTypeObserver(&network_quality_observer);

  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_4G);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_4G);

  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_3G);

  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::Milliseconds(450), tracker->GetHttpRTT());
  EXPECT_EQ(base::Milliseconds(400), tracker->GetTransportRTT());
  EXPECT_EQ(400, tracker->GetDownstreamThroughputKbps());
}

#if BUILDFLAG(IS_ANDROID)
// Turn on/off Wifi on Android and listen it in the network service.
IN_PROC_BROWSER_TEST_F(SandboxedNQEBrowserTest, TurnWifiEnabled) {
  const std::string wifi_ssid = net::android::GetWifiSSID();
  if (wifi_ssid.empty()) {
    GTEST_SKIP() << "This test requires wifi network.";
  }
  // Let NetworkQualityEstimator reports NetworkChangeNotifier::CONNECTION_WIFI
  // as EFFECTIVE_CONNECTION_TYPE_SLOW_2G since EffectiveConnectionType and
  // the production receivers doesn't notice Wifi.
  ForceNetworkQualityEstimatorReportWifiAsSlow2G();
  net::NetworkChangeNotifierDelegateAndroid::
      EnableNetworkChangeNotifierAutoDetectForTest();

  std::unique_ptr<network::NetworkQualityTracker> tracker =
      std::make_unique<network::NetworkQualityTracker>(
          base::BindRepeating(&GetNetworkService));
  TestNetworkQualityObserver network_quality_observer;
  tracker->AddEffectiveConnectionTypeObserver(&network_quality_observer);

  net::android::SetWifiEnabledForTesting(true);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  net::android::SetWifiEnabledForTesting(false);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_4G);

  net::android::SetWifiEnabledForTesting(true);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
}
#endif

}  // namespace
}  // namespace content
