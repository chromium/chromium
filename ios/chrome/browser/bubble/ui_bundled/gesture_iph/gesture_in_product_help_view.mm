// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view.h"

#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_gesture_recognizer.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view+subclassing.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Blur radius of the background beneath the in-product help.
const CGFloat kBlurRadius = 6.0f;

// Initial distance between the bubble and edge of the view the bubble arrow
// points to.
const CGFloat kInitialBubbleDistanceToEdgeSpacingVertical = 16.0f;
// The distance that the bubble should move during the animation.
const CGFloat kBubbleDistanceAnimated = 40.0f;

// The radius of the gesture indicator when the animation starts fading in and
// ends fading out.
const CGFloat kFadingGestureIndicatorRadius = 46.0f;
// Initial distance between the bubble and the center of the gesture indicator
// ellipsis.
const CGFloat kInitialGestureIndicatorToBubbleSpacingDefault = 62.0f;
const CGFloat
    kInitialGestureIndicatorToBubbleSpacingVerticalSwipeInCompactHeight = 52.0f;
// The distance that the gesture indicator should move during the animation.
const CGFloat kGestureIndicatorDistanceAnimatedDefault = 140.0f;
const CGFloat kGestureIndicatorDistanceAnimatedVerticalSwipeInCompactHeight =
    80.0f;
// The distance between the gesture indicator and the edge for horizontal side
// swipe gestures within compact widths.
const CGFloat kSideSwipeGestureIndicatorDistance = 22.0f;

// The distance between the dismiss button and the bottom edge (or the top edge,
// when the bubble points down.)
const CGFloat kDismissButtonMargin = 28.0f;

// Animation times.
const base::TimeDelta kAnimationDuration = base::Seconds(3);
const base::TimeDelta kGestureIndicatorShrinkOrExpandTime =
    base::Milliseconds(250);
const base::TimeDelta kStartSlideAnimation = base::Milliseconds(500);
const base::TimeDelta kSlideAnimationDuration = base::Milliseconds(1500);
const base::TimeDelta kStartShrinkingGestureIndicator =
    base::Milliseconds(2250);

// Time to wait for other view components to fall into place after size changes
// before captureing a snapshot to create a blurred background.
const base::TimeDelta kBlurSuperviewWaitTime = base::Milliseconds(400);

// Whether bubble with arrow direction `direction` is pointing left.
BOOL IsArrowPointingLeft(BubbleArrowDirection direction) {
  return direction == (UseRTLLayout() ? BubbleArrowDirectionTrailing
                                      : BubbleArrowDirectionLeading);
}

// Whether the gesture indicator should offset from the center. The gesture
// indicator should offset on iPhone portrait mode and iPad split screen. In
// both cases, the horizontal size class is compact while the vertical size
// class is regular.
BOOL ShouldGestureIndicatorOffsetFromCenter(
    UITraitCollection* trait_collection) {
  return trait_collection.horizontalSizeClass ==
             UIUserInterfaceSizeClassCompact &&
         trait_collection.verticalSizeClass == UIUserInterfaceSizeClassRegular;
}

// Returns the opposite direction of `direction`.
UISwipeGestureRecognizerDirection GetOppositeDirection(
    UISwipeGestureRecognizerDirection direction) {
  switch (direction) {
    case UISwipeGestureRecognizerDirectionUp:
      return UISwipeGestureRecognizerDirectionDown;
    case UISwipeGestureRecognizerDirectionDown:
      return UISwipeGestureRecognizerDirectionUp;
    case UISwipeGestureRecognizerDirectionLeft:
      return UISwipeGestureRecognizerDirectionRight;
    case UISwipeGestureRecognizerDirectionRight:
    default:
      return UISwipeGestureRecognizerDirectionLeft;
  }
}

// Returns the expected bubble arrow direction for `swipe_direction`.
BubbleArrowDirection GetExpectedBubbleArrowDirectionForSwipeDirection(
    UISwipeGestureRecognizerDirection swipe_direction) {
  switch (swipe_direction) {
    case UISwipeGestureRecognizerDirectionUp:
      return BubbleArrowDirectionDown;
    case UISwipeGestureRecognizerDirectionDown:
      return BubbleArrowDirectionUp;
    case UISwipeGestureRecognizerDirectionLeft:
      return UseRTLLayout() ? BubbleArrowDirectionLeading
                            : BubbleArrowDirectionTrailing;
    case UISwipeGestureRecognizerDirectionRight:
    default:
      return UseRTLLayout() ? BubbleArrowDirectionTrailing
                            : BubbleArrowDirectionLeading;
  }
}

// The anchor point for the bubble arrow of the side swipe view.
CGPoint GetAnchorPointForBubbleArrow(CGSize bubble_bounding_size,
                                     BubbleArrowDirection direction) {
  switch (direction) {
    case BubbleArrowDirectionUp:
      return CGPointMake(bubble_bounding_size.width / 2,
                         kInitialBubbleDistanceToEdgeSpacingVertical);
    case BubbleArrowDirectionDown:
      return CGPointMake(bubble_bounding_size.width / 2,
                         bubble_bounding_size.height -
                             kInitialBubbleDistanceToEdgeSpacingVertical);
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      if (IsArrowPointingLeft(direction)) {
        return CGPointMake(0, bubble_bounding_size.height / 2);
      }
      return CGPointMake(bubble_bounding_size.width,
                         bubble_bounding_size.height / 2);
  }
}

// The frame for the bubble in the side swipe view.
CGRect GetInitialBubbleFrameForView(CGSize bubble_bounding_size,
                                    BubbleView* bubble_view) {
  BubbleArrowDirection direction = bubble_view.direction;
  // Bubble's initial placement should NOT go beyond the middle of the screen.
  CGFloat shift_distance = 0;
  switch (direction) {
    case BubbleArrowDirectionDown:
      shift_distance = bubble_bounding_size.height / 2;
      [[fallthrough]];
    case BubbleArrowDirectionUp:
      bubble_bounding_size.height = bubble_bounding_size.height / 2 -
                                    kInitialBubbleDistanceToEdgeSpacingVertical;
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      if (!IsArrowPointingLeft(direction)) {
        shift_distance = bubble_bounding_size.width / 2;
      }
      bubble_bounding_size.width /= 2;
      break;
  }
  CGPoint anchor_pt =
      GetAnchorPointForBubbleArrow(bubble_bounding_size, direction);
  CGSize max_bubble_size = bubble_util::BubbleMaxSize(
      anchor_pt, 0, direction, BubbleAlignmentCenter, bubble_bounding_size);
  CGSize bubble_size = [bubble_view sizeThatFits:max_bubble_size];
  CGRect frame = bubble_util::BubbleFrame(anchor_pt, 0, bubble_size, direction,
                                          BubbleAlignmentCenter,
                                          bubble_bounding_size.width);
  if (direction == BubbleArrowDirectionUp ||
      direction == BubbleArrowDirectionDown) {
    frame.origin.y += shift_distance;
  } else {
    frame.origin.x += shift_distance;
  }
  return frame;
}

// Returns the transparent gesture indicator circle at its initial size and
// position.
UIView* CreateInitialGestureIndicator() {
  UIView* gesture_indicator = [[UIView alloc] initWithFrame:CGRectZero];
  gesture_indicator.translatesAutoresizingMaskIntoConstraints = NO;
  gesture_indicator.layer.cornerRadius = kFadingGestureIndicatorRadius;
  gesture_indicator.backgroundColor = UIColor.whiteColor;
  gesture_indicator.alpha = 0;
  gesture_indicator.userInteractionEnabled = NO;
  // Shadow.
  gesture_indicator.layer.shadowColor = UIColor.blackColor.CGColor;
  gesture_indicator.layer.shadowOffset = CGSizeMake(0, 4);
  gesture_indicator.layer.shadowRadius = 16;
  gesture_indicator.layer.shadowOpacity = 1.0f;
  return gesture_indicator;
}

// Returns the dismiss button with `primaryAction`.
UIButton* CreateDismissButton(UIAction* primaryAction) {
  UIButtonConfiguration* button_config =
      [UIButtonConfiguration filledButtonConfiguration];
  button_config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_IPH_SIDE_SWIPE_DISMISS_BUTTON)
              attributes:attributes];
  button_config.attributedTitle = attributedString;
  button_config.contentInsets =
      NSDirectionalEdgeInsetsMake(14.0f, 32.0f, 14.0f, 32.0f);
  button_config.baseForegroundColor = UIColor.whiteColor;
  button_config.baseBackgroundColor =
      [UIColor.whiteColor colorWithAlphaComponent:0.2f];
  UIButton* dismiss_button = [UIButton buttonWithType:UIButtonTypeCustom
                                        primaryAction:primaryAction];
  dismiss_button.configuration = button_config;
  dismiss_button.accessibilityIdentifier =
      kGestureInProductHelpViewDismissButtonAXId;
  dismiss_button.translatesAutoresizingMaskIntoConstraints = NO;
  return dismiss_button;
}

}  // namespace

@implementation GestureInProductHelpView {
  // Subclass Properties.
  BubbleView* _bubbleView;
  UIView* _gestureIndicator;
  UIButton* _dismissButton;

  // Bubble text.
  NSString* _text;
  // Gaussian blurred super view that creates a blur-filter effect.
  UIView* _blurredSuperview;
  // Gesture recognizer of the view.
  GestureInProductHelpGestureRecognizer* _gestureRecognizer;
  // Currently displaying or animating direction of the gesture indicator.
  UISwipeGestureRecognizerDirection _animatingDirection;

  // Constraints for the gesture indicator defining its size, margin to the
  // bubble view, and its center alignment. Saved as ivar to be updated during
  // the animation.
  NSArray<NSLayoutConstraint*>* _gestureIndicatorSizeConstraints;
  NSLayoutConstraint* _gestureIndicatorMarginConstraint;
  NSLayoutConstraint* _gestureIndicatorCenterConstraint;

  // Animator object that handles the animation.
  UIViewPropertyAnimator* _animator;

  // Whether the bubble and the gesture indicator needs to be repositioned;
  // value would usually be YES right after a size class change, and back to NO
  // after redrawing completes.
  BOOL _needsRepositionBubbleAndGestureIndicator;

  // Set to `YES` before a Gaussian blurred snapshot of the superview is being
  // created; used to avoid repetitive requests to do so while waiting for other
  // views to fall into place in the event of a view size change, like device
  // rotation.
  BOOL _blurringSuperview;

  // Number of times the animation has already repeated.
  int _currentAnimationRepeatCount;

  // If `YES`, a static view, instead of an animation, would be displayed and
  // auto-dismissed on timeout.
  BOOL _reduceMotion;

  // If `YES`, the in-product help view is either currently being dismissed or
  // has already been removed from superview.
  BOOL _dismissed;
}

- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              swipeDirection:(UISwipeGestureRecognizerDirection)direction
       voiceOverAnnouncement:(NSString*)voiceOverAnnouncement {
  if ((self = [super initWithFrame:CGRectZero])) {
    _text = UIAccessibilityIsVoiceOverRunning() && voiceOverAnnouncement
                ? voiceOverAnnouncement
                : text;
    _animatingDirection = direction;
    _needsRepositionBubbleAndGestureIndicator = NO;
    _blurringSuperview = NO;
    _currentAnimationRepeatCount = 0;
    _animationRepeatCount = 3;
    _reduceMotion = UIAccessibilityIsReduceMotionEnabled() ||
                    UIAccessibilityIsVoiceOverRunning();
    _dismissed = NO;

    // Background view.
    UIView* backgroundView = [[UIView alloc] initWithFrame:CGRectZero];
    backgroundView.accessibilityIdentifier =
        kGestureInProductHelpViewBackgroundAXId;
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    backgroundView.backgroundColor = UIColor.blackColor;
    backgroundView.alpha = 0.65f;
    [self addSubview:backgroundView];
    AddSameConstraints(backgroundView, self);

    // Bubble view. This has to be positioned according to the initial view's
    // size.
    [self setInitialBubbleViewWithDirection:
              GetExpectedBubbleArrowDirectionForSwipeDirection(
                  _animatingDirection)
                               boundingSize:bubbleBoundingSize];

    // Gesture indicator ellipsis.
    _gestureIndicator = CreateInitialGestureIndicator();
    [self addSubview:_gestureIndicator];
    _gestureIndicatorSizeConstraints = @[
      [_gestureIndicator.heightAnchor
          constraintEqualToConstant:kFadingGestureIndicatorRadius * 2],
      [_gestureIndicator.widthAnchor
          constraintEqualToConstant:kFadingGestureIndicatorRadius * 2]
    ];
    [NSLayoutConstraint activateConstraints:_gestureIndicatorSizeConstraints];

    if (!UIAccessibilityIsVoiceOverRunning()) {
      // Dismiss button. It will be untappable in voice over mode, so only show
      // it to non-voiceOver users. VoiceOver users are able to dismiss the
      // view by swiping to the next accessibility element, and therefore don't
      // need the button.
      __weak GestureInProductHelpView* weakSelf = self;
      UIAction* dismissButtonAction =
          [UIAction actionWithHandler:^(UIAction* _) {
            [weakSelf dismissWithReason:IPHDismissalReasonType::kTappedClose];
          }];
      _dismissButton = CreateDismissButton(dismissButtonAction);
      [self addSubview:_dismissButton];
      [NSLayoutConstraint activateConstraints:[self dismissButtonConstraints]];
    }

    _gestureRecognizer = [[GestureInProductHelpGestureRecognizer alloc]
        initWithExpectedSwipeDirection:_animatingDirection
                                target:self
                                action:@selector
                                (handleInstructedSwipeGesture:)];
    [self addGestureRecognizer:_gestureRecognizer];

    self.alpha = 0;
    self.isAccessibilityElement = YES;
    self.accessibilityViewIsModal = YES;

    if (@available(iOS 17, *)) {
      __weak __typeof(self) weakSelf = self;
      NSArray<UITrait>* traits =
          (@[ UITraitHorizontalSizeClass.self, UITraitVerticalSizeClass.self ]);
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf pauseAnimationOnTraitChange:previousCollection];
      };
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
  return self;
}

- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              swipeDirection:(UISwipeGestureRecognizerDirection)direction {
  return [self initWithText:text
         bubbleBoundingSize:bubbleBoundingSize
             swipeDirection:direction
      voiceOverAnnouncement:nil];
}

- (void)didMoveToSuperview {
  if (self.superview != nil && self.alpha < 1) {
    GestureInProductHelpView* weakSelf = self;
    [UIView
        animateWithDuration:kGestureInProductHelpViewAppearDuration.InSecondsF()
                 animations:^{
                   weakSelf.alpha = 1;
                 }];
  }
}

- (CGSize)systemLayoutSizeFittingSize:(CGSize)targetSize {
  // Computes the smallest possible size that would fit all the UI elements in
  // all their animated positions.
  CGFloat min_width = _bubbleView.frame.size.width;
  CGFloat min_height = _bubbleView.frame.size.height +
                       [_dismissButton intrinsicContentSize].height +
                       kDismissButtonMargin;
  if (_reduceMotion) {
    return CGSizeMake(min_width, min_height);
  }
  switch (_animatingDirection) {
    case BubbleArrowDirectionUp:
    case BubbleArrowDirectionDown:
      min_height += kInitialBubbleDistanceToEdgeSpacingVertical +
                    [self initialGestureIndicatorToBubbleSpacing] +
                    [self gestureIndicatorAnimatedDistance] +
                    kFadingGestureIndicatorRadius;
      min_width = MAX(min_width, kFadingGestureIndicatorRadius * 2);
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      if (ShouldGestureIndicatorOffsetFromCenter(self.traitCollection)) {
        min_width /= 2;
      } else {
        min_width += [self initialGestureIndicatorToBubbleSpacing];
      }
      min_width += [self gestureIndicatorAnimatedDistance] +
                   kFadingGestureIndicatorRadius;
      min_height = MAX(min_height, kFadingGestureIndicatorRadius * 2);
      break;
  }
  // This view can expand as large as needed.
  return CGSizeMake(MAX(min_width, targetSize.width),
                    MAX(min_height, targetSize.height));
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self pauseAnimationOnTraitChange:previousTraitCollection];
}
#endif

- (void)layoutSubviews {
  [super layoutSubviews];
  if (_needsRepositionBubbleAndGestureIndicator) {
    // Avoid loops if `reposition` methods call [superview layoutIfNeeded].
    _needsRepositionBubbleAndGestureIndicator = NO;

    _bubbleView.frame =
        GetInitialBubbleFrameForView(self.frame.size, _bubbleView);
    [self repositionBubbleViewInSafeArea];
    [self repositionGestureIndicator];
    [_animator startAnimation];
  }

  UIView* blurredBackgroundImageView = _blurredSuperview.subviews.firstObject;
  if (self.superview && blurredBackgroundImageView && !_blurringSuperview &&
      !CGSizeEqualToSize(self.superview.bounds.size,
                         blurredBackgroundImageView.bounds.size)) {
    _blurringSuperview = YES;
    [_blurredSuperview removeFromSuperview];
    _blurredSuperview = nil;
    // Wait until all views settle in place after size change.
    GestureInProductHelpView* weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf blurrifySuperview];
        }),
        kBlurSuperviewWaitTime);
  }
}

#pragma mark - Public Properties

- (BOOL)bidirectional {
  return _gestureRecognizer.bidirectional;
}

- (void)setBidirectional:(BOOL)bidirectional {
  _gestureRecognizer.bidirectional = bidirectional;
}

- (BOOL)isEdgeSwipe {
  return [_gestureRecognizer isEdgeSwipe];
}

- (void)setEdgeSwipe:(BOOL)edgeSwipe {
  _gestureRecognizer.edgeSwipe = edgeSwipe;
}

#pragma mark - Public

- (void)startAnimation {
  [self startAnimationAfterDelay:base::TimeDelta()];
}

- (void)startAnimationAfterDelay:(base::TimeDelta)delay {
  CHECK(self.superview);
  CHECK_GT(self.animationRepeatCount, 0);

  [self.superview layoutIfNeeded];

  if (!_blurringSuperview) {
    [self blurrifySuperview];
  }
  [self repositionBubbleViewInSafeArea];
  if (UIAccessibilityIsVoiceOverRunning()) {
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    _text);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector
           (handleUIAccessibilityAnnouncementDidFinishNotification:)
               name:UIAccessibilityAnnouncementDidFinishNotification
             object:nil];
  }

  __weak GestureInProductHelpView* weakSelf = self;
  if (UIAccessibilityIsReduceMotionEnabled()) {
    // Dismiss after the same timeout as with animation enabled, or when
    // voiceover stops.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf dismissWithReason:IPHDismissalReasonType::kTimedOut];
        }),
        kAnimationDuration * self.animationRepeatCount);
    return;
  }

  // Total and relative time for each cycle of keyframe animation.
  base::TimeDelta keyframeAnimationDurationPerCycle = kAnimationDuration;
  if (self.bidirectional) {
    keyframeAnimationDurationPerCycle -= kDurationBetweenBidirectionalCycles;
  }
  double gestureIndicatorSizeChangeDuration =
      kGestureIndicatorShrinkOrExpandTime / keyframeAnimationDurationPerCycle;
  double startSlidingTime =
      kStartSlideAnimation / keyframeAnimationDurationPerCycle;
  double slidingDuration =
      kSlideAnimationDuration / keyframeAnimationDurationPerCycle;
  double startShrinkingTime =
      kStartShrinkingGestureIndicator / keyframeAnimationDurationPerCycle;

  ProceduralBlock gestureIndicatorKeyframes = ^{
    [UIView
        addKeyframeWithRelativeStartTime:0
                        relativeDuration:gestureIndicatorSizeChangeDuration
                              animations:^{
                                [weakSelf
                                    animateGestureIndicatorForVisibility:YES];
                              }];
    [UIView addKeyframeWithRelativeStartTime:startSlidingTime
                            relativeDuration:slidingDuration
                                  animations:^{
                                    [weakSelf animateGestureIndicatorSwipe];
                                  }];
    [UIView
        addKeyframeWithRelativeStartTime:startShrinkingTime
                        relativeDuration:gestureIndicatorSizeChangeDuration
                              animations:^{
                                [weakSelf
                                    animateGestureIndicatorForVisibility:NO];
                              }];
  };
  ProceduralBlock bubbleKeyframes = ^{
    [UIView addKeyframeWithRelativeStartTime:startSlidingTime
                            relativeDuration:slidingDuration
                                  animations:^{
                                    [weakSelf
                                        animateBubbleSwipeInReverseDrection:NO];
                                  }];
    [UIView
        addKeyframeWithRelativeStartTime:startShrinkingTime
                        relativeDuration:gestureIndicatorSizeChangeDuration
                              animations:^{
                                [weakSelf
                                    animateBubbleSwipeInReverseDrection:YES];
                              }];
  };
  ProceduralBlock animation = ^{
    [UIView
        animateKeyframesWithDuration:keyframeAnimationDurationPerCycle
                                         .InSecondsF()
                               delay:0
                             options:
                                 UIViewKeyframeAnimationOptionCalculationModeLinear
                          animations:gestureIndicatorKeyframes
                          completion:nil];
    [UIView
        animateKeyframesWithDuration:keyframeAnimationDurationPerCycle
                                         .InSecondsF()
                               delay:0
                             options:
                                 UIViewKeyframeAnimationOptionCalculationModeCubic
                          animations:bubbleKeyframes
                          completion:nil];
  };

  // Position gesture indicator at the start of each animation cycle, as it
  // might have been shifted away from its original position at the end of the
  // last cycle.
  [self repositionGestureIndicator];
  _animator = [UIViewPropertyAnimator
      runningPropertyAnimatorWithDuration:kAnimationDuration.InSecondsF()
                                    delay:delay.InSecondsF()
                                  options:UIViewAnimationOptionTransitionNone
                               animations:animation
                               completion:^(UIViewAnimatingPosition position) {
                                 if (position == UIViewAnimatingPositionEnd) {
                                   [weakSelf onAnimationCycleComplete];
                                 }
                               }];
}

- (void)dismissWithReason:(IPHDismissalReasonType)reason {
  [self dismissWithReason:reason completionHandler:nil];
}

#pragma mark - Private

// Action handler that executes when voiceover announcement ends.
- (void)handleUIAccessibilityAnnouncementDidFinishNotification:
    (NSNotification*)notification {
  [self dismissWithReason:IPHDismissalReasonType::kVoiceOverAnnouncementEnded];
}

// If the bubble view is fully visible in safe area, do nothing; otherwise, move
// it into the safe area.
- (void)repositionBubbleViewInSafeArea {
  CHECK(self.superview);
  UIEdgeInsets safeAreaInsets = self.safeAreaInsets;
  if (UIEdgeInsetsEqualToEdgeInsets(safeAreaInsets, UIEdgeInsetsZero)) {
    return;
  }

  CGRect bubbleFrame = _bubbleView.frame;
  CGSize viewSize = self.bounds.size;
  if (bubbleFrame.origin.x < safeAreaInsets.left) {
    bubbleFrame.origin.x = safeAreaInsets.left;
  }
  if (bubbleFrame.origin.y < safeAreaInsets.top) {
    bubbleFrame.origin.y = safeAreaInsets.top;
  }
  if (bubbleFrame.origin.x + bubbleFrame.size.width >
      viewSize.width - safeAreaInsets.right) {
    bubbleFrame.origin.x =
        viewSize.width - safeAreaInsets.right - bubbleFrame.size.width;
  }
  if (bubbleFrame.origin.y + bubbleFrame.size.height >
      viewSize.height - safeAreaInsets.bottom) {
    bubbleFrame.origin.y =
        viewSize.height - safeAreaInsets.bottom - bubbleFrame.size.height;
  }
  _bubbleView.frame = bubbleFrame;
  [self.superview layoutIfNeeded];
}

// Puts the gesture indicator at its initial position.
- (void)repositionGestureIndicator {
  CHECK(self.superview);
  if (_gestureIndicatorMarginConstraint.active &&
      _gestureIndicatorCenterConstraint.active) {
    [NSLayoutConstraint deactivateConstraints:@[
      _gestureIndicatorMarginConstraint,
      _gestureIndicatorCenterConstraint,
    ]];
  }
  _gestureIndicatorMarginConstraint =
      [self initialGestureIndicatorMarginConstraint];
  _gestureIndicatorCenterConstraint =
      [self initialGestureIndicatorCenterConstraint];
  [NSLayoutConstraint activateConstraints:@[
    _gestureIndicatorMarginConstraint,
    _gestureIndicatorCenterConstraint,
  ]];
  [self.superview layoutIfNeeded];
}

// Update the bottom-most subview to be a Gaussian blurred version of the
// superview to make the in-product help act as a blur-filter as well. If the
// superview is already blurred, this method does nothing.
- (void)blurrifySuperview {
  if (!self.superview || _blurredSuperview) {
    _blurringSuperview = NO;
    return;
  }
  // Using frame based layout so we can compare its frame with the superview's
  // frame to detect whether a redraw is needed.
  UIView* superview = self.superview;
  // Hide view to capture snapshot without IPH view elements.
  self.hidden = YES;
  UIImage* backgroundImage = CaptureViewWithOption(
      superview, 1.0f, CaptureViewOption::kClientSideRendering);
  self.hidden = NO;
  UIImage* blurredBackgroundImage =
      BlurredImageWithImage(backgroundImage, kBlurRadius);
  UIImageView* blurredBackgroundImageView =
      [[UIImageView alloc] initWithImage:blurredBackgroundImage];
  blurredBackgroundImageView.contentMode = UIViewContentModeScaleAspectFill;

  // Create wrapper view to clip the blurred image to the edge of the superview.
  _blurredSuperview = [[UIView alloc] initWithFrame:CGRectZero];
  _blurredSuperview.translatesAutoresizingMaskIntoConstraints = NO;
  _blurredSuperview.clipsToBounds = YES;
  [_blurredSuperview addSubview:blurredBackgroundImageView];
  blurredBackgroundImageView.frame = [self convertRect:superview.bounds
                                              fromView:superview];

  [self insertSubview:_blurredSuperview atIndex:0];
  AddSameConstraints(_blurredSuperview, self);
  _blurringSuperview = NO;
}

// Handles the completion of each round of animation.
- (void)onAnimationCycleComplete {
  if (!self.superview) {
    return;
  }
  _currentAnimationRepeatCount++;
  if (_currentAnimationRepeatCount == self.animationRepeatCount) {
    [self dismissWithReason:IPHDismissalReasonType::kTimedOut];
    return;
  }
  if (!self.bidirectional) {
    [self startAnimation];
    return;
  }
  // Handle direction change.
  _animatingDirection = GetOppositeDirection(_animatingDirection);
  [self handleDirectionChangeToOppositeDirection];
}

// Helper of "dismissWithReason:" that comes with an optional completion
// handler.
- (void)dismissWithReason:(IPHDismissalReasonType)reason
        completionHandler:(ProceduralBlock)completionHandler {
  if (!self.superview || _dismissed) {
    return;
  }
  _dismissed = YES;
  GestureInProductHelpView* weakSelf = self;
  [UIView
      animateWithDuration:kGestureInProductHelpViewAppearDuration.InSecondsF()
      animations:^{
        weakSelf.alpha = 0;
      }
      completion:^(BOOL finished) {
        GestureInProductHelpView* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf removeFromSuperview];
        base::UmaHistogramEnumeration(kUMAGesturalIPHDismissalReason, reason);
        [strongSelf.delegate gestureInProductHelpView:strongSelf
                                 didDismissWithReason:reason];
        if (completionHandler) {
          completionHandler();
        }
      }];
}

// Pauses the animation if there is a change to the gesture indicator's offset
// position after a change to the view's trait collection.
- (void)pauseAnimationOnTraitChange:
    (UITraitCollection*)previousTraitCollection {
  if (ShouldGestureIndicatorOffsetFromCenter(previousTraitCollection) !=
      ShouldGestureIndicatorOffsetFromCenter(self.traitCollection)) {
    [_animator pauseAnimation];
    _needsRepositionBubbleAndGestureIndicator = YES;
  }
}

@end

@implementation GestureInProductHelpView (Subclassing)

#pragma mark - Subclass Properties

- (BubbleView*)bubbleView {
  return _bubbleView;
}

- (UIView*)gestureIndicator {
  return _gestureIndicator;
}

- (UIButton*)dismissButton {
  return _dismissButton;
}

- (UISwipeGestureRecognizerDirection)animatingDirection {
  return _animatingDirection;
}

#pragma mark - Positioning

- (void)setInitialBubbleViewWithDirection:(BubbleArrowDirection)direction
                             boundingSize:(CGSize)boundingSize {
  _bubbleView = [[BubbleView alloc] initWithText:_text
                                  arrowDirection:direction
                                       alignment:BubbleAlignmentCenter];
  _bubbleView.frame = GetInitialBubbleFrameForView(boundingSize, _bubbleView);
  _bubbleView.accessibilityIdentifier = kGestureInProductHelpViewBubbleAXId;
  [self addSubview:_bubbleView];
  [_bubbleView setArrowHidden:!_reduceMotion animated:NO];
}

- (CGFloat)initialGestureIndicatorToBubbleSpacing {
  BOOL verticalSwipeInCompactHeight =
      self.traitCollection.verticalSizeClass ==
          UIUserInterfaceSizeClassCompact &&
      (_animatingDirection == UISwipeGestureRecognizerDirectionUp ||
       _animatingDirection == UISwipeGestureRecognizerDirectionDown);
  return verticalSwipeInCompactHeight
             ? kInitialGestureIndicatorToBubbleSpacingVerticalSwipeInCompactHeight
             : kInitialGestureIndicatorToBubbleSpacingDefault;
}

- (CGFloat)gestureIndicatorAnimatedDistance {
  BOOL verticalSwipeInCompactHeight =
      self.traitCollection.verticalSizeClass ==
          UIUserInterfaceSizeClassCompact &&
      (_animatingDirection == UISwipeGestureRecognizerDirectionUp ||
       _animatingDirection == UISwipeGestureRecognizerDirectionDown);
  if (verticalSwipeInCompactHeight) {
    CGFloat swipeDistance =
        kGestureIndicatorDistanceAnimatedVerticalSwipeInCompactHeight;
    if ([self isEdgeSwipe]) {
      CGFloat bubbleWidth = CGRectGetWidth(_bubbleView.bounds);
      swipeDistance += bubbleWidth / 2 - kSideSwipeGestureIndicatorDistance;
    }
    return swipeDistance;
  }
  return kGestureIndicatorDistanceAnimatedDefault;
}

- (NSLayoutConstraint*)initialGestureIndicatorMarginConstraint {
  // NOTE: Despite that the returning object defines the distance between the
  // gesture indicator to the bubble, it anchors on the view's nearest edge
  // instead of the bubble's, so that the gesture indicator's movement during
  // the animation would NOT be influenced by the bubble's movement.
  CGSize bubbleSize = _bubbleView.bounds.size;
  CGFloat gestureIndicatorToBubbleSpacing =
      [self initialGestureIndicatorToBubbleSpacing];
  switch (_animatingDirection) {
    case UISwipeGestureRecognizerDirectionUp: {
      // Gesture indicator should be `kInitialGestureIndicatorToBubbleSpacing`
      // away from the bubble's top edge.
      CGFloat margin = kInitialBubbleDistanceToEdgeSpacingVertical +
                       bubbleSize.height + gestureIndicatorToBubbleSpacing;
      return [_gestureIndicator.centerYAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor
                         constant:-margin];
    }
    case UISwipeGestureRecognizerDirectionDown: {
      // Gesture indicator should be `kInitialGestureIndicatorToBubbleSpacing`
      // away from the bubble's bottom edge.
      CGFloat margin = kInitialBubbleDistanceToEdgeSpacingVertical +
                       bubbleSize.height + gestureIndicatorToBubbleSpacing;
      return [_gestureIndicator.centerYAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.topAnchor
                         constant:margin];
    }
    case UISwipeGestureRecognizerDirectionLeft:
    case UISwipeGestureRecognizerDirectionRight:
    default: {
      CGFloat margin;
      NSLayoutAnchor* anchorForMargin;
      if (ShouldGestureIndicatorOffsetFromCenter(self.traitCollection)) {
        // If the user should swipe from the edge, the gesture indicator should
        // start from the edge of the view; otherwise, it should be
        // center-aligned with the bubble.
        margin = [self isEdgeSwipe] ? kSideSwipeGestureIndicatorDistance
                                    : bubbleSize.width / 2;
      } else {
        // Gesture indicator should be `gestureIndicatorToBubbleSpacing`
        // away from the bubble's leading/trailing edge.
        margin = bubbleSize.width + gestureIndicatorToBubbleSpacing;
      }
      BOOL isSwipingLeadingDirection =
          _animatingDirection == (UseRTLLayout()
                                      ? UISwipeGestureRecognizerDirectionRight
                                      : UISwipeGestureRecognizerDirectionLeft);
      if (isSwipingLeadingDirection) {
        margin = -margin;
        anchorForMargin = self.safeAreaLayoutGuide.trailingAnchor;
      } else {
        anchorForMargin = self.safeAreaLayoutGuide.leadingAnchor;
      }
      return [_gestureIndicator.centerXAnchor
          constraintEqualToAnchor:anchorForMargin
                         constant:margin];
    }
  }
}

- (NSLayoutConstraint*)initialGestureIndicatorCenterConstraint {
  NSLayoutConstraint* gestureIndicatorCenterConstraint;
  switch (_animatingDirection) {
    case UISwipeGestureRecognizerDirectionUp:
    case UISwipeGestureRecognizerDirectionDown:
      gestureIndicatorCenterConstraint = [_gestureIndicator.centerXAnchor
          constraintEqualToAnchor:self.centerXAnchor];
      break;
    case UISwipeGestureRecognizerDirectionLeft:
    case UISwipeGestureRecognizerDirectionRight:
      CGFloat offset = [self initialGestureIndicatorToBubbleSpacing] +
                       _bubbleView.frame.size.height / 2;
      gestureIndicatorCenterConstraint = [_gestureIndicator.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor
                         constant:ShouldGestureIndicatorOffsetFromCenter(
                                      self.traitCollection)
                                      ? offset
                                      : 0];
      break;
  }
  return gestureIndicatorCenterConstraint;
}

- (NSArray<NSLayoutConstraint*>*)dismissButtonConstraints {
  NSLayoutConstraint* centerConstraint =
      [_dismissButton.centerXAnchor constraintEqualToAnchor:self.centerXAnchor];
  NSLayoutConstraint* marginConstraint =
      _animatingDirection == UISwipeGestureRecognizerDirectionUp
          ? [_dismissButton.topAnchor
                constraintEqualToAnchor:self.topAnchor
                               constant:kDismissButtonMargin]
          : [_dismissButton.bottomAnchor
                constraintEqualToAnchor:self.bottomAnchor
                               constant:-kDismissButtonMargin];
  return @[ centerConstraint, marginConstraint ];
}

#pragma mark - Animation Keyframe

- (void)animateGestureIndicatorForVisibility:(BOOL)visible {
  const CGFloat radius =
      visible ? kGestureIndicatorRadius : kFadingGestureIndicatorRadius;
  for (NSLayoutConstraint* constraint in _gestureIndicatorSizeConstraints) {
    constraint.constant = radius * 2;
  }
  _gestureIndicator.layer.cornerRadius = radius;
  if (visible) {
    _gestureIndicator.alpha =
        UIAccessibilityIsReduceTransparencyEnabled() ? 1.0f : 0.7f;
  } else {
    _gestureIndicator.alpha = 0;
  }
  [self layoutIfNeeded];
}

- (void)animateGestureIndicatorSwipe {
  BOOL animatingAwayFromOrigin =
      _animatingDirection == UISwipeGestureRecognizerDirectionDown ||
      (_animatingDirection == UISwipeGestureRecognizerDirectionLeft &&
       UseRTLLayout()) ||
      (_animatingDirection == UISwipeGestureRecognizerDirectionRight &&
       !UseRTLLayout());
  CGFloat gestureIndicatorAnimatedDistance =
      [self gestureIndicatorAnimatedDistance];
  _gestureIndicatorMarginConstraint.constant +=
      animatingAwayFromOrigin ? gestureIndicatorAnimatedDistance
                              : -gestureIndicatorAnimatedDistance;
  [self layoutIfNeeded];
}

- (void)animateBubbleSwipeInReverseDrection:(BOOL)reverse {
  UISwipeGestureRecognizerDirection direction = _animatingDirection;
  if (reverse) {
    direction = GetOppositeDirection(direction);
  }
  CGRect newFrame = _bubbleView.frame;
  switch (direction) {
    case UISwipeGestureRecognizerDirectionUp:
      newFrame.origin.y -= kBubbleDistanceAnimated;
      break;
    case UISwipeGestureRecognizerDirectionDown:
      newFrame.origin.y += kBubbleDistanceAnimated;
      break;
    case UISwipeGestureRecognizerDirectionLeft:
      newFrame.origin.x -= kBubbleDistanceAnimated;
      break;
    case UISwipeGestureRecognizerDirectionRight:
      newFrame.origin.x += kBubbleDistanceAnimated;
      break;
  }
  _bubbleView.frame = newFrame;
  [_bubbleView setArrowHidden:reverse animated:YES];
  [self layoutIfNeeded];
}

#pragma mark - Event handler

- (void)handleInstructedSwipeGesture:
    (GestureInProductHelpGestureRecognizer*)gesture {
  __weak GestureInProductHelpView* weakSelf = self;
  // Triggers an animation that resembles a user-initiated swipe on the views
  // beneath the IPH. For one directional IPH, the swipe direction should be
  // opposite to the arrow direction. Also dismisses the IPH with the reason
  // `kSwipedAsInstructedByGestureIPH`.
  [self
      dismissWithReason:IPHDismissalReasonType::kSwipedAsInstructedByGestureIPH
      completionHandler:^{
        [weakSelf.delegate
                gestureInProductHelpView:weakSelf
            shouldHandleSwipeInDirection:gesture.actualSwipeDirection];
      }];
}

- (void)handleDirectionChangeToOppositeDirection {
  BubbleView* previousBubbleView = _bubbleView;
  __weak GestureInProductHelpView* weakSelf = self;
  [UIView animateWithDuration:kDurationBetweenBidirectionalCycles.InSecondsF()
      animations:^{
        previousBubbleView.alpha = 0;
      }
      completion:^(BOOL completed) {
        [previousBubbleView removeFromSuperview];
        if (completed && weakSelf.superview) {
          [weakSelf setInitialBubbleViewWithDirection:
                        GetExpectedBubbleArrowDirectionForSwipeDirection(
                            weakSelf.animatingDirection)
                                         boundingSize:weakSelf.frame.size];
          [weakSelf startAnimation];
        } else {
          // This will be most likely caused by that the view has been
          // dismissed during animation, but in case it's not, dismiss the
          // view. If the view has already been dismissed, this call does
          // nothing.
          [weakSelf dismissWithReason:IPHDismissalReasonType::kUnknown];
        }
      }];
}

@end
