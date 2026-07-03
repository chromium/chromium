// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/common/autofill_payments_features.h"
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
                                            bool is_personal_context,
                                            CGFloat symbol_point_size,
                                            UIColor* tint_color) {
  // TODO(crbug.com/523320919): Return different icons when is_personal_context
  // is true.
  // Identify if the symbol is custom (always true for personal context
  // entities).
  bool is_custom_symbol = is_personal_context;
  NSString* symbol_name = nil;
  UIColor* color = tint_color ?: [UIColor colorNamed:kTextPrimaryColor];

  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      symbol_name =
          is_personal_context ? kPassportSparkSymbol : kPassportSymbol;
      // Passport symbols are custom symbols.
      is_custom_symbol = YES;
      break;
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
      symbol_name = is_personal_context ? kPersonTextRectangleSparkSymbol
                                        : kPersonTextRectangleSymbol;
      break;
    case EntityTypeName::kVehicle:
      symbol_name = is_personal_context ? kCarSparkSymbol : kCarSymbol;
      break;
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      symbol_name = is_personal_context ? kPersonTextRectangle2SparkSymbol
                                        : kPersonTextRectangle2Symbol;
      // Known travel numbers and redress number symbols are custom symbols.
      is_custom_symbol = YES;
      break;
    case EntityTypeName::kFlightReservation:
      symbol_name = kAirplaneUpSymbol;
      // The flight reservation symbol is a custom symbol.
      is_custom_symbol = YES;
      break;
    case EntityTypeName::kShipment:
      symbol_name =
          is_personal_context ? kTruckBoxSparkSymbol : kTruckBoxSymbol;
      break;
    case EntityTypeName::kOrder:
      symbol_name = is_personal_context ? kBagSparkSymbol : kBagSymbol;
      break;
    default:
      return nil;
  }

  return SymbolWithPalette(
      is_custom_symbol
          ? CustomSymbolWithPointSize(symbol_name, symbol_point_size)
          : DefaultSymbolWithPointSize(symbol_name, symbol_point_size),
      @[ color ]);
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
  return l10n_util::GetNSStringF(GetSaveToWalletSubtitleStringId(),
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

UIImage* GetWalletLogo(CGFloat point_size, UIColor* tint_color) {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  NSString* symbol =
      base::FeatureList::IsEnabled(features::kAutofillEnableGradientGoogleLogos)
          ? kGoogleWalletIconV2Symbol
          : kGoogleWalletIconSymbol;
  return MakeSymbolMulticolor(CustomSymbolWithPointSize(symbol, point_size));
#else
  return SymbolWithPalette(
      DefaultSymbolWithPointSize(kSparklesSymbol, point_size),
      @[ tint_color ?: [UIColor colorNamed:kBlue600Color] ]);
#endif
}

UIView* CreateBrandedTitleForWalletSave(NSString* title) {
  BrandedNavigationItemTitleView* titleView =
      [[BrandedNavigationItemTitleView alloc] init];
  titleView.imageLogo = GetWalletLogo(kWalletLogoHeight, nil);
  titleView.title = title;
  titleView.titleLogoSpacing = kWalletLogoSpacing;
  titleView.accessibilityLabel = title;
  return titleView;
}

int GetSaveToWalletSubtitleStringId() {
  return IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_NEW;
}

int GetSaveEntityAcceptButtonStringId() {
  return IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL;
}

}  // namespace autofill
