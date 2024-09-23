// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_UI_BUNDLED_PARCEL_TRACKING_OPT_IN_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_UI_BUNDLED_PARCEL_TRACKING_OPT_IN_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate protocol for ParcelTrackingOptInViewController.
@protocol ParcelTrackingOptInViewControllerDelegate <NSObject>

// Called when "always track" is selected.
- (void)alwaysTrackTapped;

// Called when "ask to track" is selected.
- (void)askToTrackTapped;

// Called when "no thanks" is selected.
- (void)noThanksTapped;

// Called when the link to open the settings page is tapped.
- (void)parcelTrackingSettingsPageLinkTapped;

@end

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_UI_BUNDLED_PARCEL_TRACKING_OPT_IN_VIEW_CONTROLLER_DELEGATE_H_
