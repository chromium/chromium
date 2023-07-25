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

// Tracks destination usage from the new overflow menu and implements a
// frecency-based sorting algorithm (i.e. an algorithm that uses the data
// frequency and data recency when determining sort order) to order destinations
// shown in the overflow menu carousel. The algorithm, at a high-level, works as
// follows:
//
// (1) Divide the destinations carousel into two groups: (A) visible
// "above-the-fold" destinations and (B) non-visible "below-the-fold"
// destinations; "below-the-fold" destinations are made visible to the user when
// they scroll the carousel.
//
// (2) Get each destination's number of clicks.
//
// (3) Compare destination with highest number of clicks in [group B] to
// destination with lowest number of clicks in [group A].
//
// (4) Swap (i.e. "promote") the [group B] destination with the [group A] one if
// B's number of clicks exceeds A's.
//
// Destinations may optionally be displayed with one of three badge types (in
// priority order):
//
//   1. Error badge
//   2. Promo badge
//   3. New badge
//
// Destinations with badges will ignore the ranking determined by the sorting
// algorithm, and instead be inserted into the carousel starting at position
// kNewDestinationsInsertionIndex.
//
// Destinations with badges of different type will order themselves according
// to the priority order defined above.
//
// Expects `-start` to be called before any other method is invoked.
//
// Expects `visibleDestinationsCount` to be set to a non zero value before
// calling `-sortedDestinationsFromCarouselDestinations:carouselDestinations`.
//
// Expects `-stop` to be called before deallocation.
@interface DestinationUsageHistory : NSObject

// The number of destinations immediately visible in the carousel when the
// overflow menu is opened.
@property(nonatomic, assign) NSInteger visibleDestinationsCount;

// Designated initializer. Initializes with `prefService`.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Records a `destination` click from the overflow menu carousel.
- (void)recordClickForDestination:(overflow_menu::Destination)destination;

// Returns a new frecency-sorted list of overflow_menu::Destination given the
// current ranking and a list of all available destinations.
- (DestinationRanking)
    sortedDestinationsFromCurrentRanking:(DestinationRanking)currentRanking
                   availableDestinations:
                       (DestinationRanking)availableDestinations;

// Tells the object to clear any stored click data for all destinations.
- (void)clearStoredClickData;

// Stops the Destination Usage History.
- (void)stop;

// Starts the Destination Usage History.
- (void)start;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_DESTINATION_USAGE_HISTORY_DESTINATION_USAGE_HISTORY_H_
