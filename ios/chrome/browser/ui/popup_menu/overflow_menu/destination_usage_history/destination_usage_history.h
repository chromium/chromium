// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_DESTINATION_USAGE_HISTORY_DESTINATION_USAGE_HISTORY_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_DESTINATION_USAGE_HISTORY_DESTINATION_USAGE_HISTORY_H_

#import <UIKit/UIKit.h>

#include "base/values.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

class PrefService;

// Maintains a history of which items from the new overflow menu carousel the
// user clicks and suggests a sorted order for carousel menu items (based on
// usage frecency).
@interface DestinationUsageHistory : NSObject

// Pref service to retrieve/store preference values.
@property(nonatomic, assign) PrefService* prefService;

// Records a destination click from the overflow menu carousel.
- (void)trackDestinationClick:(overflow_menu::Destination)destination
     numAboveFoldDestinations:(int)numAboveFoldDestinations;

// Returns a frecency-sorted list of OverflowMenuDestination* given an unsorted
// list `unrankedDestinations`.
- (NSArray<OverflowMenuDestination*>*)generateDestinationsList:
    (NSArray<OverflowMenuDestination*>*)unrankedDestinations;

// [Publicly exposed for testing purposes only] Ingests given `ranking` and
// `numAboveFoldDestinations` and returns new ranking by running frecency
// algorithm on internally-managed destination usage history.
// `numAboveFoldDestinations` represents the number of destinations immediately
// visible to the user when opening the new overflow menu (i.e. the number of
// "above-the-fold" destinations).
- (std::vector<overflow_menu::Destination>)
    updatedRankWithCurrentRanking:
        (std::vector<overflow_menu::Destination>&)previousRanking
         numAboveFoldDestinations:(int)numAboveFoldDestinations;

// Designated initializer. Initializes with `prefService`.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the Destination Usage History.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_DESTINATION_USAGE_HISTORY_DESTINATION_USAGE_HISTORY_H_
