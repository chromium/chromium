// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/resource_broker/resource_broker_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/services/resource_broker/public/mojom/resource_broker.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Returns the `ChildProcessHost` of the broker's utility process, or nullptr if
// the process is not running. `ServiceProcessHost::Launch()` uses the service's
// interface name as the process's `metrics_name`, which is how the broker's
// process is identified among the utility processes.
ChildProcessHost* FindBrokerProcessHost() {
  for (BrowserChildProcessHostIterator it(PROCESS_TYPE_UTILITY); !it.Done();
       ++it) {
    if (it.GetData().metrics_name ==
        resource_broker::mojom::ResourceBrokerService::Name_) {
      return it.GetHost();
    }
  }
  return nullptr;
}

// Asks the broker's utility process to exit via the browser's child process
// machinery. Returns false if the process is not running.
//
// `base::Process::Terminate()` cannot be used for this: on Android, utility
// processes are isolated services running under a different UID, so `kill()`
// from the browser fails with EPERM. Note that `ForceShutdown()` removes the
// host from the list that `BrowserChildProcessHostIterator` walks, so it must
// not be called while iterating.
bool ShutDownBrokerProcess() {
  ChildProcessHost* broker = FindBrokerProcessHost();
  if (!broker) {
    return false;
  }
  broker->ForceShutdown();
  return true;
}

}  // namespace

class ResourceBrokerBrowserTest : public ContentBrowserTest {
 public:
  ResourceBrokerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kResourceBroker);
  }

  void TearDownOnMainThread() override {
    ResourceBrokerHost::ResetForTesting();
    ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The first `GetService()` call launches the broker in a real utility process.
IN_PROC_BROWSER_TEST_F(ResourceBrokerBrowserTest, LaunchAndInitialize) {
  auto& host = ResourceBrokerHost::GetInstance();
  ASSERT_TRUE(host.GetService());

  // A failed launch surfaces as a disconnect during this round-trip, so a
  // still-bound remote afterwards means the utility process started and bound
  // the receiver.
  host.MaybeFlushForTesting();
  EXPECT_TRUE(host.IsServiceRunningForTesting());
  EXPECT_TRUE(FindBrokerProcessHost());
}

// Exercises the host's disconnect handling against a real process exit: the
// host must notice it and refuse to relaunch while the cooldown is active.
IN_PROC_BROWSER_TEST_F(ResourceBrokerBrowserTest, ProcessExitStartsCooldown) {
  auto& host = ResourceBrokerHost::GetInstance();
  ASSERT_TRUE(host.GetService());

  // Wait for the process to be up before asking it to exit.
  host.MaybeFlushForTesting();
  ASSERT_TRUE(ShutDownBrokerProcess()) << "broker utility process not found";

  // The shutdown request travels on the child's control pipe, independently of
  // the service pipe, so a `MaybeFlushForTesting()` round-trip may complete
  // before the process exits. Wait for the host to observe the disconnect
  // instead.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !host.IsServiceRunningForTesting(); }));

  // `GetService()` must not relaunch the service during the cooldown.
  EXPECT_FALSE(host.GetService());
}

}  // namespace content
