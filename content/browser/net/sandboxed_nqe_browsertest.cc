// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "sandbox/features.h"
#include "sandbox/policy/features.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

namespace content {
namespace {

class TestNetworkQualityObserver
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  TestNetworkQualityObserver() = default;

  // NetworkQualityTracker::EffectiveConnectionTypeObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    effective_connection_type_ = type;
    if (effective_connection_type_ != run_loop_wait_effective_connection_type_)
      return;
    run_loop_->Quit();
  }

  void WaitForNotification(
      net::EffectiveConnectionType run_loop_wait_effective_connection_type) {
    if (effective_connection_type_ == run_loop_wait_effective_connection_type)
      return;
    run_loop_wait_effective_connection_type_ =
        run_loop_wait_effective_connection_type;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  net::EffectiveConnectionType EffectiveConnectionType() const {
    return effective_connection_type_;
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  net::EffectiveConnectionType run_loop_wait_effective_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  net::EffectiveConnectionType effective_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
};

class SandboxedNQEBrowserTest : public ContentBrowserTest {
 public:
  SandboxedNQEBrowserTest() {
    std::vector<base::Feature> enabled_features = {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      // Network Service Sandboxing is unconditionally enabled on these
      // platforms.
      sandbox::policy::features::kNetworkServiceSandbox,
#endif
    };
    scoped_feature_list_.InitWithFeatures(
        enabled_features,
        /*disabled_features=*/{features::kNetworkServiceInProcess});
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    if (!sandbox::features::IsAppContainerSandboxSupported()) {
      // On *some* Windows, sandboxing cannot be enabled. We skip all the tests
      // on such platforms.
      GTEST_SKIP();
    }
#endif

    // These assertions need to precede ContentBrowserTest::SetUp to prevent the
    // test body from running when one of the assertions fails.
    ASSERT_TRUE(IsOutOfProcessNetworkService());
    ASSERT_TRUE(sandbox::policy::features::IsNetworkSandboxEnabled());

    ContentBrowserTest::SetUp();
  }

  // Simulates a network quality change.
  void SimulateNetworkQualityChange(net::EffectiveConnectionType type) {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    DCHECK(content::GetNetworkService());

    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterface(
        network_service_test.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    network_service_test->SimulateNetworkQualityChange(type,
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
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_4G,
            network_quality_observer.EffectiveConnectionType());

  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  network_quality_observer.WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            network_quality_observer.EffectiveConnectionType());

  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::Milliseconds(450), tracker->GetHttpRTT());
  EXPECT_EQ(base::Milliseconds(400), tracker->GetTransportRTT());
  EXPECT_EQ(400, tracker->GetDownstreamThroughputKbps());
}

}  // namespace
}  // namespace content
