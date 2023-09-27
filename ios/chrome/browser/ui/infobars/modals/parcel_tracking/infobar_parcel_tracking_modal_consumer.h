// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_MODAL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_MODAL_CONSUMER_H_

#import "ios/web/public/annotations/custom_text_checking_result.h"

// Consumer for model to push configurations to the parcel tracking UI. All
// setters should be called before -viewDidLoad is called.
@protocol InfobarParcelTrackingModalConsumer

// Sets the list of parcels and their tracking status. `tracking` is true if the
// parcels are being tracked.
- (void)setParcelList:(NSArray<CustomTextCheckingResult*>*)parcels
    withTrackingStatus:(bool)tracking;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_MODAL_CONSUMER_H_
