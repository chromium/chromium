// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"

namespace content {

class ChildProcessLauncherBrowserTest : public ContentBrowserTest {};

IN_PROC_BROWSER_TEST_F(ChildProcessLauncherBrowserTest, ChildSpawnFail) {
  // Ensure the next process launch will fail.
  auto scoped_simulate_launch_failure =
      std::make_unique<ScopedSimulateProcessLaunchFailureForTesting>();

  GURL url("about:blank");
  Shell* window = shell();

  TestNavigationObserver nav_observer1(window->web_contents(), 1);
  window->LoadURL(url);
  {
    ScopedAllowRendererCrashes allow_renderer_crashes(shell());
    nav_observer1.Wait();
  }
  NavigationEntry* last_entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  // Make sure we didn't commit any navigation.
  EXPECT_TRUE(last_entry->IsInitialEntry());

  // Reset the test helper so that future launches will succeed.
  scoped_simulate_launch_failure = nullptr;

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
