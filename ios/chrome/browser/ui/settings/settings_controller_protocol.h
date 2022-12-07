// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_CONTROLLER_PROTOCOL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_CONTROLLER_PROTOCOL_H_

#import <UIKit/UIKit.h>

// Protocol for settings view controllers.
@protocol SettingsControllerProtocol <NSObject>

@required

// Called when user dismissed settings. View controllers must implement this
// method and report dismissal User Action.
- (void)reportDismissalUserAction;

// Called when user goes back from a settings view controller. View controllers
// must implement this method and report appropriate User Action.
- (void)reportBackUserAction;

@optional

// Notifies the controller that the settings screen is being dismissed.
- (void)settingsWillBeDismissed;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_CONTROLLER_PROTOCOL_H_
