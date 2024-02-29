// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/gesture_iph/gesture_in_product_help_view.h"

#import "base/i18n/rtl.h"
#import "base/ios/block_types.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ui/bubble/bubble_constants.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view.h"
#import "ios/chrome/browser/ui/bubble/gesture_iph/gesture_in_product_help_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Initial distance between the bubble and edge of the view the bubble arrow
// points to.
const CGFloat kInitialBubbleDistanceToEdgeSpacingVertical = 16.0f;
// The distance that the bubble should move during the animation.
const CGFloat kBubbleDistanceAnimated = 40.0f;

// The radius of the gesture indicator when it's animating the user's finger
// movement.
const CGFloat kGestureIndicatorRadius = 33.0f;
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

// Time taken for the bubble to fade for bidirectional swipes.
const base::TimeDelta kBubbleDisappearDuration = base::Milliseconds(250);

// Whether bubble with arrow direction `direction` is pointing left.
BOOL IsArrowPointingLeft(BubbleArrowDirection direction) {
  return direction == (base::i18n::IsRTL() ? BubbleArrowDirectionTrailing
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
BubbleArrowDirection GetOppositeDirection(BubbleArrowDirection direction) {
  switch (direction) {
    case BubbleArrowDirectionUp:
      return BubbleArrowDirectionDown;
    case BubbleArrowDirectionDown:
      return BubbleArrowDirectionUp;
    case BubbleArrowDirectionLeading:
      return BubbleArrowDirectionTrailing;
    case BubbleArrowDirectionTrailing:
      return BubbleArrowDirectionLeading;
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
  // Bubble text.
  NSString* _text;
  // Bubble view.
  BubbleView* _bubbleView;
  // Ellipsis that instructs the user's finger movement.
  UIView* _gestureIndicator;
  // Button at the bottom that dismisses the IPH.
  UIButton* _dismissButton;

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

  // Number of times the animation has already repeated.
  int _currentAnimationRepeatCount;

  // If `YES`, a static view, instead of an animation, would be displayed and
  // auto-dismissed on timeout.
  BOOL _reduceMotion;
}

- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              arrowDirection:(BubbleArrowDirection)direction
       voiceOverAnnouncement:(NSString*)voiceOverAnnouncement {
  if (self = [super initWithFrame:CGRectZero]) {
    _text = UIAccessibilityIsVoiceOverRunning() && voiceOverAnnouncement
                ? voiceOverAnnouncement
                : text;
    _needsRepositionBubbleAndGestureIndicator = NO;
    _currentAnimationRepeatCount = 0;
    _dismissCallback = ^(IPHDismissalReasonType reason,
                         feature_engagement::Tracker::SnoozeAction action) {
    };
    _animationRepeatCount = 3;
    _bidirectional = NO;
    _reduceMotion = UIAccessibilityIsReduceMotionEnabled() ||
                    UIAccessibilityIsVoiceOverRunning();
    self.isAccessibilityElement = YES;
    self.accessibilityViewIsModal = YES;

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
    [self setInitialBubbleViewWithDirection:direction
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
  }
  return self;
}

- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              arrowDirection:(BubbleArrowDirection)direction {
  return [self initWithText:text
         bubbleBoundingSize:bubbleBoundingSize
             arrowDirection:direction
      voiceOverAnnouncement:nil];
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
  switch (_bubbleView.direction) {
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

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (ShouldGestureIndicatorOffsetFromCenter(previousTraitCollection) !=
      ShouldGestureIndicatorOffsetFromCenter(self.traitCollection)) {
    [_animator pauseAnimation];
    _needsRepositionBubbleAndGestureIndicator = YES;
  }
}

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
}

#pragma mark - Public

- (void)startAnimation {
  [self startAnimationAfterDelay:base::TimeDelta()];
}

- (void)startAnimationAfterDelay:(base::TimeDelta)delay {
  CHECK(self.superview);
  CHECK_GT(self.animationRepeatCount, 0);

  [self.superview layoutIfNeeded];
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
    keyframeAnimationDurationPerCycle -= kBubbleDisappearDuration;
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
  if (!self.superview) {
    return;
  }
  [self removeFromSuperview];
  self.dismissCallback(reason,
                       feature_engagement::Tracker::SnoozeAction::DISMISSED);
}

#pragma mark - Private

// Handles the completion of each round of animation.
- (void)onAnimationCycleComplete {
  if (!self.superview) {
    return;
  }
  _currentAnimationRepeatCount++;
  if (_currentAnimationRepeatCount == self.animationRepeatCount) {
    [self dismissWithReason:IPHDismissalReasonType::kTimedOut];
  } else {
    if (self.bidirectional) {
      BubbleView* previousBubbleView = _bubbleView;
      __weak GestureInProductHelpView* weakSelf = self;
      [UIView animateWithDuration:kBubbleDisappearDuration.InSecondsF()
          animations:^{
            previousBubbleView.alpha = 0;
          }
          completion:^(BOOL completed) {
            [previousBubbleView removeFromSuperview];
            [weakSelf setInitialBubbleViewWithDirection:GetOppositeDirection(
                                                            previousBubbleView
                                                                .direction)
                                           boundingSize:weakSelf.frame.size];
            [weakSelf startAnimation];
          }];
    } else {
      [self startAnimation];
    }
  }
}

- (void)handleUIAccessibilityAnnouncementDidFinishNotification:
    (NSNotification*)notification {
  [self dismissWithReason:IPHDismissalReasonType::kVoiceOverAnnouncementEnded];
}

#pragma mark - Initial positioning helpers

// Initial bubble setup and positioning.
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

// Initial distance between the bubble and the center of the gesture indicator
// ellipsis.
- (CGFloat)initialGestureIndicatorToBubbleSpacing {
  BOOL verticalSwipeInCompactHeight =
      self.traitCollection.verticalSizeClass ==
          UIUserInterfaceSizeClassCompact &&
      (_bubbleView.direction == BubbleArrowDirectionUp ||
       _bubbleView.direction == BubbleArrowDirectionDown);
  return verticalSwipeInCompactHeight
             ? kInitialGestureIndicatorToBubbleSpacingVerticalSwipeInCompactHeight
             : kInitialGestureIndicatorToBubbleSpacingDefault;
}

// Animated distance of the gesture indicator.
- (CGFloat)gestureIndicatorAnimatedDistance {
  BOOL verticalSwipeInCompactHeight =
      self.traitCollection.verticalSizeClass ==
          UIUserInterfaceSizeClassCompact &&
      (_bubbleView.direction == BubbleArrowDirectionUp ||
       _bubbleView.direction == BubbleArrowDirectionDown);
  return verticalSwipeInCompactHeight
             ? kGestureIndicatorDistanceAnimatedVerticalSwipeInCompactHeight
             : kGestureIndicatorDistanceAnimatedDefault;
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

// Returns the desired value of `_gestureIndicatorMarginConstraint`.
//
// NOTE: Despite that the returning object defines the distance between the
// gesture indicator to the bubble, it anchors on the view's nearest edge
// instead of the bubble's, so that the gesture indicator's movement during the
// animation would NOT be influenced by the bubble's movement.
- (NSLayoutConstraint*)initialGestureIndicatorMarginConstraint {
  CGSize bubbleSize = _bubbleView.frame.size;
  CGFloat gestureIndicatorToBubbleSpacing =
      [self initialGestureIndicatorToBubbleSpacing];
  switch (_bubbleView.direction) {
    case BubbleArrowDirectionUp: {
      // Gesture indicator should be `kInitialGestureIndicatorToBubbleSpacing`
      // away from the bubble's bottom edge.
      CGFloat margin = kInitialBubbleDistanceToEdgeSpacingVertical +
                       bubbleSize.height + gestureIndicatorToBubbleSpacing;
      return [_gestureIndicator.centerYAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.topAnchor
                         constant:margin];
    }
    case BubbleArrowDirectionDown: {
      // Gesture indicator should be `kInitialGestureIndicatorToBubbleSpacing`
      // away from the bubble's top edge.
      CGFloat margin = kInitialBubbleDistanceToEdgeSpacingVertical +
                       bubbleSize.height + gestureIndicatorToBubbleSpacing;
      return [_gestureIndicator.centerYAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor
                         constant:-margin];
    }
    case BubbleArrowDirectionLeading: {
      CGFloat margin;
      if (ShouldGestureIndicatorOffsetFromCenter(self.traitCollection)) {
        // Gesture indicator should be center-aligned with the bubble.
        margin = bubbleSize.width / 2;
      } else {
        // Gesture indicator should be `kInitialGestureIndicatorToBubbleSpacing`
        // away from the bubble's trailing edge.
        margin = bubbleSize.width + gestureIndicatorToBubbleSpacing;
      }
      return [_gestureIndicator.centerXAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.leadingAnchor
                         constant:margin];
    }
    case BubbleArrowDirectionTrailing: {
      CGFloat margin;
      if (ShouldGestureIndicatorOffsetFromCenter(self.traitCollection)) {
        // Gesture indicator should be center-aligned with the bubble.
        margin = bubbleSize.width / 2;
      } else {
        // Gesture indicator should be `gestureIndicatorToBubbleSpacing`
        // away from the bubble's leading edge.
        margin = bubbleSize.width + gestureIndicatorToBubbleSpacing;
      }
      return [_gestureIndicator.centerXAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.trailingAnchor
                         constant:-margin];
    }
  }
}

// Returns the desired value of `_gestureIndicatorCenterConstraints`.
- (NSLayoutConstraint*)initialGestureIndicatorCenterConstraint {
  NSLayoutConstraint* gestureIndicatorCenterConstraint;
  switch (_bubbleView.direction) {
    case BubbleArrowDirectionUp:
    case BubbleArrowDirectionDown:
      gestureIndicatorCenterConstraint = [_gestureIndicator.centerXAnchor
          constraintEqualToAnchor:self.centerXAnchor];
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
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

// Returns the desired value of `_dismissButtonConstraints`.
- (NSArray<NSLayoutConstraint*>*)dismissButtonConstraints {
  NSLayoutConstraint* centerConstraint =
      [_dismissButton.centerXAnchor constraintEqualToAnchor:self.centerXAnchor];
  NSLayoutConstraint* marginConstraint =
      _bubbleView.direction == BubbleArrowDirectionDown
          ? [_dismissButton.topAnchor
                constraintEqualToAnchor:self.topAnchor
                               constant:kDismissButtonMargin]
          : [_dismissButton.bottomAnchor
                constraintEqualToAnchor:self.bottomAnchor
                               constant:-kDismissButtonMargin];
  return @[ centerConstraint, marginConstraint ];
}

#pragma mark - Animation Helpers

// Animation block that resizes the gesture indicator and update transparency.
// If `visible`, the gesture indicator will shrink from a large size and ends
// with the correct size and correct visiblity; otherwise, it will enlarge and
// fade into background.
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

// Animate the "swipe" movement of the gesture indicator in accordance to the
// direction.
- (void)animateGestureIndicatorSwipe {
  BubbleArrowDirection direction = _bubbleView.direction;
  CGFloat gestureIndicatorAnimatedDistance =
      [self gestureIndicatorAnimatedDistance];
  CGFloat animateDistance = (direction == BubbleArrowDirectionUp ||
                             direction == BubbleArrowDirectionLeading)
                                ? gestureIndicatorAnimatedDistance
                                : -gestureIndicatorAnimatedDistance;
  _gestureIndicatorMarginConstraint.constant += animateDistance;
  [self layoutIfNeeded];
}

// If `reverse` is `NO`, animate the "swipe" movement of the bubble view in
// accordance to the direction; otherwise, swipe it in the reverse direction.
// Note that swiping in reverse direction hides the bubble arrow.
- (void)animateBubbleSwipeInReverseDrection:(BOOL)reverse {
  BubbleArrowDirection direction = _bubbleView.direction;
  if (reverse) {
    direction = GetOppositeDirection(direction);
  }
  CGRect newFrame = _bubbleView.frame;
  switch (direction) {
    case BubbleArrowDirectionUp:
      newFrame.origin.y += kBubbleDistanceAnimated;
      break;
    case BubbleArrowDirectionDown:
      newFrame.origin.y -= kBubbleDistanceAnimated;
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      if (IsArrowPointingLeft(direction)) {
        newFrame.origin.x += kBubbleDistanceAnimated;
      } else {
        newFrame.origin.x -= kBubbleDistanceAnimated;
      }
      break;
  }
  _bubbleView.frame = newFrame;
  [_bubbleView setArrowHidden:reverse animated:YES];
  [self layoutIfNeeded];
}

@end
