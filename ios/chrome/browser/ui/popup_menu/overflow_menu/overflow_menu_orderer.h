// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ORDERER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ORDERER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"

namespace overflow_menu {
enum class Destination;
enum class ActionType;
}
@class OverflowMenuAction;
@class OverflowMenuDestination;
class PrefService;

@protocol OverflowMenuDestinationProvider <NSObject>

// The default base ranking of destinations currently supported.
- (DestinationRanking)baseDestinations;

// Returns the correct `OverflowMenuDestination` for the corresponding
// `overflow_menu::Destination` on the current page. Returns nil if the current
// page does not support the given `destinationType`.
- (OverflowMenuDestination*)destinationForDestinationType:
    (overflow_menu::Destination)destinationType;

@end

@protocol OverflowMenuActionProvider <NSObject>

// The default base ranking of page actions (those that act on the current page
// state) currently supported.
- (ActionRanking)basePageActions;

// Returns the correct `OverflowMenuAction` for the corresponding
// `overflow_menu::ActionType` on the current page. Returns nil if the current
// page does not support the given `actionType`.
- (OverflowMenuAction*)actionForActionType:
    (overflow_menu::ActionType)actionType;

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

@property(nonatomic, weak) id<OverflowMenuActionProvider> actionProvider;

// Release any C++ objects that can't be reference counted.
- (void)disconnect;

// Records a `destination` click from the overflow menu carousel.
- (void)recordClickForDestination:(overflow_menu::Destination)destination;

// Returns the current sorted list of active destinations.
- (NSArray<OverflowMenuDestination*>*)sortedDestinations;

// Returns the current ordering of active page actions.
- (NSArray<OverflowMenuAction*>*)pageActions;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ORDERER_H_
