// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_model_delegate.h"

@protocol ParcelTrackingSettingsModelConsumer;
class PrefService;

@interface ParcelTrackingSettingsMediator
    : NSObject <ParcelTrackingSettingsModelDelegate>

// Initializes with `prefs`.
- (instancetype)initWithPrefs:(PrefService*)prefs NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The consumer for this mediator.
@property(nonatomic, weak) id<ParcelTrackingSettingsModelConsumer> consumer;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_MEDIATOR_H_
