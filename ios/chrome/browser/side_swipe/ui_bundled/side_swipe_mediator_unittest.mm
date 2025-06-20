// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"

#import <WebKit/WebKit.h>

#import "base/i18n/rtl.h"
#import "base/test/bind.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_consumer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator+Testing.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
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

@interface FakeSideSwipeUIController : NSObject <SideSwipeConsumer>

@property(nonatomic, assign) BOOL leadingEdgeNavigationEnabled;
@property(nonatomic, assign) BOOL trailingEdgeNavigationEnabled;

@end

@implementation FakeSideSwipeUIController

- (void)setLeadingEdgeNavigationEnabled:(BOOL)enabled {
  _leadingEdgeNavigationEnabled = enabled;
}

- (void)setTrailingEdgeNavigationEnabled:(BOOL)enabled {
  _trailingEdgeNavigationEnabled = enabled;
}

- (void)cancelOnGoingSwipe {
  // NO-OP
}

- (void)webPageLoaded {
  // NO-OP
}

@end

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

    original_web_state->SetBrowserState(profile_.get());
    original_web_state_ = original_web_state.get();

    active_web_state_index_ = browser_->GetWebStateList()->InsertWebState(
        std::move(original_web_state));
    browser_->GetWebStateList()->ActivateWebStateAt(
        active_web_state_index_.value());

    side_swipe_mediator_ = [[SideSwipeMediator alloc]
        initWithWebStateList:browser_->GetWebStateList()];

    fake_swipe_ui_controller_ = [[FakeSideSwipeUIController alloc] init];
    side_swipe_mediator_.consumer = fake_swipe_ui_controller_;
  }

  ~SideSwipeMediatorTest() override { [side_swipe_mediator_ disconnect]; }

  void CloseActiveWebState() {
    browser_->GetWebStateList()->CloseWebStateAt(
        active_web_state_index_.value(),
        WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
    active_web_state_index_.reset();
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  UIView* view_;
  SideSwipeMediator* side_swipe_mediator_;
  FakeSideSwipeUIController* fake_swipe_ui_controller_;
  ScopedKeyWindow scoped_window_;
  WKWebView* web_view_ = nil;
  CRWWebViewContentView* content_view_ = nil;
  raw_ptr<web::WebState> original_web_state_ = nil;
  std::optional<int> active_web_state_index_;
};

TEST_F(SideSwipeMediatorTest, TestConstructor) {
  EXPECT_TRUE(side_swipe_mediator_);
}

// Tests that pages that need to use Chromium native swipe
TEST_F(SideSwipeMediatorTest, TestEdgeNavigationEnabled) {
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetBrowserState(profile_.get());
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  fake_navigation_manager->SetVisibleItem(item.get());
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

  // The NTP and chrome://crash should use native swipe.
  item->SetURL(GURL(kChromeUINewTabURL));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_TRUE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://crash"));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_TRUE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("http://wwww.test.com"));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_FALSE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://foo"));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_FALSE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://version"));
  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_FALSE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);

  // Tests that when webstate is nil calling
  // updateNavigationEdgeSwipeForWebState doesn't change the edge navigation
  // state.
  item->SetURL(GURL("http://wwww.test.com"));
  [side_swipe_mediator_ updateNavigationEdgeSwipeForWebState:nil];
  EXPECT_FALSE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);
  fake_swipe_ui_controller_.leadingEdgeNavigationEnabled = YES;
  fake_swipe_ui_controller_.trailingEdgeNavigationEnabled = YES;
  [side_swipe_mediator_ updateNavigationEdgeSwipeForWebState:nil];
  EXPECT_TRUE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);
}

// Tests that when the active webState is changed or when the active webState
// finishes navigation, the edge state will be updated accordingly.
TEST_F(SideSwipeMediatorTest, ObserversTriggerStateUpdate) {
  ASSERT_FALSE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  ASSERT_FALSE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetBrowserState(profile_.get());
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
  EXPECT_TRUE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);

  // Non native URL should have shouldn't be handled by SideSwipeMediator.
  item->SetURL(GURL("http://wwww.test.test"));
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  // Navigation finish should also update the edge navigation state.
  fake_web_state_ptr->OnNavigationFinished(&context);
  EXPECT_FALSE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);
}

// Tests that the snapshot update runs the provided callback that updates
// the snapshot state when the snapshot tab helper is not present.
TEST_F(SideSwipeMediatorTest, SnapshotUpdatedWithoutTabHelper) {
  base::RunLoop run_loop;
  int snapshot_updated = 0;
  [side_swipe_mediator_
      updateActiveTabSnapshot:base::CallbackToBlock(
                                  base::BindLambdaForTesting([&]() {
                                    snapshot_updated++;
                                    run_loop.Quit();
                                  }))];
  run_loop.Run();
  EXPECT_EQ(1, snapshot_updated);
}

// Tests that the snapshot update runs the provided callback that updates
// the snapshot state when the active web state is null.
TEST_F(SideSwipeMediatorTest, SnapshotUpdatedWithoutActiveWebState) {
  base::RunLoop run_loop;
  int snapshot_updated = 0;
  CloseActiveWebState();
  [side_swipe_mediator_
      updateActiveTabSnapshot:base::CallbackToBlock(
                                  base::BindLambdaForTesting([&]() {
                                    snapshot_updated++;
                                    run_loop.Quit();
                                  }))];
  run_loop.Run();
  EXPECT_EQ(1, snapshot_updated);
}

// Tests that the snapshot update runs the provided callback that updates
// the snapshot state only once on completion.
TEST_F(SideSwipeMediatorTest, SnapshotUpdatedOnceOnCallback) {
  SnapshotTabHelper::CreateForWebState(original_web_state_);
  base::RunLoop run_loop;
  int snapshot_updated = 0;
  [side_swipe_mediator_
      updateActiveTabSnapshot:base::CallbackToBlock(
                                  base::BindLambdaForTesting([&]() {
                                    snapshot_updated++;
                                    run_loop.Quit();
                                  }))];

  // Move the clock past the update snapshot timeout delay.
  task_environment_.FastForwardBy(base::Seconds(1));
  run_loop.Run();
  EXPECT_EQ(1, snapshot_updated);
}

}  // anonymous namespace
