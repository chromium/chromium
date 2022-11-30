// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"

#import "base/check.h"
#import "base/ios/block_types.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// How long, in seconds, the bubble is visible on the screen.
const NSTimeInterval kBubbleVisibilityDuration = 5.0;
// How long, in seconds, the long duration bubble is visible on the screen. Ex.
// Follow in-product help(IPH) bubble.
const NSTimeInterval kBubbleVisibilityLongDuration = 8.0;
// How long, in seconds, the user should be considered engaged with the bubble
// after the bubble first becomes visible.
const NSTimeInterval kBubbleEngagementDuration = 30.0;

// Delay before posting the VoiceOver notification.
const CGFloat kVoiceOverAnnouncementDelay = 1;

// Possible types of dismissal reasons.
// These enums are persisted as histogram entries, so this enum should be
// treated as append-only and kept in sync with InProductHelpDismissalReason in
// enums.xml.
enum class IPHDismissalReasonType {
  kUnknown = 0,
  kTimedOut = 1,
  kOnKeyboardHide = 2,
  kTappedIPH = 3,
  kTappedOutside = 4,
  kTappedClose = 5,
  kTappedSnooze = 6,
  kMaxValue = kTappedSnooze,
};

}  // namespace

// Implements BubbleViewDelegate to handle BubbleView's close and snooze buttons
// tap.
@interface BubbleViewControllerPresenter () <UIGestureRecognizerDelegate,
                                             BubbleViewDelegate>

// Redeclared as readwrite so the value can be changed internally.
@property(nonatomic, assign, readwrite, getter=isUserEngaged) BOOL userEngaged;
// The underlying BubbleViewController managed by this object.
// `bubbleViewController` manages the BubbleView instance.
@property(nonatomic, strong) BubbleViewController* bubbleViewController;
// The tap gesture recognizer intercepting tap gestures occurring inside the
// bubble view. Taps inside must be differentiated from taps outside to track
// UMA metrics.
@property(nonatomic, strong) UITapGestureRecognizer* insideBubbleTapRecognizer;
// The tap gesture recognizer intercepting tap gestures occurring outside the
// bubble view. Does not prevent interactions with elements being tapped on.
// For example, tapping on a button both dismisses the bubble and triggers the
// button's action.
@property(nonatomic, strong) UITapGestureRecognizer* outsideBubbleTapRecognizer;
// The swipe gesture recognizer to dismiss the bubble on swipes.
@property(nonatomic, strong) UISwipeGestureRecognizer* swipeRecognizer;
// The timer used to dismiss the bubble after a certain length of time. The
// bubble is dismissed automatically if the user does not dismiss it manually.
// If the user dismisses it manually, this timer is invalidated. The timer
// maintains a strong reference to the presenter, so it must be retained weakly
// to prevent a retain cycle. The run loop retains a strong reference to the
// timer so it is not deallocated until it is invalidated.
@property(nonatomic, weak) NSTimer* bubbleDismissalTimer;
// The timer used to reset the user's engagement. The user is considered
// engaged with the bubble while it is visible and for a certain duration after
// it disappears. The timer maintains a strong reference to the presenter, so it
// must be retained weakly to prevent a retain cycle. The run loop retains a
// strong reference to the timer so it is not deallocated until it is
// invalidated.
@property(nonatomic, weak) NSTimer* engagementTimer;
// The direction the underlying BubbleView's arrow is pointing.
@property(nonatomic, assign) BubbleArrowDirection arrowDirection;
// The alignment of the underlying BubbleView's arrow.
@property(nonatomic, assign) BubbleAlignment alignment;
// The type of the bubble view's content.
@property(nonatomic, assign, readonly) BubbleViewType bubbleType;
// YES if the bubble should present longer.
@property(nonatomic, assign) BOOL isLongDurationBubble;
// Whether the bubble view controller is presented or dismissed.
@property(nonatomic, assign, getter=isPresenting) BOOL presenting;
// The block invoked when the bubble is dismissed (both via timer and via tap).
// Is optional.
@property(nonatomic, strong) ProceduralBlockWithSnoozeAction dismissalCallback;

@end

@implementation BubbleViewControllerPresenter

@synthesize bubbleViewController = _bubbleViewController;
@synthesize insideBubbleTapRecognizer = _insideBubbleTapRecognizer;
@synthesize outsideBubbleTapRecognizer = _outsideBubbleTapRecognizer;
@synthesize swipeRecognizer = _swipeRecognizer;
@synthesize bubbleDismissalTimer = _bubbleDismissalTimer;
@synthesize engagementTimer = _engagementTimer;
@synthesize userEngaged = _userEngaged;
@synthesize triggerFollowUpAction = _triggerFollowUpAction;
@synthesize arrowDirection = _arrowDirection;
@synthesize alignment = _alignment;
@synthesize dismissalCallback = _dismissalCallback;
@synthesize voiceOverAnnouncement = _voiceOverAnnouncement;

- (instancetype)initWithText:(NSString*)text
                       title:(NSString*)titleString
                       image:(UIImage*)image
              arrowDirection:(BubbleArrowDirection)arrowDirection
                   alignment:(BubbleAlignment)alignment
                  bubbleType:(BubbleViewType)type
           dismissalCallback:
               (ProceduralBlockWithSnoozeAction)dismissalCallback {
  self = [super init];
  if (self) {
    _bubbleViewController =
        [[BubbleViewController alloc] initWithText:text
                                             title:titleString
                                             image:image
                                    arrowDirection:arrowDirection
                                         alignment:alignment
                                    bubbleViewType:type
                                          delegate:self];
    _outsideBubbleTapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(tapOutsideBubbleRecognized:)];
    _outsideBubbleTapRecognizer.delegate = self;
    _outsideBubbleTapRecognizer.cancelsTouchesInView = NO;
    _insideBubbleTapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(tapInsideBubbleRecognized:)];
    _insideBubbleTapRecognizer.delegate = self;
    _insideBubbleTapRecognizer.cancelsTouchesInView = NO;
    _swipeRecognizer = [[UISwipeGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(tapOutsideBubbleRecognized:)];
    _swipeRecognizer.direction = UISwipeGestureRecognizerDirectionUp;
    _swipeRecognizer.delegate = self;
    _userEngaged = NO;
    _triggerFollowUpAction = NO;
    _arrowDirection = arrowDirection;
    _alignment = alignment;
    _bubbleType = type;
    _dismissalCallback = dismissalCallback;
    // The timers are initialized when the bubble is presented, not during
    // initialization. Because the user might not present the bubble immediately
    // after initialization, the timers cannot be started until the bubble
    // appears on screen.
  }
  return self;
}

- (instancetype)initDefaultBubbleWithText:(NSString*)text
                           arrowDirection:(BubbleArrowDirection)arrowDirection
                                alignment:(BubbleAlignment)alignment
                     isLongDurationBubble:(BOOL)isLongDurationBubble
                        dismissalCallback:
                            (ProceduralBlockWithSnoozeAction)dismissalCallback {
  self.isLongDurationBubble = isLongDurationBubble;
  return [self initWithText:text
                      title:nil
                      image:nil
             arrowDirection:arrowDirection
                  alignment:alignment
                 bubbleType:BubbleViewTypeDefault
          dismissalCallback:dismissalCallback];
}

- (BOOL)canPresentInView:(UIView*)parentView anchorPoint:(CGPoint)anchorPoint {
  CGPoint anchorPointInParent = [parentView.window convertPoint:anchorPoint
                                                         toView:parentView];
  return !CGRectIsEmpty([self frameForBubbleInRect:parentView.bounds
                                     atAnchorPoint:anchorPointInParent]);
}

- (void)presentInViewController:(UIViewController*)parentViewController
                           view:(UIView*)parentView
                    anchorPoint:(CGPoint)anchorPoint {
  CGPoint anchorPointInParent =
      [parentView.window convertPoint:anchorPoint toView:parentView];
  self.bubbleViewController.view.frame =
      [self frameForBubbleInRect:parentView.bounds
                   atAnchorPoint:anchorPointInParent];
  // The bubble's frame must be set. Call `canPresentInView` to make sure that
  // the frame can be set before calling `presentInViewController`.
  DCHECK(!CGRectIsEmpty(self.bubbleViewController.view.frame));

  self.presenting = YES;
  [parentViewController addChildViewController:self.bubbleViewController];
  [parentView addSubview:self.bubbleViewController.view];
  [self.bubbleViewController
      didMoveToParentViewController:parentViewController];
  [self.bubbleViewController animateContentIn];

  [self.bubbleViewController.view
      addGestureRecognizer:self.insideBubbleTapRecognizer];
  [parentView addGestureRecognizer:self.outsideBubbleTapRecognizer];
  [parentView addGestureRecognizer:self.swipeRecognizer];

  self.bubbleDismissalTimer = [NSTimer
      scheduledTimerWithTimeInterval:self.isLongDurationBubble
                                         ? kBubbleVisibilityLongDuration
                                         : kBubbleVisibilityDuration
                              target:self
                            selector:@selector(bubbleDismissalTimerFired:)
                            userInfo:nil
                             repeats:NO];

  self.userEngaged = YES;
  self.triggerFollowUpAction = YES;
  self.engagementTimer =
      [NSTimer scheduledTimerWithTimeInterval:kBubbleEngagementDuration
                                       target:self
                                     selector:@selector(engagementTimerFired:)
                                     userInfo:nil
                                      repeats:NO];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(onKeyboardHide:)
             name:UIKeyboardWillHideNotification
           object:nil];

  if (self.voiceOverAnnouncement) {
    if (self.bubbleShouldAutoDismissUnderAccessibility) {
      // The VoiceOverAnnouncement should be dispatched after a delay to account
      // for the fact that it can be presented right after a screen change (for
      // example when the application or a new tab is opened). This screen
      // change is changing the VoiceOver focus to focus a newly visible
      // element. If this announcement is currently being read, it is cancelled.
      // The added delay allows the announcement to be posted after the element
      // is focused, so it is not cancelled.
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW,
                        (int64_t)(kVoiceOverAnnouncementDelay * NSEC_PER_SEC)),
          dispatch_get_main_queue(), ^{
            UIAccessibilityPostNotification(
                UIAccessibilityAnnouncementNotification,
                self.voiceOverAnnouncement);
          });
    } else {
      UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                      self.bubbleViewController.view);
    }
  }
}

- (void)dismissAnimated:(BOOL)animated
                 reason:(IPHDismissalReasonType)reason
           snoozeAction:(feature_engagement::Tracker::SnoozeAction)action {
  // Because this object must stay in memory to handle the `userEngaged`
  // property correctly, it is possible for `dismissAnimated` to be called
  // multiple times. However, only the first call should have any effect.
  if (!self.presenting) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("InProductHelp.DismissalReason.iOS", reason);

  [self.bubbleDismissalTimer invalidate];
  self.bubbleDismissalTimer = nil;
  [self.insideBubbleTapRecognizer.view
      removeGestureRecognizer:self.insideBubbleTapRecognizer];
  [self.outsideBubbleTapRecognizer.view
      removeGestureRecognizer:self.outsideBubbleTapRecognizer];
  [self.swipeRecognizer.view removeGestureRecognizer:self.swipeRecognizer];
  [self.bubbleViewController dismissAnimated:animated];
  self.presenting = NO;

  if (self.dismissalCallback) {
    self.dismissalCallback(action);
  }
}

- (void)dismissAnimated:(BOOL)animated {
  [self dismissAnimated:animated reason:IPHDismissalReasonType::kUnknown];
}

- (void)dismissAnimated:(BOOL)animated reason:(IPHDismissalReasonType)reason {
  [self dismissAnimated:animated
                 reason:reason
           snoozeAction:feature_engagement::Tracker::SnoozeAction::DISMISSED];
}

- (void)dealloc {
  [self.bubbleDismissalTimer invalidate];
  self.bubbleDismissalTimer = nil;
  [self.engagementTimer invalidate];
  self.engagementTimer = nil;
  [self.insideBubbleTapRecognizer.view
      removeGestureRecognizer:self.insideBubbleTapRecognizer];
  [self.outsideBubbleTapRecognizer.view
      removeGestureRecognizer:self.outsideBubbleTapRecognizer];
  [self.swipeRecognizer.view removeGestureRecognizer:self.swipeRecognizer];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Allow swipeRecognizer and outsideBubbleTapRecognizer to be triggered at the
  // same time as other gesture recognizers.
  return gestureRecognizer == self.swipeRecognizer ||
         gestureRecognizer == self.outsideBubbleTapRecognizer;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Prevents outside gesture recognizers from triggering when tapping inside
  // the bubble.
  if (gestureRecognizer == self.outsideBubbleTapRecognizer &&
      [touch.view isDescendantOfView:self.bubbleViewController.view]) {
    return NO;
  }
  // If the swipe originated from a button inside the bubble, cancel the touch
  // instead of dismissing the bubble.
  if (gestureRecognizer == self.swipeRecognizer &&
      [touch.view isDescendantOfView:self.bubbleViewController.view] &&
      [touch.view isKindOfClass:[UIButton class]]) {
    return NO;
  }
  // Prevents inside gesture recognizers from triggering when tapping on a
  // button inside of the bubble.
  if (gestureRecognizer == self.insideBubbleTapRecognizer &&
      [touch.view isKindOfClass:[UIButton class]]) {
    return NO;
  }
  return YES;
}

#pragma mark - BubbleViewDelegate

- (void)didTapCloseButton {
  [self dismissAnimated:YES reason:IPHDismissalReasonType::kTappedClose];
}

- (void)didTapSnoozeButton {
  [self dismissAnimated:YES
                 reason:IPHDismissalReasonType::kTappedSnooze
           snoozeAction:feature_engagement::Tracker::SnoozeAction::SNOOZED];
}

#pragma mark - Private

// Invoked by tapping inside the bubble. Dismisses the bubble.
- (void)tapInsideBubbleRecognized:(id)sender {
  [self dismissAnimated:YES reason:IPHDismissalReasonType::kTappedIPH];
}

// Invoked by tapping outside the bubble. Dismisses the bubble.
- (void)tapOutsideBubbleRecognized:(id)sender {
  [self dismissAnimated:YES reason:IPHDismissalReasonType::kTappedOutside];
}

// Automatically dismisses the bubble view when `bubbleDismissalTimer` fires.
- (void)bubbleDismissalTimerFired:(id)sender {
  BOOL usesScreenReader = UIAccessibilityIsVoiceOverRunning() ||
                          UIAccessibilityIsSwitchControlRunning();
  if (usesScreenReader && !self.bubbleShouldAutoDismissUnderAccessibility) {
    // No-op. Keep the IPH available for screen reader users.
  } else {
    [self dismissAnimated:YES reason:IPHDismissalReasonType::kTimedOut];
  }
}

// Marks the user as not engaged when `engagementTimer` fires.
- (void)engagementTimerFired:(id)sender {
  self.userEngaged = NO;
  self.triggerFollowUpAction = NO;
  self.engagementTimer = nil;
}

// Invoked when the keybord is dismissed.
- (void)onKeyboardHide:(NSNotification*)notification {
  [self dismissAnimated:YES reason:IPHDismissalReasonType::kOnKeyboardHide];
}

// Calculates the frame of the BubbleView. `rect` is the frame of the bubble's
// superview. `anchorPoint` is the anchor point of the bubble. `anchorPoint`
// and `rect` must be in the same coordinates.
- (CGRect)frameForBubbleInRect:(CGRect)rect atAnchorPoint:(CGPoint)anchorPoint {
  const BOOL arrowIsFloating = self.bubbleType != BubbleViewTypeDefault;
  CGFloat bubbleAlignmentOffset = bubble_util::BubbleDefaultAlignmentOffset();
  if (arrowIsFloating) {
    bubbleAlignmentOffset = bubble_util::FloatingArrowAlignmentOffset(
        rect.size.width, anchorPoint, self.alignment);
  }
  // Set bubble alignment offset, must be set before the call to `sizeThatFits`.
  [self.bubbleViewController setBubbleAlignmentOffset:bubbleAlignmentOffset];
  CGSize maxBubbleSize = bubble_util::BubbleMaxSize(
      anchorPoint, bubbleAlignmentOffset, self.arrowDirection, self.alignment,
      rect.size);
  CGSize bubbleSize =
      [self.bubbleViewController.view sizeThatFits:maxBubbleSize];
  const BOOL bubbleIsFullWidth = self.bubbleType != BubbleViewTypeDefault &&
                                 self.bubbleType != BubbleViewTypeWithClose;
  if (bubbleIsFullWidth) {
    bubbleSize.width = maxBubbleSize.width;
  }
  // If `bubbleSize` does not fit in `maxBubbleSize`, the bubble will be
  // partially off screen and not look good. This is most likely a result of
  // an incorrect value for `alignment` (such as a trailing aligned bubble
  // anchored to an element on the leading edge of the screen).
  if (bubbleSize.width > maxBubbleSize.width ||
      bubbleSize.height > maxBubbleSize.height) {
    return CGRectNull;
  }
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      anchorPoint, bubbleAlignmentOffset, bubbleSize, self.arrowDirection,
      self.alignment, CGRectGetWidth(rect));
  // If anchorPoint is too close to the edge of the screen, the bubble will be
  // partially off screen and not look good.
  if (!CGRectContainsRect(rect, bubbleFrame)) {
    return CGRectNull;
  }
  return bubbleFrame;
}

// Whether the bubble should stick or auto-dismiss when the user uses a screen
// reader.
- (BOOL)bubbleShouldAutoDismissUnderAccessibility {
  return self.bubbleType == BubbleViewTypeDefault;
}

@end
