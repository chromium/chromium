// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/scoped_accessibility_mode.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform.h"

namespace content {

using ::testing::_;

namespace {

class MockAXModeObserver : public ui::AXModeObserver {
 public:
  MOCK_METHOD(void, OnAXModeAdded, (ui::AXMode mode), (override));
};

}  // namespace

// A test for ScopedAccessibilityModes vended by BrowserAccessibilityState.
class ScopedAccessibilityModeTest : public ContentBrowserTest {
 protected:
  ScopedAccessibilityModeTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    accessibility_state_ = BrowserAccessibilityState::GetInstance();

    // Get the initial BrowserContext and WebContents created by
    // ContentBrowserTest.
    auto* const default_shell = shell();
    browser_context1_ = default_shell->web_contents()->GetBrowserContext();
    web_contents1_ = default_shell->web_contents();

    // Create a second WebContents belonging to the initial BrowserContext.
    web_contents2_ = CreateBrowser()->web_contents();

    // Create a third WebContents belonging to a distinct BrowserContext.
    web_contents3_ = CreateOffTheRecordBrowser()->web_contents();
    browser_context2_ = web_contents3_->GetBrowserContext();
  }

  void TearDownOnMainThread() override {
    // Forget about the WebContentses and BrowserContext -- ContentBrowserTest
    // will clean them up.
    web_contents3_ = nullptr;
    browser_context2_ = nullptr;
    web_contents2_ = nullptr;
    web_contents1_ = nullptr;
    browser_context1_ = nullptr;

    accessibility_state_ = nullptr;
    ContentBrowserTest::TearDownOnMainThread();
  }

  BrowserAccessibilityState& accessibility_state() {
    return *accessibility_state_;
  }

  // The initial BrowserContext and its two WebContentses.
  BrowserContext& browser_context1() { return *browser_context1_; }
  WebContents& web_contents1() { return *web_contents1_; }
  WebContents& web_contents2() { return *web_contents2_; }

  // The second BrowserContext and its WebContents.
  BrowserContext& browser_context2() { return *browser_context2_; }
  WebContents& web_contents3() { return *web_contents3_; }

#if BUILDFLAG(IS_WIN)
  static constexpr ui::AXMode kIgnoredModeFlags{ui::AXMode::kNativeAPIs};
#else
  static constexpr ui::AXMode kIgnoredModeFlags{};
#endif

 private:
  raw_ptr<BrowserAccessibilityState> accessibility_state_ = nullptr;
  raw_ptr<BrowserContext> browser_context1_ = nullptr;
  raw_ptr<WebContents> web_contents1_ = nullptr;
  raw_ptr<WebContents> web_contents2_ = nullptr;
  raw_ptr<BrowserContext> browser_context2_ = nullptr;
  raw_ptr<WebContents> web_contents3_ = nullptr;
};

// Matches `arg` if it equals `flags`, ignoring those in `ignored_flags`. This
// is required on Windows, where kNativeAPIs may be turned on when a window
// receives focus; see https://crbug.com/1447827.
MATCHER_P2(EqualsIgnoring, flags, ignored_flags, "") {
  return (arg & ~ignored_flags) == (flags & ~ignored_flags);
}

// Verifies that all WebContentses in the process receive mode flags targeting
// the process.
IN_PROC_BROWSER_TEST_F(ScopedAccessibilityModeTest, Process) {
  ::testing::StrictMock<MockAXModeObserver> mock_observer;

  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver> observation(
      &mock_observer);
  observation.Observe(&ui::AXPlatform::GetInstance());

  // Accessibility is off to start with.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  ASSERT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  ASSERT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());

  // Add the basic flags to the process and verify that observers were notified
  // and that they were applied to all WebContentses.
  EXPECT_CALL(mock_observer, OnAXModeAdded(EqualsIgnoring(ui::kAXModeBasic,
                                                          kIgnoredModeFlags)));
  auto scoped_mode_1 =
      accessibility_state().CreateScopedModeForProcess(ui::kAXModeBasic);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(web_contents1().GetAccessibilityMode(), ui::kAXModeBasic);
  EXPECT_EQ(web_contents2().GetAccessibilityMode(), ui::kAXModeBasic);
  EXPECT_EQ(web_contents3().GetAccessibilityMode(), ui::kAXModeBasic);

  // Now add the complete flags and verify.
  EXPECT_CALL(mock_observer, OnAXModeAdded(EqualsIgnoring(ui::kAXModeComplete,
                                                          ui::kAXModeBasic)));
  auto scoped_mode_2 =
      accessibility_state().CreateScopedModeForProcess(ui::kAXModeComplete);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(web_contents1().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents2().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents3().GetAccessibilityMode(), ui::kAXModeComplete);

  // Release the basic flags and verify that complete still applies.
  scoped_mode_1.reset();
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(web_contents1().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents2().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents3().GetAccessibilityMode(), ui::kAXModeComplete);

  // Release the complete flags and verify that all flags are cleared.
  scoped_mode_2.reset();
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
}

// Verifies that only WebContentses belonging to a specific BrowserContext
// receive mode flags targeting that BrowserContext.
IN_PROC_BROWSER_TEST_F(ScopedAccessibilityModeTest, BrowserContext) {
  // Accessibility is off to start with.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  ASSERT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  ASSERT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  ASSERT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context1()) &
                ~kIgnoredModeFlags,
            ui::AXMode());
  ASSERT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context2()) &
                ~kIgnoredModeFlags,
            ui::AXMode());

  // Add the basic flags to all WebContentses for the first BrowserContext and
  // verify that they were applied only to its WebContentses.
  auto scoped_mode_1 = accessibility_state().CreateScopedModeForBrowserContext(
      &browser_context1(), ui::kAXModeBasic);
  EXPECT_EQ(web_contents1().GetAccessibilityMode(), ui::kAXModeBasic);
  EXPECT_EQ(web_contents2().GetAccessibilityMode(), ui::kAXModeBasic);
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context1()),
            ui::kAXModeBasic);
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context2()) &
                ~kIgnoredModeFlags,
            ui::AXMode());

  // Now add the complete flags and verify that they were applied.
  auto scoped_mode_2 = accessibility_state().CreateScopedModeForBrowserContext(
      &browser_context1(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents1().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents2().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context1()),
            ui::kAXModeComplete);
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context2()) &
                ~kIgnoredModeFlags,
            ui::AXMode());

  // Release the basic flags and verify that complete still applies.
  scoped_mode_1.reset();
  EXPECT_EQ(web_contents1().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents2().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context1()),
            ui::kAXModeComplete);
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context2()) &
                ~kIgnoredModeFlags,
            ui::AXMode());

  // Release the complete flags and verify that all flags are cleared.
  scoped_mode_2.reset();
  EXPECT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context1()) &
                ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context2()) &
                ~kIgnoredModeFlags,
            ui::AXMode());
}

// Verifies that only a targeted WebContentses receives mode flags.
IN_PROC_BROWSER_TEST_F(ScopedAccessibilityModeTest, WebContents) {
  // Accessibility is off to start with.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  ASSERT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  ASSERT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());

  // Add the basic flags to the WebContext and verify that they were applied.
  auto scoped_mode_1 = accessibility_state().CreateScopedModeForWebContents(
      &web_contents1(), ui::kAXModeBasic);
  EXPECT_EQ(web_contents1().GetAccessibilityMode(), ui::kAXModeBasic);
  EXPECT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());

  // Now add the complete flags and verify that they were applied.
  auto scoped_mode_2 = accessibility_state().CreateScopedModeForWebContents(
      &web_contents1(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents1().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());

  // Release the basic flags and verify that complete still applies.
  scoped_mode_1.reset();
  EXPECT_EQ(web_contents1().GetAccessibilityMode(), ui::kAXModeComplete);
  EXPECT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());

  // Release the complete flags and verify that all flags are cleared.
  scoped_mode_2.reset();
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());
}

// Verifies that filtering results in a WebContents receives various web flags
// only when web accessibility is enabled.
IN_PROC_BROWSER_TEST_F(ScopedAccessibilityModeTest, Filtering) {
  // Accessibility is off to start with.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());

  // Enable PDF OCR for the WebContents.
  auto wc_mode = accessibility_state().CreateScopedModeForWebContents(
      &web_contents1(), ui::AXMode::kPDFOcr);

  // No change to the WebContents.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());

  // Enable basic accessibility for the process.
  auto process_mode =
      accessibility_state().CreateScopedModeForProcess(ui::kAXModeBasic);

  // Now the WebContents gets both.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            (ui::kAXModeBasic | ui::AXMode::kPDFOcr) & ~kIgnoredModeFlags);

  // Add image labeling for the WebContents.
  wc_mode = accessibility_state().CreateScopedModeForWebContents(
      &web_contents1(), ui::AXMode::kPDFOcr | ui::AXMode::kLabelImages);

  // The WebContents doesn't get kLabelImages until kScreenReader appears.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            (ui::kAXModeBasic | ui::AXMode::kPDFOcr) & ~kIgnoredModeFlags);

  process_mode =
      accessibility_state().CreateScopedModeForProcess(ui::kAXModeComplete);

  ASSERT_EQ(
      web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
      (ui::kAXModeComplete | ui::AXMode::kPDFOcr | ui::AXMode::kLabelImages) &
          ~kIgnoredModeFlags);
}

}  // namespace content
