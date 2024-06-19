// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_view_controller_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// The left margin of the branding logo, if visible.
constexpr CGFloat kLeadingInset = 8;
// The size of the logo image.
constexpr CGFloat kLogoSize = 24;
// The scale used by the "pop" animation.
constexpr CGFloat kPopAnimationScale = ((CGFloat)4) / 3;
// Wait time after the keyboard settles into place to perform pop animation.
constexpr base::TimeDelta kPopAnimationWaitTime = base::Milliseconds(200);
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
  base::TimeTicks _lastPopAnimationStartTime;
  // Horizontal constraints that are used for animation purpose.
  NSLayoutConstraint* _leadingConstraint;
  NSLayoutConstraint* _widthConstraintWhenHidingBranding;
  // A boolean representing visibility of the keyboard.
  BOOL _keyboardVisible;
}

@synthesize visible = _visible;
@synthesize shouldPerformPopAnimation = _shouldPerformPopAnimation;

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  // Configure the branding.
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.accessibilityIdentifier = kBrandingButtonAXId;
  button.isAccessibilityElement = NO;  // Prevents VoiceOver users from tap.
  button.translatesAutoresizingMaskIntoConstraints = NO;

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImage* logo = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kLogoSize));
#else
  UIImage* logo = CustomSymbolWithPointSize(kChromeProductSymbol, kLogoSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

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
         selector:@selector(keyboardWillShow)
             name:UIKeyboardWillShowNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardDidShow)
             name:UIKeyboardDidShowNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardDidHide)
             name:UIKeyboardDidHideNotification
           object:nil];
}

#pragma mark - Keyboard Event Handlers

// Called right before the keyboard is visible. This method adds the autofill
// branding to the view if it should be visible, and otherwise remove it from
// the view hierarchy.
- (void)keyboardWillShow {
  // Early return if the keyboard was not hidden prior to the animation. Note
  // that this method may still be called if the user consecutively taps on two
  // input fields.
  if (_keyboardVisible) {
    return;
  }

  // Add or remove the branding icon to keyboard accessories accordingly.
  if (!_widthConstraintWhenHidingBranding) {
    _widthConstraintWhenHidingBranding =
        [self.view.widthAnchor constraintEqualToConstant:0];
  }
  BOOL shouldShow = self.visible && self.keyboardAccessoryVisible;
  if (shouldShow && _brandingIcon.superview == nil) {
    [self.view addSubview:_brandingIcon];
    _widthConstraintWhenHidingBranding.active = NO;
    AddSameConstraintsToSides(
        _brandingIcon, self.view,
        LayoutSides::kTop | LayoutSides::kBottom | LayoutSides::kTrailing);
    _leadingConstraint = [_brandingIcon.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kLeadingInset];
    _leadingConstraint.active = YES;
  } else if (!shouldShow) {
    [self hideBranding];
  }
}

// Update keybaord visibility, check if the branding icon is visible and should
// perform an animation, and do so if it should.
- (void)keyboardDidShow {
  // Early return if the keyboard was not hidden prior to the animation. Note
  // that this method may still be called if the user consecutively taps on two
  // input fields.
  if (_keyboardVisible) {
    return;
  }
  _keyboardVisible = YES;

  // Early return if branding is invisible.
  if (self.view.window == nil || _brandingIcon.superview == nil) {
    return;
  }
  [self.delegate brandingIconDidShow];
  const base::TimeTicks lastAnimationStartTime = _lastPopAnimationStartTime;
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
        kPopAnimationWaitTime);
  }
}

// Updates keyboard visibility when the keyboard is hidden.
- (void)keyboardDidHide {
  _keyboardVisible = NO;
}

#pragma mark - Private

// Hides the branding icon from the view. This does NOT mean that the branding
// would not show again when the keyboard pops up next time.
- (void)hideBranding {
  [_brandingIcon removeFromSuperview];
  _leadingConstraint.active = NO;
  _leadingConstraint = nil;
  _widthConstraintWhenHidingBranding.constant = 0;
  _widthConstraintWhenHidingBranding.active = YES;
}

// Method that is invoked when the user taps the branding icon.
- (void)onBrandingTapped {
  [_delegate brandingIconDidPress];
}

// Performs the "pop" animation. This includes a quick enlarging of the icon
// and shrinking it back to the original size, and if finishes successfully,
// also notifies the delegate on completion.
- (void)performPopAnimation {
  _lastPopAnimationStartTime = base::TimeTicks::Now();
  __weak UIButton* weakBranding = _brandingIcon;
  __weak id<BrandingViewControllerDelegate> weakDelegate = self.delegate;
  [UIView animateWithDuration:kTimeToAnimate.InSecondsF() / 2
      // Scale up the icon.
      animations:^{
        weakBranding.transform = CGAffineTransformScale(
            CGAffineTransformIdentity, kPopAnimationScale, kPopAnimationScale);
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

@end
