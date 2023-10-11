// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PARCEL_TRACKING_PARCEL_TRACKING_OPT_IN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PARCEL_TRACKING_PARCEL_TRACKING_OPT_IN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"
#import "ios/web/public/web_state.h"

// Mediator for parcel tracking opt-in prompt that manages model interactions.
@interface ParcelTrackingOptInMediator : NSObject

// Handler for ParcelTrackingOptInCommands.
@property(nonatomic, weak) id<ParcelTrackingOptInCommands>
    parcelTrackingCommandsHandler;

// Designated initializer. `webState` should not be null.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Handles user tap on primary action.
- (void)didTapPrimaryActionButton:
    (NSArray<CustomTextCheckingResult*>*)parcelList;

// Handles user tap on tertiary action.
- (void)didTapTertiaryActionButton:
    (NSArray<CustomTextCheckingResult*>*)parcelList;

@end

#endif  // IOS_CHROME_BROWSER_UI_PARCEL_TRACKING_PARCEL_TRACKING_OPT_IN_MEDIATOR_H_
