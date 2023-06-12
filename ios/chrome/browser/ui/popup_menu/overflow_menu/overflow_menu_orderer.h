// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ORDERER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ORDERER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"

namespace overflow_menu {
enum class Destination;
}
@class OverflowMenuDestination;
class PrefService;

@protocol OverflowMenuDestinationProvider <NSObject>

- (DestinationRanking)baseDestinations;

// Returns the correct `OverflowMenuDestination` for the corresponding
// `overflow_menu::Destination` on the current page. Returns nil if the current
// page does not support the given `destinationType`.
- (OverflowMenuDestination*)destinationForDestinationType:
    (overflow_menu::Destination)destinationType;

@end

// Controls the order of all the items in the overflow menu.
@interface OverflowMenuOrderer : NSObject

- (instancetype)initWithIsIncognito:(BOOL)isIncognito NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Pref service to retrieve local state preference values.
@property(nonatomic, assign) PrefService* localStatePrefs;

// The number of destinations immediately visible to the user when opening the
// new overflow menu (i.e. the number of "above-the-fold" destinations).
@property(nonatomic, assign) int visibleDestinationsCount;

@property(nonatomic, weak) id<OverflowMenuDestinationProvider>
    destinationProvider;

// Release any C++ objects that can't be reference counted.
- (void)disconnect;

// Records a `destination` click from the overflow menu carousel.
- (void)recordClickForDestination:(overflow_menu::Destination)destination;

// Returns a new, sorted list of destinations given the initial list.
- (NSArray<OverflowMenuDestination*>*)sortedDestinations;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ORDERER_H_
