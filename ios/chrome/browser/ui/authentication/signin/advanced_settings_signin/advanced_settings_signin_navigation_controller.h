// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_NAVIGATION_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#import <UIKit/UIKit.h>

// Delegate for AdvancedSettingsSigninNavigationController to receive navigation
// button events.
@protocol
    AdvancedSettingsSigninNavigationControllerNavigationDelegate <NSObject>

// Called when the navigation done button was tapped.
- (void)navigationDoneButtonWasTapped;

@end

// View controller to present the Google services settings. The super class
// needs to be `SettingsNavigationController`, since it can present
// `SyncEncryptionPassphraseTableViewController`.
// See crbug.com/1424870.
@interface AdvancedSettingsSigninNavigationController
    : SettingsNavigationController

@property(nonatomic, weak)
    id<AdvancedSettingsSigninNavigationControllerNavigationDelegate>
        navigationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_NAVIGATION_CONTROLLER_H_
