// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_UI_BUNDLED_PARCEL_TRACKING_OPT_IN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_UI_BUNDLED_PARCEL_TRACKING_OPT_IN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/commerce/core/shopping_service.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"

// Mediator for parcel tracking opt-in prompt that manages model interactions.
@interface ParcelTrackingOptInMediator : NSObject

// Handler for ParcelTrackingOptInCommands.
@property(nonatomic, weak) id<ParcelTrackingOptInCommands>
    parcelTrackingCommandsHandler;

// Designated initializer. `shoppingService` must not be null and must outlive
// this object.
- (instancetype)initWithShoppingService:
    (commerce::ShoppingService*)shoppingService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Handles user tap on "always track".
- (void)didTapAlwaysTrack:(NSArray<CustomTextCheckingResult*>*)parcelList;

// Handles user tap on "ask to track".
- (void)didTapAskToTrack:(NSArray<CustomTextCheckingResult*>*)parcelList;

@end

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_UI_BUNDLED_PARCEL_TRACKING_OPT_IN_MEDIATOR_H_
