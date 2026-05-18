// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class ProgressiveAccessibilityTest : public ContentBrowserTest,
                                     public testing::WithParamInterface<bool> {
 protected:
  ProgressiveAccessibilityTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kProgressiveAccessibilityPhase2);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kProgressiveAccessibilityPhase2);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a WebContents only gets its accessibility mode flags when it is
// revealed.
IN_PROC_BROWSER_TEST_P(ProgressiveAccessibilityTest, AccessibilityOnReveal) {
  // Create a second WebContents and hide it.
  Shell* shell_2 =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size());
  shell_2->web_contents()->WasHidden();

  // Enable accessibility.
  ScopedAccessibilityModeOverride basic(ui::kAXModeBasic);

  // The visible WebContents received it.
  EXPECT_EQ(shell()->web_contents()->GetAccessibilityMode(), ui::kAXModeBasic);

  // The hidden WebContents did not.
  EXPECT_EQ(shell_2->web_contents()->GetAccessibilityMode(), ui::AXMode());

  // But does when shown.
  shell_2->web_contents()->WasShown();
  EXPECT_EQ(shell_2->web_contents()->GetAccessibilityMode(), ui::kAXModeBasic);

  // The original WebContents should be unaffected.
  EXPECT_EQ(shell()->web_contents()->GetAccessibilityMode(), ui::kAXModeBasic);
}

// Tests that accessibility is disabled for a hidden WebContents after five
// others have been hidden and five minutes has passed.
IN_PROC_BROWSER_TEST_P(ProgressiveAccessibilityTest, DisableAfterHide) {
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_runner;

  // Enable accessibility.
  ScopedAccessibilityModeOverride basic(ui::kAXModeBasic);

  // The initial WebContents has received the new mode.
  EXPECT_EQ(shell()->web_contents()->GetAccessibilityMode(), ui::kAXModeBasic);

  // Create five WebContentses (in addition to the initial one).
  std::array<Shell*, BrowserAccessibilityStateImpl::kMaxPreservedWebContents>
      shells;
  std::ranges::generate(
      shells,
      [browser_context = shell()->web_contents()->GetBrowserContext()]() {
        auto* shell = Shell::CreateNewWindow(browser_context, GURL(), nullptr,
                                             gfx::Size());
        // Each new shell has accessibility enabled at creation.
        EXPECT_EQ(shell->web_contents()->GetAccessibilityMode(),
                  ui::kAXModeBasic);
        return shell;
      });

  // Hide all six, starting with the initial one.
  shell()->web_contents()->WasHidden();
  std::ranges::for_each(
      shells, [](Shell* shell) { shell->web_contents()->WasHidden(); });

  // Let time pass.
  mock_time_runner->FastForwardBy(
      BrowserAccessibilityStateImpl::GetMaxDisableDelay());

  // The last five all still have accessibility enabled.
  std::ranges::for_each(shells, [](Shell* shell) {
    EXPECT_EQ(shell->web_contents()->GetAccessibilityMode(), ui::kAXModeBasic);
  });

  // The initial WebContents does not if kProgressiveAccessibilityPhase2 is on.
  if (!GetParam()) {
    EXPECT_EQ(shell()->web_contents()->GetAccessibilityMode(),
              ui::kAXModeBasic);
  } else {
    EXPECT_EQ(shell()->web_contents()->GetAccessibilityMode(), ui::AXMode());
  }
}

// Tests that nothing bad happens if a WebContents is destroyed after being
// hidden.
IN_PROC_BROWSER_TEST_P(ProgressiveAccessibilityTest, DestroyAfterHide) {
  // Enable accessibility.
  ScopedAccessibilityModeOverride basic(ui::kAXModeBasic);

  // Hide the WebContents
  shell()->web_contents()->WasHidden();

  // Now destroy it.
  shell()->Close();
}

// Tests that accessibility is not disabled when a screen reader is active.
IN_PROC_BROWSER_TEST_P(ProgressiveAccessibilityTest,
                       NoDisableWithScreenReader) {
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_runner;

  // Enable accessibility via a screen reader.
  const ui::AXMode mode = ui::kAXModeBasic | ui::AXMode::kScreenReader;
  ScopedAccessibilityModeOverride basic(mode);

  // Create five WebContentses (in addition to the initial one).
  std::array<Shell*, BrowserAccessibilityStateImpl::kMaxPreservedWebContents>
      shells;
  std::ranges::generate(
      shells,
      [browser_context = shell()->web_contents()->GetBrowserContext()]() {
        return Shell::CreateNewWindow(browser_context, GURL(), nullptr,
                                      gfx::Size());
      });

  // Hide all six, starting with the initial one.
  shell()->web_contents()->WasHidden();
  std::ranges::for_each(
      shells, [](Shell* shell) { shell->web_contents()->WasHidden(); });

  // Let time pass.
  mock_time_runner->FastForwardBy(
      BrowserAccessibilityStateImpl::GetMaxDisableDelay());

  // Accessibility should still be enabled for the first WebContents.
  EXPECT_EQ(shell()->web_contents()->GetAccessibilityMode(), mode);
}

// Tests that accessibility is not disabled when a screen reader is detected
// while there are hidden WebContents.
IN_PROC_BROWSER_TEST_P(ProgressiveAccessibilityTest,
                       NoDisableWithScreenReaderLater) {
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_runner;

  // Enable accessibility.
  ScopedAccessibilityModeOverride basic(ui::kAXModeBasic);

  // Create five WebContentses (in addition to the initial one).
  std::array<Shell*, BrowserAccessibilityStateImpl::kMaxPreservedWebContents>
      shells;
  std::ranges::generate(
      shells,
      [browser_context = shell()->web_contents()->GetBrowserContext()]() {
        return Shell::CreateNewWindow(browser_context, GURL(), nullptr,
                                      gfx::Size());
      });

  // Hide all six, starting with the initial one.
  shell()->web_contents()->WasHidden();
  std::ranges::for_each(
      shells, [](Shell* shell) { shell->web_contents()->WasHidden(); });

  // Now a screen reader is detected.
  ScopedAccessibilityModeOverride screen_reader(ui::AXMode::kScreenReader);

  // Let time pass.
  mock_time_runner->FastForwardBy(
      BrowserAccessibilityStateImpl::GetMaxDisableDelay());

  // The last five all still have basic accessibility enabled. (They don't
  // know about the screen reader yet since they're hidden).
  std::ranges::for_each(shells, [](Shell* shell) {
    EXPECT_EQ(shell->web_contents()->GetAccessibilityMode(), ui::kAXModeBasic);
  });

  // As should the initial WebContents.
  EXPECT_EQ(shell()->web_contents()->GetAccessibilityMode(), ui::kAXModeBasic);
}

// Tests that accessibility is disabled for a hidden WebContents even if its
// renderer process has crashed, and that the BrowserAccessibilityManager is
// correctly destroyed.
IN_PROC_BROWSER_TEST_P(ProgressiveAccessibilityTest,
                       DisableAfterHideWithCrash) {
  if (!GetParam()) {
    GTEST_SKIP();
  }

  // Navigate to a simple page first to ensure process is ready.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(),
                            embedded_test_server()->GetURL("/title1.html")));

  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(rfhi->GetProcess()->IsReady());

  // Create five WebContentses (in addition to the initial one).
  std::array<Shell*, BrowserAccessibilityStateImpl::kMaxPreservedWebContents>
      shells;
  std::ranges::generate(
      shells,
      [browser_context = shell()->web_contents()->GetBrowserContext()]() {
        return Shell::CreateNewWindow(browser_context, GURL(), nullptr,
                                      gfx::Size());
      });

  // Enable accessibility.
  ScopedAccessibilityModeOverride basic(ui::kAXModeBasic);

  // The initial WebContents has received the new mode.
  EXPECT_EQ(shell()->web_contents()->GetAccessibilityMode(), ui::kAXModeBasic);

  // Force creation of the manager.
  EXPECT_NE(rfhi->GetOrCreateBrowserAccessibilityManager(), nullptr);
  EXPECT_NE(rfhi->browser_accessibility_manager(), nullptr);

  // Crash the initial WebContents' renderer while it is still visible.
  // This ensures that on Android, the termination is detected as abnormal
  // (OOM protected) rather than normal termination, which would fail the
  // assertions in CrashTab.
  CrashTab(shell()->web_contents());

  // Enable mock time only for the disabler delay.
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_runner;

  // Hide all six, starting with the initial one.
  shell()->web_contents()->WasHidden();
  std::ranges::for_each(
      shells, [](Shell* shell) { shell->web_contents()->WasHidden(); });

  // At this point, the frame is not live.
  EXPECT_FALSE(rfhi->IsRenderFrameLive());
  // The manager should still exist.
  EXPECT_NE(rfhi->browser_accessibility_manager(), nullptr);

  // Let time pass to trigger the disabler.
  mock_time_runner->FastForwardBy(
      BrowserAccessibilityStateImpl::GetMaxDisableDelay());

  // The initial WebContents should have accessibility disabled.
  EXPECT_EQ(shell()->web_contents()->GetAccessibilityMode(), ui::AXMode());

  // The manager should have been destroyed.
  EXPECT_EQ(rfhi->browser_accessibility_manager(), nullptr);

  // Verify that calling GetOrCreateBrowserAccessibilityManager does not crash
  // (via CHECK/DCHECK) and returns nullptr when accessibility is disabled.
  EXPECT_EQ(rfhi->GetOrCreateBrowserAccessibilityManager(), nullptr);
}

INSTANTIATE_TEST_SUITE_P(Default,
                         ProgressiveAccessibilityTest,
                         testing::Values(false));
INSTANTIATE_TEST_SUITE_P(DisableOnHide,
                         ProgressiveAccessibilityTest,
                         testing::Values(true));

}  // namespace

}  // namespace content
