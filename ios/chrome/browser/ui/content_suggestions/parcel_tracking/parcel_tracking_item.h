// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_ITEM_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

namespace base {
class Time;
}  // namespace base

@protocol ParcelTrackingCommands;
class GURL;

// The carrier type for a given tracked parcel.
enum class ParcelType {
  kUnkown = 0,
  kUSPS,
  kUPS,
  kFedex,
};

// Matches the definition of commerce::ParcelStatus::ParcelState.
enum class ParcelState {
  kUnkown = 0,
  kNew = 18,
  kLabelCreated = 1,
  kPickedUp = 2,
  kFinished = 3,
  kDeliveryFailed = 4,
  kError = 6,
  kCancelled = 11,
  kOrderTooOld = 12,
  kHandedOff = 13,
  kWithCarrier = 14,
  kOutForDelivery = 15,
  kAtPickupLocation = 20,
  kReturnToSender = 16,
  kReturnCompleted = 19,
  kUndeliverable = 17,
};

// Item containing the configurations for the ParcelTracking Module view.
@interface ParcelTrackingItem : MagicStackModule

// Favicon image of the carrier for the parcel.
@property(nonatomic, assign) ParcelType parcelType;

// Estimated delivery time of the parcel.
@property(nonatomic, assign) std::optional<base::Time> estimatedDeliveryTime;

// The id of the tracked parcel.
@property(nonatomic, copy) NSString* parcelID;

// The status of the parcel.
@property(nonatomic, assign) ParcelState status;

// The URL of the parcel status page.
@property(nonatomic, assign) GURL trackingURL;

// Command handler for user actions.
@property(nonatomic, weak) id<ParcelTrackingCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_ITEM_H_
