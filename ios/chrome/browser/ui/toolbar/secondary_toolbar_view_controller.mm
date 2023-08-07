// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_keyboard_state_provider.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {
// This is how many bits UIViewAnimationCurve needs to be shifted to be in
// UIViewAnimationOptions format. Must match the one in UIView.h.
const NSUInteger kUIViewAnimationCurveToOptionsShift = 16;
}  // namespace

@interface SecondaryToolbarViewController ()

/// Redefined to be a `SecondaryToolbarView`.
@property(nonatomic, strong) SecondaryToolbarView* view;

@end

@implementation SecondaryToolbarViewController {
  /// The disabler created when the keyboard is visible.
  std::unique_ptr<ScopedFullscreenDisabler> _keyboardDisabler;
}

@dynamic view;

- (void)loadView {
  self.view =
      [[SecondaryToolbarView alloc] initWithButtonFactory:self.buttonFactory];
  DCHECK(self.layoutGuideCenter);
  [self.layoutGuideCenter referenceView:self.view
                              underName:kSecondaryToolbarGuide];

  if (IsBottomOmniboxSteadyStateEnabled()) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillHide:)
               name:UIKeyboardWillHideNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillShow:)
               name:UIKeyboardWillShowNotification
             object:nil];
  }
}

- (void)disconnect {
  _fullscreenController = nullptr;
  _keyboardDisabler = nullptr;
}

#pragma mark - AdaptiveToolbarViewController

- (void)collapsedToolbarButtonTapped {
  [super collapsedToolbarButtonTapped];

  if ([self.view.locationBarKeyboardConstraint isActive]) {
    CHECK(IsBottomOmniboxSteadyStateEnabled());
    CHECK([self hasOmnibox]);
    UIResponder* responder = GetFirstResponder();
    [responder resignFirstResponder];
  }
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  [super updateForFullscreenProgress:progress];

  CGFloat alphaValue = fmax(progress * 1.1 - 0.1, 0);
  if (IsBottomOmniboxSteadyStateEnabled()) {
    self.view.buttonStackView.alpha = alphaValue;
  }

  self.view.locationBarTopConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:progress];
}

#pragma mark - UIKeyboardNotification

- (void)keyboardWillShow:(NSNotification*)notification {
  if (![self hasOmnibox]) {
    return;
  }
  [self constraintToKeyboard:YES withNotification:notification];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  [self constraintToKeyboard:NO withNotification:notification];
}

#pragma mark - Private

/// Returns the vertical margin to the location bar based on fullscreen
/// `progress`, aligned to the nearest pixel.
- (CGFloat)verticalMarginForLocationBarForFullscreenProgress:(CGFloat)progress {
  const CGFloat clampedFontSizeMultiplier = ToolbarClampedFontSizeMultiplier(
      self.traitCollection.preferredContentSizeCategory);

  return AlignValueToPixel(
      (kBottomAdaptiveLocationBarTopMargin * progress +
       kBottomAdaptiveLocationBarVerticalMarginFullscreen * (1 - progress)) *
          clampedFontSizeMultiplier +
      (clampedFontSizeMultiplier - 1) * kLocationBarVerticalMarginDynamicType);
}

/// Collapses secondary toolbar when it's moved above the keyboard.
- (void)collapseForKeyboard {
  // Disable fullscreen because:
  // - It interfers with the animation when moving the secondary toolbar above
  // the keyboard.
  // - Fullscreen should not resize the toolbar it's above the keyboard.
  if (_fullscreenController) {
    _keyboardDisabler =
        std::make_unique<ScopedFullscreenDisabler>(_fullscreenController);
    _fullscreenController->ForceEnterFullscreen();
  }
  self.view.locationBarTopConstraint.constant = 0;
}

/// Resets secondary toolbar when it's detached from the keyboard.
- (void)removeFromKeyboard {
  if (_fullscreenController) {
    _fullscreenController->ExitFullscreenWithoutAnimation();
    _keyboardDisabler = nullptr;
  }
}

/// Updates keyboard constraints with `notification`. When
/// `constraintToKeyboard`, the toolbar is collapsed above the keyboard.
- (void)constraintToKeyboard:(BOOL)constraintToKeyboard
            withNotification:(NSNotification*)notification {
  CHECK(IsBottomOmniboxSteadyStateEnabled());

  if (constraintToKeyboard) {
    if ([self.keyboardStateProvider keyboardIsActiveForWebContent]) {
      // Enable the constraint only when the keyboard is showing for web
      // content. This will not evaluate to true each time the keyboard's frame
      // is updating. Thus, update the keyboard's frame even if this is false.
      if (![self.view.locationBarKeyboardConstraint isActive]) {
        self.view.locationBarKeyboardConstraint.active = YES;
        [self collapseForKeyboard];
      }
    }
    const CGRect keyboardFrame =
        [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    const CGRect keyboardInFrame =
        CGRectIntersection(keyboardFrame, self.view.window.bounds);
    const CGFloat keyboardHeight = CGRectGetHeight(keyboardInFrame);

    NSTimeInterval duration =
        [notification.userInfo[UIKeyboardAnimationDurationUserInfoKey]
            doubleValue];
    UIViewAnimationCurve curve = static_cast<UIViewAnimationCurve>(
        [notification.userInfo[UIKeyboardAnimationCurveUserInfoKey]
            integerValue]);
    UIViewAnimationOptions options = curve
                                     << kUIViewAnimationCurveToOptionsShift;
    [UIView animateWithDuration:duration
                          delay:0
                        options:options
                     animations:^{
                       self.view.locationBarKeyboardConstraint.constant =
                           keyboardHeight;
                       [self.view layoutIfNeeded];
                     }
                     completion:nil];
  } else if ([self.view.locationBarKeyboardConstraint isActive]) {
    self.view.locationBarKeyboardConstraint.active = NO;
    [self removeFromKeyboard];
  }
  [self.view layoutIfNeeded];
}

@end
