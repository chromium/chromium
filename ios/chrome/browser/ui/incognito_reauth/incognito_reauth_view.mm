// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_view.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_util.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_view_label.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Button content padding (Vertical and Horizontal).
const CGFloat kButtonPaddingV = 15.0f;
const CGFloat kButtonPaddingH = 38.0f;
// Max radius for the authenticate button background.
const CGFloat kAuthenticateButtonBagroundMaxCornerRadius = 30.0f;
// Distance from top and bottom to content (buttons/logos).
const CGFloat kVerticalContentPadding = 70.0f;
}  // namespace

@interface IncognitoReauthView () <IncognitoReauthViewLabelOwner>
// The background view for the authenticate button.
// Has to be separate from the button because it's a blur view (on iOS 13+).
@property(nonatomic, weak) UIView* authenticateButtonBackgroundView;
@end

@implementation IncognitoReauthView

- (instancetype)init {
  self = [super init];
  if (self) {
    // Add a dark view to block the content better. Using only a blur view
    // (below) might be too revealing.
    UIView* darkBackgroundView = [[UIView alloc] init];
    darkBackgroundView.backgroundColor = [UIColor colorWithWhite:0 alpha:0.8];
    [self addSubview:darkBackgroundView];
    darkBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self, darkBackgroundView);

    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView* blurBackgroundView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    [self addSubview:blurBackgroundView];
    blurBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self, blurBackgroundView);

    UIImage* incognitoLogo = [UIImage imageNamed:@"incognito_logo_reauth"];
    UIImageView* logoView = [[UIImageView alloc] initWithImage:incognitoLogo];
    logoView.translatesAutoresizingMaskIntoConstraints = NO;
    [blurBackgroundView.contentView addSubview:logoView];
    AddSameCenterXConstraint(logoView, blurBackgroundView);
    [logoView.topAnchor
        constraintEqualToAnchor:blurBackgroundView.safeAreaLayoutGuide.topAnchor
                       constant:kVerticalContentPadding]
        .active = YES;

    _tabSwitcherButton = [[UIButton alloc] init];
    _tabSwitcherButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_tabSwitcherButton setTitleColor:[UIColor whiteColor]
                             forState:UIControlStateNormal];
    [_tabSwitcherButton setTitle:l10n_util::GetNSString(
                                     IDS_IOS_INCOGNITO_REAUTH_GO_TO_NORMAL_TABS)
                        forState:UIControlStateNormal];
    _tabSwitcherButton.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    _tabSwitcherButton.titleLabel.adjustsFontSizeToFitWidth = YES;
    _tabSwitcherButton.titleLabel.adjustsFontForContentSizeCategory = YES;

    if (@available(iOS 13.4, *)) {
      _tabSwitcherButton.pointerInteractionEnabled = YES;
    }

    UIView* authButtonContainer =
        [self buildAuthenticateButtonWithBlurEffect:blurEffect];
    [blurBackgroundView.contentView addSubview:authButtonContainer];
    AddSameCenterConstraints(blurBackgroundView, authButtonContainer);
    _authenticateButtonBackgroundView = authButtonContainer;

    [blurBackgroundView.contentView addSubview:_tabSwitcherButton];
    AddSameCenterXConstraint(_tabSwitcherButton, blurBackgroundView);

    [NSLayoutConstraint activateConstraints:@[
      [_tabSwitcherButton.topAnchor
          constraintEqualToAnchor:blurBackgroundView.safeAreaLayoutGuide
                                      .bottomAnchor
                         constant:-kVerticalContentPadding],
      [_authenticateButton.widthAnchor
          constraintLessThanOrEqualToAnchor:self.widthAnchor
                                   constant:-2 * kButtonPaddingH],
      [_tabSwitcherButton.widthAnchor
          constraintLessThanOrEqualToAnchor:self.widthAnchor
                                   constant:-2 * kButtonPaddingH],
    ]];

    [self setNeedsLayout];
    [self layoutIfNeeded];
  }

  return self;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self setNeedsLayout];
  [self layoutIfNeeded];
}

// Creates _authenticateButton.
// Returns a "decoration" pill-shaped view containing _authenticateButton.
- (UIView*)buildAuthenticateButtonWithBlurEffect:(UIBlurEffect*)blurEffect {
  DCHECK(!_authenticateButton);

  // Use a IncognitoReauthViewLabel for the button label, because the built-in
  // UIButton's |titleLabel| does not correctly resize for multiline labels and
  // using a UILabel doesn't provide feedback to adjust the corner radius.
  IncognitoReauthViewLabel* titleLabel =
      [[IncognitoReauthViewLabel alloc] init];
  titleLabel.owner = self;
  titleLabel.numberOfLines = 0;
  titleLabel.textColor = [UIColor whiteColor];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  titleLabel.text = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON,
      base::SysNSStringToUTF16(biometricAuthenticationTypeString()));
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  // Disable a11y; below the UIButton will get a correct label.
  titleLabel.accessibilityLabel = nil;

  UIButton* button = [[UIButton alloc] init];
  button.backgroundColor = [UIColor clearColor];

  button.accessibilityLabel = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON_VOICEOVER_LABEL,
      base::SysNSStringToUTF16(biometricAuthenticationTypeString()));
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addSubview:titleLabel];
  AddSameConstraintsWithInsets(

      button, titleLabel,
      ChromeDirectionalEdgeInsetsMake(-kButtonPaddingV, -kButtonPaddingH,
                                      -kButtonPaddingV, -kButtonPaddingH));

  if (@available(iOS 13.4, *)) {
    button.pointerInteractionEnabled = YES;
  }

  UIView* backgroundView = nil;
  if (@available(iOS 13, *)) {
    UIVisualEffectView* effectView = [[UIVisualEffectView alloc]
        initWithEffect:[UIVibrancyEffect
                           effectForBlurEffect:blurEffect
                                         style:UIVibrancyEffectStyleFill]];
    [effectView.contentView addSubview:button];
    backgroundView = effectView;
  } else {
    backgroundView = [[UIView alloc] init];
    [backgroundView addSubview:button];
  }
  backgroundView.backgroundColor = [UIColor colorWithWhite:1 alpha:0.2];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;

  AddSameConstraints(backgroundView, button);
  _authenticateButton = button;
  return backgroundView;
}

#pragma mark - voiceover

- (BOOL)accessibilityViewIsModal {
  return YES;
}

- (BOOL)accessibilityPerformMagicTap {
  [self.authenticateButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  return YES;
}

#pragma mark - IncognitoReauthViewLabelOwner

- (void)labelDidLayout {
  CGFloat cornerRadius =
      std::min(kAuthenticateButtonBagroundMaxCornerRadius,
               self.authenticateButtonBackgroundView.frame.size.height / 2);
  self.authenticateButtonBackgroundView.layer.cornerRadius = cornerRadius;
}

@end
