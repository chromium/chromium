// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_VIEW_CONTROLLER_MODEL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_VIEW_CONTROLLER_MODEL_DELEGATE_H_

@class GoogleServicesSettingsViewController;

// Delegate for GoogleServicesSettingsViewController instance, to manage the
// model.
@protocol GoogleServicesSettingsViewControllerModelDelegate <NSObject>

// Called when the model should be loaded.
- (void)googleServicesSettingsViewControllerLoadModel:
    (GoogleServicesSettingsViewController*)controller;

// Called to know if the table item is of the type for allow sign-in.
- (BOOL)isAllowChromeSigninItem:(int)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_VIEW_CONTROLLER_MODEL_DELEGATE_H_
