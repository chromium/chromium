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
@class ActionCustomizationModel;
@class DestinationCustomizationModel;
@class OverflowMenuAction;
@class OverflowMenuActionGroup;
@protocol OverflowMenuActionProvider;
@class OverflowMenuDestination;
@class OverflowMenuModel;
class PrefService;

@protocol OverflowMenuDestinationProvider <NSObject>

// The default base ranking of destinations currently supported.
- (DestinationRanking)baseDestinations;

// Returns the correct `OverflowMenuDestination` for the corresponding
// `overflow_menu::Destination` on the current page. Returns nil if the current
// page does not support the given `destinationType`.
- (OverflowMenuDestination*)destinationForDestinationType:
    (overflow_menu::Destination)destinationType;

// Returns a representative `OverflowMenuDestination` for the corresponding
// `overflow_menu::Destination` to display to the user when customizing the
// order and show/hide state of the destinations.
- (OverflowMenuDestination*)customizationDestinationForDestinationType:
    (overflow_menu::Destination)destinationType;

// Allows destination provider to fire any feature-specific logic for clearing
// feature-driven badges.
- (void)destinationCustomizationCompleted;

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

// The model provided to this orderer, allowing it to update the
// order when needed.
@property(nonatomic, weak) OverflowMenuModel* model;

// The page actions group provided to this orderer, allowing it to update the
// order when needed.
@property(nonatomic, weak) OverflowMenuActionGroup* pageActionsGroup;

// Model object to be used for customizing (reordering, showing/hiding) actions.
@property(nonatomic, readonly)
    ActionCustomizationModel* actionCustomizationModel;

// Model object to be used for customizing (reordering, showing/hiding)
// destinations.
@property(nonatomic, readonly)
    DestinationCustomizationModel* destinationCustomizationModel;

// Whether or not there is a destination customization session currently in
// progress.
@property(nonatomic, readonly) BOOL isDestinationCustomizationInProgress;

// Release any C++ objects that can't be reference counted.
- (void)disconnect;

// Records a `destination` click from the overflow menu carousel.
- (void)recordClickForDestination:(overflow_menu::Destination)destination;

// Requests that the orderer perform any per-appearance order updates.
- (void)reorderDestinationsForInitialMenu;

// Requests that the orderer update the order of the destinations in its model.
- (void)updateDestinations;

// Requests that the orderer update the order of the page actions in its page
// actions group.
- (void)updatePageActions;

// Alerts the orderer that the menu has disappeared, so it can perform any
// necessary updates.
- (void)updateForMenuDisappearance;

// Tells the orderer that actions customization has finished using the current
// data in `actionCustomizationModel`.
- (void)commitActionsUpdate;

// Tells the orderer that destinations customization has finished using the
// current data in `destinationCustomizationModel`.
- (void)commitDestinationsUpdate;

// Tells the orderer that actions customization was cancelled and should not be
// saved.
- (void)cancelActionsUpdate;

// Tells the orderer that destinations customization was cancelled and should
// not be saved.
- (void)cancelDestinationsUpdate;

// Alerts the orderer that an item linked to `actionType` has had its own
// `shown` state toggled. And that `actionSubtitle` is the proper subtitle for
// if the linked item is now not shown. For example, if the Bookmarks
// destination is shown, this method can be called for action type Bookmark.
- (void)customizationUpdateToggledShown:(BOOL)shown
                    forLinkedActionType:(overflow_menu::ActionType)actionType
                         actionSubtitle:(NSString*)actionSubtitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_ORDERER_H_
