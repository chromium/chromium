// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"

#import <WebKit/WebKit.h>

#import "base/functional/callback_helpers.h"
#import "base/i18n/rtl.h"
#import "base/test/bind.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/web_distiller/core/eligibility/reader_mode_eligibility.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_test.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_consumer.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator+Testing.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

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

class SideSwipeMediatorTest : public ReaderModeTest {
 public:
  void SetUp() override {
    ReaderModeTest::SetUp();
    browser_ = std::make_unique<TestBrowser>(profile());

    auto original_web_state = std::make_unique<web::FakeWebState>();
    original_web_state->SetBrowserState(profile());
    browser_->GetWebStateList()->InsertWebState(
        std::move(original_web_state),
        WebStateList::InsertionParams::Automatic().Activate());

    side_swipe_mediator_ = [[SideSwipeMediator alloc]
        initWithWebStateList:browser_->GetWebStateList()];

    fake_swipe_ui_controller_ = [[FakeSideSwipeUIController alloc] init];
    side_swipe_mediator_.consumer = fake_swipe_ui_controller_;
  }

  ~SideSwipeMediatorTest() override { [side_swipe_mediator_ disconnect]; }

  void CloseActiveWebState() {
    browser_->GetWebStateList()->CloseWebStateAt(
        browser_->GetWebStateList()->active_index(),
        WebStateList::ClosingReason::kDefault);
  }

  web::FakeWebState* active_web_state() {
    return static_cast<web::FakeWebState*>(
        browser_->GetWebStateList()->GetActiveWebState());
  }

  std::unique_ptr<Browser> browser_;
  SideSwipeMediator* side_swipe_mediator_;
  FakeSideSwipeUIController* fake_swipe_ui_controller_;
};

TEST_F(SideSwipeMediatorTest, TestConstructor) {
  EXPECT_TRUE(side_swipe_mediator_);
}

// Tests pages that need to use Chromium native swipe.
TEST_F(SideSwipeMediatorTest, TestEdgeNavigationEnabled) {
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetBrowserState(profile());
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

// Tests that pages with Reader Mode enabled will use Chromium native swipe.
TEST_F(SideSwipeMediatorTest, TestEdgeNavigationEnabledForReaderMode) {
  auto fake_web_state = CreateWebState();
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  fake_navigation_manager->SetVisibleItem(item.get());
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

  // Set up Reading Mode for web page.
  GURL test_url("http://wwww.test.test");
  SetReaderModeState(fake_web_state.get(), test_url,
                     web_distiller::ReaderModeEligibility::kReaderModeEligible,
                     "content");
  LoadWebpage(fake_web_state.get(), test_url);
  EnableReaderMode(fake_web_state.get(),
                   ReaderModeAccessPoint::kContextualChip);
  ASSERT_TRUE(
      WaitForAvailableReaderModeContentInWebState(fake_web_state.get()));

  [side_swipe_mediator_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_TRUE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);
}

// Tests that when the active webState is changed or when the active webState
// finishes navigation, the edge state will be updated accordingly.
TEST_F(SideSwipeMediatorTest, ObserversTriggerStateUpdate) {
  ASSERT_FALSE(fake_swipe_ui_controller_.leadingEdgeNavigationEnabled);
  ASSERT_FALSE(fake_swipe_ui_controller_.trailingEdgeNavigationEnabled);

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetBrowserState(profile());
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
  SnapshotTabHelper::CreateForWebState(active_web_state());
  SnapshotSourceTabHelper::CreateForWebState(active_web_state());
  base::RunLoop run_loop;
  int snapshot_updated = 0;
  [side_swipe_mediator_
      updateActiveTabSnapshot:base::CallbackToBlock(
                                  base::BindLambdaForTesting([&]() {
                                    snapshot_updated++;
                                    run_loop.Quit();
                                  }))];

  // Move the clock past the update snapshot timeout delay.
  task_environment()->FastForwardBy(base::Seconds(1));
  run_loop.Run();
  EXPECT_EQ(1, snapshot_updated);
}

// Tests that custom Chromium edge swipes record the expected user actions.
TEST_F(SideSwipeMediatorTest, CustomEdgeSwipeRecordsUserActions) {
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  active_web_state()->SetNavigationManager(std::move(fake_navigation_manager));
  testing::NiceMock<feature_engagement::test::MockTracker> mock_tracker;
  side_swipe_mediator_.engagementTracker = &mock_tracker;

  base::UserActionTester user_action_tester;

  [side_swipe_mediator_ navigateInDirection:NavigationDirectionBack];
  EXPECT_EQ(1, user_action_tester.GetActionCount("Back"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileEdgeSwipeBack"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Forward"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("MobileEdgeSwipeForward"));

  [side_swipe_mediator_ navigateInDirection:NavigationDirectionForward];
  EXPECT_EQ(1, user_action_tester.GetActionCount("Back"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileEdgeSwipeBack"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("Forward"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileEdgeSwipeForward"));
}

// Tests that native WKWebView edge swipes record the expected user actions
// only when an edge swipe gesture started and was not cancelled.
TEST_F(SideSwipeMediatorTest, WebViewEdgeSwipeRecordsUserActions) {
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  active_web_state()->SetNavigationManager(std::move(fake_navigation_manager));

  base::UserActionTester user_action_tester;

  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetPageTransition(ui::PAGE_TRANSITION_FORWARD_BACK);
  context.SetHasUserGesture(false);
  context.SetIsSameDocument(false);

  // Back/forward navigation without an edge swipe (e.g., JS history.back())
  // should not record edge swipe actions.
  active_web_state()->OnNavigationFinished(&context);
  EXPECT_EQ(0, user_action_tester.GetActionCount("Back"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("MobileEdgeSwipeBack"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Forward"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("MobileEdgeSwipeForward"));

  // Back edge swipe followed by committed back/forward navigation.
  [side_swipe_mediator_
      webViewEdgeSwipeDidStartInDirection:NavigationDirectionBack];
  active_web_state()->OnNavigationFinished(&context);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Back"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileEdgeSwipeBack"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Forward"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("MobileEdgeSwipeForward"));

  // Forward edge swipe followed by committed back/forward navigation.
  [side_swipe_mediator_
      webViewEdgeSwipeDidStartInDirection:NavigationDirectionForward];
  active_web_state()->OnNavigationFinished(&context);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Back"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileEdgeSwipeBack"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("Forward"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileEdgeSwipeForward"));

  // Cancelled edge swipe should not record actions on subsequent navigation.
  [side_swipe_mediator_
      webViewEdgeSwipeDidStartInDirection:NavigationDirectionBack];
  [side_swipe_mediator_ webViewEdgeSwipeDidCancel];
  active_web_state()->OnNavigationFinished(&context);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Back"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileEdgeSwipeBack"));
}

}  // anonymous namespace
