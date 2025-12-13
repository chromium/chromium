// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class RenderProcessHostShutdownDelayBrowserTest : public ContentBrowserTest {
 public:
  RenderProcessHostShutdownDelayBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(RenderProcessHostShutdownDelayBrowserTest,
                       DelayAndCancelShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  // Initial state.
  EXPECT_EQ(0u, rph->GetShutdownDelayRefCount());

  // Delay shutdown.
  auto* site_instance = static_cast<SiteInstanceImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  auto site_info = site_instance->GetSiteInfo();
  base::ScopedClosureRunner shutdown_delay_runner =
      rph->DelayProcessShutdown(base::Seconds(30), base::Seconds(0), site_info);
  EXPECT_EQ(rph->GetShutdownDelayRefCount(), 1u);

  // Cancel the shutdown delay.
  shutdown_delay_runner.RunAndReset();
  EXPECT_EQ(rph->GetShutdownDelayRefCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostShutdownDelayBrowserTest,
                       ShutdownDelayExpires) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  EXPECT_EQ(0u, rph->GetShutdownDelayRefCount());

  // Delay shutdown with a short timeout.
  auto* site_instance = static_cast<SiteInstanceImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  auto site_info = site_instance->GetSiteInfo();
  base::ScopedClosureRunner shutdown_delay_runner = rph->DelayProcessShutdown(
      base::Milliseconds(500), base::Milliseconds(0), site_info);
  EXPECT_EQ(1u, rph->GetShutdownDelayRefCount());

  // Wait for the delay to expire.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(1000));
  run_loop.Run();

  // Should be back to 0 after expiration.
  EXPECT_EQ(0u, rph->GetShutdownDelayRefCount());

  // Verify that destroying the runner doesn't decrement again.
  shutdown_delay_runner.RunAndReset();
  EXPECT_EQ(0u, rph->GetShutdownDelayRefCount());
}

IN_PROC_BROWSER_TEST_F(RenderProcessHostShutdownDelayBrowserTest,
                       MultipleDelays) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  auto* site_instance = static_cast<SiteInstanceImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  auto site_info = site_instance->GetSiteInfo();

  EXPECT_EQ(0u, rph->GetShutdownDelayRefCount());

  // Start two delays.
  base::ScopedClosureRunner runner1 =
      rph->DelayProcessShutdown(base::Seconds(30), base::Seconds(0), site_info);
  EXPECT_EQ(1u, rph->GetShutdownDelayRefCount());

  base::ScopedClosureRunner runner2 =
      rph->DelayProcessShutdown(base::Seconds(30), base::Seconds(0), site_info);
  EXPECT_EQ(2u, rph->GetShutdownDelayRefCount());

  // Cancel one.
  runner1.RunAndReset();
  EXPECT_EQ(1u, rph->GetShutdownDelayRefCount());

  // Cancel the other.
  runner2.RunAndReset();
  EXPECT_EQ(0u, rph->GetShutdownDelayRefCount());
}

}  // namespace content
