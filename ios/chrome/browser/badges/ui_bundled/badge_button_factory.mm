// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/badge_button_factory.h"

#import <ostream>

#import "base/notreached.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_button.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_delegate.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_overflow_menu_util.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
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

// Modifier for changing the infobar symbol size.
const CGFloat kInfobarSymbolPointSizeModifier = 4;

}  // namespace

@implementation BadgeButtonFactory

- (BadgeButton*)badgeButtonForBadgeType:(BadgeType)badgeType
                           usingInfoBar:(InfoBarIOS*)infoBar {
  switch (badgeType) {
    case kBadgeTypeTranslate:
      return [self translateBadgeButton];
    case kBadgeTypeIncognito:
      return [self incognitoBadgeButton];
    case kBadgeTypeOverflow:
      return [self overflowBadgeButton];
    case kBadgeTypePermissionsCamera:
      return [self permissionsCameraBadgeButton];
    case kBadgeTypePermissionsMicrophone:
      return [self permissionsMicrophoneBadgeButton];
    case kBadgeTypeReaderMode:
      return [self readerModeBadgeButton];
    case kBadgeTypeNone:
    case kBadgeTypePasswordSave:
    case kBadgeTypePasswordUpdate:
    case kBadgeTypeSaveCard:
    case kBadgeTypeSaveAddressProfile:
      NOTREACHED() << "Badge of type " << badgeType << " should not be used.";
  }
}

#pragma mark - Private

- (BadgeButton*)translateBadgeButton {
  UIImage* image =
      SymbolWithPointSize(SymbolTranslate, [self infoBarSymbolPointSize]);
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

  image = SymbolWithPalette(
      SymbolWithPointSize(SymbolIncognitoCircleFill, kSymbolIncognitoPointSize),
      SmallIncognitoPalette());
  button = [self createButtonForType:kBadgeTypeIncognito image:image];
  button.fullScreenImage = SymbolTemplateWithPointSize(
      SymbolIncognito, kSymbolIncognitoFullScreenPointSize);

  button.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  button.accessibilityTraits &= ~UIAccessibilityTraitButton;
  button.userInteractionEnabled = NO;
  button.accessibilityIdentifier = kBadgeButtonIncognitoAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BADGE_INCOGNITO_HINT);
  return button;
}

- (BadgeButton*)overflowBadgeButton {
  Symbol symbol = IsProactiveSuggestionsFrameworkEnabled()
                      ? SymbolEllipsis
                      : SymbolEllipsisCircleFill;

  UIImage* image = SymbolWithPointSize(symbol, [self infoBarSymbolPointSize]);

  if (IsProactiveSuggestionsFrameworkEnabled()) {
    image = [image imageWithTintColor:[UIColor whiteColor]
                        renderingMode:UIImageRenderingModeAlwaysOriginal];
  }

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

- (BadgeButton*)permissionsCameraBadgeButton {
  BadgeButton* button = [self
      createButtonForType:kBadgeTypePermissionsCamera
                    image:SymbolTemplateWithPointSize(
                              SymbolCameraFill, [self infoBarSymbolPointSize])];
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
  BadgeButton* button =
      [self createButtonForType:kBadgeTypePermissionsMicrophone
                          image:SymbolTemplateWithPointSize(
                                    SymbolMicrophoneFill,
                                    [self infoBarSymbolPointSize])];
  [button addTarget:self.delegate
                action:@selector(permissionsBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonPermissionsMicrophoneAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_PERMISSIONS_HINT);
  return button;
}

- (BadgeButton*)readerModeBadgeButton {
  BadgeButton* button = [self
      createButtonForType:kBadgeTypeReaderMode
                    image:SymbolTemplateWithPointSize(
                              SymbolReaderMode, [self infoBarSymbolPointSize])];
  button.accessibilityIdentifier =
      kBadgeButtonReaderModeAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_READER_MODE_CHIP_ACCESSIBILITY_LABEL);
  return button;
}

- (BadgeButton*)createButtonForType:(BadgeType)badgeType image:(UIImage*)image {
  BadgeButton* button = [BadgeButton badgeButtonWithType:badgeType];
  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:[self infoBarSymbolPointSize]
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

// Returns the size of the infobar symbol image.
- (CGFloat)infoBarSymbolPointSize {
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    return kInfobarSymbolPointSize - kInfobarSymbolPointSizeModifier;
  }

  return kInfobarSymbolPointSize;
}

@end
