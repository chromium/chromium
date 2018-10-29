// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"

#include <memory>

#include "base/feature_list.h"
#import "base/ios/block_types.h"
#include "base/scoped_observer.h"
#include "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_side_swipe_provider.h"
#import "ios/chrome/browser/ui/side_swipe/card_side_swipe_view.h"
#import "ios/chrome/browser/ui/side_swipe/history_side_swipe_provider.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_navigation_view.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_util.h"
#import "ios/chrome/browser/ui/side_swipe_gesture_recognizer.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_highlighting.h"
#include "ios/chrome/browser/ui/toolbar/public/side_swipe_toolbar_interacting.h"
#import "ios/chrome/browser/ui/toolbar/public/side_swipe_toolbar_interacting.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSideSwipeWillStartNotification =
    @"kSideSwipeWillStartNotification";
NSString* const kSideSwipeDidStopNotification =
    @"kSideSwipeDidStopNotification";

namespace {

enum class SwipeType { NONE, CHANGE_TAB, CHANGE_PAGE };

// Swipe starting distance from edge.
const CGFloat kSwipeEdge = 20;

// Distance between sections of iPad side swipe.
const CGFloat kIpadTabSwipeDistance = 100;

// Number of tabs to keep in the grey image cache.
const NSUInteger kIpadGreySwipeTabCount = 8;
}

@interface SideSwipeController ()<CRWWebStateObserver,
                                  TabModelObserver,
                                  UIGestureRecognizerDelegate> {
 @private

  __weak TabModel* model_;

  // Side swipe view for tab navigation.
  CardSideSwipeView* tabSideSwipeView_;

  // Side swipe view for page navigation.
  SideSwipeNavigationView* pageSideSwipeView_;

  // YES if the user is currently swiping.
  BOOL inSwipe_;

  // Swipe gesture recognizer.
  SideSwipeGestureRecognizer* swipeGestureRecognizer_;

  SideSwipeGestureRecognizer* panGestureRecognizer_;

  // Used in iPad side swipe gesture, tracks the starting tab index.
  NSUInteger startingTabIndex_;

  // If the swipe is for a page change or a tab change.
  SwipeType swipeType_;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> webStateObserverBridge_;

  // Scoped observer used to track registration of the WebStateObserverBridge.
  std::unique_ptr<ScopedObserver<web::WebState, web::WebStateObserver>>
      scopedWebStateObserver_;

  // Curtain over web view while waiting for it to load.
  UIView* curtain_;

  // Provides forward/back action for history entries.
  HistorySideSwipeProvider* historySideSwipeProvider_;

  // Provides forward action for reading list.
  ReadingListSideSwipeProvider* readingListSideSwipeProvider_;

  __weak id<SideSwipeContentProvider> currentContentProvider_;

  // The disabler that prevents the toolbar from being scrolled away when the
  // side swipe gesture is being recognized.
  std::unique_ptr<ScopedFullscreenDisabler> fullscreenDisabler_;

  // The animated disabler displays the toolbar when a side swipe navigation
  // gesture is being recognized.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> animatedFullscreenDisabler_;

  // Browser state passed to the initialiser.
  ios::ChromeBrowserState* browserState_;
}

// Load grey snapshots for the next |kIpadGreySwipeTabCount| tabs in
// |direction|.
- (void)createGreyCache:(UISwipeGestureRecognizerDirection)direction;
// Tell snapshot cache to clear grey cache.
- (void)deleteGreyCache;
// Handle tab side swipe for iPad.  Change tabs according to swipe distance.
- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture;
// Handle tab side swipe for iPhone. Introduces a CardSideSwipeView to convey
// the tab change.
- (void)handleiPhoneTabSwipe:(SideSwipeGestureRecognizer*)gesture;
// Overlays |curtain_| as a white view to hide the web view while it updates.
// Calls |completionHandler| when the curtain is removed.
- (void)addCurtainWithCompletionHandler:(ProceduralBlock)completionHandler;
// Removes the |curtain_| and calls |completionHandler| when the curtain is
// removed.
- (void)dismissCurtainWithCompletionHandler:(ProceduralBlock)completionHandler;
@end

@implementation SideSwipeController

@synthesize inSwipe = inSwipe_;
@synthesize swipeDelegate = swipeDelegate_;
@synthesize toolbarInteractionHandler = _toolbarInteractionHandler;
@synthesize primaryToolbarSnapshotProvider = _primaryToolbarSnapshotProvider;
@synthesize secondaryToolbarSnapshotProvider =
    _secondaryToolbarSnapshotProvider;
@synthesize snapshotDelegate = snapshotDelegate_;
@synthesize tabStripDelegate = tabStripDelegate_;

- (id)initWithTabModel:(TabModel*)model
          browserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(model);
  self = [super init];
  if (self) {
    model_ = model;
    [model_ addObserver:self];
    historySideSwipeProvider_ =
        [[HistorySideSwipeProvider alloc] initWithTabModel:model_];

    readingListSideSwipeProvider_ = [[ReadingListSideSwipeProvider alloc]
        initWithReadingList:ReadingListModelFactory::GetForBrowserState(
                                browserState)];

    webStateObserverBridge_ =
        std::make_unique<web::WebStateObserverBridge>(self);
    scopedWebStateObserver_ =
        std::make_unique<ScopedObserver<web::WebState, web::WebStateObserver>>(
            webStateObserverBridge_.get());

    browserState_ = browserState;
  }
  return self;
}

- (void)dealloc {
  [model_ removeObserver:self];

  scopedWebStateObserver_.reset();
  webStateObserverBridge_.reset();
}

- (void)addHorizontalGesturesToView:(UIView*)view {
  swipeGestureRecognizer_ = [[SideSwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleSwipe:)];
  [swipeGestureRecognizer_ setMaximumNumberOfTouches:1];
  [swipeGestureRecognizer_ setDelegate:self];
  [swipeGestureRecognizer_ setSwipeEdge:kSwipeEdge];
  [view addGestureRecognizer:swipeGestureRecognizer_];

  // Add a second gesture recognizer to handle swiping on the toolbar to change
  // tabs.
  panGestureRecognizer_ =
      [[SideSwipeGestureRecognizer alloc] initWithTarget:self
                                                  action:@selector(handlePan:)];
  [panGestureRecognizer_ setMaximumNumberOfTouches:1];
  [panGestureRecognizer_ setSwipeThreshold:48];
  [panGestureRecognizer_ setDelegate:self];
  [view addGestureRecognizer:panGestureRecognizer_];
}

- (NSSet*)swipeRecognizers {
  return [NSSet setWithObjects:swipeGestureRecognizer_, nil];
}

- (void)setEnabled:(BOOL)enabled {
  [swipeGestureRecognizer_ setEnabled:enabled];
}

- (BOOL)shouldAutorotate {
  return !([tabSideSwipeView_ window] || inSwipe_);
}

// Always return yes, as this swipe should work with various recognizers,
// including UITextTapRecognizer, UILongPressGestureRecognizer,
// UIScrollViewPanGestureRecognizer and others.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldBeRequiredToFailByGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Only take precedence over a pan gesture recognizer so that moving up and
  // down while swiping doesn't trigger overscroll actions.
  if ([otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]]) {
    return YES;
  }
  return NO;
}

// Gestures should only be recognized within |contentArea_| or the toolbar.
- (BOOL)gestureRecognizerShouldBegin:(SideSwipeGestureRecognizer*)gesture {
  if (inSwipe_) {
    return NO;
  }

  if ([swipeDelegate_ preventSideSwipe])
    return NO;

  CGPoint location = [gesture locationInView:gesture.view];

  // Since the toolbar and the contentView can overlap, check the toolbar frame
  // first, and confirm the right gesture recognizer is firing.
  if ([self.toolbarInteractionHandler
          isInsideToolbar:[gesture.view convertPoint:location toView:nil]]) {
    if (![gesture isEqual:panGestureRecognizer_]) {
      return NO;
    }

    return [swipeDelegate_ canBeginToolbarSwipe];
  }

  // Otherwise, only allow contentView touches with |swipeGestureRecognizer_|.
  // The content view frame is inset by -1 because CGRectContainsPoint does
  // include points on the max X and Y edges, which will happen frequently with
  // edge swipes from the right side.
  CGRect contentViewFrame =
      CGRectInset([[swipeDelegate_ sideSwipeContentView] frame], -1, -1);
  if (CGRectContainsPoint(contentViewFrame, location)) {
    if (![gesture isEqual:swipeGestureRecognizer_]) {
      return NO;
    }
    swipeType_ = SwipeType::CHANGE_PAGE;
    return YES;
  }
  return NO;
}

- (void)createGreyCache:(UISwipeGestureRecognizerDirection)direction {
  NSInteger dx = (direction == UISwipeGestureRecognizerDirectionLeft) ? -1 : 1;
  NSInteger index = startingTabIndex_ + dx;
  NSMutableArray* sessionIDs =
      [NSMutableArray arrayWithCapacity:kIpadGreySwipeTabCount];
  for (NSUInteger count = 0; count < kIpadGreySwipeTabCount; count++) {
    // Wrap around edges.
    if (index >= (NSInteger)[model_ count])
      index = 0;
    else if (index < 0)
      index = [model_ count] - 1;

    // Don't wrap past the starting index.
    if (index == (NSInteger)startingTabIndex_)
      break;

    Tab* tab = [model_ tabAtIndex:index];
    if (tab && PagePlaceholderTabHelper::FromWebState(tab.webState)
                   ->will_add_placeholder_for_next_navigation()) {
      [sessionIDs addObject:tab.tabId];
    }
    index = index + dx;
  }
  [SnapshotCacheFactory::GetForBrowserState(browserState_)
      createGreyCache:sessionIDs];
}

- (void)deleteGreyCache {
  [SnapshotCacheFactory::GetForBrowserState(browserState_) removeGreyCache];
}

- (void)handlePan:(SideSwipeGestureRecognizer*)gesture {
  if (!IsIPadIdiom()) {
    return [self handleiPhoneTabSwipe:gesture];
  } else {
    return [self handleiPadTabSwipe:gesture];
  }
}

- (void)handleSwipe:(SideSwipeGestureRecognizer*)gesture {
  DCHECK(swipeType_ != SwipeType::NONE);
  if (swipeType_ == SwipeType::CHANGE_TAB) {
    if (!IsIPadIdiom()) {
      return [self handleiPhoneTabSwipe:gesture];
    } else {
      return [self handleiPadTabSwipe:gesture];
    }
  }
  if (swipeType_ == SwipeType::CHANGE_PAGE) {
    return [self handleSwipeToNavigate:gesture];
  }
  NOTREACHED();
}

- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  // Don't handle swipe when there are no tabs.
  NSInteger count = [model_ count];
  if (count == 0)
    return;

  if (gesture.state == UIGestureRecognizerStateBegan) {
    // Disable fullscreen while the side swipe gesture is occurring.
    fullscreenDisabler_ = std::make_unique<ScopedFullscreenDisabler>(
        FullscreenControllerFactory::GetInstance()->GetForBrowserState(
            browserState_));
    SnapshotTabHelper::FromWebState([model_ currentTab].webState)
        ->UpdateSnapshot(/*with_overlays=*/true, /*visible_frame_only=*/true);
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kSideSwipeWillStartNotification
                      object:nil];
    [self.tabStripDelegate setHighlightsSelectedTab:YES];
    startingTabIndex_ = [model_ indexOfTab:[model_ currentTab]];
    [self createGreyCache:gesture.direction];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    // Side swipe for iPad involves changing the selected tab as the swipe moves
    // across the width of the view.  The screen is broken up into
    // |kIpadTabSwipeDistance| / |width| segments, with the current tab in the
    // first section.  The swipe does not wrap edges.
    CGFloat distance = [gesture locationInView:gesture.view].x;
    if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
      distance = gesture.startPoint.x - distance;
    } else {
      distance -= gesture.startPoint.x;
    }

    NSInteger indexDelta = std::floor(distance / kIpadTabSwipeDistance);
    // Don't wrap past the first tab.
    if (indexDelta < count) {
      // Flip delta when swiping forward.
      if (IsSwipingForward(gesture.direction))
        indexDelta = 0 - indexDelta;

      Tab* currentTab = [model_ currentTab];
      NSInteger currentIndex = [model_ indexOfTab:currentTab];

      // Wrap around edges.
      NSInteger newIndex = (NSInteger)(startingTabIndex_ + indexDelta) % count;

      // C99 defines the modulo result as negative if our offset is negative.
      if (newIndex < 0)
        newIndex += count;

      if (newIndex != currentIndex) {
        Tab* tab = [model_ tabAtIndex:newIndex];
        // Toggle overlay preview mode for selected tab.
        PagePlaceholderTabHelper::FromWebState(tab.webState)
            ->AddPlaceholderForNextNavigation();
        [model_ setCurrentTab:tab];

        // And disable overlay preview mode for last selected tab.
        PagePlaceholderTabHelper::FromWebState(currentTab.webState)
            ->CancelPlaceholderForNextNavigation();
      }
    }
  } else {
    if (gesture.state == UIGestureRecognizerStateCancelled) {
      Tab* tab = [model_ tabAtIndex:startingTabIndex_];
      PagePlaceholderTabHelper::FromWebState(tab.webState)
          ->CancelPlaceholderForNextNavigation();
      [model_ setCurrentTab:tab];
    }
    PagePlaceholderTabHelper::FromWebState([model_ currentTab].webState)
        ->CancelPlaceholderForNextNavigation();

    // Redisplay the view if it was in overlay preview mode.
    [swipeDelegate_ sideSwipeRedisplayTab:[model_ currentTab]];
    [self.tabStripDelegate setHighlightsSelectedTab:NO];
    [self deleteGreyCache];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kSideSwipeDidStopNotification
                      object:nil];

    // Stop disabling fullscreen.
    fullscreenDisabler_ = nullptr;
  }
}

- (id<SideSwipeContentProvider>)contentProviderForGesture:(BOOL)goBack {
  if (goBack && [historySideSwipeProvider_ canGoBack]) {
    return historySideSwipeProvider_;
  }
  if (!goBack && [historySideSwipeProvider_ canGoForward]) {
    return historySideSwipeProvider_;
  }
  if (goBack && [readingListSideSwipeProvider_ canGoBack]) {
    return readingListSideSwipeProvider_;
  }
  if (!goBack && [readingListSideSwipeProvider_ canGoForward]) {
    return readingListSideSwipeProvider_;
  }
  return nil;
}

// Show swipe to navigate.
- (void)handleSwipeToNavigate:(SideSwipeGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    // Make sure the Toolbar is visible by disabling Fullscreen.
    animatedFullscreenDisabler_ =
        std::make_unique<AnimatedScopedFullscreenDisabler>(
            FullscreenControllerFactory::GetInstance()->GetForBrowserState(
                browserState_));
    animatedFullscreenDisabler_->StartAnimation();

    inSwipe_ = YES;
    [swipeDelegate_ updateAccessoryViewsForSideSwipeWithVisibility:NO];
    BOOL goBack = IsSwipingBack(gesture.direction);

    currentContentProvider_ = [self contentProviderForGesture:goBack];
    BOOL canNavigate = currentContentProvider_ != nil;

    CGRect gestureBounds = gesture.view.bounds;
    CGFloat headerHeight = [swipeDelegate_ headerHeightForSideSwipe];
    CGRect navigationFrame =
        CGRectMake(CGRectGetMinX(gestureBounds),
                   CGRectGetMinY(gestureBounds) + headerHeight,
                   CGRectGetWidth(gestureBounds),
                   CGRectGetHeight(gestureBounds) - headerHeight);

    pageSideSwipeView_ = [[SideSwipeNavigationView alloc]
        initWithFrame:navigationFrame
        withDirection:gesture.direction
          canNavigate:canNavigate
                image:[currentContentProvider_ paneIcon]
        rotateForward:[currentContentProvider_ rotateForwardIcon]];
    [pageSideSwipeView_ setTargetView:[swipeDelegate_ sideSwipeContentView]];

    [gesture.view insertSubview:pageSideSwipeView_
                   belowSubview:[swipeDelegate_ topToolbarView]];
  } else if (gesture.state == UIGestureRecognizerStateCancelled ||
             gesture.state == UIGestureRecognizerStateEnded ||
             gesture.state == UIGestureRecognizerStateFailed) {
    // Enable fullscreen functionality after the Toolbar has been shown, and
    // the gesture is over.
    animatedFullscreenDisabler_ = nullptr;
  }

  __weak Tab* weakCurrentTab = [model_ currentTab];
  [pageSideSwipeView_ handleHorizontalPan:gesture
      onOverThresholdCompletion:^{
        BOOL wantsBack = IsSwipingBack(gesture.direction);
        web::WebState* webState = [weakCurrentTab webState];
        if (wantsBack) {
          [currentContentProvider_ goBack:webState];
        } else {
          [currentContentProvider_ goForward:webState];
        }

        // Checking -IsLoading() is likely incorrect, but to narrow the scope of
        // fixes for slim navigation manager, only ignore this state when
        // slim is disabled.  With slim navigation enabled, this false when
        // pages can be served from WKWebView's page cache.
        if (webState &&
            (web::GetWebClient()->IsSlimNavigationManagerEnabled() ||
             webState->IsLoading())) {
          scopedWebStateObserver_->RemoveAll();
          scopedWebStateObserver_->Add(webState);
          [self addCurtainWithCompletionHandler:^{
            inSwipe_ = NO;
          }];
        } else {
          inSwipe_ = NO;
        }
        [swipeDelegate_ updateAccessoryViewsForSideSwipeWithVisibility:YES];
      }
      onUnderThresholdCompletion:^{
        [swipeDelegate_ updateAccessoryViewsForSideSwipeWithVisibility:YES];
        inSwipe_ = NO;
      }];
}

// Show horizontal swipe stack view for iPhone.
- (void)handleiPhoneTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    Tab* currentTab = [model_ currentTab];
    DCHECK(currentTab.webState);

    inSwipe_ = YES;

    CGRect frame = [[swipeDelegate_ sideSwipeContentView] frame];

    // Add horizontal stack view controller.
    CGFloat headerHeight =
        [self.snapshotDelegate
            snapshotEdgeInsetsForWebState:currentTab.webState]
            .top;

    if (tabSideSwipeView_) {
      [tabSideSwipeView_ setFrame:frame];
      [tabSideSwipeView_ setTopMargin:headerHeight];
    } else {
      tabSideSwipeView_ = [[CardSideSwipeView alloc] initWithFrame:frame
                                                         topMargin:headerHeight
                                                             model:model_];
      tabSideSwipeView_.topToolbarSnapshotProvider =
          self.primaryToolbarSnapshotProvider;
      tabSideSwipeView_.bottomToolbarSnapshotProvider =
          self.secondaryToolbarSnapshotProvider;

      [tabSideSwipeView_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                             UIViewAutoresizingFlexibleHeight];
      [tabSideSwipeView_ setDelegate:swipeDelegate_];
      [tabSideSwipeView_ setBackgroundColor:[UIColor blackColor]];
    }

    // Ensure that there's an up-to-date snapshot of the current tab.
    SnapshotTabHelper::FromWebState(currentTab.webState)
        ->UpdateSnapshot(/*with_overlays=*/true, /*visible_frame_only=*/true);

    // Hide the infobar after snapshot has been updated (see the previous line)
    // to avoid it obscuring the cards in the side swipe view.
    [swipeDelegate_ updateAccessoryViewsForSideSwipeWithVisibility:NO];

    // Layout tabs with new snapshots in the current orientation.
    [tabSideSwipeView_ updateViewsForDirection:gesture.direction];

    // Insert behind infobar container (which is below toolbar)
    // so card border doesn't look janky during animation.
    DCHECK([swipeDelegate_ verifyToolbarViewPlacementInView:gesture.view]);
    // Insert above the toolbar.
    [gesture.view addSubview:tabSideSwipeView_];

    // Remove content area so it doesn't receive any pan events.
    [[swipeDelegate_ sideSwipeContentView] removeFromSuperview];
  }

  [tabSideSwipeView_ handleHorizontalPan:gesture];
}

- (void)addCurtainWithCompletionHandler:(ProceduralBlock)completionHandler {
  if (!curtain_) {
    curtain_ = [[UIView alloc]
        initWithFrame:[swipeDelegate_ sideSwipeContentView].bounds];
    [curtain_ setBackgroundColor:[UIColor whiteColor]];
  }
  [[swipeDelegate_ sideSwipeContentView] addSubview:curtain_];

  // Fallback in case load takes a while. 3 seconds is a balance between how
  // long it can take a web view to clear the previous page image, and what
  // feels like to 'too long' to see the curtain.
  [self performSelector:@selector(dismissCurtainWithCompletionHandler:)
             withObject:[completionHandler copy]
             afterDelay:3];
}

- (void)resetContentView {
  CGRect frame = [swipeDelegate_ sideSwipeContentView].frame;
  frame.origin.x = 0;
  [swipeDelegate_ sideSwipeContentView].frame = frame;
}

- (void)dismissCurtainWithCompletionHandler:(ProceduralBlock)completionHandler {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  scopedWebStateObserver_->RemoveAll();
  [curtain_ removeFromSuperview];
  curtain_ = nil;
  completionHandler();
}

#pragma mark - CRWWebStateObserver Methods

// Checking -webStateDidStopLoading is likely incorrect, but to narrow the scope
// of fixes for slim navigation manager, only ignore this callback when slim is
// disabled.
- (void)webStateDidStopLoading:(web::WebState*)webState {
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled())
    return;
  [self dismissCurtainWithCompletionHandler:^{
    inSwipe_ = NO;
  }];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled())
    return;
  [self dismissCurtainWithCompletionHandler:^{
    inSwipe_ = NO;
  }];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  scopedWebStateObserver_->Remove(webState);
}

#pragma mark - TabModelObserver Methods

- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)index {
  // Toggling the gesture's enabled state off and on will effectively cancel
  // the gesture recognizer.
  [swipeGestureRecognizer_ setEnabled:NO];
  [swipeGestureRecognizer_ setEnabled:YES];
}

@end
