// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_accessory_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_accessory_view_controller_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The leading inset for the icons.
constexpr CGFloat ManualFillIconsLeadingInset = 10;

// The trailing inset for the icons.
constexpr CGFloat ManualFillIconsTrailingInset = 24;

// The iPad override for the trailing inset.
constexpr CGFloat ManualFillIconsIPadTrailingInset = 20;

// Default spacing for the icons.
constexpr CGFloat ManualFillIconsSpacing = 10;

// iPad override for the icons' spacing.
constexpr CGFloat ManualFillIconsIPadSpacing = 15;

// Size of the symbols.
constexpr CGFloat kSymbolSize = 18;

// Color to use for the buttons while enabled.
UIColor* IconActiveTintColor() {
  return [UIColor colorNamed:kToolbarButtonColor];
}

// Color to use for the buttons while highlighted.
UIColor* IconHighlightTintColor() {
  return [UIColor colorNamed:kBlueColor];
}

}  // namespace

static NSTimeInterval MFAnimationDuration = 0.2;

@interface ManualFillAccessoryViewController ()

// Delegate to handle interactions.
@property(nonatomic, readonly, weak)
    id<ManualFillAccessoryViewControllerDelegate>
        delegate;

// The button to close manual fallback.
@property(nonatomic, strong) UIButton* keyboardButton;

// The button to open the passwords section.
@property(nonatomic, strong) UIButton* passwordButton;

// The button to open the credit cards section.
@property(nonatomic, strong) UIButton* cardsButton;

// The button to open the profiles section.
@property(nonatomic, strong) UIButton* accountButton;

@end

@implementation ManualFillAccessoryViewController

#pragma mark - Public

- (instancetype)initWithDelegate:
    (id<ManualFillAccessoryViewControllerDelegate>)delegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)resetAnimated:(BOOL)animated {
  // Icon states are not modified when the Keyboard Accessory Upgrade feature is
  // enabled, so no need to reset anyhting.
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:animated ? MFAnimationDuration : 0
                   animations:^{
                     [weakSelf resetIcons];
                   }];
  if (!self.keyboardButton.hidden) {
    [self setKeyboardButtonHidden:YES animated:animated];
  }
}

#pragma mark - Accessors

- (BOOL)allButtonsHidden {
  BOOL manualFillButtonsHidden = self.addressButtonHidden &&
                                 self.creditCardButtonHidden &&
                                 self.passwordButtonHidden;
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? manualFillButtonsHidden
             : self.keyboardButton.hidden && manualFillButtonsHidden;
}

- (void)setAddressButtonHidden:(BOOL)addressButtonHidden {
  if (addressButtonHidden == _addressButtonHidden) {
    return;
  }
  _accountButton.hidden = addressButtonHidden;
  _addressButtonHidden = addressButtonHidden;
}

- (void)setCreditCardButtonHidden:(BOOL)creditCardButtonHidden {
  if (creditCardButtonHidden == _creditCardButtonHidden) {
    return;
  }
  _cardsButton.hidden = creditCardButtonHidden;
  _creditCardButtonHidden = creditCardButtonHidden;
}

- (void)setPasswordButtonHidden:(BOOL)passwordButtonHidden {
  if (passwordButtonHidden == _passwordButtonHidden) {
    return;
  }
  _passwordButton.hidden = passwordButtonHidden;
  _passwordButtonHidden = passwordButtonHidden;
}

- (void)setViewHidden:(BOOL)hidden {
  self.view.hidden = hidden;
}

#pragma mark - Private

// Helper to create a system button with the passed data and `self` as the
// target. Such button has been configured to have some preset properties
- (UIButton*)manualFillButtonWithAction:(SEL)selector
                            symbolNamed:(NSString*)symbolName
                          defaultSymbol:(BOOL)defaultSymbol
                accessibilityIdentifier:(NSString*)accessibilityIdentifier
                     accessibilityLabel:(NSString*)accessibilityLabel {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImageConfiguration* imageConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  UIImage* image =
      defaultSymbol
          ? DefaultSymbolWithConfiguration(symbolName, imageConfiguration)
          : CustomSymbolWithConfiguration(symbolName, imageConfiguration);
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(0, 0, 0, 0);
  button.configuration = buttonConfiguration;

  [button setImage:image forState:UIControlStateNormal];
  button.tintColor = IconActiveTintColor();
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addTarget:self
                action:selector
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier = accessibilityIdentifier;
  button.accessibilityLabel = accessibilityLabel;
  button.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
  return button;
}

- (void)loadView {
  self.view = [[UIView alloc] init];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  NSMutableArray<UIView*>* icons = [[NSMutableArray alloc] init];

  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    self.keyboardButton = [self
        manualFillButtonWithAction:@selector(keyboardButtonPressed:)
                       symbolNamed:kKeyboardSymbol
                     defaultSymbol:YES
           accessibilityIdentifier:manual_fill::
                                       kAccessoryKeyboardAccessibilityIdentifier
                accessibilityLabel:l10n_util::GetNSString(
                                       IDS_IOS_MANUAL_FALLBACK_SHOW_KEYBOARD)];
    [icons addObject:self.keyboardButton];
    self.keyboardButton.hidden = YES;
    self.keyboardButton.alpha = 0.0;
  }

  self.passwordButton = [self
      manualFillButtonWithAction:@selector(passwordButtonPressed:)
                     symbolNamed:kPasswordSymbol
                   defaultSymbol:NO
         accessibilityIdentifier:manual_fill::
                                     kAccessoryPasswordAccessibilityIdentifier
              accessibilityLabel:l10n_util::GetNSString(
                                     IDS_IOS_MANUAL_FALLBACK_SHOW_PASSWORDS)];

  self.passwordButton.hidden = self.isPasswordButtonHidden;

  UIButtonConfiguration* buttonConfiguration =
      self.passwordButton.configuration;
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(0, 2, 0, 2);
  self.passwordButton.configuration = buttonConfiguration;

  [icons addObject:self.passwordButton];

  self.cardsButton =
      [self manualFillButtonWithAction:@selector(cardButtonPressed:)
                           symbolNamed:kCreditCardSymbol
                         defaultSymbol:YES
               accessibilityIdentifier:
                   manual_fill::kAccessoryCreditCardAccessibilityIdentifier
                    accessibilityLabel:
                        l10n_util::GetNSString(
                            IDS_IOS_MANUAL_FALLBACK_SHOW_CREDIT_CARDS)];
  self.cardsButton.hidden = self.isCreditCardButtonHidden;
  [icons addObject:self.cardsButton];

  self.accountButton = [self
      manualFillButtonWithAction:@selector(accountButtonPressed:)
                     symbolNamed:kLocationSymbol
                   defaultSymbol:NO
         accessibilityIdentifier:manual_fill::
                                     kAccessoryAddressAccessibilityIdentifier
              accessibilityLabel:l10n_util::GetNSString(
                                     IDS_IOS_MANUAL_FALLBACK_SHOW_ADDRESSES)];

  self.accountButton.hidden = self.isAddressButtonHidden;
  [icons addObject:self.accountButton];

  for (UIButton* button in icons)
    button.pointerInteractionEnabled = YES;

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:icons];
  stackView.spacing =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? ManualFillIconsIPadSpacing
          : ManualFillIconsSpacing;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:stackView];

  CGFloat trailingInset =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? ManualFillIconsIPadTrailingInset
          : ManualFillIconsTrailingInset;
  id<LayoutGuideProvider> safeAreaLayoutGuide = self.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    // Vertical constraints.
    [stackView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor],
    [stackView.topAnchor constraintEqualToAnchor:self.view.topAnchor],

    // Horizontal constraints.
    [stackView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:ManualFillIconsLeadingInset],
    [safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:stackView.trailingAnchor
                       constant:trailingInset],
  ]];
}

// Resets the icon's color and `userInteractionEnabled` state.
- (void)resetIcons {
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
  self.accountButton.userInteractionEnabled = YES;
  self.cardsButton.userInteractionEnabled = YES;
  self.passwordButton.userInteractionEnabled = YES;

  [self.accountButton setTintColor:IconActiveTintColor()];
  [self.passwordButton setTintColor:IconActiveTintColor()];
  [self.cardsButton setTintColor:IconActiveTintColor()];
}

- (void)setKeyboardButtonHidden:(BOOL)hidden animated:(BOOL)animated {
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
  [UIView animateWithDuration:animated ? MFAnimationDuration : 0
                   animations:^{
                     // Workaround setting more than once the `hidden` property
                     // in stacked views.
                     if (self.keyboardButton.hidden != hidden) {
                       self.keyboardButton.hidden = hidden;
                     }

                     if (hidden) {
                       self.keyboardButton.alpha = 0.0;
                     } else {
                       self.keyboardButton.alpha = 1.0;
                     }
                   }];
}

- (void)keyboardButtonPressed:(UIButton*)keyboardButton {
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
  base::RecordAction(base::UserMetricsAction("ManualFallback_Close"));
  [self resetAnimated:YES];
  [self.delegate manualFillAccessoryViewController:self
                            didPressKeyboardButton:keyboardButton];
}

- (void)passwordButtonPressed:(UIButton*)passwordButton {
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenPassword"));
  [self setKeyboardButtonHidden:NO animated:YES];
  [self resetIcons];
  self.passwordButton.userInteractionEnabled = NO;
  self.passwordButton.tintColor = IconHighlightTintColor();
  [self.delegate manualFillAccessoryViewController:self
                            didPressPasswordButton:passwordButton];
}

- (void)cardButtonPressed:(UIButton*)creditCardButton {
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenCreditCard"));
  [self setKeyboardButtonHidden:NO animated:YES];
  [self resetIcons];
  self.cardsButton.userInteractionEnabled = NO;
  self.cardsButton.tintColor = IconHighlightTintColor();
  [self.delegate manualFillAccessoryViewController:self
                          didPressCreditCardButton:creditCardButton];
}

- (void)accountButtonPressed:(UIButton*)accountButton {
  CHECK(!IsKeyboardAccessoryUpgradeEnabled());
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenProfile"));
  [self setKeyboardButtonHidden:NO animated:YES];
  [self resetIcons];
  self.accountButton.userInteractionEnabled = NO;
  self.accountButton.tintColor = IconHighlightTintColor();
  [self.delegate manualFillAccessoryViewController:self
                             didPressAccountButton:accountButton];
}

@end
