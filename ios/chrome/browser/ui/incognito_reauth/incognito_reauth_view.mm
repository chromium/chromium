// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_view.h"

#import <LocalAuthentication/LocalAuthentication.h>

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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

    _authenticateButton =
        [IncognitoReauthView newRoundButtonWithBlurEffect:blurEffect];
    [_authenticateButton
        setTitle:[IncognitoReauthView authenticationActionLabel]
        forState:UIControlStateNormal];

    _tabSwitcherButton =
        [IncognitoReauthView newRoundButtonWithBlurEffect:blurEffect];
    // TODO(crbug.com/1138892): add localized text.
    [_tabSwitcherButton setTitle:@"[Test String] Go to Tab Switcher"
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

+ (NSString*)authenticationActionLabel {
  LAContext* ctx = [[LAContext alloc] init];
  // Call canEvaluatePolicy:error: once to populate biometrics type
  [ctx canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                   error:nil];
  switch (ctx.biometryType) {
    case LABiometryTypeFaceID:
      return @"Face ID";
    case LABiometryTypeTouchID:
      return @"Touch ID";
    default:
      // TODO(crbug.com/1138892): add localized text.
      return @"[Test String] Passcode";
  }
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
