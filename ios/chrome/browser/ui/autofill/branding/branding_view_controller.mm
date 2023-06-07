// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/branding/branding_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/branding/branding_view_controller_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The left margin of the branding logo, if visible.
constexpr CGFloat kLeadingInset = 10;
// The scale used by the "pop" animation.
constexpr CGFloat kAnimationScale = ((CGFloat)4) / 3;
// Wait time after the keyboard settles into place to perform pop animation.
constexpr base::TimeDelta kAnimationWaitTime = base::Milliseconds(200);
// Time it takes the "pop" animation to perform.
constexpr base::TimeDelta kTimeToAnimate = base::Milliseconds(400);
// Minimum time interval between two animations.
constexpr base::TimeDelta kMinTimeIntervalBetweenAnimations = base::Seconds(3);
// Accessibility ID of the view.
constexpr NSString* kBrandingButtonAXId = @"kBrandingButtonAXId";
}  // namespace

@implementation BrandingViewController {
  // Button that shows the branding.
  UIButton* _brandingIcon;
  // The start time of the last or ongoing animation.
  base::TimeTicks _lastAnimationStartTime;
  // A constraint of the view that should be activated when the branding is
  // invisible.
  NSLayoutConstraint* _constraintToHideView;
}

@synthesize visible = _visible;
@synthesize shouldPerformPopAnimation = _shouldPerformPopAnimation;

#pragma mark - Life Cycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  // Configure the branding.
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.accessibilityIdentifier = kBrandingButtonAXId;
  button.isAccessibilityElement = NO;  // Prevents VoiceOver users from tap.
  button.translatesAutoresizingMaskIntoConstraints = NO;
  UIImage* logo = [[UIImage imageNamed:@"fullcolor_branding_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  [button setImage:logo forState:UIControlStateNormal];
  [button setImage:logo forState:UIControlStateHighlighted];
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;
  [button addTarget:self
                action:@selector(onBrandingTapped)
      forControlEvents:UIControlEventTouchUpInside];
  _brandingIcon = button;

  // Adds keyboard popup listener to show animation when keyboard is fully
  // settled.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(onKeyboardAnimationStart)
             name:UIKeyboardWillShowNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(onKeyboardAnimationComplete)
             name:UIKeyboardDidShowNotification
           object:nil];
}

#pragma mark - Keyboard Event Handlers

// Called right before the keyboard is visible. This method adds the autofill
// branding to the view if it should be visible, and otherwise remove it from
// the view hierarchy.
- (void)onKeyboardAnimationStart {
  if (!_constraintToHideView) {
    _constraintToHideView = [self.view.widthAnchor constraintEqualToConstant:0];
  }
  BOOL shouldShow = self.visible && self.keyboardAccessoryVisible;
  if (shouldShow && _brandingIcon.superview == nil) {
    [self.view addSubview:_brandingIcon];
    _constraintToHideView.active = NO;
    AddSameConstraintsWithInsets(
        _brandingIcon, self.view,
        NSDirectionalEdgeInsetsMake(0, kLeadingInset, 0, 0));
  } else if (!shouldShow) {
    [_brandingIcon removeFromSuperview];
    _constraintToHideView.active = YES;
  }
}

// Check if the branding icon is visible and should perform an animation, and do
// so if it should.
- (void)onKeyboardAnimationComplete {
  // Early return if branding is invisible.
  if (self.view.window == nil || _brandingIcon.hidden) {
    return;
  }
  const base::TimeTicks lastAnimationStartTime = _lastAnimationStartTime;
  BOOL shouldPerformPopAnimation =
      self.shouldPerformPopAnimation &&
      (lastAnimationStartTime.is_null() ||
       (base::TimeTicks::Now() - lastAnimationStartTime) >
           kMinTimeIntervalBetweenAnimations);
  // Branding is visible; animate if it should.
  if (shouldPerformPopAnimation) {
    // The "pop" animation should start after a slight timeout.
    __weak BrandingViewController* weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf performPopAnimation];
        }),
        kAnimationWaitTime);
  }
}

#pragma mark - Private

// Method that is invoked when the user taps the branding icon.
- (void)onBrandingTapped {
  [_delegate brandingIconPressed];
}

// Performs the "pop" animation. This includes a quick enlarging of the icon
// and shrinking it back to the original size, and if finishes successfully,
// also notifies the delegate on completion.
- (void)performPopAnimation {
  _lastAnimationStartTime = base::TimeTicks::Now();
  __weak UIButton* weakBranding = _brandingIcon;
  __weak id<BrandingViewControllerDelegate> weakDelegate = self.delegate;
  [UIView animateWithDuration:kTimeToAnimate.InSecondsF() / 2
      // Scale up the icon.
      animations:^{
        weakBranding.transform = CGAffineTransformScale(
            CGAffineTransformIdentity, kAnimationScale, kAnimationScale);
      }
      completion:^(BOOL finished) {
        if (!finished) {
          return;
        }
        // Scale the icon back down.
        [UIView animateWithDuration:kTimeToAnimate.InSecondsF() / 2
            animations:^{
              weakBranding.transform = CGAffineTransformIdentity;
            }
            completion:^(BOOL innerFinished) {
              if (innerFinished) {
                [weakDelegate brandingIconDidPerformPopAnimation];
              }
            }];
      }];
}

// TODO(crbug.com/1447909): Implement method for "exit to the left" animation.

@end
