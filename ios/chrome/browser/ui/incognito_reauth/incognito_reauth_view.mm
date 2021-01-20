// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_view.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_util.h"
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
// Distance from top and bottom to content (buttons/logos).
const CGFloat kVerticalContentPadding = 70.0f;
}  // namespace

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

    NSString* unlockButtonTitle = l10n_util::GetNSStringF(
        IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON,
        base::SysNSStringToUTF16(biometricAuthenticationTypeString()));
    _authenticateButton =
        [IncognitoReauthView newRoundButtonWithBlurEffect:blurEffect];

    [_authenticateButton setTitle:unlockButtonTitle
                         forState:UIControlStateNormal];
    _authenticateButton.accessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON_VOICEOVER_LABEL,
        base::SysNSStringToUTF16(biometricAuthenticationTypeString()));

    _tabSwitcherButton = [[UIButton alloc] init];
    _tabSwitcherButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_tabSwitcherButton setTitleColor:[UIColor whiteColor]
                             forState:UIControlStateNormal];
    [_tabSwitcherButton setTitle:l10n_util::GetNSString(
                                     IDS_IOS_INCOGNITO_REAUTH_GO_TO_NORMAL_TABS)
                        forState:UIControlStateNormal];
    _tabSwitcherButton.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];

    [blurBackgroundView.contentView addSubview:_authenticateButton];
    AddSameCenterConstraints(blurBackgroundView, _authenticateButton);

    [blurBackgroundView.contentView addSubview:_tabSwitcherButton];
    AddSameCenterXConstraint(_tabSwitcherButton, blurBackgroundView);
    [_tabSwitcherButton.topAnchor
        constraintEqualToAnchor:blurBackgroundView.safeAreaLayoutGuide
                                    .bottomAnchor
                       constant:-kVerticalContentPadding]
        .active = YES;
  }

  return self;
}

+ (UIButton*)newRoundButtonWithBlurEffect:(UIBlurEffect*)blurEffect {
  UIButton* button = [[UIButton alloc] init];
  button.backgroundColor = [UIColor clearColor];
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  button.contentEdgeInsets = UIEdgeInsetsMake(kButtonPaddingV, kButtonPaddingH,
                                              kButtonPaddingV, kButtonPaddingH);
  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button sizeToFit];

  UIView* backgroundView = nil;
  if (@available(iOS 13, *)) {
    backgroundView = [[UIVisualEffectView alloc]
        initWithEffect:[UIVibrancyEffect
                           effectForBlurEffect:blurEffect
                                         style:UIVibrancyEffectStyleFill]];
  } else {
    backgroundView = [[UIView alloc] init];
  }
  backgroundView.backgroundColor = [UIColor colorWithWhite:1 alpha:0.2];
  backgroundView.layer.cornerRadius = button.frame.size.height / 2;
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  backgroundView.userInteractionEnabled = NO;
  [button addSubview:backgroundView];
  AddSameConstraints(backgroundView, button);

  return button;
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

@end
