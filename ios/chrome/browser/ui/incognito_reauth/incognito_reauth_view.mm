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
const CGFloat kButtonHeight = 60.0f;
const CGFloat kButtonWidth = 190.0f;
const CGFloat kButtonSpacing = 16.0f;
}  // namespace

@implementation IncognitoReauthView

- (instancetype)init {
  self = [super init];
  if (self) {
    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView* blurBackgroundView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    [self addSubview:blurBackgroundView];
    blurBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    // TODO(crbug.com/1138892): add localized text.
    blurBackgroundView.accessibilityLabel =
        @"[Test String] Authenticate to access Incognito content";
    blurBackgroundView.isAccessibilityElement = YES;
    AddSameConstraints(self, blurBackgroundView);

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

    _tabSwitcherButton =
        [IncognitoReauthView newRoundButtonWithBlurEffect:blurEffect];
    [_tabSwitcherButton setTitle:l10n_util::GetNSString(
                                     IDS_IOS_INCOGNITO_REAUTH_GO_TO_NORMAL_TABS)
                        forState:UIControlStateNormal];

    UIStackView* stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _authenticateButton, _tabSwitcherButton ]];
    stackView.axis = UILayoutConstraintAxisVertical;
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.spacing = kButtonSpacing;
    [self addSubview:stackView];
    AddSameCenterConstraints(blurBackgroundView, stackView);
  }

  return self;
}

+ (UIButton*)newRoundButtonWithBlurEffect:(UIBlurEffect*)blurEffect {
  UIButton* button = [[UIButton alloc] init];
  button.backgroundColor = [UIColor clearColor];

  [NSLayoutConstraint activateConstraints:@[
    [button.heightAnchor constraintEqualToConstant:kButtonHeight],
    [button.widthAnchor constraintEqualToConstant:kButtonWidth],
  ]];

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
  backgroundView.layer.cornerRadius = kButtonHeight / 2;
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
