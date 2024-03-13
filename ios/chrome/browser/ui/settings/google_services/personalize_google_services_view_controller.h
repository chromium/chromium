// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PERSONALIZE_GOOGLE_SERVICES_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PERSONALIZE_GOOGLE_SERVICES_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@class PersonalizeGoogleServicesViewController;
@protocol PersonalizeGoogleServicesCommandHandler;

// Delegate for presentation events related to
// PersonalizeGoogleServicesViewController which is usually handled by a class
// that holds the view controller.
@protocol PersonalizeGoogleServicesViewControllerPresentationDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)personalizeGoogleServicesViewcontrollerDidRemove:
    (PersonalizeGoogleServicesViewController*)controller;

@end

// View controller related to PersonalizeGoogleServices settings.
@interface PersonalizeGoogleServicesViewController
    : SettingsRootTableViewController

// Presentation delegate.
@property(nonatomic, weak)
    id<PersonalizeGoogleServicesViewControllerPresentationDelegate>
        presentationDelegate;

// Command handler
@property(nonatomic, weak) id<PersonalizeGoogleServicesCommandHandler> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PERSONALIZE_GOOGLE_SERVICES_VIEW_CONTROLLER_H_
