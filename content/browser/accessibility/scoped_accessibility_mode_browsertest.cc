// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/scoped_accessibility_mode.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/escape.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/render_accessibility_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"

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

  // Enable basic accessibility for the process.
  auto process_mode =
      accessibility_state().CreateScopedModeForProcess(ui::kAXModeBasic);

  // The WebContents gets it.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            (ui::kAXModeBasic) & ~kIgnoredModeFlags);

  // Add image labeling for the WebContents.
  auto wc_mode = accessibility_state().CreateScopedModeForWebContents(
      &web_contents1(), ui::AXMode::kLabelImages);

  // The WebContents doesn't get kLabelImages until kExtendedProperties appears.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::kAXModeBasic & ~kIgnoredModeFlags);

  process_mode =
      accessibility_state().CreateScopedModeForProcess(ui::kAXModeComplete);

  ASSERT_EQ(
      web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
      (ui::kAXModeComplete | ui::AXMode::kLabelImages) & ~kIgnoredModeFlags);
}

// Verifies that usage from the platform will set AXModes and that if the
// AXModes do not change, platform still have the chance to discover assistive
// technology in case we have enough signals of a screen reader may be running.
IN_PROC_BROWSER_TEST_F(ScopedAccessibilityModeTest, AXModesFromPlatform) {
  // Set changes from platform, as this is turned off by default for tests.
  accessibility_state().SetActivationFromPlatformEnabled(true);

  // Accessibility is off to start with.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode());

  ui::AXPlatform::GetInstance().OnPropertiesUsedInBrowserUI();
  ASSERT_EQ(accessibility_state().GetAccessibilityMode(),
            ui::AXMode(ui::AXMode::kNativeAPIs));

  ui::AXPlatform::GetInstance().OnExtendedPropertiesUsedInWebContent();
  ASSERT_EQ(accessibility_state().GetAccessibilityMode(),
            ui::kAXModeBasic | ui::AXMode::kExtendedProperties);

  ui::AXPlatform::GetInstance().OnInlineTextBoxesUsedInWebContent();
  ASSERT_EQ(accessibility_state().GetAccessibilityMode(), ui::kAXModeComplete);

  // Configure the detection of a screen reader, that will be triggered the next
  // time a page loads. Since the AXMode is already kAXModeComplete, there will
  // be no change in the AXMode the next time the platforms tries to access
  // accessibility APIs.

  std::unique_ptr<ScopedAccessibilityMode> screen_reader_mode;
  BrowserAccessibilityStateImpl::GetInstance()
      ->SetDiscoverAssistiveTechnologyCallbackForTesting(
          base::BindLambdaForTesting([&]() {
            BrowserAccessibilityStateImpl::GetInstance()->OnAssistiveTechFound(
                ui::AssistiveTech::kGenericScreenReader);
            screen_reader_mode =
                accessibility_state().CreateScopedModeForProcess(
                    ui::kAXModeComplete | ui::AXMode::kScreenReader);
          }));

  ui::AXPlatform::GetInstance().OnExtendedPropertiesUsedInWebContent();

  // The mode will not change because it is already complete, so the discovery
  // phase of ATs did not run yet.
  ASSERT_FALSE(screen_reader_mode);
  ASSERT_FALSE(accessibility_state().GetAccessibilityMode().has_mode(
      ui::AXMode::kScreenReader));

  const std::string html = "<p>Hello World</p>";
  GURL url("data:text/html," + base::EscapeQueryParamValue(html, false));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Simulate an action coming from ATs, which likely indicates a potential
  // screen reader.
  ui::AXPlatform::GetInstance().OnActionFromAssistiveTech();
  ASSERT_EQ(web_contents1().GetAccessibilityMode(),
            ui::kAXModeComplete | ui::AXMode::kScreenReader);
  BrowserAccessibilityStateImpl::GetInstance()
      ->SetDiscoverAssistiveTechnologyCallbackForTesting({});
}

class AccessibilityPerformanceMeasurementExperimentTest
    : public ScopedAccessibilityModeTest,
      public testing::WithParamInterface<std::string> {
 protected:
  AccessibilityPerformanceMeasurementExperimentTest() {
    // Initialize the feature and its parameters before the browser is
    // created, as its value is accessed during browser initialization. Please
    // see base::test::ScopedFeatureList documentation for more details. Note
    // that `GetParam()`will be instantiated for each test case with the
    // possible experiment groups.
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAccessibilityPerformanceMeasurementExperiment,
        {{"accessibility_performance_group_name", GetParam()}});
  }

  static ui::AXMode ExpectedAXModeForExperimentGroup() {
    if (GetParam() == "AXModeComplete") {
      return ui::kAXModeComplete & ~kIgnoredModeFlags;
    }
    if (GetParam() == "WebContentsOnly") {
      return ui::kAXModeBasic & ~kIgnoredModeFlags;
    }
    if (GetParam() == "AXModeCompleteNoInlineTextBoxes") {
      return (ui::kAXModeComplete & ~ui::AXMode::kInlineTextBoxes) &
             ~kIgnoredModeFlags;
    }
    if (GetParam() == "RendererSerializationOnly") {
      return ui::kAXModeComplete & ~kIgnoredModeFlags;
    }
    NOTREACHED();
  }

  void WaitForExperimentShutDown() {
    base::RunLoop loop;
    base::RepeatingClosure check_task;  // Declare upfront for self-capture
    check_task = base::BindLambdaForTesting([&]() {
      if (!accessibility_state()
               .IsAccessibilityPerformanceMeasurementExperimentActive()) {
        return loop.Quit();
      }
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, check_task, base::Milliseconds(10));
    });

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             check_task);
    loop.Run();
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AXModeComplete,
                         AccessibilityPerformanceMeasurementExperimentTest,
                         testing::Values("AXModeComplete"));
INSTANTIATE_TEST_SUITE_P(WebContentsOnly,
                         AccessibilityPerformanceMeasurementExperimentTest,
                         testing::Values("WebContentsOnly"));
INSTANTIATE_TEST_SUITE_P(AXModeCompleteNoInlineTextBoxes,
                         AccessibilityPerformanceMeasurementExperimentTest,
                         testing::Values("AXModeCompleteNoInlineTextBoxes"));
INSTANTIATE_TEST_SUITE_P(RendererSerializationOnly,
                         AccessibilityPerformanceMeasurementExperimentTest,
                         testing::Values("RendererSerializationOnly"));

// Verifies that when the feature
// `AccessibilityPerformanceMeasurementExperiment`is enabled, it modifies the
// AXModes.
IN_PROC_BROWSER_TEST_P(
    AccessibilityPerformanceMeasurementExperimentTest,
    AccessibilityPerformanceMeasurementExperimentChangesAXModes) {
  ASSERT_TRUE(
      features::IsAccessibilityPerformanceMeasurementExperimentEnabled());
  ASSERT_TRUE(accessibility_state()
                  .IsAccessibilityPerformanceMeasurementExperimentActive());

  // The AXModes must be the ones coming from the experiment group.
  ASSERT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ExpectedAXModeForExperimentGroup());
  ASSERT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ExpectedAXModeForExperimentGroup());
  ASSERT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ExpectedAXModeForExperimentGroup());

  // Set a new AXMode, which will cause the experiment to stop running and the
  // new AXMode to be only the one being set.
  auto scoped_mode = accessibility_state().CreateScopedModeForProcess(
      ui::kAXModeBasic & ~kIgnoredModeFlags);

  WaitForExperimentShutDown();
  ASSERT_FALSE(accessibility_state()
                   .IsAccessibilityPerformanceMeasurementExperimentActive());

  EXPECT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::kAXModeBasic & ~kIgnoredModeFlags);
  EXPECT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::kAXModeBasic & ~kIgnoredModeFlags);
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::kAXModeBasic & ~kIgnoredModeFlags);
}

// Verifies that when the experiment is running, it gets turned off in case a
// BrowserContext has AXModes set.
IN_PROC_BROWSER_TEST_P(AccessibilityPerformanceMeasurementExperimentTest,
                       ExperimentStopsAfterAXModeChangesForBrowserContext) {
  ASSERT_TRUE(
      features::IsAccessibilityPerformanceMeasurementExperimentEnabled());
  ASSERT_TRUE(accessibility_state()
                  .IsAccessibilityPerformanceMeasurementExperimentActive());

  auto scoped_mode = accessibility_state().CreateScopedModeForBrowserContext(
      &browser_context1(), ui::kAXModeComplete);

  WaitForExperimentShutDown();
  ASSERT_FALSE(accessibility_state()
                   .IsAccessibilityPerformanceMeasurementExperimentActive());
  EXPECT_EQ(web_contents1().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::kAXModeComplete & ~kIgnoredModeFlags);
  EXPECT_EQ(web_contents2().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::kAXModeComplete & ~kIgnoredModeFlags);
  EXPECT_EQ(web_contents3().GetAccessibilityMode() & ~kIgnoredModeFlags,
            ui::AXMode() & ~kIgnoredModeFlags);
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context1()) &
                ~kIgnoredModeFlags,
            ui::kAXModeComplete & ~kIgnoredModeFlags);
  EXPECT_EQ(accessibility_state().GetAccessibilityModeForBrowserContext(
                &browser_context2()) &
                ~kIgnoredModeFlags,
            ui::AXMode() & ~kIgnoredModeFlags);
}

class AccessibilityPerformanceMeasurementExperimentSerializationOnlyResetTest
    : public AccessibilityPerformanceMeasurementExperimentTest {
 protected:
  AccessibilityPerformanceMeasurementExperimentSerializationOnlyResetTest() =
      default;
};

INSTANTIATE_TEST_SUITE_P(
    RendererSerializationOnly,
    AccessibilityPerformanceMeasurementExperimentSerializationOnlyResetTest,
    testing::Values("RendererSerializationOnly"));

// This test verifies that accessibility is reset when leaving the Renderer
// Serialization Only experiment variant.
IN_PROC_BROWSER_TEST_P(
    AccessibilityPerformanceMeasurementExperimentSerializationOnlyResetTest,
    WebContentsResetAfterExperimentShutDown) {
  ASSERT_TRUE(
      features::IsAccessibilityPerformanceMeasurementExperimentEnabled());
  ASSERT_TRUE(accessibility_state()
                  .IsAccessibilityPerformanceMeasurementExperimentActive());

  base::RunLoop loop;
  RenderAccessibilityHost::SetAccessibilityDataDiscardedCallbackForTesting(
      loop.QuitClosure());

  // Navigate to a page and check that the accessibility events have been
  // discarded.
  const std::string html = "<p>Hello World</p>";
  GURL url("data:text/html," + base::EscapeQueryParamValue(html, false));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // This loop will quit once accessibility events arrive in the browser and
  // are discarded because the "RendererSerializationOnly" variant of the
  // performance experiment is active.
  loop.Run();
  RenderAccessibilityHost::SetAccessibilityDataDiscardedCallbackForTesting({});

  // Check that events have been discarded properly. This means that a
  // BrowserAccessibilityManager (BAM) was not created for this WebContents,
  // as a BAM is typically instantiated when processing accessibility events.
  auto* web_contents_impl = static_cast<WebContentsImpl*>(&web_contents1());
  ASSERT_FALSE(web_contents_impl->GetRootBrowserAccessibilityManager());

  AccessibilityNotificationWaiter waiter(&web_contents1(),
                                         ax::mojom::Event::kLoadComplete);

  // Create a new mode, which will stop the experiment.
  auto scoped_mode =
      accessibility_state().CreateScopedModeForProcess(ui::kAXModeBasic);

  ASSERT_TRUE(waiter.WaitForNotification());

  // Verify that the accessibility tree has now been loaded as a result of
  // resetting accessibility on the WebContents.
  ASSERT_TRUE(web_contents_impl->GetRootBrowserAccessibilityManager());
  ASSERT_TRUE(web_contents_impl->GetRootBrowserAccessibilityManager()
                  ->GetBrowserAccessibilityRoot());
}

}  // namespace content
