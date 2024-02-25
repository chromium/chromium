// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_MODEL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_MODEL_DELEGATE_H_

enum class IOSParcelTrackingOptInStatus;

// Delegate for ParcelTrackingSettingsTableViewController instance, to manage
// the model.
@protocol ParcelTrackingSettingsModelDelegate <NSObject>

// Called when the model should handle a row selection at `indexPath`.
- (void)tableViewDidSelectStatus:(IOSParcelTrackingOptInStatus)status;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PARCEL_TRACKING_SETTINGS_MODEL_DELEGATE_H_
