// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/popup_menu/coordinator/popup_menu_help_coordinator.h"

#import "base/i18n/test/scoped_rtl_for_testing.h"
#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter+Testing.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_coordinator.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Exposes the private methods that decide where tools menu bubbles point.
@interface PopupMenuHelpCoordinator (Testing)
- (CGRect)toolsMenuFrameInBaseView;
- (BOOL)isToolsMenuAtLeading;
- (BubbleViewControllerPresenter*)
    newOverflowMenuBubblePresenterWithAnchorXInParent:(CGFloat)anchorXInParent
                                      parentViewWidth:(CGFloat)parentViewWidth
                                                 text:(NSString*)text
                                           bubbleType:(BubbleViewType)bubbleType
                                      pageControlPage:
                                          (BubblePageControlPage)pageControlPage
                                totalPageControlPages:
                                    (NSInteger)totalPageControlPages
                                    dismissalCallback:
                                        (CallbackWithIPHDismissalReasonType)
                                            dismissalCallback;
@end

namespace {

class PopupMenuHelpCoordinatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    ClearDefaultBrowserPromoData();

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindOnce(
            &std::make_unique<feature_engagement::test::MockTracker>));

    profile_ = std::move(builder).Build();

    scene_state_ = [[SceneState alloc] init];
    LayoutGuideSceneAgent* layout_guide_scene_agent =
        [[LayoutGuideSceneAgent alloc] init];
    [scene_state_ addAgent:layout_guide_scene_agent];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    root_view_controller_ = [[UIViewController alloc] init];
    popup_menu_help_coordinator_ = [[PopupMenuHelpCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()];
    popupMenuUIUpdating_ = OCMProtocolMock(@protocol(PopupMenuUIUpdating));
    popup_menu_help_coordinator_.UIUpdater = popupMenuUIUpdating_;
    [popup_menu_help_coordinator_ start];

    tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));
    ON_CALL(*tracker_, IsInitialized()).WillByDefault(testing::Return(true));
  }

  void TearDown() override {
    ClearDefaultBrowserPromoData();
    [popup_menu_help_coordinator_ stop];
    popup_menu_help_coordinator_ = nil;
    root_view_controller_ = nil;
    browser_.reset();
    tracker_ = nullptr;
    profile_.reset();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
    PlatformTest::TearDown();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  // Owned here, as the coordinator only keeps a weak reference to it.
  UIViewController* root_view_controller_;
  PopupMenuHelpCoordinator* popup_menu_help_coordinator_;
  id<PopupMenuUIUpdating> popupMenuUIUpdating_;
  raw_ptr<feature_engagement::test::MockTracker> tracker_;
};

// Test that blue dot is set on foreground.
TEST_F(PopupMenuHelpCoordinatorTest, ShowBlueDotSetOnForeground) {
  ON_CALL(
      *tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserOverflowMenuBadgeFeature)))
      .WillByDefault(testing::Return(true));
  OCMExpect([popupMenuUIUpdating_ setOverflowMenuBlueDot:YES]);

  // Move to foreground and check the expectations.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE([popup_menu_help_coordinator_ hasBlueDotForOverflowMenu]);
  EXPECT_OCMOCK_VERIFY((id)popupMenuUIUpdating_);
}
//

// Test that blue dot is not set on foreground when FET feature is not eligible.
TEST_F(PopupMenuHelpCoordinatorTest, DontShowBlueDotSetOnForeground) {
  ON_CALL(
      *tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserOverflowMenuBadgeFeature)))
      .WillByDefault(testing::Return(false));
  OCMExpect([popupMenuUIUpdating_ setOverflowMenuBlueDot:NO]);

  // Move to foreground and check the expectations.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE([popup_menu_help_coordinator_ hasBlueDotForOverflowMenu]);
  EXPECT_OCMOCK_VERIFY((id)popupMenuUIUpdating_);
}

// Verifies that the Level Up walkthrough target item (Tools menu layout guide)
// can be created in the LayoutGuideCenter.
TEST_F(PopupMenuHelpCoordinatorTest, HasToolsMenuLayoutGuideLevelUpTarget) {
  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(browser_.get());
  EXPECT_NE([layoutGuideCenter makeLayoutGuideNamed:kToolsMenuGuide], nil);
}

// Returns whether `coordinator` considers the tools menu button at `frame`, in
// the base view's coordinates, at the leading edge. The button's frame is
// stubbed, as `LayoutGuideCenter` only tracks views in a window.
bool IsToolsMenuAtLeading(PopupMenuHelpCoordinator* coordinator, CGRect frame) {
  id mock = OCMPartialMock(coordinator);
  OCMStub([mock toolsMenuFrameInBaseView]).andReturn(frame);
  bool at_leading = [mock isToolsMenuAtLeading];
  [mock stopMocking];
  return at_leading;
}

// Tests that, in the Next IA, the tools menu button is at the leading edge in
// the left half of the base view in LTR, and in its right half in RTL, as
// bubble alignments are expressed in leading and trailing terms.
TEST_F(PopupMenuHelpCoordinatorTest,
       NextIaToolsMenuAtLeadingFollowsLayoutDirection) {
  base::test::ScopedFeatureList feature_list(kChromeNextIa);
  root_view_controller_.view.frame = CGRectMake(0, 0, 400, 800);
  const CGRect left_frame = CGRectMake(0, 50, 38, 38);
  const CGRect right_frame = CGRectMake(362, 50, 38, 38);
  EXPECT_TRUE(IsToolsMenuAtLeading(popup_menu_help_coordinator_, left_frame));
  EXPECT_FALSE(IsToolsMenuAtLeading(popup_menu_help_coordinator_, right_frame));

  base::i18n::ScopedRTLForTesting scoped_rtl(/*rtl=*/true);
  EXPECT_FALSE(IsToolsMenuAtLeading(popup_menu_help_coordinator_, left_frame));
  EXPECT_TRUE(IsToolsMenuAtLeading(popup_menu_help_coordinator_, right_frame));
}

// Tests that overflow menu IPHs, e.g. the Level Up walkthrough's, are leading
// aligned when anchored in the leading half of the menu: its left half in LTR,
// and its right half in RTL.
TEST_F(PopupMenuHelpCoordinatorTest,
       OverflowMenuIPHAlignmentFollowsLayoutDirection) {
  constexpr CGFloat kMenuWidth = 400;
  constexpr CGFloat kLeftAnchorX = 50;
  constexpr CGFloat kRightAnchorX = 350;
  PopupMenuHelpCoordinator* coordinator = popup_menu_help_coordinator_;
  auto alignment = [coordinator](CGFloat anchor_x) {
    return [coordinator
               newOverflowMenuBubblePresenterWithAnchorXInParent:anchor_x
                                                 parentViewWidth:kMenuWidth
                                                            text:@"text"
                                                      bubbleType:
                                                          BubbleViewTypeDefault
                                                 pageControlPage:
                                                     BubblePageControlPageNone
                                           totalPageControlPages:0
                                               dismissalCallback:nil]
        .alignment;
  };

  EXPECT_EQ(BubbleAlignmentTopOrLeading, alignment(kLeftAnchorX));
  EXPECT_EQ(BubbleAlignmentBottomOrTrailing, alignment(kRightAnchorX));

  base::i18n::ScopedRTLForTesting scoped_rtl(/*rtl=*/true);
  EXPECT_EQ(BubbleAlignmentBottomOrTrailing, alignment(kLeftAnchorX));
  EXPECT_EQ(BubbleAlignmentTopOrLeading, alignment(kRightAnchorX));
}

}  // namespace
