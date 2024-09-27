// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_view_controller.h"

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
#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_view_controller_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_suggestion_view.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_accessory_view_controller_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

using autofill::FillingProduct;
using manual_fill::ManualFillDataType;

namespace {

// Logs the right metrics when the manual fallback menu is opened from the
// keyboard accessory's expand icon.
void LogManualFallbackEntryThroughExpandIcon(ManualFillDataType data_type,
                                             NSInteger suggestion_count) {
  switch (data_type) {
    case ManualFillDataType::kPassword:
      base::RecordAction(
          base::UserMetricsAction("ManualFallback_ExpandIcon_OpenPassword"));
      UMA_HISTOGRAM_COUNTS_100(
          "ManualFallback.VisibleSuggestions.ExpandIcon.OpenPasswords",
          suggestion_count);
      break;
    case ManualFillDataType::kPaymentMethod:
      base::RecordAction(base::UserMetricsAction(
          "ManualFallback_ExpandIcon_OpenPaymentMethod"));
      UMA_HISTOGRAM_COUNTS_100(
          "ManualFallback.VisibleSuggestions.ExpandIcon.OpenPaymentMethods",
          suggestion_count);
      break;
    case ManualFillDataType::kAddress:
      base::RecordAction(
          base::UserMetricsAction("ManualFallback_ExpandIcon_OpenAddress"));
      UMA_HISTOGRAM_COUNTS_100(
          "ManualFallback.VisibleSuggestions.ExpandIcon.OpenAddresses",
          suggestion_count);
      break;
    case manual_fill::ManualFillDataType::kOther:
      // The expand icon should only be available if the mapped `data_type` is
      // either associated with passwords, payment methods or addresses.
      NOTREACHED_NORETURN();
  }
}

}  // namespace

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
    // TODO(crbug.com/326398845): Completely remove the
    // `_manualFillAccessoryViewController` property (and its class) once the
    // Keyboard Accessory Upgrade feature has launched both on iPhone and iPad.
    _manualFillAccessoryViewController.view.hidden =
        IsKeyboardAccessoryUpgradeEnabled();
    _keyboardWasClosed = YES;

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(nil);
      [self registerForTraitChanges:traits
                         withAction:@selector(updateUIOnTraitChange)];
    }
  }
  return self;
}

- (void)loadView {
  [self createFormInputAccessoryViewIfNeeded];

  self.view = self.formInputAccessoryView;

  if (IsBottomOmniboxAvailable()) {
    [self updateOmniboxTypingShieldVisibility];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateUIOnTraitChange];
}
#endif

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
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    // Reset the delegate.
    [self.formInputAccessoryViewControllerDelegate
        formInputAccessoryViewControllerReset:self];
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
  BOOL hasSingleManualFillButton =
      suggestions.count > 0 &&
      (_mainFillingProduct != FillingProduct::kAutocomplete);
  self.formInputAccessoryView.manualFillButton.hidden =
      !hasSingleManualFillButton;
  self.formInputAccessoryView.passwordManualFillButton.hidden =
      hasSingleManualFillButton;
  self.formInputAccessoryView.creditCardManualFillButton.hidden =
      hasSingleManualFillButton;
  self.formInputAccessoryView.addressManualFillButton.hidden =
      hasSingleManualFillButton;

  [self createFormSuggestionViewIfNeeded];
  __weak __typeof(self) weakSelf = self;
  auto completion = ^(BOOL finished) {
    // Disable the scroll hint once it's been shown once.
    if (finished) {
      weakSelf.showScrollHint = NO;
    }
  };
  // Check if the view is in the current hierarchy before performing the layout.
  if (self.formInputAccessoryView.window) {
    [self.formInputAccessoryView layoutIfNeeded];
  }
  [self.formSuggestionView
          updateSuggestions:suggestions
             showScrollHint:self.showScrollHint
      accessoryTrailingView:self.formInputAccessoryView.trailingView
                 completion:completion];
  self.brandingViewController.keyboardAccessoryVisible =
      self.formAccessoryVisible;
  [self announceVoiceOverMessageIfNeeded:[suggestions count]];
}

- (void)manualFillButtonPressed:(UIButton*)button {
  ManualFillDataType dataType =
      [ManualFillUtil manualFillDataTypeFromFillingProduct:_mainFillingProduct];
  LogManualFallbackEntryThroughExpandIcon(
      dataType, self.formSuggestionView.suggestions.count);
  [self manualFillButtonPressed:button forDataType:dataType];
}

- (void)passwordManualFillButtonPressed:(UIButton*)button {
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenPassword"));
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenPasswords",
                           self.formSuggestionView.suggestions.count);
  [self manualFillButtonPressed:button
                    forDataType:ManualFillDataType::kPassword];
}

- (void)creditCardManualFillButtonPressed:(UIButton*)button {
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenCreditCard"));
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenCreditCards",
                           self.formSuggestionView.suggestions.count);
  [self manualFillButtonPressed:button
                    forDataType:ManualFillDataType::kPaymentMethod];
}

- (void)addressManualFillButtonPressed:(UIButton*)button {
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenProfile"));
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenProfiles",
                           self.formSuggestionView.suggestions.count);
  [self manualFillButtonPressed:button
                    forDataType:ManualFillDataType::kAddress];
}

- (void)newOmniboxPositionIsBottom:(BOOL)isBottomOmnibox {
  _isBottomOmnibox = isBottomOmnibox;
  [self updateOmniboxTypingShieldVisibility];
}

- (void)keyboardHeightChanged:(CGFloat)newHeight oldHeight:(CGFloat)oldHeight {
  if (newHeight < oldHeight) {
    // Add a quick animation to move the keyboard accessory view, which will
    // prevent it from moving if this is a quick flicker of the keyboard.
    [self verticalOffset:newHeight - oldHeight];
    [UIView animateWithDuration:0.1
                          delay:0.1
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                       [self verticalOffset:0];
                     }
                     completion:nil];
  } else if (newHeight > oldHeight) {
    // If the height is increasing, whether or not this was a flicker, we can
    // cancel the animations and offset to immediately return to the default
    // state.
    [self.formInputAccessoryView.layer removeAllAnimations];
    [self verticalOffset:0];
  }
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
  [self.formInputAccessoryViewControllerDelegate
      formInputAccessoryViewController:self
          didTapFormInputAccessoryView:self.formInputAccessoryView];
}

#pragma mark - Private

// Invoked after the user taps any of the `manual fill` buttons.
- (void)manualFillButtonPressed:(UIButton*)button
                    forDataType:(manual_fill::ManualFillDataType)dataType {
  DCHECK(IsKeyboardAccessoryUpgradeEnabled());

  self.formInputAccessoryView.hidden = YES;

  [_formInputAccessoryViewControllerDelegate
      formInputAccessoryViewController:self
              didPressManualFillButton:button
                           forDataType:dataType];
}

// Resets this view to its original state. Can be animated.
- (void)resetAnimated:(BOOL)animated {
  self.formInputAccessoryView.hidden = NO;

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
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      [formInputAccessoryView
                setUpWithLeadingView:self.leadingView
                  navigationDelegate:self.navigationDelegate
                    manualFillSymbol:DefaultSymbolWithPointSize(
                                         kExpandSymbol, kSymbolActionPointSize)
            passwordManualFillSymbol:CustomSymbolWithPointSize(
                                         kPasswordSymbol,
                                         kSymbolActionPointSize)
          creditCardManualFillSymbol:DefaultSymbolWithPointSize(
                                         kCreditCardSymbol,
                                         kSymbolActionPointSize)
             addressManualFillSymbol:CustomSymbolWithPointSize(
                                         kLocationSymbol,
                                         kSymbolActionPointSize)
                   closeButtonSymbol:DefaultSymbolWithPointSize(
                                         kKeyboardDownSymbol,
                                         kSymbolActionPointSize)];
    } else {
      self.formSuggestionView.trailingView =
          self.manualFillAccessoryViewController.view;
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

  // Adds tap recognizer.
  self.formInputAccessoryTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(tapInsideRecognized:)];
  self.formInputAccessoryTapRecognizer.cancelsTouchesInView = NO;
  [formInputAccessoryView
      addGestureRecognizer:self.formInputAccessoryTapRecognizer];

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
      case FillingProduct::kStandaloneCvc:
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
      case FillingProduct::kPredictionImprovements:
      case FillingProduct::kNone:
        // `kMerchantPromoCode` and `kCompose` cases are currently not available
        // on iOS. Also, there shouldn't be suggestions of type `kNone`.
        NOTREACHED();
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

// Moves the main view down by a certain offset (negative offsets move the view
// up).
- (void)verticalOffset:(CGFloat)offset {
  self.formInputAccessoryView.transform =
      CGAffineTransformMakeTranslation(0, offset);
}

// Updates the UI when any UITrait changes on the device.
- (void)updateUIOnTraitChange {
  if (IsBottomOmniboxAvailable()) {
    [self updateOmniboxTypingShieldVisibility];
  }
}

#pragma mark - ManualFillAccessoryViewControllerDelegate

- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                   didPressKeyboardButton:(UIButton*)keyboardButton {
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
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
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenCreditCards",
                           self.formSuggestionView.suggestions.count);
  [self.formInputAccessoryViewControllerDelegate
      formInputAccessoryViewController:self
              didPressCreditCardButton:creditCardButton];
}

- (void)manualFillAccessoryViewController:(ManualFillAccessoryViewController*)
                                              manualFillAccessoryViewController
                   didPressPasswordButton:(UIButton*)passwordButton {
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.VisibleSuggestions.OpenPasswords",
                           self.formSuggestionView.suggestions.count);
  [self.formInputAccessoryViewControllerDelegate
      formInputAccessoryViewController:self
                didPressPasswordButton:passwordButton];
}

#pragma mark - FormSuggestionViewDelegate

- (void)formSuggestionView:(FormSuggestionView*)formSuggestionView
       didAcceptSuggestion:(FormSuggestion*)suggestion
                   atIndex:(NSInteger)index {
  [self.formSuggestionClient didSelectSuggestion:suggestion atIndex:index];
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
