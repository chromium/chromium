// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/badge_button_factory.h"

#import <ostream>

#import "base/notreached.h"
#import "build/build_config.h"
#import "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_button.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_delegate.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_overflow_menu_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// The identifier for the new popup menu action trigger.
NSString* const kOverflowPopupMenuActionIdentifier =
    @"kOverflowPopupMenuActionIdentifier";

// The size of the incognito symbol image.
const CGFloat kSymbolIncognitoPointSize = 28.;

// The size of the incognito full screen symbol image.
const CGFloat kSymbolIncognitoFullScreenPointSize = 14.;

}  // namespace

@implementation BadgeButtonFactory

- (BadgeButton*)badgeButtonForBadgeType:(BadgeType)badgeType
                           usingInfoBar:(InfoBarIOS*)infoBar {
  switch (badgeType) {
    case kBadgeTypePasswordSave:
      return [self passwordsSaveBadgeButton];
    case kBadgeTypePasswordUpdate:
      return [self passwordsUpdateBadgeButton];
    case kBadgeTypeSaveCard:
      return [self saveCardBadgeButton];
    case kBadgeTypeTranslate:
      return [self translateBadgeButton];
    case kBadgeTypeIncognito:
      return [self incognitoBadgeButton];
    case kBadgeTypeOverflow:
      return [self overflowBadgeButton];
    case kBadgeTypeSaveAddressProfile:
      return [self saveAddressProfileBadgeButton:infoBar];
    case kBadgeTypePermissionsCamera:
      return [self permissionsCameraBadgeButton];
    case kBadgeTypePermissionsMicrophone:
      return [self permissionsMicrophoneBadgeButton];
    case kBadgeTypeParcelTracking:
      return [self parcelTrackingBadgeButton];
    case kBadgeTypeNone:
      NOTREACHED_IN_MIGRATION() << "A badge should not have kBadgeTypeNone";
      return nil;
  }
}

#pragma mark - Private
- (BadgeButton*)passwordsSaveBadgeButton {
  UIImage* image =
      CustomSymbolWithPointSize(kPasswordSymbol, kInfobarSymbolPointSize);
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  image = CustomSymbolWithPointSize(kMulticolorPasswordSymbol,
                                    kInfobarSymbolPointSize);
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  BadgeButton* button = [self createButtonForType:kBadgeTypePasswordSave
                                            image:image];
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
  UIImage* image =
      CustomSymbolWithPointSize(kPasswordSymbol, kInfobarSymbolPointSize);
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  image = CustomSymbolWithPointSize(kMulticolorPasswordSymbol,
                                    kInfobarSymbolPointSize);
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  BadgeButton* button = [self createButtonForType:kBadgeTypePasswordUpdate
                                            image:image];
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
  UIImage* image =
      DefaultSymbolWithPointSize(kCreditCardSymbol, kInfobarSymbolPointSize);
  BadgeButton* button = [self createButtonForType:kBadgeTypeSaveCard
                                            image:image];
  [button addTarget:self.delegate
                action:@selector(saveCardBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier = kBadgeButtonSaveCardAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD_BADGE_HINT);
  return button;
}

- (BadgeButton*)translateBadgeButton {
  UIImage* image =
      CustomSymbolWithPointSize(kTranslateSymbol, kInfobarSymbolPointSize);
  BadgeButton* button = [self createButtonForType:kBadgeTypeTranslate
                                            image:image];
  [button addTarget:self.delegate
                action:@selector(translateBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier = kBadgeButtonTranslateAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_TRANSLATE_HINT);
  return button;
}

- (BadgeButton*)incognitoBadgeButton {
  BadgeButton* button;
  UIImage* image;

  image =
      SymbolWithPalette(CustomSymbolWithPointSize(kIncognitoCircleFillSymbol,
                                                  kSymbolIncognitoPointSize),
                        SmallIncognitoPalette());
  button = [self createButtonForType:kBadgeTypeIncognito image:image];
  button.fullScreenImage = CustomSymbolTemplateWithPointSize(
      kIncognitoSymbol, kSymbolIncognitoFullScreenPointSize);

  button.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  button.accessibilityTraits &= ~UIAccessibilityTraitButton;
  button.userInteractionEnabled = NO;
  button.accessibilityIdentifier = kBadgeButtonIncognitoAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BADGE_INCOGNITO_HINT);
  return button;
}

- (BadgeButton*)overflowBadgeButton {
  UIImage* image = DefaultSymbolWithPointSize(kEllipsisCircleFillSymbol,
                                              kInfobarSymbolPointSize);
  BadgeButton* button = [self createButtonForType:kBadgeTypeOverflow
                                            image:image];
  button.accessibilityIdentifier = kBadgeButtonOverflowAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_BADGE_HINT);

  // Configure new overflow popup menu if enabled.
  button.showsMenuAsPrimaryAction = YES;

  // Adds an empty menu so the event triggers the first time.
  button.menu = [UIMenu menuWithChildren:@[]];
  [button removeActionForIdentifier:kOverflowPopupMenuActionIdentifier
                   forControlEvents:UIControlEventMenuActionTriggered];

  // Configure actions that should be executed on each tap of the overflow
  // badge button to make sure the right overflow menu items are showing up.
  __weak UIButton* weakButton = button;
  __weak BadgeButtonFactory* weakSelf = self;
  void (^showModalFunction)(BadgeType) = ^(BadgeType badgeType) {
    [weakSelf.delegate showModalForBadgeType:badgeType];
  };
  void (^buttonTapHandler)(UIAction*) = ^(UIAction* action) {
    [weakSelf.delegate overflowBadgeButtonTapped:weakButton];
    weakButton.menu = GetOverflowMenuFromBadgeTypes(
        weakSelf.delegate.badgeTypesForOverflowMenu, showModalFunction);
  };
  UIAction* action =
      [UIAction actionWithTitle:@""
                          image:nil
                     identifier:kOverflowPopupMenuActionIdentifier
                        handler:buttonTapHandler];

  // Attach the action to the button.
  [button addAction:action forControlEvents:UIControlEventMenuActionTriggered];
  return button;
}

- (BadgeButton*)saveAddressProfileBadgeButton:(InfoBarIOS*)infoBar {
  UIImage* image =
      CustomSymbolWithPointSize(kLocationSymbol, kInfobarSymbolPointSize);

  if (infoBar) {
    autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
        static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
            infoBar->delegate());
    CHECK(delegate);
    if (delegate->IsMigrationToAccount()) {
      image = CustomSymbolWithPointSize(kCloudAndArrowUpSymbol,
                                        kInfobarSymbolPointSize);
    }
  }

  BadgeButton* button = [self createButtonForType:kBadgeTypeSaveAddressProfile
                                            image:image];
  [button addTarget:self.delegate
                action:@selector(saveAddressProfileBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonSaveAddressProfileAccessibilityIdentifier;
  // TODO(crbug.com/40103383): Create a11y label hint.
  return button;
}

- (BadgeButton*)permissionsCameraBadgeButton {
  BadgeButton* button = [self
      createButtonForType:kBadgeTypePermissionsCamera
                    image:CustomSymbolTemplateWithPointSize(
                              kCameraFillSymbol, kInfobarSymbolPointSize)];
  [button addTarget:self.delegate
                action:@selector(permissionsBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonPermissionsCameraAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_PERMISSIONS_HINT);
  return button;
}

- (BadgeButton*)permissionsMicrophoneBadgeButton {
  BadgeButton* button = [self
      createButtonForType:kBadgeTypePermissionsMicrophone
                    image:DefaultSymbolTemplateWithPointSize(
                              kMicrophoneFillSymbol, kInfobarSymbolPointSize)];
  [button addTarget:self.delegate
                action:@selector(permissionsBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonPermissionsMicrophoneAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_PERMISSIONS_HINT);
  return button;
}

- (BadgeButton*)parcelTrackingBadgeButton {
  UIImage* image =
      DefaultSymbolWithPointSize(kShippingBoxSymbol, kInfobarSymbolPointSize);
  BadgeButton* button = [self createButtonForType:kBadgeTypeParcelTracking
                                            image:image];
  [button addTarget:self.delegate
                action:@selector(parcelTrackingBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonParcelTrackingAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_PARCEL_TRACKING_HINT);
  return button;
}

- (BadgeButton*)createButtonForType:(BadgeType)badgeType image:(UIImage*)image {
  BadgeButton* button = [BadgeButton badgeButtonWithType:badgeType];
  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kInfobarSymbolPointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  [button setPreferredSymbolConfiguration:symbolConfig
                          forImageInState:UIControlStateNormal];
  button.image = image;
  button.fullScreenOn = NO;
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;
  [NSLayoutConstraint
      activateConstraints:@[ [button.widthAnchor
                              constraintEqualToAnchor:button.heightAnchor] ]];
  return button;
}

@end
