// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"

#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SideSwipeController (ExposedForTesting)
@property(nonatomic, assign) BOOL leadingEdgeNavigationEnabled;
@property(nonatomic, assign) BOOL trailingEdgeNavigationEnabled;
- (void)updateNavigationEdgeSwipeForWebState:(web::WebState*)webState;
@end

namespace {

class TestWebStateListDelegate : public WebStateListDelegate {
  void WillAddWebState(web::WebState* web_state) override {}
  void WebStateDetached(web::WebState* web_state) override {}
};

class SideSwipeControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    // Create a mock for the TabModel that owns the object under test.
    tab_model_ = [OCMockObject niceMockForClass:[TabModel class]];
    std::unique_ptr<web::TestWebState> original_web_state(
        std::make_unique<web::TestWebState>());

    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    web_state_list_->InsertWebState(0, std::move(original_web_state),
                                    WebStateList::INSERT_NO_FLAGS,
                                    WebStateOpener());

    WebStateList* web_state_list = web_state_list_.get();
    [[[tab_model_ stub] andReturnValue:OCMOCK_VALUE(web_state_list)]
        webStateList];

    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
    // Create the object to test.
    side_swipe_controller_ =
        [[SideSwipeController alloc] initWithTabModel:tab_model_
                                         browserState:browser_state_.get()];

    view_ = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 320, 240)];

    [side_swipe_controller_ addHorizontalGesturesToView:view_];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  TestWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;

  UIView* view_;
  id tab_model_;
  SideSwipeController* side_swipe_controller_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SideSwipeControllerTest, TestConstructor) {
  EXPECT_TRUE(side_swipe_controller_);
}

TEST_F(SideSwipeControllerTest, TestSwipeRecognizers) {
  NSSet* recognizers = [side_swipe_controller_ swipeRecognizers];
  BOOL hasRecognizer = NO;
  for (UISwipeGestureRecognizer* swipeRecognizer in recognizers) {
    hasRecognizer = YES;
    EXPECT_TRUE(swipeRecognizer);
  }
  EXPECT_TRUE(hasRecognizer);
}

// Tests that pages that need to use Chromium native swipe
TEST_F(SideSwipeControllerTest, TestEdgeNavigationEnabled) {
  feature_list_.InitAndEnableFeature(web::features::kSlimNavigationManager);

  auto testWebState = std::make_unique<web::TestWebState>();
  auto testNavigationManager = std::make_unique<web::TestNavigationManager>();
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  testNavigationManager->SetVisibleItem(item.get());
  testWebState->SetNavigationManager(std::move(testNavigationManager));

  // The NTP and chrome://crash should use native swipe.
  item->SetURL(GURL(kChromeUINewTabURL));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:testWebState.get()];
  EXPECT_TRUE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://crash"));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:testWebState.get()];
  EXPECT_TRUE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("http://wwww.test.com"));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:testWebState.get()];
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://foo"));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:testWebState.get()];
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://version"));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:testWebState.get()];
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  // Tests that when webstate is nil calling
  // updateNavigationEdgeSwipeForWebState doesn't change the edge navigation
  // state.
  item->SetURL(GURL("http://wwww.test.com"));
  [side_swipe_controller_ updateNavigationEdgeSwipeForWebState:nil];
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);
  side_swipe_controller_.leadingEdgeNavigationEnabled = YES;
  side_swipe_controller_.trailingEdgeNavigationEnabled = YES;
  [side_swipe_controller_ updateNavigationEdgeSwipeForWebState:nil];
  EXPECT_TRUE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_controller_.trailingEdgeNavigationEnabled);
}

// Tests that when the active webState is changed or when the active webState
// finishes navigation, the edge state will be updated accordingly.
TEST_F(SideSwipeControllerTest, ObserversTriggerStateUpdate) {
  feature_list_.InitAndEnableFeature(web::features::kSlimNavigationManager);

  ASSERT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  ASSERT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  auto testWebState = std::make_unique<web::TestWebState>();
  web::TestWebState* testWebStatePtr = testWebState.get();
  auto testNavigationManager = std::make_unique<web::TestNavigationManager>();
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  testNavigationManager->SetVisibleItem(item.get());
  testNavigationManager->SetLastCommittedItem(item.get());
  testWebState->SetNavigationManager(std::move(testNavigationManager));

  // The NTP and chrome://crash should use native swipe.
  item->SetURL(GURL(kChromeUINewTabURL));
  // Insert the WebState and make sure it's active. This should trigger
  // didChangeActiveWebState and update edge navigation state.
  web_state_list_->InsertWebState(1, std::move(testWebState),
                                  WebStateList::INSERT_ACTIVATE,
                                  WebStateOpener());
  EXPECT_TRUE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  // Non native URL should have shouldn't be handled by SideSwipeController.
  item->SetURL(GURL("http://wwww.test.test"));
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  // Navigation finish should also update the edge navigation state.
  testWebStatePtr->OnNavigationFinished(&context);
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);
}

}  // anonymous namespace
