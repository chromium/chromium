// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view_label.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Button content padding (Vertical and Horizontal).
const CGFloat kButtonPaddingV = 15.0f;
const CGFloat kButtonPaddingH = 38.0f;
// Max radius for the authenticate button background.
const CGFloat kAuthenticateButtonBagroundMaxCornerRadius = 30.0f;
// Distance from top and bottom to content (buttons/logos).
const CGFloat kVerticalContentPadding = 70.0f;
// Distance from the Logo to the primary button.
const CGFloat kLogoToPrimaryButtonMargin = 32.0f;
// Optimal content width to use for button sizing.
const CGFloat kContentOptimalWidth = 327;
}  // namespace

@interface IncognitoReauthView () <IncognitoReauthViewLabelOwner>
@end

@implementation IncognitoReauthView {
  // The background view for the authenticate button.
  // Has to be separate from the button because it's a blur view (on iOS 13+).
  UIView* _authenticateButtonBackgroundView;
  // Use a IncognitoReauthViewLabel for the button label, because the built-in
  // UIButton's `titleLabel` does not correctly resize for multiline labels and
  // using a UILabel doesn't provide feedback to adjust the corner radius.
  IncognitoReauthViewLabel* _authenticateButtonLabel;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    if (!IsIOSSoftLockEnabled()) {
      // Increase blur intensity by layering some blur views to make
      // content behind really not recognizeable.
      for (int i = 0; i < 3; i++) {
        UIBlurEffect* blurEffect =
            [UIBlurEffect effectWithStyle:UIBlurEffectStyleLight];
        UIVisualEffectView* blurView =
            [[UIVisualEffectView alloc] initWithEffect:blurEffect];
        [self addSubview:blurView];
        blurView.translatesAutoresizingMaskIntoConstraints = NO;
        AddSameConstraints(self, blurView);
      }
    }

    UIBlurEffect* blurEffect = [UIBlurEffect
        effectWithStyle:IsIOSSoftLockEnabled()
                            ? UIBlurEffectStyleSystemThickMaterialDark
                            : UIBlurEffectStyleDark];
    UIVisualEffectView* blurBackgroundView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    [self addSubview:blurBackgroundView];
    blurBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self, blurBackgroundView);

    CGFloat imageSize = IsIOSSoftLockEnabled() ? 50 : 28;
    UIImage* incognitoLogo =
        CustomSymbolWithPointSize(kIncognitoSymbol, imageSize);
    _logoView = [[UIImageView alloc] initWithImage:incognitoLogo];
    _logoView.tintColor = UIColor.whiteColor;
    _logoView.translatesAutoresizingMaskIntoConstraints = NO;
    [blurBackgroundView.contentView addSubview:_logoView];
    AddSameCenterXConstraint(_logoView, blurBackgroundView);

    _secondaryButton = [self buildSecondaryButton];
    [blurBackgroundView.contentView addSubview:_secondaryButton];
    AddSameCenterXConstraint(_secondaryButton, blurBackgroundView);

    if (IsIOSSoftLockEnabled()) {
      _authenticateButton = [self buildAuthenticateButton];
      [blurBackgroundView.contentView addSubview:_authenticateButton];
      AddSameCenterXConstraint(blurBackgroundView, _authenticateButton);
    } else {
      UIView* authButtonContainer =
          [self buildAuthenticateButtonWithBlurEffect:blurEffect];
      [blurBackgroundView.contentView addSubview:authButtonContainer];
      AddSameCenterConstraints(blurBackgroundView, authButtonContainer);
      _authenticateButtonBackgroundView = authButtonContainer;
    }

    // Setup Constraints
    NSMutableArray<NSLayoutConstraint*>* constraints =
        [[NSMutableArray alloc] initWithArray:@[
          [_secondaryButton.widthAnchor
              constraintLessThanOrEqualToAnchor:self.widthAnchor
                                       constant:-2 * kButtonPaddingH],
          [_authenticateButton.widthAnchor
              constraintLessThanOrEqualToAnchor:self.widthAnchor
                                       constant:-2 * kButtonPaddingH],
        ]];

    if (IsIOSSoftLockEnabled()) {
      NSLayoutConstraint* authenticateButtonPreferredWidthConstraint =
          [_authenticateButton.widthAnchor
              constraintEqualToConstant:kContentOptimalWidth];
      NSLayoutConstraint* secondaryButtonPreferredWidthConstraint =
          [_authenticateButton.widthAnchor
              constraintEqualToConstant:kContentOptimalWidth];
      authenticateButtonPreferredWidthConstraint.priority =
          UILayoutPriorityDefaultHigh;
      secondaryButtonPreferredWidthConstraint.priority =
          UILayoutPriorityDefaultHigh;

      [constraints addObjectsFromArray:@[
        [_authenticateButton.centerYAnchor
            constraintEqualToAnchor:self.centerYAnchor],
        [_secondaryButton.topAnchor
            constraintEqualToAnchor:_authenticateButton.bottomAnchor
                           constant:kButtonPaddingV],
        [_logoView.bottomAnchor
            constraintEqualToAnchor:_authenticateButton.topAnchor
                           constant:-kLogoToPrimaryButtonMargin],
        authenticateButtonPreferredWidthConstraint,
        secondaryButtonPreferredWidthConstraint,
      ]];
    } else {
      [constraints addObjectsFromArray:@[
        [_secondaryButton.topAnchor
            constraintEqualToAnchor:blurBackgroundView.safeAreaLayoutGuide
                                        .bottomAnchor
                           constant:-kVerticalContentPadding],
        [_logoView.topAnchor
            constraintEqualToAnchor:blurBackgroundView.safeAreaLayoutGuide
                                        .topAnchor
                           constant:kVerticalContentPadding]
      ]];
    }

    [NSLayoutConstraint activateConstraints:constraints];

    NSArray<UITrait>* traits = TraitCollectionSetForTraits(nil);
    __weak IncognitoReauthView* weakSelf = self;
    [self registerForTraitChanges:traits
                      withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                    UITraitCollection* previousCollection) {
                        [weakSelf relayoutView];
                      }];
  }

  return self;
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  [self relayoutView];
}

#pragma mark - public

- (void)setAuthenticateButtonText:(NSString*)text
               accessibilityLabel:(NSString*)accessibilityLabel {
  if (IsIOSSoftLockEnabled()) {
    SetConfigurationTitle(self.authenticateButton, text);
  } else {
    _authenticateButtonLabel.text = text;
  }
  self.authenticateButton.accessibilityLabel = accessibilityLabel;
}

#pragma mark - UIAccessibility

- (BOOL)accessibilityViewIsModal {
  return YES;
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformMagicTap {
  [self.authenticateButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  return YES;
}

#pragma mark - private

// Create a rounded blue background, white text button, used as the authenticate
// button.
- (UIButton*)buildAuthenticateButton {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kBlue300Color];
  buttonConfiguration.background.cornerRadius = kPrimaryButtonCornerRadius;
  buttonConfiguration.baseForegroundColor = [UIColor whiteColor];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);

  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* string =
      [[NSMutableAttributedString alloc] initWithString:@" "];
  [string addAttributes:attributes range:NSMakeRange(0, string.length)];
  buttonConfiguration.attributedTitle = string;
  buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;

  UIButton* button = [UIButton buttonWithConfiguration:buttonConfiguration
                                         primaryAction:nil];
  button.configuration = buttonConfiguration;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

  return button;
}

// Creates _authenticateButton.
// Returns a "decoration" pill-shaped view containing _authenticateButton.
- (UIView*)buildAuthenticateButtonWithBlurEffect:(UIBlurEffect*)blurEffect {
  DCHECK(!_authenticateButton);

  _authenticateButtonLabel = [[IncognitoReauthViewLabel alloc] init];
  _authenticateButtonLabel.owner = self;
  _authenticateButtonLabel.numberOfLines = 0;
  _authenticateButtonLabel.textColor = [UIColor colorWithWhite:1 alpha:0.95];
  _authenticateButtonLabel.textAlignment = NSTextAlignmentCenter;
  _authenticateButtonLabel.adjustsFontForContentSizeCategory = YES;
  _authenticateButtonLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  _authenticateButtonLabel.text = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  [_authenticateButtonLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_authenticateButtonLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  _authenticateButtonLabel.translatesAutoresizingMaskIntoConstraints = NO;
  // Disable a11y; below the UIButton will get a correct label.
  _authenticateButtonLabel.accessibilityLabel = nil;

  UIButton* button = [[UIButton alloc] init];
  button.backgroundColor = [UIColor clearColor];

  button.accessibilityLabel = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON_VOICEOVER_LABEL,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.pointerInteractionEnabled = YES;

  UIView* backgroundView = nil;
  UIVisualEffectView* effectView = [[UIVisualEffectView alloc]
      initWithEffect:[UIVibrancyEffect
                         effectForBlurEffect:blurEffect
                                       style:UIVibrancyEffectStyleFill]];

  [button addSubview:_authenticateButtonLabel];
  [effectView.contentView addSubview:button];
  backgroundView = effectView;
  AddSameConstraintsWithInsets(
      button, _authenticateButtonLabel,
      NSDirectionalEdgeInsetsMake(-kButtonPaddingV, -kButtonPaddingH,
                                  -kButtonPaddingV, -kButtonPaddingH));

  backgroundView.backgroundColor =
      [IncognitoReauthView blurButtonBackgroundColor];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;

  AddSameConstraints(backgroundView, button);

  // Handle touch up and down events to create a "highlight" state.
  // The normal button highlight state is not usable here because the actual
  // button is transparent.
  [button addTarget:self
                action:@selector(blurButtonEventHandler)
      forControlEvents:UIControlEventAllEvents];

  _authenticateButton = button;

  return backgroundView;
}

// Create a secondary button with a certain look and text based on the different
// flag states.
- (UIButton*)buildSecondaryButton {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
  [button setTitleColor:[UIColor colorWithWhite:1 alpha:0.4]
               forState:UIControlStateHighlighted];

  if (IsIOSSoftLockEnabled()) {
    [button setTitle:l10n_util::GetNSString(
                         IDS_IOS_INCOGNITO_REAUTH_CLOSE_INCOGNITO_TABS)
            forState:UIControlStateNormal];
    button.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  } else {
    [button setTitle:l10n_util::GetNSString(
                         IDS_IOS_INCOGNITO_REAUTH_GO_TO_NORMAL_TABS)
            forState:UIControlStateNormal];
    button.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  }
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.pointerInteractionEnabled = YES;

  return button;
}

- (void)blurButtonEventHandler {
  UIView* buttonBackgroundView = _authenticateButtonBackgroundView;
  UIColor* newColor =
      [self.authenticateButton isHighlighted]
          ? [IncognitoReauthView blurButtonHighlightBackgroundColor]
          : [IncognitoReauthView blurButtonBackgroundColor];

  [UIView animateWithDuration:0.1
                   animations:^{
                     buttonBackgroundView.backgroundColor = newColor;
                   }];
}

- (void)relayoutView {
  if (IsIOSSoftLockEnabled()) {
    return;
  }
  [self setNeedsLayout];
  [self layoutIfNeeded];
}

#pragma mark - IncognitoReauthViewLabelOwner

- (void)labelDidLayout {
  CHECK(!IsIOSSoftLockEnabled());
  CGFloat cornerRadius =
      std::min(kAuthenticateButtonBagroundMaxCornerRadius,
               _authenticateButtonBackgroundView.frame.size.height / 2);
  _authenticateButtonBackgroundView.layer.cornerRadius = cornerRadius;
}

#pragma mark - helpers

+ (UIColor*)blurButtonBackgroundColor {
  return [UIColor colorWithWhite:1 alpha:0.15];
}

+ (UIColor*)blurButtonHighlightBackgroundColor {
  return [UIColor colorWithWhite:1 alpha:0.6];
}

@end
