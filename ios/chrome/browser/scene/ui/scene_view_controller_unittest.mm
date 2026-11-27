// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/scene_view_controller.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/scene/ui/scene_ui_constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_test_passkey_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using layout_state::LayoutStateTestPassKeyFactory;

class SceneViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {kChromeNextIa, kFullscreenRefactoring, kAssistantSidePanel}, {});

    scoped_key_window_.emplace();
    UIWindow* window = scoped_key_window_->Get();
    window.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassRegular;
    window.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassRegular;
    window.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;

    layout_state_ = [[SceneLayoutState alloc] init];
    scene_view_controller_ = [[SceneViewController alloc] init];
    scene_view_controller_.layoutGuideCenter = [[LayoutGuideCenter alloc] init];
    scene_view_controller_.layoutState = layout_state_;

    window.rootViewController = scene_view_controller_;
    [window layoutIfNeeded];
    [window updateTraitsIfNeeded];
    [scene_view_controller_.view updateTraitsIfNeeded];
  }

  void TearDown() override {
    [scene_view_controller_ removeAssistantContainerViewController];
    scene_view_controller_.layoutState = nil;
    scoped_key_window_->Get().rootViewController = nil;
    scene_view_controller_ = nil;
    layout_state_ = nil;
    scoped_key_window_.reset();
    PlatformTest::TearDown();
  }

  // Updates the key window's `overrideUserInterfaceStyle`, flushes trait
  // propagation synchronously, and returns whether the scene view adopted
  // `style`.
  testing::AssertionResult SetWindowUserInterfaceStyle(
      UIUserInterfaceStyle style) {
    UIWindow* window = scoped_key_window_->Get();
    window.overrideUserInterfaceStyle = style;
    [window updateTraitsIfNeeded];
    [scene_view_controller_.view updateTraitsIfNeeded];
    UIUserInterfaceStyle actual =
        scene_view_controller_.view.traitCollection.userInterfaceStyle;
    if (actual != style) {
      return testing::AssertionFailure()
             << "Expected userInterfaceStyle " << style << ", got " << actual;
    }
    return testing::AssertionSuccess();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<ScopedKeyWindow> scoped_key_window_;
  SceneLayoutState* layout_state_ = nil;
  SceneViewController* scene_view_controller_ = nil;
};

// Tests that toggling `UITraitUserInterfaceStyle` between Light and Dark mode
// updates side-panel border color and shadow opacity on both the app content
// and assistant container views when the side panel is active.
TEST_F(SceneViewControllerTest,
       UserInterfaceStyleChangeUpdatesSidePanelAestheticsWhenActive) {
  UIView* app_content_view = chrome_test_util::FindViewById(
      scene_view_controller_.view, kAppContentAccessibilityIdentifier);
  ASSERT_NE(nil, app_content_view);
  UIView* app_content_shadow_view = app_content_view.superview;
  ASSERT_NE(nil, app_content_shadow_view);

  AssistantContainerViewController* assistant_view_controller =
      [[AssistantContainerViewController alloc]
          initWithViewController:[[UIViewController alloc] init]];
  [scene_view_controller_
      addAssistantContainerViewController:assistant_view_controller];
  UIView* assistant_shadow_view = assistant_view_controller.view.superview;
  ASSERT_NE(nil, assistant_shadow_view);

  [layout_state_
      setContainedLayoutActive:YES
                  scenePassKey:LayoutStateTestPassKeyFactory::CreateSceneKey()];
  ASSERT_EQ(UIUserInterfaceStyleLight,
            scene_view_controller_.view.traitCollection.userInterfaceStyle);
  ASSERT_NE(nullptr, app_content_view.layer.borderColor);
  ASSERT_NE(nullptr, assistant_view_controller.view.layer.borderColor);
  EXPECT_GT(app_content_shadow_view.layer.shadowOpacity, 0.0f);
  EXPECT_GT(assistant_shadow_view.layer.shadowOpacity, 0.0f);
  UIColor* light_border =
      [UIColor colorWithCGColor:app_content_view.layer.borderColor];

  // Toggle to Dark Mode: borders switch to `kSeparatorColor` and shadows hide.
  ASSERT_TRUE(SetWindowUserInterfaceStyle(UIUserInterfaceStyleDark));
  UITraitCollection* dark_traits = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleDark];
  UIColor* expected_dark_border = [[UIColor colorNamed:kSeparatorColor]
      resolvedColorWithTraitCollection:dark_traits];
  ASSERT_TRUE(expected_dark_border);
  ASSERT_NE(nullptr, app_content_view.layer.borderColor);
  ASSERT_NE(nullptr, assistant_view_controller.view.layer.borderColor);
  EXPECT_TRUE(CGColorEqualToColor(app_content_view.layer.borderColor,
                                  expected_dark_border.CGColor));
  EXPECT_TRUE(
      CGColorEqualToColor(assistant_view_controller.view.layer.borderColor,
                          expected_dark_border.CGColor));
  EXPECT_FALSE(CGColorEqualToColor(app_content_view.layer.borderColor,
                                   light_border.CGColor));
  EXPECT_FLOAT_EQ(0.0f, app_content_shadow_view.layer.shadowOpacity);
  EXPECT_FLOAT_EQ(0.0f, assistant_shadow_view.layer.shadowOpacity);

  // Toggle back to Light Mode: light border and drop shadows are restored.
  ASSERT_TRUE(SetWindowUserInterfaceStyle(UIUserInterfaceStyleLight));
  EXPECT_TRUE(CGColorEqualToColor(app_content_view.layer.borderColor,
                                  light_border.CGColor));
  EXPECT_TRUE(CGColorEqualToColor(
      assistant_view_controller.view.layer.borderColor, light_border.CGColor));
  EXPECT_GT(app_content_shadow_view.layer.shadowOpacity, 0.0f);
  EXPECT_GT(assistant_shadow_view.layer.shadowOpacity, 0.0f);
}

// Tests that when `containedLayoutActive` is NO (even when side panel layout is
// supported), app content has no side-panel aesthetics applied and live
// `UITraitUserInterfaceStyle` toggles leave its layer styling untouched.
TEST_F(SceneViewControllerTest,
       UserInterfaceStyleChangeLeavesViewsUntouchedWhenInactive) {
  ASSERT_TRUE(layout_state_.containedLayoutSupported);
  ASSERT_FALSE(layout_state_.containedLayoutActive);

  UIView* app_content_view = chrome_test_util::FindViewById(
      scene_view_controller_.view, kAppContentAccessibilityIdentifier);
  ASSERT_NE(nil, app_content_view);
  UIView* app_content_shadow_view = app_content_view.superview;
  ASSERT_NE(nil, app_content_shadow_view);

  EXPECT_EQ(0.0, app_content_view.layer.cornerRadius);
  EXPECT_EQ(0.0, app_content_view.layer.borderWidth);
  EXPECT_EQ(nullptr, app_content_view.layer.borderColor);
  EXPECT_FLOAT_EQ(0.0f, app_content_shadow_view.layer.shadowOpacity);

  ASSERT_TRUE(SetWindowUserInterfaceStyle(UIUserInterfaceStyleDark));
  EXPECT_EQ(0.0, app_content_view.layer.cornerRadius);
  EXPECT_EQ(0.0, app_content_view.layer.borderWidth);
  EXPECT_EQ(nullptr, app_content_view.layer.borderColor);
  EXPECT_FLOAT_EQ(0.0f, app_content_shadow_view.layer.shadowOpacity);

  ASSERT_TRUE(SetWindowUserInterfaceStyle(UIUserInterfaceStyleLight));
  EXPECT_EQ(0.0, app_content_view.layer.cornerRadius);
  EXPECT_EQ(0.0, app_content_view.layer.borderWidth);
  EXPECT_EQ(nullptr, app_content_view.layer.borderColor);
  EXPECT_FLOAT_EQ(0.0f, app_content_shadow_view.layer.shadowOpacity);
}

}  // namespace
