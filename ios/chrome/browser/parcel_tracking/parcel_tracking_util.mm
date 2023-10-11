// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"

#import <string>
#import <vector>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_types.h"
#import "components/commerce/core/shopping_service.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"

BASE_FEATURE(kIOSParcelTracking,
             "IOSParcelTracking",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsIOSParcelTrackingEnabled() {
  return base::FeatureList::IsEnabled(kIOSParcelTracking) &&
         !IsParcelTrackingDisabled(GetApplicationContext()->GetLocalState());
}

bool IsUserEligibleParcelTrackingOptInPrompt(
    ChromeBrowserState* browser_state) {
  PrefService* pref_service = browser_state->GetPrefs();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  if (!authentication_service || !authentication_service->initialized()) {
    return false;
  }
  bool signed_in =
      authentication_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  return IsIOSParcelTrackingEnabled() &&
         !pref_service->GetBoolean(
             prefs::kIosParcelTrackingOptInPromptDisplayed) &&
         signed_in;
}

std::vector<std::pair<commerce::ParcelIdentifier::Carrier, std::string>>
ConvertCustomTextCheckingResult(NSArray<CustomTextCheckingResult*>* result) {
  std::vector<std::pair<commerce::ParcelIdentifier::Carrier, std::string>>
      new_parcel_list;
  for (CustomTextCheckingResult* parcel : result) {
    commerce::ParcelIdentifier::Carrier carrier =
        static_cast<commerce::ParcelIdentifier::Carrier>(parcel.carrier);
    std::string tracking_number = base::SysNSStringToUTF8(parcel.carrierNumber);
    new_parcel_list.push_back(std::make_pair(carrier, tracking_number));
  }
  return new_parcel_list;
}

void TrackParcels(
    commerce::ShoppingService* shopping_service,
    NSArray<CustomTextCheckingResult*>* parcels,
    std::string domain,
    id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler,
    bool display_infobar) {
  shopping_service->StartTrackingParcels(
      ConvertCustomTextCheckingResult(parcels), domain,
      base::BindOnce(
          [](bool display_infobar,
             id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler,
             NSArray<CustomTextCheckingResult*>* parcels, bool success,
             std::unique_ptr<std::vector<commerce::ParcelTrackingStatus>>
                 parcel_status) {
            if (success && display_infobar) {
              [parcel_tracking_commands_handler
                  showParcelTrackingInfobarWithParcels:parcels
                                               forStep:ParcelTrackingStep::
                                                           kNewPackageTracked];
            }
          },
          display_infobar, parcel_tracking_commands_handler, parcels));
}
