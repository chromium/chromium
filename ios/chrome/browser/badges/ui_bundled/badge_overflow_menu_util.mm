// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/badge_overflow_menu_util.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/badges/ui_bundled/badges_histograms.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#import <UIKit/UIKit.h>

namespace {

// The image used for password related badges.
UIImage* GetPasswordImage() {
  return CustomSymbolTemplateWithPointSize(kPasswordSymbol,
                                           kInfobarSymbolPointSize);
}

// The menu element for `badgeType` shown in the overflow menu when the overflow
// badge is tapped.
UIAction* GetOverflowMenuElementForBadgeType(
    BadgeType badge_type,
    ShowModalFunction show_modal_function) {
  NSString* title;
  UIActionIdentifier action_identifier;
  UIImage* image;
  MobileMessagesInfobarType histogram_type = MobileMessagesInfobarType::Confirm;

  switch (badge_type) {
    case kBadgeTypePasswordSave:
      action_identifier = kBadgeButtonSavePasswordActionIdentifier;
      title =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_TITLE);
      image = GetPasswordImage();
      histogram_type = MobileMessagesInfobarType::SavePassword;
      break;
    case kBadgeTypePasswordUpdate:
      action_identifier = kBadgeButtonUpdatePasswordActionIdentifier;
      title = l10n_util::GetNSString(
          IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD_TITLE);
      image = GetPasswordImage();
      histogram_type = MobileMessagesInfobarType::UpdatePassword;
      break;
    case kBadgeTypeSaveAddressProfile:
      action_identifier = kBadgeButtonSaveAddressProfileActionIdentifier;
      title =
          l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
      image =
          CustomSymbolWithPointSize(kLocationSymbol, kInfobarSymbolPointSize);
      histogram_type = MobileMessagesInfobarType::AutofillSaveAddressProfile;
      break;
    case kBadgeTypeSaveCard:
      action_identifier = kBadgeButtonSaveCardActionIdentifier;
      title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
      image = DefaultSymbolWithPointSize(kCreditCardSymbol,
                                         kInfobarSymbolPointSize);
      histogram_type = MobileMessagesInfobarType::SaveCard;
      break;
    case kBadgeTypeTranslate:
      action_identifier = kBadgeButtonTranslateActionIdentifier;
      title = l10n_util::GetNSString(IDS_IOS_TRANSLATE_INFOBAR_MODAL_TITLE);
      image =
          CustomSymbolWithPointSize(kTranslateSymbol, kInfobarSymbolPointSize);
      break;
    case kBadgeTypePermissionsCamera:
      action_identifier = kBadgeButtonPermissionsActionIdentifier;
      title = l10n_util::GetNSString(
          IDS_IOS_PERMISSIONS_INFOBAR_OVERFLOW_POPUP_TITLE);
      image = CustomSymbolWithPointSize(kCameraSymbol, kInfobarSymbolPointSize);
      histogram_type = MobileMessagesInfobarType::Permissions;
      break;
    case kBadgeTypePermissionsMicrophone:
      action_identifier = kBadgeButtonPermissionsActionIdentifier;
      title = l10n_util::GetNSString(
          IDS_IOS_PERMISSIONS_INFOBAR_OVERFLOW_POPUP_TITLE);
      image = DefaultSymbolTemplateWithPointSize(kMicrophoneSymbol,
                                                 kInfobarSymbolPointSize);
      histogram_type = MobileMessagesInfobarType::Permissions;
      break;
    case kBadgeTypeParcelTracking:
      action_identifier = kBadgeButtonParcelTrackingActionIdentifier;
      title = l10n_util::GetNSString(
          IDS_IOS_PARCEL_TRACKING_INFOBAR_NEW_PACKAGE_TRACKED_TITLE);
      image = DefaultSymbolWithPointSize(kShippingBoxSymbol,
                                         kInfobarSymbolPointSize);
      break;
    case kBadgeTypeIncognito:
      NOTREACHED_IN_MIGRATION()
          << "An overflow menu badge should not be an Incognito badge";
      break;
    case kBadgeTypeOverflow:
      NOTREACHED_IN_MIGRATION()
          << "A overflow menu badge should not be an overflow badge";
      break;
    case kBadgeTypeNone:
      NOTREACHED_IN_MIGRATION() << "A badge should not have kBadgeTypeNone";
      break;
  }

  UIActionHandler handler = ^(UIAction* action) {
    base::UmaHistogramEnumeration(kInfobarOverflowMenuTappedHistogram,
                                  histogram_type);
    show_modal_function(badge_type);
  };

  return [UIAction actionWithTitle:title
                             image:image
                        identifier:action_identifier
                           handler:handler];
}

}  // namespace

UIMenu* GetOverflowMenuFromBadgeTypes(NSArray<NSNumber*>* badge_types,
                                      ShowModalFunction show_modal_function) {
  NSMutableArray<UIMenuElement*>* menu_elements = [NSMutableArray array];
  for (NSNumber* badge_type_wrapped in badge_types) {
    BadgeType badgeType = BadgeType(badge_type_wrapped.unsignedIntegerValue);
    [menu_elements addObject:GetOverflowMenuElementForBadgeType(
                                 badgeType, show_modal_function)];
  }
  return [UIMenu menuWithTitle:@"" children:menu_elements];
}
