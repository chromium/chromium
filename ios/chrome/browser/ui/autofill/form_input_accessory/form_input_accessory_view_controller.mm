// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/form_suggestion_client.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Delay between the time the view is shown, and the time the suggestion label
// is highlighted.
const NSTimeInterval kAnimateSuggestionLabelDelay = 1.0f;
}

@interface FormInputAccessoryViewController () <
    FormSuggestionViewDelegate,
    ManualFillAccessoryViewControllerDelegate>

// The leading view with the suggestions in FormInputAccessoryView.
@property(nonatomic, strong) FormSuggestionView* formSuggestionView;

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
    _manualFillAccessoryViewControllerDelegate =
        manualFillAccessoryViewControllerDelegate;
    _manualFillAccessoryViewController =
        [[ManualFillAccessoryViewController alloc] initWithDelegate:self];
  }
  return self;
}

- (void)loadView {
  [self createFormSuggestionViewIfNeeded];

  FormInputAccessoryView* formInputAccessoryView =
      [[FormInputAccessoryView alloc] init];
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [formInputAccessoryView
        setUpWithLeadingView:self.formSuggestionView
          customTrailingView:self.manualFillAccessoryViewController.view];
  } else {
    formInputAccessoryView.accessibilityViewIsModal = YES;
    self.formSuggestionView.trailingView =
        self.manualFillAccessoryViewController.view;
    [formInputAccessoryView setUpWithLeadingView:self.formSuggestionView
                              navigationDelegate:self.navigationDelegate];
    formInputAccessoryView.nextButton.enabled = self.formInputNextButtonEnabled;
    formInputAccessoryView.previousButton.enabled =
        self.formInputPreviousButtonEnabled;
  }
  self.view = formInputAccessoryView;
}

// The custom view that should be shown in the input accessory view.
- (FormInputAccessoryView*)formInputAccessoryView {
  return base::mac::ObjCCastStrict<FormInputAccessoryView>(self.view);
}

#pragma mark - Public

- (void)lockManualFallbackView {
  [self.formSuggestionView lockTrailingView];
}

- (void)reset {
  [self resetAnimated:YES];
}

#pragma mark - FormInputAccessoryConsumer

- (void)showAccessorySuggestions:(NSArray<FormSuggestion*>*)suggestions {
  [self createFormSuggestionViewIfNeeded];
  [self.formSuggestionView updateSuggestions:suggestions];
}

- (void)animateSuggestionLabel {
  __weak FormSuggestionView* weakSuggestionView = self.formSuggestionView;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    (int64_t)(kAnimateSuggestionLabelDelay * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [weakSuggestionView animateSuggestionLabel];
      });
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
  self.formInputAccessoryView.nextButton.enabled = _formInputNextButtonEnabled;
}

- (void)setFormInputPreviousButtonEnabled:(BOOL)formInputPreviousButtonEnabled {
  if (formInputPreviousButtonEnabled == _formInputPreviousButtonEnabled) {
    return;
  }
  _formInputPreviousButtonEnabled = formInputPreviousButtonEnabled;
  self.formInputAccessoryView.previousButton.enabled =
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

- (void)formSuggestionView:(FormSuggestionView*)formSuggestionView
       didAcceptSuggestion:(FormSuggestion*)suggestion {
  [self.formSuggestionClient didSelectSuggestion:suggestion];
}

- (void)formSuggestionViewShouldResetFromPull:
    (FormSuggestionView*)formSuggestionView {
  base::RecordAction(base::UserMetricsAction("ManualFallback_ClosePull"));
  // The pull gesture has the same effect as when the keyboard button is
  // pressed.
  [self.manualFillAccessoryViewControllerDelegate keyboardButtonPressed];
  [self.manualFillAccessoryViewController resetAnimated:YES];
}

@end
