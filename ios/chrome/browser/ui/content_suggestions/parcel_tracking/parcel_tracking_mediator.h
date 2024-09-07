// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_commands.h"

namespace commerce {
class ShoppingService;
}

enum class ContentSuggestionsModuleType;
@protocol NewTabPageActionsDelegate;
@class ParcelTrackingItem;
enum class ParcelType;
class UrlLoadingBrowserAgent;
class PrefService;

// Delegate used to communicate events back to the owner of
// ParcelTrackingMediator.
@protocol ParcelTrackingMediatorDelegate

// Logs a user Magic Stack engagement for module `type`.
- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type;

// Indicates to the receiver that new parcels were received.
- (void)newParcelsAvailable;

// Indicates to the receiver that parcel tracking was disabled.
- (void)parcelTrackingDisabled;

@end

// Mediator for managing the state of tracked parcels for its Magic Stack
// module.
@interface ParcelTrackingMediator : NSObject <ParcelTrackingCommands>

// Delegate used to communicate events back to the owner of this class.
@property(nonatomic, weak) id<ParcelTrackingMediatorDelegate> delegate;

// Delegate for reporting content suggestions actions to the NTP.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

// Default initializer.
- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
     URLLoadingBrowserAgent:(UrlLoadingBrowserAgent*)URLLoadingBrowserAgent
                prefService:(PrefService*)prefService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Resets the latest fetched tracked packages and re-fecthes if applicable.
- (void)reset;

// Fetches the latest tracked parcels.
- (void)fetchTrackedParcels;

// Returns the parcel tracking items to show.
- (ParcelTrackingItem*)parcelTrackingItemToShow;

// Returns all parcel tracking items received.
- (NSArray<ParcelTrackingItem*>*)allParcelTrackingItems;

// Disables and hides the parcel tracking module.
- (void)disableModule;

// Indicates that `parcelID` should be untracked.
- (void)untrackParcel:(NSString*)parcelID;

// Indicates that `parcelID` should be tracked.
- (void)trackParcel:(NSString*)parcelID carrier:(ParcelType)carrier;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_MEDIATOR_H_
