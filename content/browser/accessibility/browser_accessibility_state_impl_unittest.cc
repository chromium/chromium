// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/events/base_event_utils.h"

namespace content {

class BrowserAccessibilityStateImplTest : public ::testing::Test {
 public:
  BrowserAccessibilityStateImplTest() = default;
  BrowserAccessibilityStateImplTest(const BrowserAccessibilityStateImplTest&) =
      delete;
  ~BrowserAccessibilityStateImplTest() override = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutoDisableAccessibility);
    // Set the initial time to something non-zero.
    task_environment_.FastForwardBy(base::Seconds(100));
    state_ = BrowserAccessibilityStateImpl::GetInstance();
  }

  void TearDown() override {
    // Disable accessibility so that it does not impact subsequent tests.
    state_->DisableAccessibility();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<BrowserAccessibilityStateImpl> state_;
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;
};

TEST_F(BrowserAccessibilityStateImplTest,
       DisableAccessibilityBasedOnUserEvents) {
  base::HistogramTester histograms;

  // Initially, accessibility should be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::AXMode());

  // Enable accessibility based on usage of accessibility APIs.
  state_->OnScreenReaderDetected();
  EXPECT_TRUE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::kAXModeComplete);

  // Send user input, wait 31 seconds, then send another user input event.
  // Don't simulate any accessibility APIs in that time.
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();
  task_environment_.FastForwardBy(base::Seconds(31));
  state_->OnUserInputEvent();

  // Accessibility should now be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::AXMode());

  // A histogram should record that accessibility was disabled with
  // 3 input events.
  histograms.ExpectUniqueSample("Accessibility.AutoDisabled.EventCount", 3, 1);

  // A histogram should record that accessibility was enabled for
  // 31 seconds.
  histograms.ExpectUniqueTimeSample("Accessibility.AutoDisabled.EnabledTime",
                                    base::Seconds(31), 1);
}

TEST_F(BrowserAccessibilityStateImplTest,
       AccessibilityApiUsagePreventsAutoDisableAccessibility) {
  // Initially, accessibility should be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::AXMode());

  // Enable accessibility based on usage of accessibility APIs.
  state_->OnScreenReaderDetected();
  EXPECT_TRUE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::kAXModeComplete);

  // Send user input, wait 31 seconds, then send another user input event -
  // but simulate accessibility APIs in that time.
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();
  task_environment_.FastForwardBy(base::Seconds(31));
  state_->OnAccessibilityApiUsage();
  state_->OnUserInputEvent();

  // Accessibility should still be enabled.
  EXPECT_TRUE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::kAXModeComplete);

  // Same test, but simulate accessibility API usage after the first
  // user input event, before the delay.
  state_->OnUserInputEvent();
  state_->OnAccessibilityApiUsage();
  task_environment_.FastForwardBy(base::Seconds(31));
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();

  // Accessibility should still be enabled.
  EXPECT_TRUE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::kAXModeComplete);

  // Advance another 31 seconds and simulate another user input event;
  // now accessibility should be disabled.
  task_environment_.FastForwardBy(base::Seconds(31));
  state_->OnUserInputEvent();
  EXPECT_FALSE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::AXMode());
}

TEST_F(BrowserAccessibilityStateImplTest,
       AddAccessibilityModeFlagsPreventsAutoDisableAccessibility) {
  // Initially, accessibility should be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::AXMode());

  // Enable accessibility.
  state_->OnScreenReaderDetected();
  EXPECT_TRUE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::kAXModeComplete);

  // Send user input, wait 31 seconds, then send another user input event -
  // but add a new accessibility mode flag.
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();
  task_environment_.FastForwardBy(base::Seconds(31));
  state_->AddAccessibilityModeFlags(ui::kAXModeComplete);
  state_->OnUserInputEvent();

  // Accessibility should still be enabled.
  EXPECT_TRUE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::kAXModeComplete);
}

TEST_F(BrowserAccessibilityStateImplTest,
       GetRolePreventsAutoDisableAccessibility) {
  // Create a bare-minimum accessibility tree so we can call GetRole().
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(root),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibility* ax_root =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, ax_root);

  // Initially, accessibility should be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::AXMode());

  // Enable accessibility.
  state_->OnScreenReaderDetected();
  EXPECT_TRUE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::kAXModeComplete);

  // Send user input, wait 31 seconds, then send another user input event after
  // checking the role, which should register accessibility API usage.
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();
  task_environment_.FastForwardBy(base::Seconds(31));
  ax_root->GetRole();
  state_->OnUserInputEvent();

  // Accessibility should still be enabled due to GetRole() being called.
  EXPECT_TRUE(state_->IsAccessibleBrowser());
  EXPECT_EQ(ui::AXPlatformNode::GetAccessibilityMode(), ui::kAXModeComplete);
}

TEST_F(BrowserAccessibilityStateImplTest, DisableAccessibilityHasADelay) {
  // Initially accessibility should be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());

  // Enable accessibility.
  state_->OnScreenReaderDetected();
  EXPECT_TRUE(state_->IsAccessibleBrowser());

  // After 10 seconds, disable accessibility in response to client being quit.
  task_environment_.FastForwardBy(base::Seconds(10));
  state_->OnScreenReaderStopped();

  // After one second, accessibility support should still be enabled. This is
  // because we delay disabling accessibility support in response to the client
  // being quit just in case it is about to be toggled back on.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(state_->IsAccessibleBrowser());

  // After the delay has passed without support being re-enabled, accessibility
  // should now be disabled.
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(state_->IsAccessibleBrowser());
}

TEST_F(BrowserAccessibilityStateImplTest,
       EnableImmediatelyAfterDisablePreventsDisable) {
  // Initially accessibility should be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());

  // Enable accessibility.
  state_->OnScreenReaderDetected();
  EXPECT_TRUE(state_->IsAccessibleBrowser());

  // After 10 seconds, disable accessibility in response to client being quit.
  // Then re-enable it immediately. Accessibility support should never get
  // disabled because it was re-enabled before the delay to disable support
  // had passed.
  task_environment_.FastForwardBy(base::Seconds(10));
  state_->OnScreenReaderStopped();
  EXPECT_TRUE(state_->IsAccessibleBrowser());

  task_environment_.FastForwardBy(base::Milliseconds(10));
  state_->OnScreenReaderDetected();
  for (int i = 0; i < 10; i++) {
    task_environment_.FastForwardBy(base::Seconds(i));
    EXPECT_TRUE(state_->IsAccessibleBrowser());
  }
}

}  // namespace content
