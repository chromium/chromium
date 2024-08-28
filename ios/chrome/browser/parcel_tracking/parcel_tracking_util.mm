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
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/parcel_tracking/metrics.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_opt_in_status.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

namespace {
const CGFloat parcelLimit = 5;
}  // namespace

bool IsUserEligibleParcelTrackingOptInPrompt(
    PrefService* pref_service,
    commerce::ShoppingService* shopping_service) {
  return IsIOSParcelTrackingEnabled() &&
         !IsParcelTrackingDisabled(
             IsHomeCustomizationEnabled()
                 ? pref_service
                 : GetApplicationContext()->GetLocalState()) &&
         !pref_service->GetBoolean(
             prefs::kIosParcelTrackingOptInPromptDisplayLimitMet) &&
         shopping_service->IsParcelTrackingEligible();
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
    bool display_infobar,
    TrackingSource source) {
  shopping_service->StartTrackingParcels(
      ConvertCustomTextCheckingResult(parcels), domain,
      base::BindOnce(
          [](bool display_infobar,
             id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler,
             NSArray<CustomTextCheckingResult*>* parcels, TrackingSource source,
             bool success,
             std::unique_ptr<std::vector<commerce::ParcelTrackingStatus>>
                 parcel_status) {
            if (success) {
              RecordModuleFreshnessSignal(
                  ContentSuggestionsModuleType::kParcelTracking);
              parcel_tracking::RecordParcelsTracked(source, parcels.count);
              if (display_infobar) {
                [parcel_tracking_commands_handler
                    showParcelTrackingInfobarWithParcels:parcels
                                                 forStep:
                                                     ParcelTrackingStep::
                                                         kNewPackageTracked];
              }
            }
          },
          display_infobar, parcel_tracking_commands_handler, parcels, source));
}

void FilterParcelsAndShowParcelTrackingUI(
    commerce::ShoppingService* shopping_service,
    NSArray<CustomTextCheckingResult*>* parcels,
    id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler) {
  shopping_service->GetAllParcelStatuses(base::BindOnce(
      [](id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler,
         NSArray<CustomTextCheckingResult*>* parcels, bool success,
         std::unique_ptr<std::vector<commerce::ParcelTrackingStatus>>
             statuses) {
        NSMutableSet* parcel_numbers = [NSMutableSet
            setWithArray:[parcels valueForKeyPath:@"carrierNumber"]];
        // Remove the tracking numbers of already tracked parcels from
        // parcel_numbers array.
        for (commerce::ParcelTrackingStatus status : *statuses) {
          NSString* tracking_id = base::SysUTF8ToNSString(status.tracking_id);
          if ([parcel_numbers containsObject:tracking_id]) {
            [parcel_numbers removeObject:tracking_id];
          }
        }
        // Add the remaining parcels to filtered_parcels array.
        NSMutableArray<CustomTextCheckingResult*>* filtered_parcels =
            [[NSMutableArray alloc] init];
        for (CustomTextCheckingResult* parcel : parcels) {
          if ([parcel_numbers containsObject:parcel.carrierNumber]) {
            [filtered_parcels addObject:parcel];
          }
        }
        // Only track or offer to track when number of packages is less than 6.
        if (filtered_parcels.count > 0 &&
            filtered_parcels.count <= parcelLimit) {
          [parcel_tracking_commands_handler
              showTrackingForFilteredParcels:filtered_parcels];
        }
      },
      parcel_tracking_commands_handler, parcels));
}
