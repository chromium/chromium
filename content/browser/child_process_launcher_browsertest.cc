// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"

namespace {

class MockChildProcessLauncherClient
    : public content::ChildProcessLauncher::Client {
 public:
  MockChildProcessLauncherClient()
      : client_(nullptr), simulate_failure_(false) {}

  void OnProcessLaunched() override {
    if (simulate_failure_)
      client_->OnProcessLaunchFailed(content::LAUNCH_RESULT_FAILURE);
    else
      client_->OnProcessLaunched();
  }

  void OnProcessLaunchFailed(int error_code) override {
    client_->OnProcessLaunchFailed(error_code);
  }

#if BUILDFLAG(IS_ANDROID)
  bool CanUseWarmUpConnection() override { return true; }
#endif

  raw_ptr<content::ChildProcessLauncher::Client> client_;
  bool simulate_failure_;
};

}  // namespace

namespace content {

class ChildProcessLauncherBrowserTest : public ContentBrowserTest {};

IN_PROC_BROWSER_TEST_F(ChildProcessLauncherBrowserTest, ChildSpawnFail) {
  GURL url("about:blank");
  Shell* window = shell();
  MockChildProcessLauncherClient* client(nullptr);

  // Navigate once and simulate a process failing to spawn.
  TestNavigationObserver nav_observer1(window->web_contents(), 1);
  client = new MockChildProcessLauncherClient;
  window->LoadURL(url);
  client->client_ =
      static_cast<RenderProcessHostImpl*>(
          window->web_contents()->GetPrimaryMainFrame()->GetProcess())
          ->child_process_launcher_->ReplaceClientForTest(client);
  client->simulate_failure_ = true;
  {
    ScopedAllowRendererCrashes allow_renderer_crashes(shell());
    nav_observer1.Wait();
  }
  delete client;
  NavigationEntry* last_entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  // Make sure we didn't commit any navigation.
  EXPECT_TRUE(last_entry->IsInitialEntry());

  // Navigate again and let the process spawn correctly.
  TestNavigationObserver nav_observer2(window->web_contents(), 1);
  window->LoadURL(url);
  nav_observer2.Wait();
  last_entry = shell()->web_contents()->GetController().GetLastCommittedEntry();
  // Make sure that we navigated to the proper URL.
  ASSERT_FALSE(last_entry->IsInitialEntry());
  EXPECT_EQ(last_entry->GetPageType(), PAGE_TYPE_NORMAL);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url);

  // Navigate again, using the same renderer.
  url = GURL("data:text/html,dataurl");
  TestNavigationObserver nav_observer3(window->web_contents(), 1);
  window->LoadURL(url);
  nav_observer3.Wait();
  last_entry = shell()->web_contents()->GetController().GetLastCommittedEntry();
  // Make sure that we navigated to the proper URL.
  ASSERT_FALSE(last_entry->IsInitialEntry());
  EXPECT_EQ(last_entry->GetPageType(), PAGE_TYPE_NORMAL);
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), url);
}

}  // namespace content
