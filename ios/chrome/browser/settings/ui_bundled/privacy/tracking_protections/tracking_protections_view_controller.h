// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_TRACKING_PROTECTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_TRACKING_PROTECTIONS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class TrackingProtectionsViewController;

// Delegate for presentation events related to
// TrackingProtectionsViewController.
@protocol TrackingProtectionsViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)trackingProtectionsViewControllerDidRemove:
    (TrackingProtectionsViewController*)controller;

@end

// View controller for tracking protection settings.
@interface TrackingProtectionsViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak)
    id<TrackingProtectionsViewControllerPresentationDelegate>
        presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_TRACKING_PROTECTIONS_VIEW_CONTROLLER_H_
