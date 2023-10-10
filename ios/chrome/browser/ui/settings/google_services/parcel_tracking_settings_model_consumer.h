// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_MODEL_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_MODEL_CONSUMER_H_

#import <UIKit/UIKit.h>

enum class IOSParcelTrackingOptInStatus;

// Consumer protocol for managing the parcel tracking settings.
@protocol ParcelTrackingSettingsModelConsumer

- (void)updateCheckedState:(IOSParcelTrackingOptInStatus)newState;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_MODEL_CONSUMER_H_
