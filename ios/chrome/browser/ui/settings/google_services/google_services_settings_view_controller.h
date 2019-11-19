// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_consumer.h"

@class GoogleServicesSettingsViewController;
@protocol GoogleServicesSettingsServiceDelegate;
@protocol GoogleServicesSettingsViewControllerModelDelegate;

extern NSString* const kGoogleServicesSettingsViewIdentifier;

// Delegate for presentation events related to
// GoogleServicesSettingsViewController.
@protocol GoogleServicesSettingsViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)googleServicesSettingsViewControllerDidRemove:
    (GoogleServicesSettingsViewController*)controller;

@end

// View controller to related to Google services settings.
@interface GoogleServicesSettingsViewController
    : SettingsRootTableViewController <GoogleServicesSettingsConsumer>

// Presentation delegate.
@property(nonatomic, weak)
    id<GoogleServicesSettingsViewControllerPresentationDelegate>
        presentationDelegate;
// Model delegate.
@property(nonatomic, weak) id<GoogleServicesSettingsViewControllerModelDelegate>
    modelDelegate;
// Handler for GoogleServicesSettingsCommand.
@property(nonatomic, weak) id<GoogleServicesSettingsServiceDelegate>
    serviceDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_VIEW_CONTROLLER_H_
