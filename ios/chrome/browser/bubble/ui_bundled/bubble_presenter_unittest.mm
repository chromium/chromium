// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_presenter_delegate.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeBubblePresenterDelegate : NSObject <BubblePresenterDelegate>
@property(nonatomic, assign) BOOL rootViewVisible;
@end

@implementation FakeBubblePresenterDelegate
- (BOOL)rootViewVisibleForBubblePresenter:(BubblePresenter*)bubblePresenter {
  return _rootViewVisible;
}
- (BOOL)isNTPActiveForBubblePresenter:(BubblePresenter*)bubblePresenter {
  return NO;
}
- (void)scrollNTPToTopForBubblePresenter:(BubblePresenter*)bubblePresenter {
}
- (BOOL)isOverscrollActionsSupportedForBubblePresenter:
    (BubblePresenter*)bubblePresenter {
  return YES;
}
- (void)bubblePresenterDidPerformPullToRefreshGesture:
    (BubblePresenter*)bubblePresenter {
}
- (void)bubblePresenter:(BubblePresenter*)bubblePresenter
    didPerformSwipeToNavigateInDirection:
        (UISwipeGestureRecognizerDirection)direction {
}
@end

@interface BubblePresenter (Testing)
- (BubbleViewControllerPresenter*)sendTabToSelfOmniboxBubblePresenterForTesting;
@end

namespace {

class BubblePresenterTest : public PlatformTest {
 protected:
  BubblePresenterTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    web_state_list_ = browser_->GetWebStateList();
    layout_guide_center_ = [[LayoutGuideCenter alloc] init];

    bubble_presenter_ = [[BubblePresenter alloc]
            initWithLayoutGuideCenter:layout_guide_center_
                    engagementTracker:feature_engagement::TrackerFactory::
                                          GetForProfile(profile_.get())
                         webStateList:web_state_list_
                 fullscreenController:nullptr
                          layoutState:nil
        overlayPresenterForWebContent:nullptr
                        infobarBanner:nullptr
                         infobarModal:nullptr];

    delegate_ = [[FakeBubblePresenterDelegate alloc] init];
    bubble_presenter_.delegate = delegate_;
  }

  void TearDown() override {
    [bubble_presenter_ disconnect];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  LayoutGuideCenter* layout_guide_center_;
  raw_ptr<WebStateList> web_state_list_ = nullptr;
  BubblePresenter* bubble_presenter_ = nil;
  FakeBubblePresenterDelegate* delegate_ = nil;
};

// Test presentSendTabToSelfOmniboxBubble when preconditions are not met.
TEST_F(BubblePresenterTest, PresentSendTabToSelfOmniboxBubblePreconditions) {
  delegate_.rootViewVisible = NO;
  [bubble_presenter_ presentSendTabToSelfOmniboxBubble];
  EXPECT_EQ(nil,
            [bubble_presenter_ sendTabToSelfOmniboxBubblePresenterForTesting]);
}

// Test presentSendTabToSelfOmniboxBubble execution logic when preconditions
// pass.
TEST_F(BubblePresenterTest, PresentSendTabToSelfOmniboxBubble) {
  UIViewController* root_view_controller = [[UIViewController alloc] init];
  bubble_presenter_.rootViewController = root_view_controller;
  delegate_.rootViewVisible = YES;
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state_list_->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  [bubble_presenter_ presentSendTabToSelfOmniboxBubble];
  EXPECT_NE(nil,
            [bubble_presenter_ sendTabToSelfOmniboxBubblePresenterForTesting]);
}

}  // namespace
