// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"
#include "ui/events/base_event_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#endif

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

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<BrowserAccessibilityStateImpl> state_;
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ui::TestAXPlatformTreeManagerDelegate>
      test_browser_accessibility_delegate_;
  ui::TestAXNodeIdDelegate node_id_delegate_;
};

TEST_F(BrowserAccessibilityStateImplTest,
       AddAccessibilityModeFlagsPreventsAutoDisableAccessibility) {
  // Initially, accessibility should be disabled.
  EXPECT_EQ(ui::AXPlatform::GetInstance().GetMode(), ui::AXMode());

  // Enable accessibility.
  ScopedAccessibilityModeOverride scoped_mode(ui::kAXModeComplete);
  EXPECT_EQ(ui::AXPlatform::GetInstance().GetMode(), ui::kAXModeComplete);

  // Send user input, wait 31 seconds, then send another user input event -
  // but add a new accessibility mode flag.
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();
  task_environment_.FastForwardBy(base::Seconds(31));
  ScopedAccessibilityModeOverride scoped_mode_2(ui::kAXModeComplete);
  state_->OnUserInputEvent();

  // Accessibility should still be enabled.
  EXPECT_EQ(ui::AXPlatform::GetInstance().GetMode(), ui::kAXModeComplete);
}

TEST_F(BrowserAccessibilityStateImplTest,
       GetRolePreventsAutoDisableAccessibility) {
  // Create a bare-minimum accessibility tree so we can call GetRole().
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  ui::BrowserAccessibilityManager* manager;
#if BUILDFLAG(IS_ANDROID)
  manager = BrowserAccessibilityManagerAndroid::Create(
      MakeAXTreeUpdateForTesting(root), node_id_delegate_,
      test_browser_accessibility_delegate_.get());
#else
  manager = ui::BrowserAccessibilityManager::Create(
      MakeAXTreeUpdateForTesting(root), node_id_delegate_,
      test_browser_accessibility_delegate_.get());
#endif
  std::unique_ptr<ui::BrowserAccessibilityManager>
      browser_accessibility_manager(manager);

  ui::BrowserAccessibility* ax_root =
      browser_accessibility_manager->GetBrowserAccessibilityRoot();
  ASSERT_NE(nullptr, ax_root);

  // Initially, accessibility should be disabled.
  EXPECT_EQ(ui::AXPlatform::GetInstance().GetMode(), ui::AXMode());

  // Enable accessibility.
  ScopedAccessibilityModeOverride scoped_mode(ui::kAXModeComplete);
  EXPECT_EQ(ui::AXPlatform::GetInstance().GetMode(), ui::kAXModeComplete);

  // Send user input, wait 31 seconds, then send another user input event after
  // checking the role, which should register accessibility API usage.
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();
  task_environment_.FastForwardBy(base::Seconds(31));
  ax_root->GetRole();
  state_->OnUserInputEvent();

  // Accessibility should still be enabled due to GetRole() being called.
  EXPECT_EQ(ui::AXPlatform::GetInstance().GetMode(), ui::kAXModeComplete);
}

namespace {
using ::testing::_;

class MockAXModeObserver : public ui::AXModeObserver {
 public:
  MOCK_METHOD(void, OnAXModeAdded, (ui::AXMode mode), (override));
};

}  // namespace

TEST_F(BrowserAccessibilityStateImplTest,
       EnablingAccessibilityTwiceSendsASingleNotification) {
  // Initially accessibility should be disabled.
  EXPECT_EQ(ui::AXPlatform::GetInstance().GetMode(), ui::AXMode());

  auto& ax_platform = ui::AXPlatform::GetInstance();
  ::testing::StrictMock<MockAXModeObserver> mock_observer;
  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      scoped_observation(&mock_observer);
  scoped_observation.Observe(&ax_platform);

  // Enable accessibility.
  EXPECT_CALL(mock_observer, OnAXModeAdded(ui::kAXModeComplete));
  ScopedAccessibilityModeOverride scoped_mode(ui::kAXModeComplete);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // A second call should be a no-op.
  ScopedAccessibilityModeOverride scoped_mode_2(ui::kAXModeComplete);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
}

// Tests platform activation filtering.
TEST_F(BrowserAccessibilityStateImplTest, PlatformActivationFiltering) {
  // Disabled by default in all unit tests.
  ASSERT_FALSE(state_->IsActivationFromPlatformEnabled());
  ASSERT_EQ(state_->GetAccessibilityMode(), ui::AXMode());

  {
    // Adding a modes from the platform is a no-op.
    auto complete = state_->CreateScopedModeForProcess(
        ui::kAXModeComplete | ui::AXMode::kFromPlatform);
    ASSERT_EQ(state_->GetAccessibilityMode(), ui::AXMode());

    // Until platform activation is enabled.
    state_->SetActivationFromPlatformEnabled(true);
    ASSERT_TRUE(state_->IsActivationFromPlatformEnabled());
    EXPECT_EQ(state_->GetAccessibilityMode(), ui::kAXModeComplete);

    // Enabling when already enabled does nothing.
    state_->SetActivationFromPlatformEnabled(true);
    ASSERT_TRUE(state_->IsActivationFromPlatformEnabled());
    EXPECT_EQ(state_->GetAccessibilityMode(), ui::kAXModeComplete);

    state_->SetActivationFromPlatformEnabled(false);
  }

  {
    // Adding modes without the bit works as expected.
    auto basic = state_->CreateScopedModeForProcess(ui::kAXModeBasic);
    EXPECT_EQ(state_->GetAccessibilityMode() & ui::kAXModeBasic,
              ui::kAXModeBasic);

    // And filtering has no impact.
    state_->SetActivationFromPlatformEnabled(true);
    EXPECT_EQ(state_->GetAccessibilityMode() & ui::kAXModeBasic,
              ui::kAXModeBasic);
    state_->SetActivationFromPlatformEnabled(false);
  }
}

}  // namespace content
