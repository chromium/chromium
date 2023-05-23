// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/branding_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/features.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/branding_view_controller_delegate.h"
#import "ios/chrome/common/button_configuration_util.h"

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

@interface BrandingViewController ()

// The start time of the last or ongoing animation.
@property(nonatomic, assign) base::TimeTicks lastAnimationStartTime;

// Whether the animation should be shown; should be checked each time the
// animation is visible.
@property(nonatomic, readonly) BOOL shouldAnimate;

@end

@implementation BrandingViewController

#pragma mark - Life Cycle

- (void)loadView {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  if (IsUIButtonConfigurationEnabled()) {
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets =
        NSDirectionalEdgeInsetsMake(0, kLeadingInset, 0, 0);
    button.configuration = buttonConfiguration;
  } else {
    UIEdgeInsets imageEdgeInsets = UIEdgeInsetsMake(0, kLeadingInset, 0, 0);
    SetImageEdgeInsets(button, imageEdgeInsets);
  }

  button.accessibilityIdentifier = kBrandingButtonAXId;
  button.isAccessibilityElement = NO;  // Prevents VoiceOver users from tap.
  button.translatesAutoresizingMaskIntoConstraints = NO;
  self.view = button;
  [self configureBrandingWithImageName:@"fullcolor_branding_icon"];

  // Adds keyboard popup listener to show animation when keyboard is fully
  // settled.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(onKeyboardAnimationComplete)
             name:UIKeyboardDidShowNotification
           object:nil];
}

#pragma mark - Accessors

- (BOOL)shouldAnimate {
  if (![self.delegate brandingIconShouldPerformPopAnimation]) {
    return NO;
  }
  const base::TimeTicks lastAnimationStartTime = self.lastAnimationStartTime;
  return lastAnimationStartTime.is_null() ||
         (base::TimeTicks::Now() - lastAnimationStartTime) >
             kMinTimeIntervalBetweenAnimations;
}

- (void)setDelegate:(id<BrandingViewControllerDelegate>)delegate {
  _delegate = delegate;
  if (_delegate != nil) {
    [base::mac::ObjCCast<UIButton>(self.view)
               addTarget:_delegate
                  action:@selector(brandingIconPressed)
        forControlEvents:UIControlEventTouchUpInside];
  }
}

#pragma mark - Private

// Add the branding image with the correct size, and make sure it persists
// across different button states.
- (void)configureBrandingWithImageName:(NSString*)name {
  UIImage* logo = [[UIImage imageNamed:name]
      imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  UIButton* button = base::mac::ObjCCast<UIButton>(self.view);
  [button setImage:logo forState:UIControlStateNormal];
  [button setImage:logo forState:UIControlStateHighlighted];
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;
}

// Check if the branding icon is visible and should perform an animation, and do
// so if it should.
- (void)onKeyboardAnimationComplete {
  // Branding is invisible.
  if (self.view.window == nil || self.view.hidden) {
    return;
  }
  // Branding is visible; animate if it should.
  DCHECK(self.delegate);
  if (!self.shouldAnimate) {
    return;
  }
  // The "pop" animation should start after a slight timeout.
  __weak BrandingViewController* weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf performPopAnimation];
      }),
      kAnimationWaitTime);
}

// Performs the "pop" animation. This includes a quick enlarging of the icon
// and shrinking it back to the original size, and if finishes successfully,
// also notifies the delegate on completion.
- (void)performPopAnimation {
  self.lastAnimationStartTime = base::TimeTicks::Now();
  __weak BrandingViewController* weakSelf = self;
  [UIView animateWithDuration:kTimeToAnimate.InSecondsF() / 2
      // Scale up the icon.
      animations:^{
        // Resets the transform to original state before animation starts.
        weakSelf.view.transform = CGAffineTransformIdentity;
        weakSelf.view.transform = CGAffineTransformScale(
            weakSelf.view.transform, kAnimationScale, kAnimationScale);
      }
      completion:^(BOOL finished) {
        if (!finished) {
          return;
        }
        // Scale the icon back down.
        [UIView animateWithDuration:kTimeToAnimate.InSecondsF() / 2
            animations:^{
              weakSelf.view.transform = CGAffineTransformIdentity;
            }
            completion:^(BOOL innerFinished) {
              if (innerFinished) {
                [weakSelf.delegate brandingIconDidPerformPopAnimation];
              }
            }];
      }];
}

@end
