// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Delegate for AdvancedSettingsSigninNavigationController to receive navigation
// button events.
@protocol
    AdvancedSettingsSigninNavigationControllerNavigationDelegate <NSObject>

// Called when the navigation done button was tapped.
- (void)navigationDoneButtonWasTapped;

@end

// View controller to present the Google services settings.
@interface AdvancedSettingsSigninNavigationController : UINavigationController

@property(nonatomic, weak)
    id<AdvancedSettingsSigninNavigationControllerNavigationDelegate>
        navigationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_ADVANCED_SETTINGS_SIGNIN_ADVANCED_SETTINGS_SIGNIN_NAVIGATION_CONTROLLER_H_
