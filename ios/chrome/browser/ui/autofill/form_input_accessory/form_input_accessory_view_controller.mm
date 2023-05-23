// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/form_suggestion_client.h"
#import "ios/chrome/browser/ui/autofill/features.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/branding_view_controller.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_suggestion_view.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryViewController () <
    FormSuggestionViewDelegate,
    ManualFillAccessoryViewControllerDelegate>

// The leading view that contains the branding and form suggestions.
@property(nonatomic, strong) UIStackView* leadingView;

// Whether the branding logo should be present; it should be hidden when
// autofill branding is disabled, or when there are no suggestions or mandatory
// fill buttons in the form input accessory.
@property(nonatomic, readonly, getter=isBrandingVisible) BOOL brandingVisible;

// The view controller to show the branding logo.
@property(nonatomic, strong) BrandingViewController* brandingViewController;

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

@implementation FormInputAccessoryViewController

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
  if (self.brandingVisible) {
    [self addChildViewController:self.brandingViewController];
    self.brandingViewController.delegate = self.brandingViewControllerDelegate;
    [self.leadingView addArrangedSubview:self.brandingViewController.view];
    [self.brandingViewController didMoveToParentViewController:self];
  }
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
  [self updateBrandingVisibility];
  [self announceVoiceOverMessageIfNeeded:[suggestions count]];
}

#pragma mark - Getter

- (BOOL)isBrandingVisible {
  if (autofill::features::GetAutofillBrandingFrequencyType() ==
      autofill::features::AutofillBrandingFrequencyType::kNever) {
    return NO;
  }
  return !(self.manualFillAccessoryViewController.allButtonsHidden &&
           self.formSuggestionView.suggestions.count == 0);
}

- (BrandingViewController*)brandingViewController {
  if (!_brandingViewController) {
    DCHECK(self.brandingVisible);
    _brandingViewController = [[BrandingViewController alloc] init];
  }
  return _brandingViewController;
}

#pragma mark - Setters

- (void)setPasswordButtonHidden:(BOOL)passwordButtonHidden {
  _passwordButtonHidden = passwordButtonHidden;
  self.manualFillAccessoryViewController.passwordButtonHidden =
      passwordButtonHidden;
  [self updateBrandingVisibility];
}

- (void)setAddressButtonHidden:(BOOL)addressButtonHidden {
  _addressButtonHidden = addressButtonHidden;
  self.manualFillAccessoryViewController.addressButtonHidden =
      addressButtonHidden;
  [self updateBrandingVisibility];
}

- (void)setCreditCardButtonHidden:(BOOL)creditCardButtonHidden {
  _creditCardButtonHidden = creditCardButtonHidden;
  self.manualFillAccessoryViewController.creditCardButtonHidden =
      creditCardButtonHidden;
  [self updateBrandingVisibility];
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

- (void)setBrandingViewControllerDelegate:
    (id<BrandingViewControllerDelegate>)delegate {
  _brandingViewControllerDelegate = delegate;
  if (self.brandingVisible) {
    // If the branding view controller is created previously without the
    // delegate, attach it.
    self.brandingViewController.delegate = delegate;
  }
}

#pragma mark - Private

// Resets this view to its original state. Can be animated.
- (void)resetAnimated:(BOOL)animated {
  [self.formSuggestionView resetContentInsetAndDelegateAnimated:animated];
  [self.manualFillAccessoryViewController resetAnimated:animated];
  [self updateBrandingVisibility];
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

// Shows or hides branding when the number of suggestions and/or buttons
// changes.
- (void)updateBrandingVisibility {
  if (self.brandingVisible) {
    self.brandingViewController.delegate = self.brandingViewControllerDelegate;
    UIView* branding = self.brandingViewController.view;
    if (branding.superview == nil) {
      [self addChildViewController:self.brandingViewController];
      [self.leadingView insertArrangedSubview:branding atIndex:0];
      [self.brandingViewController didMoveToParentViewController:self];
    }
  } else if (self.leadingView.subviews.count ==
             2) {  // Branding button and form suggestions view.
    UIView* branding = self.brandingViewController.view;
    DCHECK_EQ(branding, self.leadingView.arrangedSubviews[0]);
    [self.brandingViewController willMoveToParentViewController:nil];
    [branding removeFromSuperview];
    [self.brandingViewController removeFromParentViewController];
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
