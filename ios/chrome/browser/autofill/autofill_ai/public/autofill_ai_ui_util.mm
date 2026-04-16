// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kWalletLogoHeight = 26.0;
constexpr CGFloat kWalletLogoSpacing = 6.0;
}  // namespace

namespace autofill {

UIImage* DefaultIconForAutofillAiEntityType(EntityTypeName entity_type_name,
                                            CGFloat symbol_point_size) {
  NSString* symbol_name = nil;
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return SymbolWithPalette(
          CustomSymbolWithPointSize(kPassportSymbol, symbol_point_size), @[
            [UIColor colorNamed:kTextPrimaryColor],
          ]);
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
      symbol_name = kPersonTextRectangleSymbol;
      break;
    case EntityTypeName::kVehicle:
      symbol_name = kCarSymbol;
      break;
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      symbol_name = kPersonFillCheckmarkSymbol;
      break;
    case EntityTypeName::kFlightReservation:
      if (@available(iOS 26, *)) {
        symbol_name = kAirplaneUpRightSymbol;
      } else {
        symbol_name = kAirplaneSymbol;
      }
      break;
    case EntityTypeName::kShipment:
      symbol_name = kBoxTruckFillSymbol;
      break;
    default:
      return nil;
  }

  return SymbolWithPalette(
      DefaultSymbolWithPointSize(symbol_name, symbol_point_size), @[
        [UIColor colorNamed:kTextPrimaryColor],
      ]);
}

NSString* DisplayNameForAutofillAiAttributeType(AttributeType attribute_type) {
  if (attribute_type.name() == AttributeTypeName::kVehicleVin) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_VEHICLE_VIN_NAME);
  }
  return base::SysUTF16ToNSString(attribute_type.GetNameForI18n());
}

NSString* GetDialogTitleForSaveEntity(EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kVehicle:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE);
    default:
      return base::SysUTF16ToNSString(
          EntityType(entity_type_name).GetNameForI18n());
  }
}

NSString* GetDialogTitleForUpdateEntity(EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kVehicle:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE);
    default:
      return base::SysUTF16ToNSString(
          EntityType(entity_type_name).GetNameForI18n());
  }
}

NSString* GetDialogTitleForAddEntity(EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_ADD_PASSPORT_ENTITY);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_ADD_DRIVERS_LICENSE_ENTITY);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_ADD_NATIONAL_ID_CARD_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_ADD_VEHICLE_ENTITY);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_ADD_KNOWN_TRAVELER_NUMBER_ENTITY);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_ADD_REDRESS_NUMBER_ENTITY);
    default:
      return base::SysUTF16ToNSString(
          EntityType(entity_type_name).GetNameForI18n());
  }
}

NSString* GetDialogTitleForViewEntity(EntityTypeName entity_type_name) {
  return base::SysUTF16ToNSString(
      EntityType(entity_type_name).GetNameForI18n());
}

NSString* GetDialogTitleForEditEntity(EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_EDIT_PASSPORT_ENTITY);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_EDIT_DRIVERS_LICENSE_ENTITY);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_EDIT_NATIONAL_ID_CARD_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_EDIT_VEHICLE_ENTITY);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_EDIT_KNOWN_TRAVELER_NUMBER_ENTITY);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_EDIT_REDRESS_NUMBER_ENTITY);
    default:
      return base::SysUTF16ToNSString(
          EntityType(entity_type_name).GetNameForI18n());
  }
}

NSString* GetSaveEntityToWalletFooterText(NSString* user_email) {
  NSString* googleWallet =
      l10n_util::GetNSString(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);
  NSString* linkText =
      l10n_util::GetNSString(IDS_AUTOFILL_MANAGE_YOUR_INFO_LINK);
  NSString* formattedLink =
      [NSString stringWithFormat:@"BEGIN_LINK%@END_LINK", linkText];
  return l10n_util::GetNSStringF(
      IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_NEW,
      base::SysNSStringToUTF16(googleWallet),
      base::SysNSStringToUTF16(formattedLink),
      base::SysNSStringToUTF16(googleWallet),
      base::SysNSStringToUTF16(user_email));
}

NSString* GetUpdateEntitySavedInWalletFooterText(NSString* user_email) {
  NSString* googleWallet =
      l10n_util::GetNSString(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);
  NSString* formattedLink =
      [NSString stringWithFormat:@"BEGIN_LINK%@END_LINK", googleWallet];
  return l10n_util::GetNSStringF(
      IDS_AUTOFILL_AI_UPDATE_ENTITY_TO_WALLET_DIALOG_SUBTITLE,
      base::SysNSStringToUTF16(formattedLink),
      base::SysNSStringToUTF16(user_email));
}

GURL GetManageYourInfoURL() {
  return GURL("https://support.google.com/wallet?p=private_use_across_google");
}

GURL GetGoogleWalletPassesURL() {
  return GURL("https://wallet.google.com/wallet/passes");
}

UIView* CreateBrandedTitleForWalletSave(NSString* title) {
  BrandedNavigationItemTitleView* titleView =
      [[BrandedNavigationItemTitleView alloc] init];
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  titleView.imageLogo = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleWalletIconSymbol, kWalletLogoHeight));
#else
  titleView.imageLogo =
      CustomSymbolWithPointSize(kSparklesSymbol, kWalletLogoHeight);
#endif
  titleView.title = title;
  titleView.titleLogoSpacing = kWalletLogoSpacing;
  titleView.accessibilityLabel = title;
  return titleView;
}

}  // namespace autofill
