// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"

#import <QuartzCore/QuartzCore.h>

#import <algorithm>
#import <memory>

#import "base/check_op.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "build/blink_buildflags.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_gesture_recognizer.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_view.h"
#import "ios/chrome/browser/voice/ui_bundled/voice_search_notification_names.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/public/provider/chrome/browser/fullscreen/fullscreen_api.h"
#import "ios/web/common/features.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

namespace {
// This enum is used to record the overscroll actions performed by the user on
// the histogram named `OverscrollActions`.
enum {
  // Records each time the user selects the new tab action.
  OVERSCROLL_ACTION_NEW_TAB,
  // Records each time the user selects the refresh action.
  OVERSCROLL_ACTION_REFRESH,
  // Records each time the user selects the close tab action.
  OVERSCROLL_ACTION_CLOSE_TAB,
  // Records each time the user cancels the overscroll action.
  OVERSCROLL_ACTION_CANCELED,
  // NOTE: Add new actions in sources only immediately above this line.
  // Also, make sure the enum list for histogram `OverscrollActions` in
  // tools/histogram/histograms.xml is updated with any change in here.
  OVERSCROLL_ACTION_COUNT
};

// The histogram used to record user actions.
const char kOverscrollActionsHistogram[] = "Tab.PullDownGesture";

// The pulling threshold in point at which the controller will start accepting
// actions.
// Past this pulling value the scrollView will start to resist from pulling.
const CGFloat kHeaderMaxExpansionThreshold = 56.0;
// The default overall distance in point to select different actions
// horizontally.
const CGFloat kHorizontalPanDistance = 400.0;
// The distance from the top content offset which will be used to detect
// if the scrollview is scrolled to top.
const CGFloat kScrolledToTopToleranceInPoint = 50;
// The minimum duration between scrolling in order to allow overscroll actions.
constexpr base::TimeDelta kMinimumDurationBetweenScrolling =
    base::Milliseconds(150);
// The minimum duration that the pull must last in order to trigger an action.
constexpr base::TimeDelta kMinimumPullDurationToTriggerAction =
    base::Milliseconds(200);
// Bounce dynamic constants.
// Since the bounce effect of the scrollview is cancelled by setting the
// contentInsets to the value of the overscroll contentOffset, the bounce
// bounce back have to be emulated manually using a spring simulation.
const CGFloat kSpringTightness = 4;
const CGFloat kSpringDampiness = 0.35;

// Investigation into crbug.com/1102494 shows that the most likely issue is
// that there are many many instances of OverscrollActionsController live at
// once. This tracks how many live instances there are.
static int gInstanceCount = 0;

// This holds the current state of the bounce back animation.
typedef struct {
  CGFloat yInset;
  CGFloat headerInset;
  CGFloat velocityInset;
  CGFloat initialTopMargin;
  CFAbsoluteTime time;
} SpringInsetState;

// Used to set the height of a view frame.
// Implicit animations are disabled when setting the new frame.
void SetViewFrameHeight(UIView* view, CGFloat height, CGFloat topMargin) {
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  CGRect viewFrame = view.frame;
  viewFrame.size.height = height - topMargin;
  viewFrame.origin.y = topMargin;
  view.frame = viewFrame;
  [CATransaction commit];
}

// Clamp a value between min and max.
CGFloat Clamp(CGFloat value, CGFloat min, CGFloat max) {
  DCHECK(min < max);
  if (value < min)
    return min;
  if (value > max)
    return max;
  return value;
}

// Returns `scrollView`.contentInset with an updated `topInset`.
UIEdgeInsets TopContentInset(UIScrollView* scrollView, CGFloat topInset) {
  UIEdgeInsets insets = scrollView.contentInset;
  insets.top = topInset;
  return insets;
}

}  // namespace

// This protocol describes the subset of methods used between the
// CRWWebViewScrollViewProxy and the UIWebView.
@protocol OverscrollActionsScrollView<NSObject>

@property(nonatomic, assign) UIEdgeInsets contentInset;
@property(nonatomic, assign) CGPoint contentOffset;
@property(nonatomic, assign) UIEdgeInsets scrollIndicatorInsets;
@property(nonatomic, readonly) UIPanGestureRecognizer* panGestureRecognizer;
@property(nonatomic, readonly) BOOL isZooming;

- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated;
- (void)addGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer;
- (void)removeGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer;

@end

@interface OverscrollActionsController ()<CRWWebViewScrollViewProxyObserver,
                                          UIGestureRecognizerDelegate,
                                          OverscrollActionsViewDelegate> {
  // Display link used to animate the bounce back effect.
  CADisplayLink* _dpLink;
  SpringInsetState _bounceState;
  NSInteger _overscrollActionLock;
  // The last time the user started scrolling the view.
  base::TimeTicks _lastScrollBeginTime;
  // Set to YES when the bounce animation must be independent of the scrollview
  // contentOffset change.
  // This is done when an action has been triggered. In that case the webview's
  // scrollview will change state depending on the action being triggered so
  // relying on the contentInset is not possible at that time.
  BOOL _performingScrollViewIndependentAnimation;
  // Force processing state changes in scrollviewDidScroll: even if
  // overscroll actions are disabled.
  // This is used to always process contentOffset changes on specific cases like
  // when playing the bounce back animation if no actions has been triggered.
  BOOL _forceStateUpdate;
  // Instructs the controller to ignore the scroll event resulting from setting
  // `disablingFullscreen` to YES.
  BOOL _ignoreScrollForDisabledFullscreen;
  // True when the overscroll actions are disabled for loading.
  BOOL _isOverscrollActionsDisabledForLoading;
  // True when the pull gesture started close enough from the top and the
  // delegate allows it.
  // Use isOverscrollActionEnabled to take into account locking.
  BOOL _allowPullingActions;
  // Records if a transition to the overscroll state ACTION_READY was made.
  // This is used to record a cancel gesture.
  BOOL _didTransitionToActionReady;
  // Records that the controller will be dismissed at the end of the current
  // animation. No new action should be started.
  BOOL _shouldInvalidate;
  // Store the set of notifications that did increment the overscroll actions
  // lock. It is used in order to enforce the fact that the lock should only be
  // incremented/decremented once for a given notification.
  NSMutableSet* _lockIncrementNotifications;
  // Store the notification name counterpart of another notification name.
  // Overscroll actions locking and unlocking works by listening to balanced
  // notifications. One notification lock and it's counterpart unlock. This
  // dictionary is used to retrieve the notification name from it's notification
  // counterpart name. Example:
  // UIKeyboardWillShowNotification trigger a lock. Its counterpart notification
  // name is UIKeyboardWillHideNotification.
  NSDictionary* _lockNotificationsCounterparts;
  // A view used to catch touches on the webview.
  UIView* _dummyView;
  // The proxy used to interact with the webview.
  id<CRWWebViewProxy> _webViewProxy;
  // The proxy used to interact with the webview's scrollview.
  CRWWebViewScrollViewProxy* _webViewScrollViewProxy;
  // The scrollview driving the OverscrollActionsController when not using
  // the scrollview from the WebState.
  UIScrollView* _scrollview;
  // The disabler that prevents fullscreen calculations from occurring while
  // overscroll actions are being recognized.
  std::unique_ptr<ScopedFullscreenDisabler> _fullscreenDisabler;
}

// The view displayed over the header view holding the actions.
@property(nonatomic, strong) OverscrollActionsView* overscrollActionView;
// Initial top inset added to the scrollview for the header.
// This property is set from the delegate headerInset and cached on first
// call. The cached value is reset when the webview proxy is set.
@property(nonatomic, readonly) CGFloat initialContentInset;
// Initial content offset for the scroll view. This is used to determine how
// far the view scrolled and where to return the content offset to when
// bouncing
@property(nonatomic, readonly) CGFloat initialContentOffset;
// Initial top inset for the header.
// This property is set from the delegate headerInset and cached on first
// call. The cached value is reset when the webview proxy is set.
@property(nonatomic, readonly) CGFloat initialHeaderInset;
// Initial height of the header view.
// This property is set everytime the user starts pulling.
@property(nonatomic, readonly) CGFloat initialHeaderHeight;
// Redefined to be read-write.
@property(nonatomic, assign, readwrite) OverscrollState overscrollState;
// Point where the horizontal gesture started when the state of the
// overscroll controller is in OverscrollStateActionReady.
@property(nonatomic, assign) CGPoint panPointScreenOrigin;
// Pan gesture recognizer used to track horizontal touches.
@property(nonatomic, strong) UIPanGestureRecognizer* panGestureRecognizer;
// Whether the scroll view is dragged by the user.
@property(nonatomic, assign) BOOL scrollViewDragged;
// Whether the scroll view's viewport is being adjusted by the content inset.
@property(nonatomic, readonly) BOOL viewportAdjustsContentInset;
// Whether fullscreen is disabled.
@property(nonatomic, assign, getter=isDisablingFullscreen)
    BOOL disablingFullscreen;

// Registers notifications to lock the overscroll actions on certain UI states.
- (void)registerNotifications;
// Setup/tearDown methods are used to register values when the delegate is set.
- (void)tearDown;
- (void)setup;
// Resets scroll view's top content inset to `self.initialContentInset`.
- (void)resetScrollViewTopContentInset;
// Locking/unlocking methods used to disable/enable the overscroll actions
// with a reference count.
- (void)incrementOverscrollActionLockForNotification:
    (NSNotification*)notification;
- (void)decrementOverscrollActionLockForNotification:
    (NSNotification*)notification;
// Indicates whether the overscroll action is allowed.
- (BOOL)isOverscrollActionEnabled;
// Triggers a call to delegate if an action has been triggered.
- (void)triggerActionIfNeeded;
// Performs work based on overscroll action state changes.
- (void)onOverscrollStateChangeWithPreviousState:
    (OverscrollState)previousOverscrollState;
// Disables all interactions on the webview except pan.
- (void)setWebViewInteractionEnabled:(BOOL)enabled;
// Bounce dynamic animations methods.
// Starts the bounce animation with an initial velocity.
- (void)startBounceWithInitialVelocity:(CGPoint)velocity;
// Stops bounce animation.
- (void)stopBounce;
// Called from the display link to update the bounce dynamic animation.
- (void)updateBounce;
// Applies bounce state to the scroll view.
- (void)applyBounceState;

- (instancetype)initWithScrollView:(UIScrollView*)scrollView
                      webViewProxy:(id<CRWWebViewProxy>)webViewProxy
    NS_DESIGNATED_INITIALIZER;

@end

@implementation OverscrollActionsController

@synthesize overscrollActionView = _overscrollActionView;
@synthesize initialHeaderHeight = _initialHeaderHeight;
@synthesize overscrollState = _overscrollState;
@synthesize delegate = _delegate;
@synthesize panPointScreenOrigin = _panPointScreenOrigin;
@synthesize panGestureRecognizer = _panGestureRecognizer;
@synthesize scrollViewDragged = _scrollViewDragged;

- (instancetype)initWithScrollView:(UIScrollView*)scrollView
                      webViewProxy:(id<CRWWebViewProxy>)webViewProxy {
  DCHECK_NE(!!scrollView, !!webViewProxy)
      << "exactly one of scrollView and webViewProxy must be non-nil";

  if ((self = [super init])) {
    gInstanceCount++;
    _overscrollActionView =
        [[OverscrollActionsView alloc] initWithFrame:CGRectZero];
    _overscrollActionView.delegate = self;
    if (scrollView) {
      _scrollview = scrollView;
    } else {
      _webViewProxy = webViewProxy;
      _webViewScrollViewProxy = [_webViewProxy scrollViewProxy];
      [_webViewScrollViewProxy addObserver:self];
    }

    _lockIncrementNotifications = [[NSMutableSet alloc] init];
    _lockNotificationsCounterparts = @{
      UIKeyboardWillHideNotification : UIKeyboardWillShowNotification,
      kVoiceSearchWillHideNotification : kVoiceSearchWillShowNotification,
      kSideSwipeDidStopNotification : kSideSwipeWillStartNotification

    };
    [self registerNotifications];

    if (_webViewProxy) {
      // -enableOverscrollAction calls -setup, so it must not be called again
      // if _webViewProxy is non-nil
      [self enableOverscrollActions];
    } else {
      [self setup];
    }
  }
  return self;
}

- (instancetype)initWithWebViewProxy:(id<CRWWebViewProxy>)webViewProxy {
  return [self initWithScrollView:nil webViewProxy:webViewProxy];
}

- (instancetype)initWithScrollView:(UIScrollView*)scrollView {
  return [self initWithScrollView:scrollView webViewProxy:nil];
}

- (void)dealloc {
  self.overscrollActionView.delegate = nil;
  [self invalidate];
  gInstanceCount--;
}

+ (int)instanceCount {
  return gInstanceCount;
}

- (void)scheduleInvalidate {
  if (self.overscrollState == OverscrollState::NO_PULL_STARTED) {
    [self invalidate];
  } else {
    _shouldInvalidate = YES;
  }
}

- (void)invalidate {
  [self clear];
  [self stopBounce];
  [self tearDown];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self setWebViewInteractionEnabled:YES];
  _delegate = nil;
  _webViewProxy = nil;
  [_webViewScrollViewProxy removeObserver:self];
  _webViewScrollViewProxy = nil;
}

- (void)clear {
  self.scrollViewDragged = NO;
  self.overscrollState = OverscrollState::NO_PULL_STARTED;
}

- (void)enableOverscrollActions {
  _isOverscrollActionsDisabledForLoading = NO;
  [self setup];
}

- (void)disableOverscrollActions {
  _isOverscrollActionsDisabledForLoading = YES;
  [self tearDown];
}

- (void)setStyle:(OverscrollStyle)style {
  self.overscrollActionView.style = style;
}

#pragma mark - webViewScrollView and UIScrollView delegates implementations

- (void)scrollViewDidScroll {
  if (!_forceStateUpdate && (![self isOverscrollActionEnabled] ||
                             _performingScrollViewIndependentAnimation ||
                             _ignoreScrollForDisabledFullscreen)) {
    return;
  }

  const UIEdgeInsets insets =
      TopContentInset(self.scrollView, -[self scrollView].contentOffset.y);
  // Start pulling (on top).
  CGFloat contentOffsetFromTheTop = [self scrollView].contentOffset.y;
  if (!self.viewportAdjustsContentInset) {
    // Content offset is shifted for WKWebView when
    // self.viewportAdjustsContentInset is NO, to workaround bug with
    // UIScollView.contentInset (rdar://23584409).
    contentOffsetFromTheTop -= _webViewProxy.contentInset.top;
  }
  CGFloat contentOffsetFromExpandedHeader =
      contentOffsetFromTheTop + self.initialHeaderInset;
  CGFloat topMargin = 0;
  if (!_webViewProxy)
    topMargin = self.scrollView.safeAreaInsets.top;
  if (contentOffsetFromExpandedHeader >= 0) {
    // Record initial content offset and dispatch delegate on state change.
    self.overscrollState = OverscrollState::NO_PULL_STARTED;
  } else {
    if (contentOffsetFromExpandedHeader < -kHeaderMaxExpansionThreshold) {
      self.overscrollState = OverscrollState::ACTION_READY;
    } else {
      // Set the contentInset to remove the bounce that would fight with drag.
      [self setScrollViewContentInset:insets];
      _initialHeaderHeight =
          [[self delegate] headerHeightForOverscrollActionsController:self];
      self.overscrollState = OverscrollState::STARTED_PULLING;
    }
    [self updateWithVerticalOffset:-contentOffsetFromExpandedHeader
                         topMargin:topMargin];
  }
}

- (void)scrollViewWillBeginDragging {
  self.scrollViewDragged = YES;
  [self stopBounce];
  _allowPullingActions = NO;
  _didTransitionToActionReady = NO;
  [self.overscrollActionView pullStarted];
  if (!_performingScrollViewIndependentAnimation)
    _allowPullingActions = [self isOverscrollActionsAllowed];
  _lastScrollBeginTime = base::TimeTicks::Now();
}

- (void)forceAnimatedScrollRefresh {
  _forceStateUpdate = YES;
  [self scrollViewWillBeginDragging];
  const CGFloat animatedScrollHeight = kHeaderMaxExpansionThreshold + 10;
  if (self.viewportAdjustsContentInset) {
    [self.scrollView scrollRectToVisible:CGRectMake(0, -animatedScrollHeight, 1,
                                                    animatedScrollHeight)
                                animated:YES];
  } else {
    [self.scrollView setContentOffset:CGPointMake(0, -animatedScrollHeight)
                             animated:YES];
  }
}

- (BOOL)isOverscrollActionsAllowed {
  const BOOL isZooming = [[self scrollView] isZooming];
  // Check that the scrollview is scrolled to top.
  const BOOL isScrolledToTop = fabs([[self scrollView] contentOffset].y +
                                    [[self scrollView] contentInset].top) <=
                               kScrolledToTopToleranceInPoint;
  // Check that the user is not quickly scrolling the view repeatedly.
  const BOOL isMinimumTimeBetweenScrollRespected =
      (base::TimeTicks::Now() - _lastScrollBeginTime) >=
      kMinimumDurationBetweenScrolling;
  // Finally check that the delegate allow overscroll actions.
  const BOOL delegateAllowOverscrollActions = [self.delegate
      shouldAllowOverscrollActionsForOverscrollActionsController:self];
  const BOOL isCurrentlyProcessingOverscroll =
      self.overscrollState != OverscrollState::NO_PULL_STARTED;
  const BOOL fullscreenModeDisablesOverscrollActions =
      [_webViewProxy isWebPageInFullscreenMode];
  return isCurrentlyProcessingOverscroll ||
         (isScrolledToTop && isMinimumTimeBetweenScrollRespected &&
          delegateAllowOverscrollActions && !isZooming &&
          !fullscreenModeDisablesOverscrollActions);
}

- (void)scrollViewDidEndDraggingWillDecelerate:(BOOL)decelerate
                                 contentOffset:(CGPoint)contentOffset {
  self.scrollViewDragged = NO;
  // Content is now hidden behind toolbar, make sure that contentInset is
  // restored to initial value.
  // If Overscroll actions are triggered and dismissed quickly, it is
  // possible to be in a state where drag is enough to be in STARTED_PULLING
  // or ACTION_READY state, but with no selectedAction.
  // TODO: This is not quite correct for blink, the contentOffset is always
  // positive while scrolling the main content, resetting the insets causes
  // after scrolling is done seems wrong.
#if !BUILDFLAG(USE_BLINK)
  if (contentOffset.y >= 0 ||
      self.overscrollState == OverscrollState::NO_PULL_STARTED ||
      self.overscrollActionView.selectedAction == OverscrollAction::NONE) {
    [self resetScrollViewTopContentInset];
  }
#endif

  [self triggerActionIfNeeded];
  _allowPullingActions = NO;
}

- (void)scrollViewWillEndDraggingWithVelocity:(CGPoint)velocity
                          targetContentOffset:
                              (inout CGPoint*)targetContentOffset {
  if (![self isOverscrollActionEnabled])
    return;

  if (self.overscrollState != OverscrollState::NO_PULL_STARTED) {
    *targetContentOffset = [[self scrollView] contentOffset];
    [self startBounceWithInitialVelocity:velocity];
  }
}

- (void)webViewScrollViewProxyDidSetScrollView:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self setup];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  DCHECK_EQ(static_cast<id>(scrollView), [self scrollView]);
  [self scrollViewDidScroll];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  DCHECK_EQ(static_cast<id>(scrollView), [self scrollView]);
  [self scrollViewWillBeginDragging];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  DCHECK_EQ(static_cast<id>(scrollView), [self scrollView]);
  [self scrollViewDidEndDraggingWillDecelerate:decelerate
                                 contentOffset:scrollView.contentOffset];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  DCHECK_EQ(static_cast<id>(scrollView), [self scrollView]);
  [self scrollViewWillEndDraggingWithVelocity:velocity
                          targetContentOffset:targetContentOffset];
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  DCHECK_EQ(static_cast<id>(webViewScrollViewProxy), [self scrollView]);
  [self scrollViewDidScroll];
}

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  DCHECK_EQ(static_cast<id>(webViewScrollViewProxy), [self scrollView]);
  [self scrollViewWillBeginDragging];
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  DCHECK_EQ(static_cast<id>(webViewScrollViewProxy), [self scrollView]);
  [self scrollViewDidEndDraggingWillDecelerate:decelerate
                                 contentOffset:webViewScrollViewProxy
                                                   .contentOffset];
}

- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset {
  DCHECK_EQ(static_cast<id>(webViewScrollViewProxy), [self scrollView]);
  [self scrollViewWillEndDraggingWithVelocity:velocity
                          targetContentOffset:targetContentOffset];
}

- (void)webViewScrollViewDidEndScrollingAnimation:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  CHECK_EQ(static_cast<id>(webViewScrollViewProxy), [self scrollView]);
  [self scrollViewDidEndDraggingWillDecelerate:YES
                                 contentOffset:webViewScrollViewProxy
                                                   .contentOffset];
}

#pragma mark - Pan gesture recognizer handling

- (void)panGesture:(UIPanGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateCancelled) {
    [self setWebViewInteractionEnabled:YES];
  }
  if (self.overscrollState == OverscrollState::NO_PULL_STARTED) {
    return;
  }

  if (gesture.state == UIGestureRecognizerStateBegan) {
    [self setWebViewInteractionEnabled:NO];
  }

  const CGPoint panPointScreen = [gesture locationInView:nil];
  if (self.overscrollState == OverscrollState::ACTION_READY) {
    const CGFloat direction = UseRTLLayout() ? -1 : 1;
    const CGFloat xOffset = direction *
                            (panPointScreen.x - self.panPointScreenOrigin.x) /
                            kHorizontalPanDistance;

    [self.overscrollActionView updateWithHorizontalOffset:xOffset];
  }
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

#pragma mark - Private

- (void)handleAction:(OverscrollAction)action {
  // The action index holds the current triggered action which are numbered left
  // to right.
  switch (action) {
    case OverscrollAction::NEW_TAB:
      base::RecordAction(base::UserMetricsAction("MobilePullGestureNewTab"));
      [self.delegate overscrollActionNewTab:self];
      break;
    case OverscrollAction::CLOSE_TAB:
      base::RecordAction(base::UserMetricsAction("MobilePullGestureCloseTab"));
      [self.delegate overscrollActionCloseTab:self];
      break;
    case OverscrollAction::REFRESH:
      base::RecordAction(base::UserMetricsAction("MobilePullGestureReload"));
      [self.delegate overscrollActionRefresh:self];
      break;
    case OverscrollAction::NONE:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

- (BOOL)viewportAdjustsContentInset {
  if (_webViewProxy.shouldUseViewContentInset)
    return YES;
  return ios::provider::IsFullscreenSmoothScrollingSupported();
}

- (void)recordMetricForTriggeredAction:(OverscrollAction)action {
  switch (action) {
    case OverscrollAction::NONE:
      UMA_HISTOGRAM_ENUMERATION(kOverscrollActionsHistogram,
                                OVERSCROLL_ACTION_CANCELED,
                                OVERSCROLL_ACTION_COUNT);
      break;
    case OverscrollAction::NEW_TAB:
      UMA_HISTOGRAM_ENUMERATION(kOverscrollActionsHistogram,
                                OVERSCROLL_ACTION_NEW_TAB,
                                OVERSCROLL_ACTION_COUNT);
      break;
    case OverscrollAction::REFRESH:
      UMA_HISTOGRAM_ENUMERATION(kOverscrollActionsHistogram,
                                OVERSCROLL_ACTION_REFRESH,
                                OVERSCROLL_ACTION_COUNT);
      break;
    case OverscrollAction::CLOSE_TAB:
      UMA_HISTOGRAM_ENUMERATION(kOverscrollActionsHistogram,
                                OVERSCROLL_ACTION_CLOSE_TAB,
                                OVERSCROLL_ACTION_COUNT);
      break;
  }
}

- (void)registerNotifications {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  for (NSString* counterpartNotificationName in _lockNotificationsCounterparts
           .allKeys) {
    [center addObserver:self
               selector:@selector(incrementOverscrollActionLockForNotification:)
                   name:[_lockNotificationsCounterparts
                            objectForKey:counterpartNotificationName]
                 object:nil];
    [center addObserver:self
               selector:@selector(decrementOverscrollActionLockForNotification:)
                   name:counterpartNotificationName
                 object:nil];
  }
  [center addObserver:self
             selector:@selector(deviceOrientationDidChange)
                 name:UIDeviceOrientationDidChangeNotification
               object:nil];
}

- (void)tearDown {
  [[self scrollView] removeGestureRecognizer:self.panGestureRecognizer];
  self.panGestureRecognizer = nil;
}

- (void)setup {
  UIPanGestureRecognizer* panGesture;
  // Workaround a bug occurring when Speak Selection is enabled.
  // See crbug.com/699655.
  if (UIAccessibilityIsSpeakSelectionEnabled()) {
    panGesture = [[OverscrollActionsGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(panGesture:)];
  } else {
    panGesture =
        [[UIPanGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(panGesture:)];
  }
  [panGesture setMaximumNumberOfTouches:1];
  [panGesture setDelegate:self];
  [[self scrollView] addGestureRecognizer:panGesture];
  self.panGestureRecognizer = panGesture;
}

- (id<OverscrollActionsScrollView>)scrollView {
  if (_scrollview) {
    return static_cast<id<OverscrollActionsScrollView>>(_scrollview);
  } else {
    return static_cast<id<OverscrollActionsScrollView>>(
        _webViewScrollViewProxy);
  }
}

- (void)setScrollViewContentInset:(UIEdgeInsets)contentInset {
  if (_scrollview)
    [_scrollview setContentInset:contentInset];
  else
    [_webViewScrollViewProxy setContentInset:contentInset];
}

- (void)resetScrollViewTopContentInset {
  const UIEdgeInsets insets =
      TopContentInset(self.scrollView, self.initialContentInset);
  [self setScrollViewContentInset:insets];
}

- (void)incrementOverscrollActionLockForNotification:(NSNotification*)notif {
  if (![_lockIncrementNotifications containsObject:notif.name]) {
    [_lockIncrementNotifications addObject:notif.name];
    ++_overscrollActionLock;
  }
}

- (void)decrementOverscrollActionLockForNotification:(NSNotification*)notif {
  NSString* counterpartName =
      [_lockNotificationsCounterparts objectForKey:notif.name];
  if ([_lockIncrementNotifications containsObject:counterpartName]) {
    [_lockIncrementNotifications removeObject:counterpartName];
    if (_overscrollActionLock > 0)
      --_overscrollActionLock;
  }
}

- (void)deviceOrientationDidChange {
  if (self.overscrollState == OverscrollState::NO_PULL_STARTED &&
      !_performingScrollViewIndependentAnimation)
    return;

  const UIDeviceOrientation deviceOrientation =
      [[UIDevice currentDevice] orientation];
  if (deviceOrientation != UIDeviceOrientationLandscapeRight &&
      deviceOrientation != UIDeviceOrientationLandscapeLeft &&
      deviceOrientation != UIDeviceOrientationPortrait) {
    return;
  }

  // If the orientation change happen while the user is still scrolling the
  // scrollview, we need to reset the pan gesture recognizer.
  // Not doing so would result in a graphic issue where the scrollview jumps
  // when scrolling after a change in UI orientation.
  [[self scrollView] panGestureRecognizer].enabled = NO;
  [[self scrollView] panGestureRecognizer].enabled = YES;

  [self setScrollViewContentInset:TopContentInset(self.scrollView,
                                                  self.initialContentInset)];
  [self clear];
}

- (BOOL)isOverscrollActionEnabled {
  return _overscrollActionLock == 0 && _allowPullingActions &&
         !_isOverscrollActionsDisabledForLoading;
}

- (void)triggerActionIfNeeded {
  if ([self isOverscrollActionEnabled]) {
    const BOOL isOverscrollStateActionReady =
        self.overscrollState == OverscrollState::ACTION_READY;
    const OverscrollAction selectedAction =
        self.overscrollActionView.selectedAction;
    const BOOL isOverscrollActionNone =
        selectedAction == OverscrollAction::NONE;

    if ((!isOverscrollStateActionReady && _didTransitionToActionReady) ||
        (isOverscrollStateActionReady && isOverscrollActionNone)) {
      [self recordMetricForTriggeredAction:OverscrollAction::NONE];
    } else if (isOverscrollStateActionReady && !isOverscrollActionNone) {
      if ((base::TimeTicks::Now() - _lastScrollBeginTime) >=
          kMinimumPullDurationToTriggerAction) {
        _performingScrollViewIndependentAnimation = YES;
        __weak __typeof(self) weakSelf = self;
        [UIView animateWithDuration:kMaterialDuration1
                         animations:^{
                           [weakSelf setScrollViewContentInset:
                                         TopContentInset(
                                             weakSelf.scrollView,
                                             weakSelf.initialContentInset)];
                           CGPoint contentOffset =
                               weakSelf.scrollView.contentOffset;
                           contentOffset.y = -self.initialContentInset;
                           self.scrollView.contentOffset = contentOffset;
                         }];
        [self.overscrollActionView displayActionAnimation];
        dispatch_async(dispatch_get_main_queue(), ^{
          [self recordMetricForTriggeredAction:selectedAction];
          TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleMedium);
          [self handleAction:selectedAction];
        });
      }
    }
  }
}

- (void)setOverscrollState:(OverscrollState)overscrollState {
  if (_overscrollState != overscrollState) {
    const OverscrollState previousState = _overscrollState;
    _overscrollState = overscrollState;
    [self onOverscrollStateChangeWithPreviousState:previousState];
  }
}

- (void)onOverscrollStateChangeWithPreviousState:
    (OverscrollState)previousOverscrollState {
  __weak OverscrollActionsController* weakSelf = self;
  [UIView animateWithDuration:0.2
                   animations:^{
                     [weakSelf
                         animateOverscrollStateChange:previousOverscrollState];
                   }
                   completion:nil];
}

// Helper to animate onOverscrollStateChangeWithPreviousState
- (void)animateOverscrollStateChange:(OverscrollState)previousOverscrollState {
  switch (self.overscrollState) {
    case OverscrollState::NO_PULL_STARTED: {
      [self.overscrollActionView removeFromSuperview];
      CGRect statusBarFrame =
          [self scrollView].window.windowScene.statusBarManager.statusBarFrame;

      SetViewFrameHeight(self.overscrollActionView,
                         self.initialContentInset + statusBarFrame.size.height,
                         0);
      self.panPointScreenOrigin = CGPointZero;
      [self resetScrollViewTopContentInset];
      self.disablingFullscreen = NO;
      if (_shouldInvalidate) {
        [self invalidate];
      }
    } break;
    case OverscrollState::STARTED_PULLING: {
      if (!self.overscrollActionView.superview && self.scrollViewDragged) {
        UIView* headerView =
            [self.delegate headerViewForOverscrollActionsController:self];
        DCHECK(headerView);
        if (previousOverscrollState == OverscrollState::NO_PULL_STARTED) {
          UIView* view = [self.delegate
              toolbarSnapshotViewForOverscrollActionsController:self];
          if (view) {
            // The NTP does not grab a snapshot
            [self.overscrollActionView addSnapshotView:view];
          }
          self.disablingFullscreen = YES;
        }
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        self.overscrollActionView.backgroundView.alpha = 1;
        [self.overscrollActionView updateWithVerticalOffset:0];
        [self.overscrollActionView updateWithHorizontalOffset:0];
        self.overscrollActionView.frame = headerView.bounds;
        [headerView addSubview:self.overscrollActionView];
        [CATransaction commit];
      }
    } break;
    case OverscrollState::ACTION_READY: {
      _didTransitionToActionReady = YES;
      if (CGPointEqualToPoint(self.panPointScreenOrigin, CGPointZero)) {
        CGPoint panPointScreen = [self.panGestureRecognizer locationInView:nil];
        self.panPointScreenOrigin = panPointScreen;
      }
    } break;
  }
}

- (void)setWebViewInteractionEnabled:(BOOL)enabled {
  // All interactions are disabled except pan.
  for (UIGestureRecognizer* gesture in [_webViewProxy gestureRecognizers]) {
    [gesture setEnabled:enabled];
  }
  for (UIGestureRecognizer* gesture in
       [_webViewScrollViewProxy gestureRecognizers]) {
    if (![gesture isKindOfClass:[UIPanGestureRecognizer class]]) {
      [gesture setEnabled:enabled];
    }
  }
  // Add a dummy view on top of the webview in order to catch touches on some
  // specific subviews.
  if (!enabled) {
    if (!_dummyView)
      _dummyView = [[UIView alloc] init];
    [_dummyView setFrame:[_webViewProxy bounds]];
    [_webViewProxy addSubview:_dummyView];
  } else {
    [_dummyView removeFromSuperview];
  }
}

- (void)updateWithVerticalOffset:(CGFloat)verticalOffset
                       topMargin:(CGFloat)topMargin {
  self.overscrollActionView.backgroundView.alpha =
      1.0 -
      Clamp((verticalOffset) / (kHeaderMaxExpansionThreshold / 2.0), 0.0, 1.0);
  SetViewFrameHeight(self.overscrollActionView,
                     self.initialHeaderHeight + verticalOffset, topMargin);
  [self.overscrollActionView updateWithVerticalOffset:verticalOffset];
}

- (CGFloat)initialContentInset {
  // Content inset is not used for displaying header if the web view's
  // `self.viewportAdjustsContentInset` is NO, instead the whole web view frame
  // is changed.
  if (!_scrollview && !self.viewportAdjustsContentInset)
    return 0;
  return self.initialHeaderInset;
}

- (CGFloat)initialContentOffset {
  return
      [self.delegate initialContentOffsetForOverscrollActionsController:self];
}

- (CGFloat)initialHeaderInset {
  return [self.delegate headerInsetForOverscrollActionsController:self];
}

- (BOOL)isDisablingFullscreen {
  return _fullscreenDisabler.get() != nullptr;
}

- (void)setDisablingFullscreen:(BOOL)disablingFullscreen {
  if (self.disablingFullscreen == disablingFullscreen)
    return;

  _fullscreenDisabler = nullptr;
  if (!disablingFullscreen)
    return;

  // Ask the delegate for a fullscreen controller. It may return nothing if
  // (for example) the UI is in the middle of teardown.
  FullscreenController* fullscreenController =
      [self.delegate fullscreenControllerForOverscrollActionsController:self];
  if (!fullscreenController)
    return;

  // Disabling fullscreen will show the toolbars, which may potentially produce
  // a `-scrollViewDidScroll` event if the browser viewport insets need to be
  // updated.  `_ignoreScrollForDisabledFullscreen` is set to YES while the
  // viewport insets are being updated for the disabled state so that this
  // scroll event can be ignored.
  _ignoreScrollForDisabledFullscreen = YES;
  _fullscreenDisabler =
      std::make_unique<ScopedFullscreenDisabler>(fullscreenController);
  _ignoreScrollForDisabledFullscreen = NO;
}

#pragma mark - Bounce dynamic

- (void)startBounceWithInitialVelocity:(CGPoint)velocity {
  if (_shouldInvalidate) {
    return;
  }
  [self stopBounce];
  CADisplayLink* dpLink =
      [CADisplayLink displayLinkWithTarget:self
                                  selector:@selector(updateBounce)];
  _dpLink = dpLink;
  memset(&_bounceState, 0, sizeof(_bounceState));
  if (self.overscrollState == OverscrollState::ACTION_READY) {
    CGFloat distanceScrolled =
        [self scrollView].contentOffset.y - self.initialContentOffset;
    const UIEdgeInsets insets = TopContentInset(
        self.scrollView, -distanceScrolled + self.initialContentInset);
    [self setScrollViewContentInset:insets];
  }
  _bounceState.headerInset = self.initialContentInset;
  _bounceState.yInset =
      [self scrollView].contentInset.top - _bounceState.headerInset;
  _bounceState.initialTopMargin = self.overscrollActionView.frame.origin.y;
  _bounceState.time = CACurrentMediaTime();
  _bounceState.velocityInset = -velocity.y * 1000.0;

  if (fabs(_bounceState.yInset) < 0.5) {
    // If no bounce is required, then clear state, as the necessary
    // `-scrollViewDidScroll` callback will not be triggered to reset
    // `overscrollState` to NO_PULL_STARTED.
    [self stopBounce];
    [self clear];
  } else {
    [dpLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
  }
}

- (void)stopBounce {
  [_dpLink invalidate];
  _dpLink = nil;
  if (_performingScrollViewIndependentAnimation) {
    self.overscrollState = OverscrollState::NO_PULL_STARTED;
    _performingScrollViewIndependentAnimation = NO;
  }
}

- (void)updateBounce {
  const double time = CACurrentMediaTime();
  const double dt = time - _bounceState.time;
  CGFloat force = -_bounceState.yInset * kSpringTightness;
  if (_bounceState.yInset > 0) {
    force -= _bounceState.velocityInset * kSpringDampiness;
  }
  _bounceState.velocityInset += force;
  _bounceState.yInset += _bounceState.velocityInset * dt;
  _bounceState.time = time;
  [self applyBounceState];
  if (fabs(_bounceState.yInset) < 0.5) {
    [self stopBounce];
  }
}

- (void)applyBounceState {
  if (_bounceState.yInset < 0.5) {
    _bounceState.yInset = 0;
  }
  if (_performingScrollViewIndependentAnimation) {
    [self updateWithVerticalOffset:_bounceState.yInset
                         topMargin:_bounceState.initialTopMargin];
  } else {
    const UIEdgeInsets insets = TopContentInset(
        self.scrollView, _bounceState.yInset + _bounceState.headerInset);
    _forceStateUpdate = YES;
    [self setScrollViewContentInset:insets];
    _forceStateUpdate = NO;
  }
}

#pragma mark - OverscrollActionsViewDelegate

- (void)overscrollActionsViewDidTapTriggerAction:
    (OverscrollActionsView*)overscrollActionsView {
  if (_shouldInvalidate) {
    return;
  }
  [self.overscrollActionView displayActionAnimation];
  [self
      recordMetricForTriggeredAction:self.overscrollActionView.selectedAction];

  // Reset all pan gesture recognizers.
  _allowPullingActions = NO;
  _panGestureRecognizer.enabled = NO;
  _panGestureRecognizer.enabled = YES;
  [self scrollView].panGestureRecognizer.enabled = NO;
  [self scrollView].panGestureRecognizer.enabled = YES;
  [self startBounceWithInitialVelocity:CGPointZero];

  TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleMedium);
  [self handleAction:self.overscrollActionView.selectedAction];
}

- (void)overscrollActionsView:(OverscrollActionsView*)view
      selectedActionDidChange:(OverscrollAction)newAction {
  TriggerHapticFeedbackForSelectionChange();
}

@end
