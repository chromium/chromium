// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_child_process_backgrounded_bridge.h"

#include "base/mac/mac_util.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "gpu/config/gpu_finch_features.h"

namespace content {

base::Process::Priority GetProcessPriority(base::ProcessId pid) {
  base::Process process = base::Process::Open(pid);
  if (process.is_current()) {
    base::SelfPortProvider self_port_provider;
    return process.GetPriority(&self_port_provider);
  }

  return process.GetPriority(
      content::BrowserChildProcessHost::GetPortProvider());
}

void SetProcessPriority(base::ProcessId pid, base::Process::Priority priority) {
  base::Process process = base::Process::Open(pid);
  if (process.is_current()) {
    base::SelfPortProvider self_port_provider;
    process.SetPriority(&self_port_provider, priority);
    return;
  }

  process.SetPriority(content::BrowserChildProcessHost::GetPortProvider(),
                      priority);
}

class BrowserChildProcessBackgroundedBridgeTest
    : public content::ContentBrowserTest,
      public base::PortProvider::Observer,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAdjustGpuProcessPriority);
    content::BrowserChildProcessBackgroundedBridge::
        SetOSNotificationsEnabledForTesting(false);
    content::ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    content::ContentBrowserTest::TearDown();
    content::BrowserChildProcessBackgroundedBridge::
        SetOSNotificationsEnabledForTesting(true);
  }

  // Waits until the port for the GPU process is available.
  void WaitForPort() {
    auto* port_provider = content::BrowserChildProcessHost::GetPortProvider();
    // Note: On macOS, a process id and a process handle are the same thing.
    DCHECK(port_provider->TaskForHandle(
               content::GpuProcessHost::Get()->process_id()) == MACH_PORT_NULL);
    port_provider->AddObserver(this);
    base::RunLoop run_loop;

    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void EnsureBackgroundedStateChange() {
    // Do a round-trip to the process launcher task to ensure any queued task is
    // run.
    base::RunLoop run_loop;
    GetProcessLauncherTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  void OnReceivedTaskPort(base::ProcessHandle process_handle) override {
    if (process_handle != content::GpuProcessHost::Get()->process_id()) {
      return;
    }

    content::BrowserChildProcessHost::GetPortProvider()->RemoveObserver(this);
    std::move(quit_closure_).Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(BrowserChildProcessBackgroundedBridgeTest,
                       InitiallyForegrounded) {
  if (base::mac::MacOSMajorVersion() >= 13) {
    GTEST_SKIP() << "Flaking on macOS 13: https://crbug.com/1444130";
  }
  // Set the browser process as foregrounded.
  SetProcessPriority(base::Process::Current().Pid(),
                     base::Process::Priority::kUserBlocking);

  // Wait until we receive the port for the GPU process.
  WaitForPort();

  // Ensure that the initial backgrounded state changed.
  EnsureBackgroundedStateChange();

  auto* gpu_process_host = content::GpuProcessHost::Get();
  EXPECT_TRUE(gpu_process_host);
  EXPECT_EQ(GetProcessPriority(gpu_process_host->process_id()),
            base::Process::Priority::kUserBlocking);
}

// TODO(crbug.com/40899195): Disabled because this test is flaky.
IN_PROC_BROWSER_TEST_F(BrowserChildProcessBackgroundedBridgeTest,
                       DISABLED_InitiallyBackgrounded) {
  // Set the browser process as backgrounded.
  SetProcessPriority(base::Process::Current().Pid(),
                     base::Process::Priority::kBestEffort);

  // Wait until we receive the port for the GPU process.
  WaitForPort();

  // Ensure that the initial backgrounded state changed.
  EnsureBackgroundedStateChange();

  auto* gpu_process_host = content::GpuProcessHost::Get();
  EXPECT_TRUE(gpu_process_host);
  EXPECT_EQ(GetProcessPriority(gpu_process_host->process_id()),
            base::Process::Priority::kUserVisible);
}

// Flaky: https://crbug.com/1443367
IN_PROC_BROWSER_TEST_F(BrowserChildProcessBackgroundedBridgeTest,
                       DISABLED_OnBackgroundedStateChanged) {
  // Wait until we receive the port for the GPU process.
  WaitForPort();

  auto* gpu_process_host = content::GpuProcessHost::Get();
  EXPECT_TRUE(gpu_process_host);

  auto* bridge =
      gpu_process_host->browser_child_process_backgrounded_bridge_for_testing();
  ASSERT_TRUE(bridge);

  bridge->SimulateBrowserProcessForegroundedForTesting();
  EnsureBackgroundedStateChange();

  EXPECT_EQ(GetProcessPriority(gpu_process_host->process_id()),
            base::Process::Priority::kUserBlocking);

  bridge->SimulateBrowserProcessBackgroundedForTesting();
  EnsureBackgroundedStateChange();

  EXPECT_EQ(GetProcessPriority(gpu_process_host->process_id()),
            base::Process::Priority::kUserVisible);

  bridge->SimulateBrowserProcessForegroundedForTesting();
  EnsureBackgroundedStateChange();

  EXPECT_EQ(GetProcessPriority(gpu_process_host->process_id()),
            base::Process::Priority::kUserBlocking);
}

}  // namespace content
