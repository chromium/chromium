// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"

#import "base/strings/string_util.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#import "components/autofill/core/browser/webdata/account_settings/account_setting_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/autofill/model/ios_account_setting_service_factory.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/metrics/model/google_groups_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace autofill {

const std::string GetCountryCodeFromVariations() {
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  return variations_service
             ? base::ToUpperASCII(variations_service->GetLatestCountry())
             : std::string();
}

bool IsWalletStorageEnabled(ProfileIOS* profile) {
  AccountSettingService* setting_service =
      IOSAccountSettingServiceFactory::GetForProfile(profile);
  return setting_service &&
         setting_service->IsWalletPrivacyContextualSurfacingEnabled();
}

bool CanPerformAutofillAiAction(ProfileIOS* profile, AutofillAiAction action) {
  EntityDataManager* entity_data_manager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile);
  if (!entity_data_manager) {
    return false;
  }

  // `MayPerformAutofillAiAction` has two overloads:
  // - One that takes a reference to `AutofillClient`, an action and two other
  //   parameters.
  // - One that takes 8 separate parameters, an action and two other
  //   parameters.
  //
  // The latter is used here since Settings, where this function is being used,
  // does not have an associated `WebState`, and therefore, does not have an
  // `AutofillClient`.
  // IdentityManagerFactory, GoogleGroupsManagerFactory and SyncServiceFactory
  // require original profile. Here SyncServiceFactory uses the profile directly
  // just as ChromeAutofillClientIOS does.
  //
  // For Incognito profiles, it is allowed to fill only based on cached
  // predictions. It is also allowed to manage data. Although it is not relevant
  // for Bling since the management of data is done in Settings, which is not
  // in Incognito. No save. No model inference on forms. Therefore, there will
  // be no sync service if the profile is an Incognito profile.
  return MayPerformAutofillAiAction(
      GoogleGroupsManagerFactory::GetForProfile(profile->GetOriginalProfile()),
      profile->GetPrefs(), entity_data_manager,
      IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile()),
      SyncServiceFactory::GetForProfile(profile),
      IsWalletStorageEnabled(profile), profile->IsOffTheRecord(),
      GeoIpCountryCode(GetCountryCodeFromVariations()), action);
}

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
    default:
      return nil;
  }

  return SymbolWithPalette(
      DefaultSymbolWithPointSize(symbol_name, symbol_point_size), @[
        [UIColor colorNamed:kTextPrimaryColor],
      ]);
}

bool IsEnhancedAutofillEnabled(ProfileIOS* profile) {
  ProfileIOS* original_profile = profile->GetOriginalProfile();
  return autofill::GetAutofillAiOptInStatus(
      original_profile->GetPrefs(),
      IdentityManagerFactory::GetForProfile(original_profile));
}

void SetEnhancedAutofillEnabled(ProfileIOS* profile, bool enabled) {
  ProfileIOS* original_profile = profile->GetOriginalProfile();
  autofill::SetAutofillAiOptInStatus(
      GoogleGroupsManagerFactory::GetForProfile(original_profile),
      original_profile->GetPrefs(),
      IOSAutofillEntityDataManagerFactory::GetForProfile(original_profile),
      IdentityManagerFactory::GetForProfile(original_profile),
      SyncServiceFactory::GetForProfile(original_profile),
      IsWalletStorageEnabled(original_profile),
      original_profile->IsOffTheRecord(),
      GeoIpCountryCode(GetCountryCodeFromVariations()),
      enabled ? autofill::AutofillAiOptInStatus::kOptedIn
              : autofill::AutofillAiOptInStatus::kOptedOut);
}

}  // namespace autofill
