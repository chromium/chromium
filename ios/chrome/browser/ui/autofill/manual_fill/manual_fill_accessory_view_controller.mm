// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"

#include "base/metrics/user_metrics.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const AccessoryKeyboardAccessibilityIdentifier =
    @"kManualFillAccessoryKeyboardAccessibilityIdentifier";
NSString* const AccessoryPasswordAccessibilityIdentifier =
    @"kManualFillAccessoryPasswordAccessibilityIdentifier";
NSString* const AccessoryAddressAccessibilityIdentifier =
    @"kManualFillAccessoryAddressAccessibilityIdentifier";
NSString* const AccessoryCreditCardAccessibilityIdentifier =
    @"kManualFillAccessoryCreditCardAccessibilityIdentifier";

}  // namespace manual_fill

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
  [UIView animateWithDuration:animated ? MFAnimationDuration : 0
                   animations:^{
                     [self resetIcons];
                   }];
  if (!self.keyboardButton.hidden) {
    [self setKeyboardButtonHidden:YES animated:animated];
  }
}

#pragma mark - Setters

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

#pragma mark - Private

// Helper to create a system button with the passed data and |self| as the
// target. Such button has been configured to have some preset properties
- (UIButton*)manualFillButtonWithAction:(SEL)selector
                             ImageNamed:(NSString*)imageName
                accessibilityIdentifier:(NSString*)accessibilityIdentifier
                     accessibilityLabel:(NSString*)accessibilityLabel {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* image = [UIImage imageNamed:imageName];
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

  if (!IsIPadIdiom()) {
    self.keyboardButton = [self
        manualFillButtonWithAction:@selector(keyboardButtonPressed)
                        ImageNamed:@"mf_keyboard"
           accessibilityIdentifier:manual_fill::
                                       AccessoryKeyboardAccessibilityIdentifier
                accessibilityLabel:l10n_util::GetNSString(
                                       IDS_IOS_MANUAL_FALLBACK_SHOW_KEYBOARD)];
    [icons addObject:self.keyboardButton];
    self.keyboardButton.hidden = YES;
    self.keyboardButton.alpha = 0.0;
  }

  self.passwordButton = [self
      manualFillButtonWithAction:@selector(passwordButtonPressed:)
                      ImageNamed:@"ic_vpn_key"
         accessibilityIdentifier:manual_fill::
                                     AccessoryPasswordAccessibilityIdentifier
              accessibilityLabel:l10n_util::GetNSString(
                                     IDS_IOS_MANUAL_FALLBACK_SHOW_PASSWORDS)];

  self.passwordButton.hidden = self.isPasswordButtonHidden;
  self.passwordButton.contentEdgeInsets = UIEdgeInsetsMake(0, 2, 0, 2);
  [icons addObject:self.passwordButton];

    self.cardsButton =
        [self manualFillButtonWithAction:@selector(cardButtonPressed:)
                              ImageNamed:@"ic_credit_card"
                 accessibilityIdentifier:
                     manual_fill::AccessoryCreditCardAccessibilityIdentifier
                      accessibilityLabel:
                          l10n_util::GetNSString(
                              IDS_IOS_MANUAL_FALLBACK_SHOW_CREDIT_CARDS)];
    self.cardsButton.hidden = self.isCreditCardButtonHidden;
    [icons addObject:self.cardsButton];

    self.accountButton = [self
        manualFillButtonWithAction:@selector(accountButtonPressed:)
                        ImageNamed:@"ic_place"
           accessibilityIdentifier:manual_fill::
                                       AccessoryAddressAccessibilityIdentifier
                accessibilityLabel:l10n_util::GetNSString(
                                       IDS_IOS_MANUAL_FALLBACK_SHOW_ADDRESSES)];

    self.accountButton.hidden = self.isAddressButtonHidden;
    [icons addObject:self.accountButton];

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:icons];
  stackView.spacing =
      IsIPadIdiom() ? ManualFillIconsIPadSpacing : ManualFillIconsSpacing;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:stackView];

  CGFloat trailingInset = IsIPadIdiom() ? ManualFillIconsIPadTrailingInset
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

// Resets the icon's color and userInteractionEnabled.
- (void)resetIcons {
  self.accountButton.userInteractionEnabled = YES;
  self.cardsButton.userInteractionEnabled = YES;
  self.passwordButton.userInteractionEnabled = YES;

  [self.accountButton setTintColor:IconActiveTintColor()];
  [self.passwordButton setTintColor:IconActiveTintColor()];
  [self.cardsButton setTintColor:IconActiveTintColor()];
}

- (void)setKeyboardButtonHidden:(BOOL)hidden animated:(BOOL)animated {
  [UIView animateWithDuration:animated ? MFAnimationDuration : 0
                   animations:^{
                     // Workaround setting more than once the |hidden| property
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

- (void)keyboardButtonPressed {
  base::RecordAction(base::UserMetricsAction("ManualFallback_Close"));
  [self resetAnimated:YES];
  [self.delegate keyboardButtonPressed];
}

- (void)passwordButtonPressed:(UIButton*)sender {
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenPassword"));
  [self setKeyboardButtonHidden:NO animated:YES];
  [self resetIcons];
  self.passwordButton.userInteractionEnabled = NO;
  self.passwordButton.tintColor = IconHighlightTintColor();
  [self.delegate passwordButtonPressed:sender];
}

- (void)cardButtonPressed:(UIButton*)sender {
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenCreditCard"));
  [self setKeyboardButtonHidden:NO animated:YES];
  [self resetIcons];
  self.cardsButton.userInteractionEnabled = NO;
  self.cardsButton.tintColor = IconHighlightTintColor();
  [self.delegate cardButtonPressed:sender];
}

- (void)accountButtonPressed:(UIButton*)sender {
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenProfile"));
  [self setKeyboardButtonHidden:NO animated:YES];
  [self resetIcons];
  self.accountButton.userInteractionEnabled = NO;
  self.accountButton.tintColor = IconHighlightTintColor();
  [self.delegate accountButtonPressed:sender];
}

@end
