// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/suggestions_from_gemini_entry_point_consumer.h"

@class TableViewItem;

namespace autofill {
class EntityType;
}  // namespace autofill

// Consumer protocol for the Travel Info settings page.
@protocol TravelInfoConsumer <SuggestionsFromGeminiEntryPointConsumer>

// Sets the lists of travel information.
- (void)setTravelInfoWithFlightReservations:
            (NSArray<TableViewItem*>*)flightReservations
                       knownTravelerNumbers:
                           (NSArray<TableViewItem*>*)knownTravelerNumbers
                             redressNumbers:
                                 (NSArray<TableViewItem*>*)redressNumbers
                                   vehicles:(NSArray<TableViewItem*>*)vehicles;

// Sets the writable entity types that can be added.
- (void)setWritableEntityTypes:
    (const std::vector<autofill::EntityType>&)writableEntityTypes;

// Sets the toggle state for "save and fill travel info", its enabled and
// managed states.
- (void)setTravelInfoToggleState:(BOOL)on
                         enabled:(BOOL)enabled
                         managed:(BOOL)managed;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_CONSUMER_H_
