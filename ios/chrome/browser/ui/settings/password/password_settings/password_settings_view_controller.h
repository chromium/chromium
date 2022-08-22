// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// Protocol used to display Passwords Settings.
@protocol PasswordSettingsPresentationDelegate

// Method invoked when the page is dismissed.
- (void)passwordSettingsViewControllerDidDismiss;

@end

@interface PasswordSettingsViewController : ChromeTableViewController

@property(nonatomic, weak) id<PasswordSettingsPresentationDelegate>
    presentationDelegate;

- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_VIEW_CONTROLLER_H_
