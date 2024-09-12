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
#import "ios/chrome/browser/ui/toolbar/public/toolbar_height_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_keyboard_state_provider.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"
#import "ios/chrome/common/ui/util/ui_util.h"

@interface SecondaryToolbarViewController ()

/// Redefined to be a `SecondaryToolbarView`.
@property(nonatomic, strong) SecondaryToolbarView* view;

@end

@implementation SecondaryToolbarViewController

@dynamic view;

- (void)loadView {
  self.view =
      [[SecondaryToolbarView alloc] initWithButtonFactory:self.buttonFactory];
  DCHECK(self.layoutGuideCenter);
  [self.layoutGuideCenter referenceView:self.view
                              underName:kSecondaryToolbarGuide];

  if (IsBottomOmniboxAvailable()) {
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
}

#pragma mark - AdaptiveToolbarViewController

- (void)collapsedToolbarButtonTapped {
  [super collapsedToolbarButtonTapped];

  if ([self.view.locationBarKeyboardConstraint isActive]) {
    // When the bottom omnibox is collapsed above the keyboard, it's positioned
    // behind an `omniboxTypingShield` (transparent view) in the
    // `formInputAccessoryView`. This allow the keyboard to know about the size
    // of the omnibox (crbug.com/1490601).
    // When voice over is off, tapping the collapsed bottom omnibox interacts
    // with the `omniboxTypingShield`. The logic to dismiss the keyboard is
    // handled in `formInputAccessoryViewHandler`. However, the typing shield
    // has `isAccessibilityElement` equals NO to let the user interact with the
    // omnibox on voice over. In this mode, logic to dismiss the keyboard is
    // handled here in `SecondaryToolbarViewController`.
    CHECK([self hasOmnibox]);
    UIResponder* responder = GetFirstResponder();
    [responder resignFirstResponder];
  }
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  [super updateForFullscreenProgress:progress];

  CGFloat alphaValue = fmax(progress * 1.1 - 0.1, 0);
  if (IsBottomOmniboxAvailable()) {
    self.view.buttonStackView.alpha = alphaValue;
  }

  self.view.locationBarTopConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:progress];
}

#pragma mark - SecondaryToolbarConsumer

- (void)makeTranslucent {
  [self.view makeTranslucent];
}

- (void)makeOpaque {
  [self.view makeOpaque];
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

  const BOOL hasBottomSafeArea = self.view.window.safeAreaInsets.bottom;
  const CGFloat fullscreenMargin =
      hasBottomSafeArea ? kBottomAdaptiveLocationBarVerticalMarginFullscreen
                        : 0;

  return AlignValueToPixel((kBottomAdaptiveLocationBarTopMargin * progress +
                            fullscreenMargin * (1 - progress)) *
                               clampedFontSizeMultiplier +
                           (clampedFontSizeMultiplier - 1) *
                               kLocationBarVerticalMarginDynamicType);
}

/// Collapses secondary toolbar when it's moved above the keyboard.
- (void)collapseForKeyboard {
  if (_fullscreenController) {
    _fullscreenController->EnterForceFullscreenMode();
  }
  self.view.locationBarTopConstraint.constant = 0;
  self.view.bottomSeparator.alpha = 1.0;
  [self.toolbarHeightDelegate secondaryToolbarMovedAboveKeyboard];
}

/// Resets secondary toolbar when it's detached from the keyboard.
- (void)removeFromKeyboard {
  if (_fullscreenController) {
    _fullscreenController->ExitForceFullscreenMode();
  }
  self.view.bottomSeparator.alpha = 0.0;
  [self.toolbarHeightDelegate secondaryToolbarRemovedFromKeyboard];
}

/// Updates keyboard constraints with `notification`. When
/// `constraintToKeyboard`, the toolbar is collapsed above the keyboard.
- (void)constraintToKeyboard:(BOOL)constraintToKeyboard
            withNotification:(NSNotification*)notification {
  if (constraintToKeyboard) {
    if ([self.keyboardStateProvider keyboardIsActiveForWebContent]) {
      // Enable the constraint only when the keyboard is showing for web
      // content. This will not evaluate to true each time the keyboard's frame
      // is updating. Thus, update the keyboard's frame even if this is false.
      if (![self.view.locationBarKeyboardConstraint isActive]) {
        self.view.locationBarKeyboardConstraint.active = YES;
        [self collapseForKeyboard];
        [self.view layoutIfNeeded];
      }
    }
  } else if ([self.view.locationBarKeyboardConstraint isActive]) {
    self.view.locationBarKeyboardConstraint.active = NO;
    [self removeFromKeyboard];
    [self.view layoutIfNeeded];
  }
}

@end
