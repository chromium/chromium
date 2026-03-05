// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/network_service_process_tracker_win.h"

#include <windows.h>

#include "base/location.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Ensure these tests run with the network service out-of-process.
class NetworkServiceProcessTrackerTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndDisableFeature(features::kNetworkServiceInProcess);
    content::ForceOutOfProcessNetworkService();
    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NetworkServiceProcessTrackerTest,
                       GetNetworkServiceProcessHandle) {
  ASSERT_FALSE(IsInProcessNetworkService());

  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  // This ensures network service is fully running.
  network_service_test.FlushForTesting();

  // This scoped base::Process ensures that the refcount to the underlying
  // process handle remains, meaning that the same handle won't ever be reused
  // for future processes.
  base::Process first_process = GetNetworkServiceProcessForTesting();
  ASSERT_NE(first_process.Handle(), base::kNullProcessHandle);

  SimulateNetworkServiceCrash();

  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test2;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test2.BindNewPipeAndPassReceiver());
  // This ensures network service is fully running.
  network_service_test2.FlushForTesting();
  base::Process second_process = GetNetworkServiceProcessForTesting();

  ASSERT_NE(second_process.Handle(), base::kNullProcessHandle);
  ASSERT_NE(first_process.Handle(), second_process.Handle());
}

// This test relies on the fact that the OpenProcess() Windows API call permits
// opening a process that has exited as long as some process has an open handle
// on it, and that base::Process::Open() exposes this behavior. If either of
// those things should change, the test will stop working.
IN_PROC_BROWSER_TEST_F(NetworkServiceProcessTrackerTest,
                       KeepsNetworkServiceProcessHandle) {
  // Keep the network service process handle alive for 1 seconds instead of the
  // default, to make this test run acceptably quickly.
  ScopedKeepOldProcessHandlePeriodForTesting keep_old_process_handle_period(
      base::Seconds(1));

  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  // This ensures network service is fully running.
  network_service_test.FlushForTesting();

  EnsureNetworkServiceListenerStarted();
  base::ProcessId original_process_id =
      GetNetworkServiceProcessForTesting().Pid();

  base::RunLoop run_loop1;
  base::OneShotTimer timer1;
  timer1.Start(FROM_HERE, base::Seconds(0.5), run_loop1.QuitClosure());

  SimulateNetworkServiceCrash();

  // Wait 0.5 seconds.
  run_loop1.Run();

  {
    base::Process original_process = base::Process::Open(original_process_id);
    EXPECT_TRUE(original_process.IsValid());

    // Make sure the process has actually exited.
    original_process.WaitForExit(nullptr);
  }

  // Make sure the NetworkServiceListener has actually picked up the new
  // process and had a chance to start its own timer.
  ASSERT_TRUE(base::test::RunUntil([&] {
    base::Process network_service_process =
        GetNetworkServiceProcessForTesting();
    return network_service_process.IsValid() &&
           network_service_process.Pid() != original_process_id;
  }));

  // Wait another second. The timer set by NetworkServiceListener is definitely
  // running at this point, so is guaranteed to fire before `timer2` by the
  // semantics of base::OneShotTimer.
  base::RunLoop run_loop2;
  base::OneShotTimer timer2;
  timer2.Start(FROM_HERE, base::Seconds(1.0), run_loop2.QuitClosure());
  run_loop2.Run();

  // Verify that we didn't pass the first expectation by accident.
  EXPECT_FALSE(base::Process::Open(original_process_id).IsValid());
}

class NetworkServiceSingleProcessTrackerTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kNetworkServiceInProcess);
    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NetworkServiceSingleProcessTrackerTest, GetProcess) {
  base::Process process = GetNetworkServiceProcessForTesting();
  ASSERT_EQ(process.Pid(), ::GetCurrentProcessId());
}

}  // namespace content
