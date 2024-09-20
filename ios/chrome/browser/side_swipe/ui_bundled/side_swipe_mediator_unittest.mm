// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"

#import <WebKit/WebKit.h>

#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator+Testing.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/common/crw_web_view_content_view.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

namespace {

class SideSwipeMediatorTest : public PlatformTest {
 public:
  SideSwipeMediatorTest()
      : web_view_([[WKWebView alloc]
            initWithFrame:scoped_window_.Get().bounds
            configuration:[[WKWebViewConfiguration alloc] init]]),
        content_view_([[CRWWebViewContentView alloc]
            initWithWebView:web_view_
                 scrollView:web_view_.scrollView
            fullscreenState:CrFullscreenState::kNotInFullScreen]) {
    auto original_web_state(std::make_unique<web::FakeWebState>());
    original_web_state->SetView(content_view_);
    CRWWebViewScrollViewProxy* scroll_view_proxy =
        [[CRWWebViewScrollViewProxy alloc] init];
    UIScrollView* scroll_view = [[UIScrollView alloc] init];
    [scroll_view_proxy setScrollView:scroll_view];
    id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
    original_web_state->SetWebViewProxy(web_view_proxy_mock);

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    browser_->GetWebStateList()->InsertWebState(std::move(original_web_state));

    FullscreenController* fullscreen_controller =
        FullscreenController::FromBrowser(browser_.get());
    side_swipe_mediator_ = [[SideSwipeMediator alloc]
        initWithFullscreenController:fullscreen_controller
                        webStateList:browser_->GetWebStateList()];

    view_ = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 320, 240)];

    [side_swipe_mediator_ addHorizontalGesturesToView:view_];
  }

  ~SideSwipeMediatorTest() override { [side_swipe_mediator_ disconnect]; }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  UIView* view_;
  SideSwipeMediator* side_swipe_mediator_;
  ScopedKeyWindow scoped_window_;
  WKWebView* web_view_ = nil;
  CRWWebViewContentView* content_view_ = nil;
};

TEST_F(SideSwipeMediatorTest, TestConstructor) {
  EXPECT_TRUE(side_swipe_mediator_);
}

// Tests that pages that need to use Chromium native swipe
TEST_F(SideSwipeMediatorTest, TestEdgeNavigationEnabled) {
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  fake_navigation_manager->SetVisibleItem(item.get());
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

  // The NTP and chrome://crash should use native swipe.
  item->SetURL(GURL(kChromeUINewTabURL));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_TRUE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_mediator_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://crash"));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_TRUE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_mediator_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("http://wwww.test.com"));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_FALSE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_mediator_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://foo"));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_FALSE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_mediator_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://version"));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_FALSE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_mediator_.trailingEdgeNavigationEnabled);

  // Tests that when webstate is nil calling
  // updateNavigationEdgeSwipeForWebState doesn't change the edge navigation
  // state.
  item->SetURL(GURL("http://wwww.test.com"));
  [side_swipe_mediator_ updateNavigationEdgeSwipeForWebState:nil];
  EXPECT_FALSE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_mediator_.trailingEdgeNavigationEnabled);
  side_swipe_mediator_.leadingEdgeNavigationEnabled = YES;
  side_swipe_mediator_.trailingEdgeNavigationEnabled = YES;
  [side_swipe_mediator_ updateNavigationEdgeSwipeForWebState:nil];
  EXPECT_TRUE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_mediator_.trailingEdgeNavigationEnabled);
}

// Tests that when the active webState is changed or when the active webState
// finishes navigation, the edge state will be updated accordingly.
TEST_F(SideSwipeMediatorTest, ObserversTriggerStateUpdate) {
  ASSERT_FALSE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  ASSERT_FALSE(side_swipe_mediator_.trailingEdgeNavigationEnabled);

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetView(content_view_);
  CRWWebViewScrollViewProxy* scroll_view_proxy =
      [[CRWWebViewScrollViewProxy alloc] init];
  UIScrollView* scroll_view = [[UIScrollView alloc] init];
  [scroll_view_proxy setScrollView:scroll_view];
  id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
  [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
  fake_web_state->SetWebViewProxy(web_view_proxy_mock);
  web::FakeWebState* fake_web_state_ptr = fake_web_state.get();
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  fake_navigation_manager->SetVisibleItem(item.get());
  fake_navigation_manager->SetLastCommittedItem(item.get());
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

  // The NTP and chrome://crash should use native swipe.
  item->SetURL(GURL(kChromeUINewTabURL));
  // Insert the WebState and make sure it's active. This should trigger
  // the activation WebState change and update edge navigation state.
  browser_->GetWebStateList()->InsertWebState(
      std::move(fake_web_state),
      WebStateList::InsertionParams::AtIndex(1).Activate());
  EXPECT_TRUE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_mediator_.trailingEdgeNavigationEnabled);

  // Non native URL should have shouldn't be handled by SideSwipeMediator.
  item->SetURL(GURL("http://wwww.test.test"));
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  // Navigation finish should also update the edge navigation state.
  fake_web_state_ptr->OnNavigationFinished(&context);
  EXPECT_FALSE(side_swipe_mediator_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_mediator_.trailingEdgeNavigationEnabled);
}

}  // anonymous namespace
