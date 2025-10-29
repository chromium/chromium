// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class GoogleServicesSettingsViewController;
@protocol GoogleServicesSettingsViewControllerModelDelegate;

// Delegate for presentation events related to
// GoogleServicesSettingsViewController.
@protocol GoogleServicesSettingsViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)googleServicesSettingsViewControllerDidRemove:
    (GoogleServicesSettingsViewController*)controller;

@end

// View controller to related to Google services settings.
@interface GoogleServicesSettingsViewController
    : SettingsRootTableViewController <GoogleServicesSettingsConsumer,
                                       SettingsControllerProtocol>

// Presentation delegate.
@property(nonatomic, weak)
    id<GoogleServicesSettingsViewControllerPresentationDelegate>
        presentationDelegate;
// Model delegate.
@property(nonatomic, weak) id<GoogleServicesSettingsViewControllerModelDelegate>
    modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_VIEW_CONTROLLER_H_
