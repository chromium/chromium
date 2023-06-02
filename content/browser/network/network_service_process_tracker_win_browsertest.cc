// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/network_service_process_tracker_win.h"

#include <windows.h>

#include "base/process/process_handle.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using NetworkServiceProcessTrackerTest = ContentBrowserTest;

IN_PROC_BROWSER_TEST_F(NetworkServiceProcessTrackerTest,
                       GetNetworkServiceProcessHandle) {
  if (IsInProcessNetworkService())
    return;

  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());
  // This ensures network service is fully running.
  network_service_test.FlushForTesting();

  // This scoped base::Process ensures that the refcount to the underlying
  // process handle remains, meaning that the same handle won't ever be reused
  // for future processes.
  base::Process first_process = GetNetworkServiceProcess();
  ASSERT_NE(first_process.Handle(), base::kNullProcessHandle);

  SimulateNetworkServiceCrash();

  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test2;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test2.BindNewPipeAndPassReceiver());
  // This ensures network service is fully running.
  network_service_test2.FlushForTesting();
  base::Process second_process = GetNetworkServiceProcess();

  ASSERT_NE(second_process.Handle(), base::kNullProcessHandle);
  ASSERT_NE(first_process.Handle(), second_process.Handle());
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
  base::Process process = GetNetworkServiceProcess();
  ASSERT_EQ(process.Pid(), ::GetCurrentProcessId());
}

}  // namespace content
