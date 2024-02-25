// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_model_consumer.h"

@protocol ParcelTrackingSettingsModelDelegate;

// Settings page for Parcel Tracking auto-track feature.
@interface ParcelTrackingSettingsViewController
    : SettingsRootTableViewController <ParcelTrackingSettingsModelConsumer>

// The model delegate for this ViewController.
@property(nonatomic, weak) id<ParcelTrackingSettingsModelDelegate>
    modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_VIEW_CONTROLLER_H_
