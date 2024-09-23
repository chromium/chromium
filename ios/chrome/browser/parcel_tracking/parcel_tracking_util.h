// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_UTIL_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_UTIL_H_

#import "base/feature_list.h"
#import "components/commerce/core/proto/parcel.pb.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/parcel_tracking/tracking_source.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"

namespace commerce {
class ShoppingService;
}  // namespace commerce

@protocol ParcelTrackingOptInCommands;

// Returns true if the user is eligible for the parcel tracking opt-in prompt.
// The user must have never before seen the prompt and must be signed in.
bool IsUserEligibleParcelTrackingOptInPrompt(
    PrefService* pref_service,
    commerce::ShoppingService* shopping_service);

// Takes NSArray<CustomTextCheckingResult*>* `result` and returns a
// corresponding vector of parcel carrier and tracking number pairs.
std::vector<std::pair<commerce::ParcelIdentifier::Carrier, std::string>>
ConvertCustomTextCheckingResult(NSArray<CustomTextCheckingResult*>* result);

// Tracks the list of parcels. If successful and `display_infobar` is true,
// triggers an infobar display to confirm that the package has been tracked.
void TrackParcels(
    commerce::ShoppingService* shopping_service,
    NSArray<CustomTextCheckingResult*>* parcels,
    std::string domain,
    id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler,
    bool display_infobar,
    TrackingSource source);

// Displays the parcel tracking opt-in UI for the new parcels from parcel list
// `parcels`. "New parcels" are parcels that are not already being tracked.
void FilterParcelsAndShowParcelTrackingUI(
    commerce::ShoppingService* shopping_service,
    NSArray<CustomTextCheckingResult*>* parcels,
    id<ParcelTrackingOptInCommands> parcel_tracking_commands_handler);

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_PARCEL_TRACKING_UTIL_H_
