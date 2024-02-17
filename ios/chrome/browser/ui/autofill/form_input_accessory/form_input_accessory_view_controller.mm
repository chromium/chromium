// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/filling_product.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_client.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/branding/branding_view_controller.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view_controller_delegate.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_suggestion_view.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

using autofill::FillingProduct;
using manual_fill::ManualFillDataType;

@interface FormInputAccessoryViewController () <
    FormSuggestionViewDelegate,
    ManualFillAccessoryViewControllerDelegate>

// The leading view that contains the branding and form suggestions.
@property(nonatomic, strong) UIStackView* leadingView;

// A BOOL value indicating whether any form accessory is visible. If YES, at
// lease one form accessory is visible.
@property(nonatomic, readonly, getter=isFormAccessoryVisible)
    BOOL formAccessoryVisible;

// The custom view that should be shown in the input accessory view.
@property(nonatomic, strong) FormInputAccessoryView* formInputAccessoryView;

// The view with the suggestions in FormInputAccessoryView.
@property(nonatomic, strong) FormSuggestionView* formSuggestionView;

// The manual fill accessory view controller to add at the end of the
// suggestions.
@property(nonatomic, strong, readonly)
    ManualFillAccessoryViewController* manualFillAccessoryViewController;

// Delegate to handle interactions with the manual fill buttons.
@property(nonatomic, readonly, weak)
    id<FormInputAccessoryViewControllerDelegate>
        formInputAccessoryViewControllerDelegate;

// The ID of the field that was last announced by VoiceOver.
@property(nonatomic, assign) autofill::FieldRendererId lastAnnouncedFieldId;

// Whether to show the scroll hint.
@property(nonatomic, assign) BOOL showScrollHint;

// UI tap recognizer used to dismiss bubble presenter.
@property(nonatomic, strong)
    UITapGestureRecognizer* formInputAccessoryTapRecognizer;

@end

@implementation FormInputAccessoryViewController {
  // Is the preferred omnibox position at the bottom.
  BOOL _isBottomOmnibox;

  // Whether the keyboard was closed before the keyboard accessory appeared.
  BOOL _keyboardWasClosed;
}

@synthesize addressButtonHidden = _addressButtonHidden;
@synthesize creditCardButtonHidden = _creditCardButtonHidden;
@synthesize formInputNextButtonEnabled = _formInputNextButtonEnabled;
@synthesize formInputPreviousButtonEnabled = _formInputPreviousButtonEnabled;
@synthesize navigationDelegate = _navigationDelegate;
@synthesize passwordButtonHidden = _passwordButtonHidden;
@synthesize mainFillingProduct = _mainFillingProduct;
@synthesize currentFieldId = _currentFieldId;

#pragma mark - Life Cycle

- (instancetype)initWithFormInputAccessoryViewControllerDelegate:
    (id<FormInputAccessoryViewControllerDelegate>)
        formInputAccessoryViewControllerDelegate {
  self = [super init];
  if (self) {
    _formInputAccessoryViewControllerDelegate =
        formInputAccessoryViewControllerDelegate;
    _manualFillAccessoryViewController =
        [[ManualFillAccessoryViewController alloc] initWithDelegate:self];
    [self addChildViewController:_manualFillAccessoryViewController];
    _keyboardWasClosed = YES;
  }
  return self;
}

- (void)loadView {
  [self createFormInputAccessoryViewIfNeeded];

  self.view = self.formInputAccessoryView;
  [self showManualFillView:NO];

  if (IsBottomOmniboxSteadyStateEnabled()) {
    [self updateOmniboxTypingShieldVisibility];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (IsBottomOmniboxSteadyStateEnabled()) {
    [self updateOmniboxTypingShieldVisibility];
  }
}

#pragma mark - UIViewController

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // `showScrollHint` will be set to YES only when `viewDidAppear` is called
  // from a state where the keyboard was previously closed, otherwise it will be
  // set to NO. `viewDidAppear` is called when they keyboard accessory remains
  // open after a user switches between fields on a form, but `viewDidDisappear`
  // is not, so `showScrollHint` will not be set to YES in that scenario.
  self.showScrollHint = _keyboardWasClosed;
  _keyboardWasClosed = NO;
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];

  // Exit the manual fill view, so that the next time the keyboard opens, it is
  // showing the keyboard and not the manual fill view.
  if ([self isManualFillViewVisible]) {
    // Hide the manual fill view.
    [self showManualFillView:NO];

    // Reset the delegate.
    [self.formInputAccessoryViewControllerDelegate
        formInputAccessoryViewControllerReset:self];

    // Reset the manual fill view controller.
    [self.manualFillAccessoryViewController resetAnimated:NO];
  }

  // Whether the keyboard was closed the next time the keyboard accessory opens.
  _keyboardWasClosed = YES;
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
  __weak __typeof(self) weakSelf = self;
  auto completion = ^(BOOL finished) {
    // Disable the scroll hint once it's been shown once.
    if (finished) {
      weakSelf.showScrollHint = NO;
    }
  };
  [self.formSuggestionView updateSuggestions:suggestions
                              showScrollHint:self.showScrollHint
                                  completion:completion];
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;
  [self announceVoiceOverMessageIfNeeded:[suggestions count]];
}

- (void)manualFillButtonPressed:(UIButton*)button {
  DCHECK(IsKeyboardAccessoryUpgradeEnabled());

  ManualFillDataType dataType =
      [self manualFillDataTypeFromFillingProduct:_mainFillingProduct];
  [_formInputAccessoryViewControllerDelegate
      formInputAccessoryViewController:self
              didPressManualFillButton:button
                           forDataType:dataType];
  [self showManualFillView:YES];
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

#pragma mark - Actions

- (void)tapInsideRecognized:(id)sender {
  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    [self.formInputAccessoryViewControllerDelegate
        formInputAccessoryViewController:self
            didTapFormInputAccessoryView:self.view];
  } else {
    // This method can't be reached when `kEnableStartupImprovements` is
    // enabled.
    NOTREACHED();
  }
}

#pragma mark - Private

// Resets this view to its original state. Can be animated.
- (void)resetAnimated:(BOOL)animated {
  [self.formSuggestionView resetContentInsetAndDelegateAnimated:animated];
  [self.manualFillAccessoryViewController resetAnimated:animated];
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;
}

// Creates formInputAccessoryView if not done yet.
- (void)createFormInputAccessoryViewIfNeeded {
  if (self.formInputAccessoryView) {
    return;
  }

  [self createFormSuggestionViewIfNeeded];

  FormInputAccessoryView* formInputAccessoryView =
      [[FormInputAccessoryView alloc] init];

  // Sets up leading view.
  self.leadingView = [[UIStackView alloc] init];
  self.leadingView.axis = UILayoutConstraintAxisHorizontal;

  [self addChildViewController:self.brandingViewController];
  [self.leadingView addArrangedSubview:self.brandingViewController.view];
  [self.brandingViewController didMoveToParentViewController:self];

  [self.leadingView addArrangedSubview:self.formSuggestionView];

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [formInputAccessoryView
        setUpWithLeadingView:self.leadingView
          customTrailingView:self.manualFillAccessoryViewController.view];
  } else {
    formInputAccessoryView.accessibilityViewIsModal = YES;
    self.formSuggestionView.trailingView =
        self.manualFillAccessoryViewController.view;
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      [formInputAccessoryView
          setUpWithLeadingView:self.leadingView
            navigationDelegate:self.navigationDelegate
              manualFillSymbol:DefaultSymbolWithPointSize(
                                   kExpandSymbol, kSymbolActionPointSize)
             closeButtonSymbol:DefaultSymbolWithPointSize(
                                   kKeyboardDownSymbol,
                                   kSymbolActionPointSize)];
    } else {
      [formInputAccessoryView setUpWithLeadingView:self.leadingView
                                navigationDelegate:self.navigationDelegate];
    }
    formInputAccessoryView.nextButton.enabled = self.formInputNextButtonEnabled;
    formInputAccessoryView.previousButton.enabled =
        self.formInputPreviousButtonEnabled;
  }

  // Update branding view keyboard accessory visibility after
  // `self.manualFillAccessoryViewController` loaded its view, as
  // `self.formAccessoryVisible` depends on the visible state of its view.
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;

  if (base::FeatureList::IsEnabled(kEnableStartupImprovements)) {
    // Adds tap recognizer.
    self.formInputAccessoryTapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(tapInsideRecognized:)];
    self.formInputAccessoryTapRecognizer.cancelsTouchesInView = NO;
    [formInputAccessoryView
        addGestureRecognizer:self.formInputAccessoryTapRecognizer];
  }

  self.formInputAccessoryView = formInputAccessoryView;
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
    std::u16string mainFillingProductString;
    switch (_mainFillingProduct) {
      case FillingProduct::kAddress:
      case FillingProduct::kPlusAddresses:
        mainFillingProductString = l10n_util::GetPluralStringFUTF16(
            IDS_IOS_AUTOFILL_ADDRESS_SUGGESTIONS_AVAILABLE_ACCESSIBILITY_ANNOUNCEMENT,
            suggestionCount);
        break;
      case FillingProduct::kPassword:
        mainFillingProductString = l10n_util::GetPluralStringFUTF16(
            IDS_IOS_AUTOFILL_PASSWORD_SUGGESTIONS_AVAILABLE_ACCESSIBILITY_ANNOUNCEMENT,
            suggestionCount);
        break;
      case FillingProduct::kCreditCard:
      case FillingProduct::kIban:
        mainFillingProductString = l10n_util::GetPluralStringFUTF16(
            IDS_IOS_AUTOFILL_PAYMENT_METHOD_SUGGESTIONS_AVAILABLE_ACCESSIBILITY_ANNOUNCEMENT,
            suggestionCount);
        break;
      case FillingProduct::kAutocomplete:
        mainFillingProductString = l10n_util::GetPluralStringFUTF16(
            IDS_IOS_AUTOFILL_AUTOCOMPLETE_SUGGESTIONS_AVAILABLE_ACCESSIBILITY_ANNOUNCEMENT,
            suggestionCount);
        break;
      case FillingProduct::kMerchantPromoCode:
      case FillingProduct::kCompose:
        // These cases are currently not available on iOS.
        NOTREACHED_NORETURN();
      case FillingProduct::kNone:
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
            initWithString:base::SysUTF16ToNSString(mainFillingProductString)
                attributes:attributes];

    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    suggestionsVoiceOverMessage);
  }
  self.lastAnnouncedFieldId = _currentFieldId;
}

- (void)updateOmniboxTypingShieldVisibility {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  if (!self.formInputAccessoryView) {
    return;
  }
  const BOOL shouldShowTypingShield =
      _isBottomOmnibox && IsSplitToolbarMode(self.traitCollection);
  const CGFloat typingShieldHeight =
      shouldShowTypingShield
          ? ToolbarCollapsedHeight(
                self.traitCollection.preferredContentSizeCategory)
          : 0.0;
  [self.formInputAccessoryView setOmniboxTypingShieldHeight:typingShieldHeight];
}

- (BOOL)isManualFillViewVisible {
  return IsKeyboardAccessoryUpgradeEnabled() &&
         !self.manualFillAccessoryViewController.view.hidden;
}

- (void)showManualFillView:(BOOL)visible {
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    [self.manualFillAccessoryViewController setViewHidden:!visible];
    self.formInputAccessoryView.manualFillButton.hidden = visible;
  }
}

// Returns a ManualFillDataType based on the provided FillingProduct.
- (ManualFillDataType)manualFillDataTypeFromFillingProduct:
    (FillingProduct)fillingProduct {
  switch (fillingProduct) {
    case FillingProduct::kAddress:
    case FillingProduct::kPlusAddresses:
      return ManualFillDataType::kAddress;
    case FillingProduct::kCreditCard:
    case FillingProduct::kIban:
      return ManualFillDataType::kPaymentMethod;
    case FillingProduct::kPassword:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kNone:
      // `kPassword` acts as the default value when the FillingProduct
      // doesn't point towards a specific data type.
      return ManualFillDataType::kPassword;
    case FillingProduct::kCompose:
    case FillingProduct::kMerchantPromoCode:
      // These cases are currently not available on iOS.
      NOTREACHED_NORETURN();
  }
}

#pragma mark - ManualFillAccessoryViewControllerDelegate

- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                   didPressKeyboardButton:(UIButton*)keyboardButton {
  [self showManualFillView:NO];
  [self.formInputAccessoryViewControllerDelegate
      formInputAccessoryViewController:self
                didPressKeyboardButton:keyboardButton];
}

- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                    didPressAccountButton:(UIButton*)accountButton {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenProfiles",
                           self.formSuggestionView.suggestions.count);
  [self.formInputAccessoryViewControllerDelegate
      formInputAccessoryViewController:self
                 didPressAccountButton:accountButton];
}

- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                 didPressCreditCardButton:(UIButton*)creditCardButton {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenCreditCards",
                           self.formSuggestionView.suggestions.count);
  [self.formInputAccessoryViewControllerDelegate
      formInputAccessoryViewController:self
              didPressCreditCardButton:creditCardButton];
}

- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                   didPressPasswordButton:(UIButton*)passwordButton {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenPasswords",
                           self.formSuggestionView.suggestions.count);
  [self.formInputAccessoryViewControllerDelegate
      formInputAccessoryViewController:self
                didPressPasswordButton:passwordButton];
}

#pragma mark - FormSuggestionViewDelegate

- (void)formSuggestionView:(FormSuggestionView*)formSuggestionView
       didAcceptSuggestion:(FormSuggestion*)suggestion {
  [self.formSuggestionClient didSelectSuggestion:suggestion];
}

- (void)formSuggestionViewShouldResetFromPull:
    (FormSuggestionView*)formSuggestionView {
  DCHECK(!IsKeyboardAccessoryUpgradeEnabled());

  base::RecordAction(base::UserMetricsAction("ManualFallback_ClosePull"));
  // The pull gesture has the same effect as when the keyboard button is
  // pressed.
  [self manualFillAccessoryViewController:self.manualFillAccessoryViewController
                   didPressKeyboardButton:nil];
  [self.manualFillAccessoryViewController resetAnimated:YES];
}

@end
