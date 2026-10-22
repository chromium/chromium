// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_constants.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Test pan gesture recognizer subclass that stores translation without
// requiring an active touch sequence.
@interface FullscreenTestPanGestureRecognizer : UIPanGestureRecognizer
@end

@implementation FullscreenTestPanGestureRecognizer {
  CGPoint _translationOverride;
}
- (CGPoint)translationInView:(UIView*)view {
  return _translationOverride;
}
- (void)setTranslation:(CGPoint)translation inView:(UIView*)view {
  _translationOverride = translation;
}
@end

// Test scroll view subclass allowing gesture state properties to be configured.
@interface FullscreenTestScrollView : UIScrollView
@property(nonatomic, assign) BOOL isDraggingOverride;
@property(nonatomic, assign) BOOL isTrackingOverride;
@property(nonatomic, assign) BOOL isDeceleratingOverride;
@end

@implementation FullscreenTestScrollView {
  FullscreenTestPanGestureRecognizer* _testPanGestureRecognizer;
}
- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    _testPanGestureRecognizer =
        [[FullscreenTestPanGestureRecognizer alloc] init];
  }
  return self;
}
- (UIPanGestureRecognizer*)panGestureRecognizer {
  return _testPanGestureRecognizer;
}
- (BOOL)isDragging {
  return _isDraggingOverride;
}
- (BOOL)isTracking {
  return _isTrackingOverride;
}
- (BOOL)isDecelerating {
  return _isDeceleratingOverride;
}
@end

namespace {

// Observer to track calls to WillUpdateObscuredInsetRange.
class FullscreenMediatorTestObserver : public FullscreenBrowserAgentObserver {
 public:
  void WillUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override {
    agent->AddObscuredInsetRange(UIRectEdgeTop, 0, 50);
    will_update_obscured_inset_range_count_++;
  }

  int will_update_obscured_inset_range_count() const {
    return will_update_obscured_inset_range_count_;
  }

 private:
  int will_update_obscured_inset_range_count_ = 0;
};

}  // namespace

// Test fixture for testing FullscreenMediator class.
class FullscreenMediatorTest : public PlatformTest {
 protected:
  FullscreenMediatorTest() {
    scene_state_ = [[SceneState alloc] init];

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    FullscreenBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = FullscreenBrowserAgent::FromBrowser(browser_.get());
    agent_->AddObserver(&observer_);
    agent_->InvalidateInsetRange();
    mediator_ = [[FullscreenMediator alloc]
        initWithBrowserAgent:agent_
                webStateList:browser_->GetWebStateList()
          browserLayoutState:browser_->GetBrowserLayoutState()];
  }

  void TearDown() override {
    agent_->RemoveObserver(&observer_);
    [mediator_ disconnect];
  }

  struct ScrollViewSetup {
    FullscreenTestScrollView* scroll_view;
    CRWWebViewScrollViewProxy* scroll_view_proxy;
  };

  // Sets up an active WebState with a scroll view proxy and returns the
  // test UIScrollView and CRWWebViewScrollViewProxy.
  ScrollViewSetup SetUpActiveWebStateWithScrollView() {
    auto web_state = std::make_unique<web::FakeWebState>();
    WebViewProxyTabHelper::CreateForWebState(web_state.get());

    FullscreenTestScrollView* scroll_view = [[FullscreenTestScrollView alloc]
        initWithFrame:CGRectMake(0, 0, 320, 480)];
    scroll_view.contentSize = CGSizeMake(320, 2000);
    scroll_view.contentOffset = CGPointMake(0, 100);
    scroll_view.isDraggingOverride = YES;

    CRWWebViewScrollViewProxy* scroll_view_proxy =
        [[CRWWebViewScrollViewProxy alloc] init];
    [scroll_view_proxy setScrollView:scroll_view];

    id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    OCMStub([web_view_proxy_mock scrollViewProxy]).andReturn(scroll_view_proxy);

    WebViewProxyTabHelper::FromWebState(web_state.get())
        ->SetOverridingWebViewProxy(web_view_proxy_mock);

    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());

    return {scroll_view, scroll_view_proxy};
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SceneState* scene_state_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<FullscreenBrowserAgent> agent_;
  FullscreenMediatorTestObserver observer_;
  FullscreenMediator* mediator_;
};

// Tests that the mediator can be disconnected.
TEST_F(FullscreenMediatorTest, Disconnect) {
  [mediator_ disconnect];
}

// Tests that the mediator starts observing the scroll view proxy when the
// active web state changes.
TEST_F(FullscreenMediatorTest, ObservesScrollViewProxy) {
  auto web_state = std::make_unique<web::FakeWebState>();
  WebViewProxyTabHelper::CreateForWebState(web_state.get());

  id scroll_view_proxy_mock = OCMClassMock([CRWWebViewScrollViewProxy class]);
  id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
  OCMStub([web_view_proxy_mock scrollViewProxy])
      .andReturn(scroll_view_proxy_mock);

  WebViewProxyTabHelper::FromWebState(web_state.get())
      ->SetOverridingWebViewProxy(web_view_proxy_mock);

  OCMExpect([scroll_view_proxy_mock
      addObserver:static_cast<id<CRWWebViewScrollViewProxyObserver>>(
                      mediator_)]);

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  [scroll_view_proxy_mock verify];
  [mediator_ disconnect];
}

// Tests that webStateWasShown invalidates the inset range only once on startup.
TEST_F(FullscreenMediatorTest, WebStateWasShownInvalidatesInsetsOnce) {
  auto web_state = std::make_unique<web::FakeWebState>();
  WebViewProxyTabHelper::CreateForWebState(web_state.get());
  web::FakeWebState* web_state_ptr = web_state.get();

  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  int initial_count = observer_.will_update_obscured_inset_range_count();

  // First WebState shown should invalidate.
  id<CRWWebStateObserver> web_state_observer =
      static_cast<id<CRWWebStateObserver>>(mediator_);
  [web_state_observer webStateWasShown:web_state_ptr];
  EXPECT_EQ(observer_.will_update_obscured_inset_range_count(),
            initial_count + 1);

  // Subsequent calls should NOT invalidate.
  [web_state_observer webStateWasShown:web_state_ptr];
  EXPECT_EQ(observer_.will_update_obscured_inset_range_count(),
            initial_count + 1);
}

// Tests that scrolling down past the threshold triggers EnterFullscreen.
TEST_F(FullscreenMediatorTest, EnterFullscreenWhenThresholdHit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kFullscreenRefactoring, kFullscreenEasedTransitions}, {});

  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();

  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);
  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];

  // Scroll down past threshold (kEnterFullscreenProgressThreshold).
  scroll_view.contentOffset = CGPointMake(
      0, scroll_view.contentOffset.y + kEasedTransitionScrollDistance * 0.5);
  [observer webViewScrollViewDidScroll:scroll_view_proxy];

  EXPECT_TRUE(agent_->is_animating());
  EXPECT_EQ(agent_->top_progress(), 0.0);
}

// Tests that scrolling below the threshold updates progress without triggering
// an animated fullscreen transition.
TEST_F(FullscreenMediatorTest, IncrementalScrollBelowThreshold) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kFullscreenRefactoring, kFullscreenEasedTransitions}, {});

  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();

  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);
  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];

  // Scroll down 10pt, which is below the threshold.
  constexpr CGFloat kScrollDistance = 10.0;
  scroll_view.contentOffset =
      CGPointMake(0, scroll_view.contentOffset.y + kScrollDistance);
  [observer webViewScrollViewDidScroll:scroll_view_proxy];

  EXPECT_FALSE(agent_->is_animating());
  EXPECT_EQ(agent_->top_progress(),
            1.0 - (kScrollDistance / kEasedTransitionScrollDistance));
}

// Tests that scrolling up past the threshold triggers ExitFullscreen.
TEST_F(FullscreenMediatorTest, ExitFullscreenWhenThresholdHit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kFullscreenRefactoring, kFullscreenEasedTransitions}, {});

  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();

  // Transition to collapsed first.
  [mediator_
      enterFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                        animated:NO];
  EXPECT_EQ(agent_->top_progress(), 0.0);
  EXPECT_EQ(agent_->settled_state(), FullscreenState::kUICollapsed);

  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);
  scroll_view.contentOffset = CGPointMake(0, 300);
  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];

  // Scroll up past threshold (kExitFullscreenProgressThreshold).
  scroll_view.contentOffset = CGPointMake(
      0, scroll_view.contentOffset.y - kEasedTransitionScrollDistance * 0.5);
  [observer webViewScrollViewDidScroll:scroll_view_proxy];

  EXPECT_TRUE(agent_->is_animating());
  EXPECT_EQ(agent_->top_progress(), 1.0);
}

// Tests that scrolling to the bottom records the scroll to bottom time metric.
TEST_F(FullscreenMediatorTest, ScrollToTheBottomTime) {
  base::HistogramTester histogram_tester;
  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();
  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);

  // Start dragging.
  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];

  // Advance time by 3 seconds.
  task_environment_.FastForwardBy(base::Seconds(3));

  // Scroll to bottom (content height 2000 - scroll view height 480 = 1520).
  scroll_view.contentOffset = CGPointMake(0, 1520);
  [observer webViewScrollViewDidScroll:scroll_view_proxy];

  // End dragging without decelerating.
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:NO];

  histogram_tester.ExpectTotalCount(kFullscreenScrollToTheBottomTime, 1);
  histogram_tester.ExpectTimeBucketCount(kFullscreenScrollToTheBottomTime,
                                         base::Seconds(3), 1);

  // Subsequent drags at the bottom should not record duplicate metrics.
  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];
  task_environment_.FastForwardBy(base::Seconds(2));
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:NO];
  histogram_tester.ExpectTotalCount(kFullscreenScrollToTheBottomTime, 1);
}

// Tests that scroll to bottom time metric is recorded after deceleration ends.
TEST_F(FullscreenMediatorTest, ScrollToTheBottomTimeDecelerating) {
  base::HistogramTester histogram_tester;
  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();
  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);

  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];
  task_environment_.FastForwardBy(base::Seconds(2));

  scroll_view.contentOffset = CGPointMake(0, 1520);
  [observer webViewScrollViewDidScroll:scroll_view_proxy];

  // End dragging with deceleration pending.
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:YES];
  histogram_tester.ExpectTotalCount(kFullscreenScrollToTheBottomTime, 0);

  task_environment_.FastForwardBy(base::Seconds(1));
  [observer webViewScrollViewDidEndDecelerating:scroll_view_proxy];

  histogram_tester.ExpectTotalCount(kFullscreenScrollToTheBottomTime, 1);
  histogram_tester.ExpectTimeBucketCount(kFullscreenScrollToTheBottomTime,
                                         base::Seconds(3), 1);
}

// Tests that navigation resets the scroll to bottom time recording state.
TEST_F(FullscreenMediatorTest, ScrollToTheBottomTimeResetsOnNavigation) {
  base::HistogramTester histogram_tester;
  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();
  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);

  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];
  task_environment_.FastForwardBy(base::Seconds(3));
  scroll_view.contentOffset = CGPointMake(0, 1520);
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:NO];
  histogram_tester.ExpectTotalCount(kFullscreenScrollToTheBottomTime, 1);

  // Same-document navigation should not reset recording state.
  id<CRWWebStateObserver> web_state_observer =
      static_cast<id<CRWWebStateObserver>>(mediator_);
  web::FakeNavigationContext same_doc_context;
  same_doc_context.SetIsSameDocument(true);
  [web_state_observer webState:browser_->GetWebStateList()->GetActiveWebState()
           didFinishNavigation:&same_doc_context];

  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];
  task_environment_.FastForwardBy(base::Seconds(2));
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:NO];
  histogram_tester.ExpectTotalCount(kFullscreenScrollToTheBottomTime, 1);

  // New page navigation should reset recording state.
  web::FakeNavigationContext new_page_context;
  new_page_context.SetIsSameDocument(false);
  [web_state_observer webState:browser_->GetWebStateList()->GetActiveWebState()
           didFinishNavigation:&new_page_context];

  scroll_view.contentOffset = CGPointMake(0, 100);
  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];
  task_environment_.FastForwardBy(base::Seconds(5));
  scroll_view.contentOffset = CGPointMake(0, 1520);
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:NO];

  histogram_tester.ExpectTotalCount(kFullscreenScrollToTheBottomTime, 2);
  histogram_tester.ExpectTimeBucketCount(kFullscreenScrollToTheBottomTime,
                                         base::Seconds(5), 1);
}

// Tests that scroll to bottom time metric is not recorded if content cannot
// collapse toolbar.
TEST_F(FullscreenMediatorTest,
       ScrollToTheBottomTimeNotRecordedIfCannotCollapse) {
  base::HistogramTester histogram_tester;
  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();
  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);

  // Set content size smaller than scroll view height.
  scroll_view.contentSize = CGSizeMake(320, 400);

  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];
  task_environment_.FastForwardBy(base::Seconds(3));
  scroll_view.contentOffset = CGPointMake(0, 0);
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:NO];

  histogram_tester.ExpectTotalCount(kFullscreenScrollToTheBottomTime, 0);
}

// Test that the browser remains in fullscreen after the first scroll to bottom.
TEST_F(FullscreenMediatorTest, StaysFullscreenOnFirstScrollToBottom) {
  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();
  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);

  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];
  scroll_view.contentOffset = CGPointMake(0, 1520);
  [observer webViewScrollViewDidScroll:scroll_view_proxy];
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:NO];

  EXPECT_EQ(agent_->State(), FullscreenState::kUICollapsed);
  EXPECT_EQ(agent_->top_progress(), 0.0);
}

// Test that starting a new scroll while already scrolled to the bottom in
// fullscreen exits fullscreen when dragging ends at the bottom.
TEST_F(FullscreenMediatorTest, ExitsFullscreenOnSecondScrollToBottom) {
  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();
  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);

  // First scroll to bottom enters fullscreen.
  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];
  scroll_view.contentOffset = CGPointMake(0, 1520);
  [observer webViewScrollViewDidScroll:scroll_view_proxy];
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:NO];
  EXPECT_EQ(agent_->State(), FullscreenState::kUICollapsed);

  // Second drag starting at the bottom and ending at the bottom exits
  // fullscreen.
  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];
  scroll_view.contentOffset = CGPointMake(0, 1550);
  [observer webViewScrollViewDidScroll:scroll_view_proxy];
  [observer webViewScrollViewDidEndDragging:scroll_view_proxy
                             willDecelerate:NO];

  EXPECT_EQ(agent_->State(), FullscreenState::kUIExpanded);
  EXPECT_EQ(agent_->top_progress(), 1.0);
}

// Tests that programmatic `contentOffset` adjustments (during `contentSize`
// updates, active tracking, or momentum deceleration) are ignored and that
// opposing adjustments during tracking or deceleration restore the previous
// `contentOffset`.
TEST_F(FullscreenMediatorTest, IgnoresAndRestoresProgrammaticScrolls) {
  auto [scroll_view, scroll_view_proxy] = SetUpActiveWebStateWithScrollView();
  scroll_view.isTrackingOverride = YES;
  id<CRWWebViewScrollViewProxyObserver> observer =
      static_cast<id<CRWWebViewScrollViewProxyObserver>>(mediator_);

  [observer webViewScrollViewWillBeginDragging:scroll_view_proxy];

  // Scroll accompanied by a `contentSize.height` change is ignored.
  [scroll_view.panGestureRecognizer setTranslation:CGPointMake(0, -10.0)
                                            inView:scroll_view];
  scroll_view.contentSize = CGSizeMake(scroll_view.contentSize.width, 2100.0);
  scroll_view.contentOffset = CGPointMake(0, 110.0);
  EXPECT_EQ(agent_->top_progress(), 1.0);

  // User drags finger up (`panTranslation.y` decreases), scrolling content down
  // (`delta` > 0).
  constexpr CGFloat kTrackedOffsetY = 125.0;
  [scroll_view.panGestureRecognizer setTranslation:CGPointMake(0, -25.0)
                                            inView:scroll_view];
  scroll_view.contentOffset = CGPointMake(0, kTrackedOffsetY);
  CGFloat progress_after_user_scroll = agent_->top_progress();
  EXPECT_LT(progress_after_user_scroll, 1.0);

  // Programmatic scroll snapping `contentOffset` backward without pan movement
  // is ignored and restored.
  scroll_view.contentOffset = CGPointMake(0, 112.0);
  EXPECT_EQ(agent_->top_progress(), progress_after_user_scroll);
  EXPECT_EQ(scroll_view.contentOffset.y, kTrackedOffsetY);

  // Finger lifts into downward deceleration; momentum scroll proceeds, while an
  // opposing programmatic adjustment is ignored and restored.
  CGPoint target_offset = CGPointMake(0, 300.0);
  [observer webViewScrollViewWillEndDragging:scroll_view_proxy
                                withVelocity:CGPointMake(0, 1.5)
                         targetContentOffset:&target_offset];
  scroll_view.isTrackingOverride = NO;
  scroll_view.isDeceleratingOverride = YES;

  constexpr CGFloat kDecelerationOffsetY = 135.0;
  scroll_view.contentOffset = CGPointMake(0, kDecelerationOffsetY);
  CGFloat progress_after_deceleration = agent_->top_progress();
  EXPECT_LT(progress_after_deceleration, progress_after_user_scroll);

  scroll_view.contentOffset = CGPointMake(0, 120.0);
  EXPECT_EQ(agent_->top_progress(), progress_after_deceleration);
  EXPECT_EQ(scroll_view.contentOffset.y, kDecelerationOffsetY);
}
