// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/form_suggestion_client.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/branding/branding_view_controller.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_suggestion_view.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

@interface FormInputAccessoryViewController () <
    FormSuggestionViewDelegate,
    ManualFillAccessoryViewControllerDelegate>

// The leading view that contains the branding and form suggestions.
@property(nonatomic, strong) UIStackView* leadingView;

// A BOOL value indicating whether any form accessory is visible. If YES, at
// lease one form accessory is visible.
@property(nonatomic, readonly, getter=isFormAccessoryVisible)
    BOOL formAccessoryVisible;

// The view with the suggestions in FormInputAccessoryView.
@property(nonatomic, strong) FormSuggestionView* formSuggestionView;

// The manual fill accessory view controller to add at the end of the
// suggestions.
@property(nonatomic, strong, readonly)
    ManualFillAccessoryViewController* manualFillAccessoryViewController;

// Delegate to handle interactions with the manual fill buttons.
@property(nonatomic, readonly, weak)
    id<ManualFillAccessoryViewControllerDelegate>
        manualFillAccessoryViewControllerDelegate;

// The ID of the field that was last announced by VoiceOver.
@property(nonatomic, assign) autofill::FieldRendererId lastAnnouncedFieldId;

@end

@implementation FormInputAccessoryViewController {
  // Is the preferred omnibox position at the bottom.
  BOOL _isBottomOmnibox;
}

@synthesize addressButtonHidden = _addressButtonHidden;
@synthesize creditCardButtonHidden = _creditCardButtonHidden;
@synthesize formInputNextButtonEnabled = _formInputNextButtonEnabled;
@synthesize formInputPreviousButtonEnabled = _formInputPreviousButtonEnabled;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize passwordButtonHidden = _passwordButtonHidden;
@synthesize suggestionType = _suggestionType;
@synthesize currentFieldId = _currentFieldId;

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
    [self addChildViewController:_manualFillAccessoryViewController];
  }
  return self;
}

- (void)loadView {
  [self createFormSuggestionViewIfNeeded];

  FormInputAccessoryView* formInputAccessoryView =
      [[FormInputAccessoryView alloc] init];

  // Sets up leading view.
  self.leadingView = [[UIStackView alloc] init];
  self.leadingView.axis = UILayoutConstraintAxisHorizontal;

  [self addChildViewController:self.brandingViewController];
  [self.leadingView addArrangedSubview:self.brandingViewController.view];
  [self.brandingViewController didMoveToParentViewController:self];
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;

  [self.leadingView addArrangedSubview:self.formSuggestionView];

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [formInputAccessoryView
        setUpWithLeadingView:self.leadingView
          customTrailingView:self.manualFillAccessoryViewController.view];
  } else {
    formInputAccessoryView.accessibilityViewIsModal = YES;
    self.formSuggestionView.trailingView =
        self.manualFillAccessoryViewController.view;
    [formInputAccessoryView setUpWithLeadingView:self.leadingView
                              navigationDelegate:self.navigationDelegate];
    formInputAccessoryView.nextButton.enabled = self.formInputNextButtonEnabled;
    formInputAccessoryView.previousButton.enabled =
        self.formInputPreviousButtonEnabled;
  }
  self.view = formInputAccessoryView;
}

// The custom view that should be shown in the input accessory view.
- (FormInputAccessoryView*)formInputAccessoryView {
  return base::apple::ObjCCastStrict<FormInputAccessoryView>(self.view);
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (IsBottomOmniboxSteadyStateEnabled()) {
    [self updateOmniboxTypingShieldVisibility];
  }
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
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;
  [self announceVoiceOverMessageIfNeeded:[suggestions count]];
}

- (void)newOmniboxPositionIsBottom:(BOOL)isBottomOmnibox {
  _isBottomOmnibox = isBottomOmnibox;
  [self updateOmniboxTypingShieldVisibility];
}

#pragma mark - Getter

- (BOOL)isFormAccessoryVisible {
  return !(self.manualFillAccessoryViewController.allButtonsHidden &&
           self.formSuggestionView.suggestions.count == 0);
}

#pragma mark - Setters

- (void)setPasswordButtonHidden:(BOOL)passwordButtonHidden {
  _passwordButtonHidden = passwordButtonHidden;
  self.manualFillAccessoryViewController.passwordButtonHidden =
      passwordButtonHidden;
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;
}

- (void)setAddressButtonHidden:(BOOL)addressButtonHidden {
  _addressButtonHidden = addressButtonHidden;
  self.manualFillAccessoryViewController.addressButtonHidden =
      addressButtonHidden;
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;
}

- (void)setCreditCardButtonHidden:(BOOL)creditCardButtonHidden {
  _creditCardButtonHidden = creditCardButtonHidden;
  self.manualFillAccessoryViewController.creditCardButtonHidden =
      creditCardButtonHidden;
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;
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
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;
}

// Creates formSuggestionView if not done yet.
- (void)createFormSuggestionViewIfNeeded {
  if (!self.formSuggestionView) {
    self.formSuggestionView = [[FormSuggestionView alloc] init];
    self.formSuggestionView.formSuggestionViewDelegate = self;
    self.formSuggestionView.layoutGuideCenter = self.layoutGuideCenter;
    self.formSuggestionView.translatesAutoresizingMaskIntoConstraints = NO;
  }
}

// Sets up and posts the VoiceOver message that announces the presence of
// suggestions above the keyboard. The message should be announced when a new
// field enters edit mode and has suggestions available.
- (void)announceVoiceOverMessageIfNeeded:(int)suggestionCount {
  if (UIAccessibilityIsVoiceOverRunning() && suggestionCount > 0 &&
      self.lastAnnouncedFieldId != _currentFieldId) {
    std::u16string suggestionTypeString;
    switch (_suggestionType) {
      case autofill::PopupType::kAddresses:
        suggestionTypeString = l10n_util::GetPluralStringFUTF16(
            IDS_IOS_AUTOFILL_ADDRESS_SUGGESTIONS_AVAILABLE_ACCESSIBILITY_ANNOUNCEMENT,
            suggestionCount);
        break;
      case autofill::PopupType::kPasswords:
        suggestionTypeString = l10n_util::GetPluralStringFUTF16(
            IDS_IOS_AUTOFILL_PASSWORD_SUGGESTIONS_AVAILABLE_ACCESSIBILITY_ANNOUNCEMENT,
            suggestionCount);
        break;
      case autofill::PopupType::kCreditCards:
      case autofill::PopupType::kIbans:
        suggestionTypeString = l10n_util::GetPluralStringFUTF16(
            IDS_IOS_AUTOFILL_PAYMENT_METHOD_SUGGESTIONS_AVAILABLE_ACCESSIBILITY_ANNOUNCEMENT,
            suggestionCount);
        break;
      case autofill::PopupType::kPersonalInformation:
        suggestionTypeString = l10n_util::GetPluralStringFUTF16(
            IDS_IOS_AUTOFILL_PROFILE_SUGGESTIONS_AVAILABLE_ACCESSIBILITY_ANNOUNCEMENT,
            suggestionCount);
        break;
      case autofill::PopupType::kUnspecified:
        return;
    }

    // VoiceOver message setup with
    // UIAccessibilitySpeechAttributeQueueAnnouncement attribute so it doesn't
    // interrupt another message.
    NSMutableDictionary* attributes = [NSMutableDictionary dictionary];
    [attributes setObject:@YES
                   forKey:UIAccessibilitySpeechAttributeQueueAnnouncement];
    NSMutableAttributedString* suggestionsVoiceOverMessage =
        [[NSMutableAttributedString alloc]
            initWithString:base::SysUTF16ToNSString(suggestionTypeString)
                attributes:attributes];

    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    suggestionsVoiceOverMessage);
  }
  self.lastAnnouncedFieldId = _currentFieldId;
}

- (void)updateOmniboxTypingShieldVisibility {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  const BOOL shouldShowTypingShield =
      _isBottomOmnibox && IsSplitToolbarMode(self.traitCollection);
  const CGFloat typingShieldHeight =
      shouldShowTypingShield
          ? ToolbarCollapsedHeight(
                self.traitCollection.preferredContentSizeCategory)
          : 0.0;
  [[self formInputAccessoryView]
      setOmniboxTypingShieldHeight:typingShieldHeight];
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
