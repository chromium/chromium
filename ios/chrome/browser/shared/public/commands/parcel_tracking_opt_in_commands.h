// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PARCEL_TRACKING_OPT_IN_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PARCEL_TRACKING_OPT_IN_COMMANDS_H_

#import "ios/web/public/annotations/custom_text_checking_result.h"

// Commands related to the parcel tracking opt-in prompt.
@protocol ParcelTrackingOptInCommands <NSObject>

// Shows the parcel tracking opt-in UI if the user is eligible.
- (void)showParcelTrackingUIWithParcels:
    (NSArray<CustomTextCheckingResult*>*)parcels;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PARCEL_TRACKING_OPT_IN_COMMANDS_H_
