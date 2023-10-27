// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/side_swipe_bubble/side_swipe_bubble_view.h"

#import "base/i18n/rtl.h"
#import "base/ios/block_types.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ui/bubble/bubble_constants.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view.h"
#import "ios/chrome/browser/ui/bubble/side_swipe_bubble/side_swipe_bubble_constants.h"
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
const CGFloat kInitialGestureIndicatorToBubbleSpacing = 62.0f;
// The distance that the gesture indicator should move during the animation.
const CGFloat kGestureIndicatorDistanceAnimated = 140.0f;

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

// Maximum animation repeat count.
const int kMaxAnimationRepeatCount = 3;

// Whether bubble with arrow direction `direction` is pointing left.
BOOL IsArrowPointingLeft(BubbleArrowDirection direction) {
  return direction == (base::i18n::IsRTL() ? BubbleArrowDirectionTrailing
                                           : BubbleArrowDirectionLeading);
}

// Whether the gesture indicator should offset from the center. The gesture
// indicator should offset on iPhone portrait mode and iPad split screen. In
// both cases, the horizontal size class is compact while the vertical size
// class is regular.
BOOL GestureIndicatorShouldOffsetFromCenter(
    UITraitCollection* trait_collection) {
  return trait_collection.horizontalSizeClass ==
             UIUserInterfaceSizeClassCompact &&
         trait_collection.verticalSizeClass == UIUserInterfaceSizeClassRegular;
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
  switch (direction) {
    case BubbleArrowDirectionUp:
    case BubbleArrowDirectionDown:
      bubble_bounding_size.height = bubble_bounding_size.height / 2 -
                                    kInitialBubbleDistanceToEdgeSpacingVertical;
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      bubble_bounding_size.width /= 2;
      break;
  }
  CGPoint anchor_pt =
      GetAnchorPointForBubbleArrow(bubble_bounding_size, direction);
  CGSize max_bubble_size = bubble_util::BubbleMaxSize(
      anchor_pt, 0, direction, BubbleAlignmentCenter, bubble_bounding_size);
  CGSize bubble_size = [bubble_view sizeThatFits:max_bubble_size];
  return bubble_util::BubbleFrame(anchor_pt, 0, bubble_size, direction,
                                  BubbleAlignmentCenter,
                                  bubble_bounding_size.width);
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
  UIFont* font = [UIFont boldSystemFontOfSize:15];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_IPH_SIDE_SWIPE_DISMISS_BUTTON)
              attributes:attributes];
  button_config.attributedTitle = attributedString;
  button_config.baseForegroundColor = [UIColor colorNamed:kGrey800Color];
  button_config.baseBackgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  UIButton* dismiss_button = [UIButton buttonWithType:UIButtonTypeCustom
                                        primaryAction:primaryAction];
  dismiss_button.configuration = button_config;
  dismiss_button.accessibilityIdentifier =
      kSideSwipeBubbleViewDismissButtonAXId;
  dismiss_button.alpha =
      UIAccessibilityIsReduceTransparencyEnabled() ? 1.0f : 0.65f;
  dismiss_button.translatesAutoresizingMaskIntoConstraints = NO;
  return dismiss_button;
}

// Returns the relative timing for a single keyframe animation.
double GetRelativeTimeForKeyframeAnimation(base::TimeDelta time) {
  return time.InMillisecondsF() / kAnimationDuration.InMillisecondsF();
}

}  // namespace

@implementation SideSwipeBubbleView {
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

  // Whether the bubble and the gesture indicator needs to be redrawn; value
  // would usually be YES right after a size class change, and back to NO after
  // redrawing completes.
  BOOL _needsRedrawBubbleAndGestureIndicator;

  // Number of times the animation has already repeated.
  int _currentAnimationRepeatCount;
}

- (instancetype)initWithText:(NSString*)text
          bubbleBoundingSize:(CGSize)bubbleBoundingSize
              arrowDirection:(BubbleArrowDirection)direction {
  if (self = [super initWithFrame:CGRectZero]) {
    self.isAccessibilityElement = YES;
    _needsRedrawBubbleAndGestureIndicator = NO;
    _currentAnimationRepeatCount = 0;
    _dismissCallback = ^(IPHDismissalReasonType reason,
                         feature_engagement::Tracker::SnoozeAction action) {
    };

    // Background view.
    UIView* backgroundView = [[UIView alloc] initWithFrame:CGRectZero];
    backgroundView.accessibilityIdentifier = kSideSwipeBubbleViewBackgroundAXId;
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    backgroundView.backgroundColor = UIColor.blackColor;
    backgroundView.alpha = 0.65f;
    [self addSubview:backgroundView];
    AddSameConstraints(backgroundView, self);

    // Bubble view. This has to be positioned according to the initial view's
    // size.
    _bubbleView = [[BubbleView alloc] initWithText:text
                                    arrowDirection:direction
                                         alignment:BubbleAlignmentCenter];
    _bubbleView.frame =
        GetInitialBubbleFrameForView(bubbleBoundingSize, _bubbleView);
    _bubbleView.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;
    _bubbleView.accessibilityIdentifier = kSideSwipeBubbleViewBubbleAXId;
    [self addSubview:_bubbleView];
    [_bubbleView setArrowHidden:YES animated:NO];

    // Gesture indicator ellipsis.
    _gestureIndicator = CreateInitialGestureIndicator();
    [self addSubview:_gestureIndicator];
    _gestureIndicatorSizeConstraints = @[
      [_gestureIndicator.heightAnchor
          constraintEqualToConstant:kFadingGestureIndicatorRadius * 2],
      [_gestureIndicator.widthAnchor
          constraintEqualToConstant:kFadingGestureIndicatorRadius * 2]
    ];
    _gestureIndicatorMarginConstraint =
        [self initialGestureIndicatorMarginConstraint];
    _gestureIndicatorCenterConstraint =
        [self initialGestureIndicatorCenterConstraint];
    NSArray<NSLayoutConstraint*>* initialGestureIndicatorConstraints =
        [_gestureIndicatorSizeConstraints arrayByAddingObjectsFromArray:@[
          _gestureIndicatorMarginConstraint,
          _gestureIndicatorCenterConstraint,
        ]];
    [NSLayoutConstraint activateConstraints:initialGestureIndicatorConstraints];

    // Dismiss button.
    __weak SideSwipeBubbleView* weakSelf = self;
    UIAction* dismissButtonAction = [UIAction actionWithHandler:^(UIAction* _) {
      [weakSelf dismissWithReason:IPHDismissalReasonType::kTappedClose];
    }];
    _dismissButton = CreateDismissButton(dismissButtonAction);
    [self addSubview:_dismissButton];
    [NSLayoutConstraint activateConstraints:[self dismissButtonConstraints]];
  }
  return self;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (GestureIndicatorShouldOffsetFromCenter(previousTraitCollection) !=
      GestureIndicatorShouldOffsetFromCenter(self.traitCollection)) {
    // TODO(crbug.com/1467873): Stop animations.
    _needsRedrawBubbleAndGestureIndicator = YES;
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];
  if (_needsRedrawBubbleAndGestureIndicator) {
    // TODO(crbug.com/1467873): Reposition bubble and restart
    // animation. Constraints may need to be deactivated and reactivated as
    // well.
    _needsRedrawBubbleAndGestureIndicator = NO;
  }
}

#pragma mark - Public

- (void)startAnimation {
  [self startAnimationAfterDelay:base::TimeDelta()];
}

- (void)startAnimationAfterDelay:(base::TimeDelta)delay {
  if (UIAccessibilityIsReduceMotionEnabled()) {
    // TODO(crbug.com/1467873): Show static image.
    return;
  }
  double gestureIndicatorSizeChangeDuration =
      GetRelativeTimeForKeyframeAnimation(kGestureIndicatorShrinkOrExpandTime);
  double startSlidingTime =
      GetRelativeTimeForKeyframeAnimation(kStartSlideAnimation);
  double slidingDuration =
      GetRelativeTimeForKeyframeAnimation(kSlideAnimationDuration);
  double startShrinkingTime =
      GetRelativeTimeForKeyframeAnimation(kStartShrinkingGestureIndicator);

  __weak SideSwipeBubbleView* weakSelf = self;
  ProceduralBlock keyframes = ^{
    [UIView
        addKeyframeWithRelativeStartTime:0
                        relativeDuration:gestureIndicatorSizeChangeDuration
                              animations:^{
                                [weakSelf
                                    keyframeThatShowsAndShrinksGestureIndicator];
                              }];
    [UIView addKeyframeWithRelativeStartTime:startSlidingTime
                            relativeDuration:slidingDuration
                                  animations:^{
                                    [weakSelf keyframeThatSwipes];
                                  }];
    [UIView
        addKeyframeWithRelativeStartTime:startShrinkingTime
                        relativeDuration:gestureIndicatorSizeChangeDuration
                              animations:^{
                                [weakSelf
                                    keyframeThatExpandsAndHidesGestureIndicator];
                              }];
  };

  // Force layout to set the initial frame of the animation.
  [self layoutIfNeeded];
  int remainingAnimationRepeatCount =
      kMaxAnimationRepeatCount - _currentAnimationRepeatCount;
  [UIView animateKeyframesWithDuration:kAnimationDuration.InSecondsF()
      delay:delay.InSecondsF()
      options:UIViewAnimationOptionRepeat | UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [UIView modifyAnimationsWithRepeatCount:remainingAnimationRepeatCount
                                   autoreverses:NO
                                     animations:keyframes];
      }
      completion:^(BOOL completed) {
        if (completed) {
          [weakSelf dismissWithReason:IPHDismissalReasonType::kTimedOut];
        }
      }];
}

#pragma mark - Private

// Dismiss the view with `reason`.
- (void)dismissWithReason:(IPHDismissalReasonType)reason {
  // Only destroy the view after callback executed.
  [self removeFromSuperview];
  self.dismissCallback(reason,
                       feature_engagement::Tracker::SnoozeAction::DISMISSED);
}

#pragma mark - Initial constraint helpers

// Returns the desired value of `_gestureIndicatorMarginConstraint`.
//
// NOTE: Despite that the returning object defines the distance between the
// gesture indicator to the bubble, it anchors on the view's nearest edge
// instead of the bubble's, so that the gesture indicator's movement during the
// animation would NOT be influenced by the bubble's movement.
- (NSLayoutConstraint*)initialGestureIndicatorMarginConstraint {
  CGSize bubbleSize = _bubbleView.frame.size;
  switch (_bubbleView.direction) {
    case BubbleArrowDirectionUp: {
      // Gesture indicator should be `kInitialGestureIndicatorToBubbleSpacing`
      // away from the bubble's bottom edge.
      CGFloat margin = kInitialBubbleDistanceToEdgeSpacingVertical +
                       bubbleSize.height +
                       kInitialGestureIndicatorToBubbleSpacing;
      return [_gestureIndicator.centerYAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:margin];
    }
    case BubbleArrowDirectionDown: {
      // Gesture indicator should be `kInitialGestureIndicatorToBubbleSpacing`
      // away from the bubble's top edge.
      CGFloat margin = kInitialBubbleDistanceToEdgeSpacingVertical +
                       bubbleSize.height +
                       kInitialGestureIndicatorToBubbleSpacing;
      return [_gestureIndicator.centerYAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-margin];
    }
    case BubbleArrowDirectionLeading: {
      CGFloat margin;
      if (GestureIndicatorShouldOffsetFromCenter(self.traitCollection)) {
        // Gesture indicator should be center-aligned with the bubble.
        margin = bubbleSize.width / 2;
      } else {
        // Gesture indicator should be `kInitialGestureIndicatorToBubbleSpacing`
        // away from the bubble's trailing edge.
        margin = bubbleSize.width + kInitialGestureIndicatorToBubbleSpacing;
      }
      return [_gestureIndicator.centerXAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:margin];
    }
    case BubbleArrowDirectionTrailing: {
      CGFloat margin;
      if (GestureIndicatorShouldOffsetFromCenter(self.traitCollection)) {
        // Gesture indicator should be center-aligned with the bubble.
        margin = bubbleSize.width / 2;
      } else {
        // Gesture indicator should be `kInitialGestureIndicatorToBubbleSpacing`
        // away from the bubble's leading edge.
        margin = bubbleSize.width + kInitialGestureIndicatorToBubbleSpacing;
      }
      return [_gestureIndicator.centerXAnchor
          constraintEqualToAnchor:self.trailingAnchor
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
      CGFloat offset = kInitialGestureIndicatorToBubbleSpacing +
                       _bubbleView.frame.size.height / 2;
      gestureIndicatorCenterConstraint = [_gestureIndicator.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor
                         constant:GestureIndicatorShouldOffsetFromCenter(
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

#pragma mark - Animation keyframes

// Fades in the gesture indicator, starting with a large size and ends with the
// correct size.
- (void)keyframeThatShowsAndShrinksGestureIndicator {
  for (NSLayoutConstraint* constraint in _gestureIndicatorSizeConstraints) {
    constraint.constant = kGestureIndicatorRadius * 2;
  }
  _gestureIndicator.layer.cornerRadius = kGestureIndicatorRadius;
  _gestureIndicator.alpha =
      UIAccessibilityIsReduceTransparencyEnabled() ? 1.0f : 0.5f;
  [self layoutIfNeeded];
}

// Moves the bubble and the gesture indicator away from the edge of the view
// that the bubble arrow points to.
- (void)keyframeThatSwipes {
  CGRect newFrame = _bubbleView.frame;
  BubbleArrowDirection direction = _bubbleView.direction;
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
  [_bubbleView setArrowHidden:NO animated:YES];

  CGFloat animateDistance = (direction == BubbleArrowDirectionUp ||
                             direction == BubbleArrowDirectionLeading)
                                ? kGestureIndicatorDistanceAnimated
                                : -kGestureIndicatorDistanceAnimated;
  _gestureIndicatorMarginConstraint.constant += animateDistance;
  [self layoutIfNeeded];
}

// Fades out the gesture indicator by expanding its size and decreasing opacity.
- (void)keyframeThatExpandsAndHidesGestureIndicator {
  for (NSLayoutConstraint* constraint in _gestureIndicatorSizeConstraints) {
    constraint.constant = kFadingGestureIndicatorRadius * 2;
  }
  _gestureIndicator.layer.cornerRadius = kFadingGestureIndicatorRadius;
  _gestureIndicator.alpha = 0;
  CGRect newFrame = _bubbleView.frame;
  BubbleArrowDirection direction = _bubbleView.direction;
  switch (direction) {
    case BubbleArrowDirectionUp:
      newFrame.origin.y -= kBubbleDistanceAnimated;
      break;
    case BubbleArrowDirectionDown:
      newFrame.origin.y += kBubbleDistanceAnimated;
      break;
    case BubbleArrowDirectionLeading:
    case BubbleArrowDirectionTrailing:
      if (IsArrowPointingLeft(direction)) {
        newFrame.origin.x -= kBubbleDistanceAnimated;
      } else {
        newFrame.origin.x += kBubbleDistanceAnimated;
      }
      break;
  }
  _bubbleView.frame = newFrame;
  [_bubbleView setArrowHidden:YES animated:YES];
  [self layoutIfNeeded];
  _currentAnimationRepeatCount++;
}

@end
