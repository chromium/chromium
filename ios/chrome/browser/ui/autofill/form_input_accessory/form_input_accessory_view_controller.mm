// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
CGFloat const kInputAccessoryHeight = 44.0f;
}  // namespace autofill

@interface FormInputAccessoryViewController () <
    FormSuggestionViewDelegate,
    ManualFillAccessoryViewControllerDelegate>

// The keyboard replacement view, if any.
@property(nonatomic, weak) UIView* keyboardReplacementView;

// The custom view that should be shown in the input accessory view.
@property(nonatomic, strong) FormInputAccessoryView* inputAccessoryView;

// The leading view with the suggestions in FormInputAccessoryView.
@property(nonatomic, strong) FormSuggestionView* formSuggestionView;

// If this view controller is paused it shouldn't add its views to the keyboard.
@property(nonatomic, getter=isPaused) BOOL paused;

// The manual fill accessory view controller to add at the end of the
// suggestions.
@property(nonatomic, strong, readonly)
    ManualFillAccessoryViewController* manualFillAccessoryViewController;

// Delegate to handle interactions with the manual fill buttons.
@property(nonatomic, readonly, weak)
    id<ManualFillAccessoryViewControllerDelegate>
        manualFillAccessoryViewControllerDelegate;

// Remember last keyboard state to allow resuming properly.
@property(nonatomic, assign) KeyboardState lastKeyboardState;

@end

@implementation FormInputAccessoryViewController

@synthesize addressButtonHidden = _addressButtonHidden;
@synthesize creditCardButtonHidden = _creditCardButtonHidden;
@synthesize formInputNextButtonEnabled = _formInputNextButtonEnabled;
@synthesize formInputPreviousButtonEnabled = _formInputPreviousButtonEnabled;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize passwordButtonHidden = _passwordButtonHidden;

#pragma mark - Life Cycle

- (instancetype)initWithManualFillAccessoryViewControllerDelegate:
    (id<ManualFillAccessoryViewControllerDelegate>)
        manualFillAccessoryViewControllerDelegate {
  self = [super init];
  if (self) {
    _paused = YES;
    _manualFillAccessoryViewControllerDelegate =
        manualFillAccessoryViewControllerDelegate;
    _manualFillAccessoryViewController =
        [[ManualFillAccessoryViewController alloc] initWithDelegate:self];
  }
  return self;
}

// Returns YES if the keyboard constraint view is present. This view is the one
// used to constraint any presented view. iPad always presents in a separate
// popover.
- (BOOL)canPresentView {
  return IsIPadIdiom() || KeyboardObserverHelper.keyboardLayoutGuide;
}

#pragma mark - Public

- (void)presentView:(UIView*)view {
  if (self.paused || ![self canPresentView]) {
    return;
  }
  DCHECK(view);
  DCHECK(!view.superview);
  UIView* keyboardView = KeyboardObserverHelper.keyboardView;
  view.accessibilityViewIsModal = YES;
  [keyboardView.superview addSubview:view];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(view, KeyboardObserverHelper.keyboardLayoutGuide);
  self.keyboardReplacementView = view;
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  view);
}

- (void)lockManualFallbackView {
  [self.formSuggestionView lockTrailingView];
}

- (void)reset {
  [self resetAnimated:YES];
}

#pragma mark - FormInputAccessoryConsumer

- (void)prepareToShowSuggestions {
  // Hides the Manual Fallback icons when there is no proper keyboard to present
  // those views. And shows them if there is a keyboard present.
  // Hidding |manualFillAccessoryViewController|'s view was causing an issue
  // with the Stack Views and Auto Layout in iOS 11, hidding each icon avoids
  // it.
  if ([self canPresentView]) {
    self.manualFillAccessoryViewController.passwordButtonHidden =
        self.passwordButtonHidden;
    self.manualFillAccessoryViewController.addressButtonHidden =
        self.addressButtonHidden;
    self.manualFillAccessoryViewController.creditCardButtonHidden =
        self.creditCardButtonHidden;
  } else {
    self.manualFillAccessoryViewController.passwordButtonHidden = YES;
    self.manualFillAccessoryViewController.addressButtonHidden = YES;
    self.manualFillAccessoryViewController.creditCardButtonHidden = YES;
  }
}

- (void)keyboardWillChangeToState:(KeyboardState)keyboardState {
  self.lastKeyboardState = keyboardState;

  if (!IsIPadIdiom()) {
    // On iPhones, when using a hardware keyboard, for most models, there's no
    // space to show suggestions because of the on-screen menu button.
    self.inputAccessoryView.leadingView.hidden = keyboardState.isHardware;

    // On iPhones when the field is a selector the keyboard becomes a picker.
    // Restore the keyboard in these cases, but allow the user to return to see
    // the info in Manual Fallback.
    if (keyboardState.isPicker) {
      [self resetAnimated:NO];
      [self.keyboardReplacementView removeFromSuperview];
      self.keyboardReplacementView = nil;
      return;
    }
  }

  // Create the views if they don't exist already.
  if (keyboardState.isVisible && !self.inputAccessoryView) {
    [self createFormSuggestionViewIfNeeded];

    self.inputAccessoryView = [[FormInputAccessoryView alloc] init];
    if (IsIPadIdiom()) {
      [self.inputAccessoryView
          setUpWithLeadingView:self.formSuggestionView
            customTrailingView:self.manualFillAccessoryViewController.view];
    } else {
      self.inputAccessoryView.accessibilityViewIsModal = YES;
      self.formSuggestionView.trailingView =
          self.manualFillAccessoryViewController.view;
      [self.inputAccessoryView setUpWithLeadingView:self.formSuggestionView
                                 navigationDelegate:self.navigationDelegate];
      self.inputAccessoryView.nextButton.enabled =
          self.formInputNextButtonEnabled;
      self.inputAccessoryView.previousButton.enabled =
          self.formInputPreviousButtonEnabled;
    }
  }

  if (self.inputAccessoryView) {
    if (!keyboardState.isVisible || keyboardState.isSplit || self.paused) {
      self.inputAccessoryView.hidden = true;
    } else {
      // Make sure the input accessory is there if needed.
      [self prepareToShowSuggestions];
      [self addInputAccessoryViewIfNeeded];
      [self addCustomKeyboardViewIfNeeded];
      self.inputAccessoryView.hidden = false;
    }
  }
}

- (void)showAccessorySuggestions:(NSArray<FormSuggestion*>*)suggestions
                suggestionClient:(id<FormSuggestionClient>)suggestionClient {
  [self createFormSuggestionViewIfNeeded];
  [self.formSuggestionView updateClient:suggestionClient
                            suggestions:suggestions];
  [self addInputAccessoryViewIfNeeded];
}

- (void)restoreOriginalKeyboardView {
  [self.manualFillAccessoryViewController resetAnimated:NO];
  [self removeCustomInputAccessoryView];
  [self.keyboardReplacementView removeFromSuperview];
  self.keyboardReplacementView = nil;
}

- (void)pauseCustomKeyboardView {
  [self removeCustomInputAccessoryView];
  [self.keyboardReplacementView removeFromSuperview];
  self.paused = YES;
}

- (void)continueCustomKeyboardView {
  self.paused = NO;
  // Apply any keyboard state change that happened while this controller was
  // paused.
  [self keyboardWillChangeToState:self.lastKeyboardState];
}

- (void)removeAnimationsOnKeyboardView {
  // Work Around. On focus event, keyboardReplacementView is animated but the
  // keyboard isn't. Cancel the animation to match the keyboard behavior
  if (self.keyboardReplacementView.superview) {
    [self.keyboardReplacementView.layer removeAllAnimations];
  }
}

#pragma mark - Setters

- (void)setPasswordButtonHidden:(BOOL)passwordButtonHidden {
  _passwordButtonHidden = passwordButtonHidden;
  self.manualFillAccessoryViewController.passwordButtonHidden =
      passwordButtonHidden;
}

- (void)setAddressButtonHidden:(BOOL)addressButtonHidden {
  _addressButtonHidden = addressButtonHidden;
  self.manualFillAccessoryViewController.addressButtonHidden =
      addressButtonHidden;
}

- (void)setCreditCardButtonHidden:(BOOL)creditCardButtonHidden {
  _creditCardButtonHidden = creditCardButtonHidden;
  self.manualFillAccessoryViewController.creditCardButtonHidden =
      creditCardButtonHidden;
}

- (void)setFormInputNextButtonEnabled:(BOOL)formInputNextButtonEnabled {
  if (formInputNextButtonEnabled == _formInputNextButtonEnabled) {
    return;
  }
  _formInputNextButtonEnabled = formInputNextButtonEnabled;
  self.inputAccessoryView.nextButton.enabled = _formInputNextButtonEnabled;
}

- (void)setFormInputPreviousButtonEnabled:(BOOL)formInputPreviousButtonEnabled {
  if (formInputPreviousButtonEnabled == _formInputPreviousButtonEnabled) {
    return;
  }
  _formInputPreviousButtonEnabled = formInputPreviousButtonEnabled;
  self.inputAccessoryView.previousButton.enabled =
      _formInputPreviousButtonEnabled;
}

#pragma mark - Private

// Resets this view to its original state. Can be animated.
- (void)resetAnimated:(BOOL)animated {
  [self.formSuggestionView resetContentInsetAndDelegateAnimated:animated];
  [self.manualFillAccessoryViewController resetAnimated:animated];
}

// Create formSuggestionView if not done yet.
- (void)createFormSuggestionViewIfNeeded {
  if (!self.formSuggestionView) {
    self.formSuggestionView = [[FormSuggestionView alloc] init];
    self.formSuggestionView.formSuggestionViewDelegate = self;
  }
}

// Removes the custom views related to the input accessory view.
- (void)removeCustomInputAccessoryView {
  [self.inputAccessoryView removeFromSuperview];
}

- (void)addCustomKeyboardViewIfNeeded {
  if (self.isPaused) {
    return;
  }
  if (self.keyboardReplacementView && !self.keyboardReplacementView.superview) {
    [self presentView:self.keyboardReplacementView];
  }
}

// Adds the inputAccessoryView and the backgroundView (on iPads), if those are
// not already in the hierarchy.
- (void)addInputAccessoryViewIfNeeded {
  if (self.isPaused) {
    return;
  }
  if (self.inputAccessoryView) {
    if (IsIPadIdiom()) {
      // On iPad the keyboard view can change so this updates it when needed.
      UIView* keyboardView = KeyboardObserverHelper.keyboardView;
      if (!keyboardView) {
        return;
      }
      if (self.inputAccessoryView.superview) {
        if (keyboardView == self.inputAccessoryView.superview) {
          return;
        }
        // The keyboard view is a different one.
        [self.manualFillAccessoryViewController resetAnimated:NO];
        [self.inputAccessoryView removeFromSuperview];
      }
      self.inputAccessoryView.translatesAutoresizingMaskIntoConstraints = NO;
      [keyboardView addSubview:self.inputAccessoryView];
      [NSLayoutConstraint activateConstraints:@[
        [self.inputAccessoryView.leadingAnchor
            constraintEqualToAnchor:keyboardView.leadingAnchor],
        [self.inputAccessoryView.trailingAnchor
            constraintEqualToAnchor:keyboardView.trailingAnchor],
        [self.inputAccessoryView.bottomAnchor
            constraintEqualToAnchor:keyboardView.topAnchor],
        [self.inputAccessoryView.heightAnchor
            constraintEqualToConstant:autofill::kInputAccessoryHeight]
      ]];
    } else if (!self.inputAccessoryView.superview) {  // Is not an iPad.
      UIResponder* firstResponder = GetFirstResponder();
      if (firstResponder.inputAccessoryView) {
        [firstResponder.inputAccessoryView addSubview:self.inputAccessoryView];
        AddSameConstraints(self.inputAccessoryView,
                           firstResponder.inputAccessoryView);
      }
    }
  }
}

#pragma mark - ManualFillAccessoryViewControllerDelegate

- (void)keyboardButtonPressed {
  [self.manualFillAccessoryViewControllerDelegate keyboardButtonPressed];
}

- (void)accountButtonPressed:(UIButton*)sender {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenProfiles",
                           self.formSuggestionView.suggestions.count);
  [self.manualFillAccessoryViewControllerDelegate accountButtonPressed:sender];
}

- (void)cardButtonPressed:(UIButton*)sender {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenCreditCards",
                           self.formSuggestionView.suggestions.count);
  [self.manualFillAccessoryViewControllerDelegate cardButtonPressed:sender];
}

- (void)passwordButtonPressed:(UIButton*)sender {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenPasswords",
                           self.formSuggestionView.suggestions.count);
  [self.manualFillAccessoryViewControllerDelegate passwordButtonPressed:sender];
}

#pragma mark - FormSuggestionViewDelegate

- (void)formSuggestionViewShouldResetFromPull:
    (FormSuggestionView*)formSuggestionView {
  base::RecordAction(base::UserMetricsAction("ManualFallback_ClosePull"));
  // The pull gesture has the same effect as when the keyboard button is
  // pressed.
  [self.manualFillAccessoryViewControllerDelegate keyboardButtonPressed];
  [self.manualFillAccessoryViewController resetAnimated:YES];
}

@end
