// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_button_factory.h"

#include <ostream>

#import "base/notreached.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_constants.h"
#import "ios/chrome/browser/ui/badges/badge_delegate.h"
#import "ios/chrome/common/ui/colors/dynamic_color_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BadgeButtonFactory

- (BadgeButton*)badgeButtonForBadgeType:(BadgeType)badgeType {
  switch (badgeType) {
    case BadgeType::kBadgeTypePasswordSave:
      return [self passwordsSaveBadgeButton];
      break;
    case BadgeType::kBadgeTypePasswordUpdate:
      return [self passwordsUpdateBadgeButton];
    case BadgeType::kBadgeTypeSaveCard:
      return [self saveCardBadgeButton];
    case BadgeType::kBadgeTypeTranslate:
      return [self translateBadgeButton];
    case BadgeType::kBadgeTypeIncognito:
      return [self incognitoBadgeButton];
    case BadgeType::kBadgeTypeOverflow:
      return [self overflowBadgeButton];
    case BadgeType::kBadgeTypeSaveAddressProfile:
      return [self saveAddressProfileBadgeButton];
    case BadgeType::kBadgeTypeNone:
      NOTREACHED() << "A badge should not have kBadgeTypeNone";
      return nil;
  }
}

#pragma mark - Private

- (BadgeButton*)passwordsSaveBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypePasswordSave
                     imageNamed:@"password_key"
                  renderingMode:UIImageRenderingModeAlwaysTemplate];
  [button addTarget:self.delegate
                action:@selector(passwordsBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonSavePasswordAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_PASSWORD_HINT);
  return button;
}

- (BadgeButton*)passwordsUpdateBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypePasswordUpdate
                     imageNamed:@"password_key"
                  renderingMode:UIImageRenderingModeAlwaysTemplate];
  [button addTarget:self.delegate
                action:@selector(passwordsBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonUpdatePasswordAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_PASSWORD_HINT);
  return button;
}

- (BadgeButton*)saveCardBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypeSaveCard
                     imageNamed:@"infobar_save_card_icon"
                  renderingMode:UIImageRenderingModeAlwaysTemplate];
  [button addTarget:self.delegate
                action:@selector(saveCardBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier = kBadgeButtonSaveCardAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD_BADGE_HINT);
  return button;
}

- (BadgeButton*)translateBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypeTranslate
                     imageNamed:@"infobar_translate_icon"
                  renderingMode:UIImageRenderingModeAlwaysTemplate];
  [button addTarget:self.delegate
                action:@selector(translateBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier = kBadgeButtonTranslateAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_TRANSLATE_HINT);
  return button;
}

- (BadgeButton*)incognitoBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypeIncognito
                     imageNamed:@"incognito_badge"
                  renderingMode:UIImageRenderingModeAlwaysOriginal];
  button.fullScreenImage = [[UIImage imageNamed:@"incognito_small_badge"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  button.tintColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kTextPrimaryColor], self.incognito,
      [UIColor colorNamed:kTextPrimaryDarkColor]);
  button.accessibilityTraits &= ~UIAccessibilityTraitButton;
  button.userInteractionEnabled = NO;
  button.accessibilityIdentifier = kBadgeButtonIncognitoAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BADGE_INCOGNITO_HINT);
  return button;
}

- (BadgeButton*)overflowBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypeOverflow
                     imageNamed:@"wrench_badge"
                  renderingMode:UIImageRenderingModeAlwaysTemplate];
  [button addTarget:self.delegate
                action:@selector(overflowBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier = kBadgeButtonOverflowAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_BADGE_HINT);
  return button;
}

- (BadgeButton*)saveAddressProfileBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypeSaveAddressProfile
                     imageNamed:@"ic_place"
                  renderingMode:UIImageRenderingModeAlwaysTemplate];
  [button addTarget:self.delegate
                action:@selector(saveAddressProfileBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonSaveAddressProfileAccessibilityIdentifier;
  // TODO(crbug.com/1014652): Create a11y label hint.
  return button;
}

- (BadgeButton*)createButtonForType:(BadgeType)badgeType
                         imageNamed:(NSString*)imageName
                      renderingMode:(UIImageRenderingMode)renderingMode {
  BadgeButton* button = [BadgeButton badgeButtonWithType:badgeType];
  UIImage* image =
      [[UIImage imageNamed:imageName] imageWithRenderingMode:renderingMode];
  button.image = image;
  button.fullScreenOn = NO;
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;
  [NSLayoutConstraint
      activateConstraints:@[ [button.widthAnchor
                              constraintEqualToAnchor:button.heightAnchor] ]];
  return button;
}

@end
