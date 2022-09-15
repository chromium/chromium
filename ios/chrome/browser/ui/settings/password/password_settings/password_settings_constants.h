// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import "ios/chrome/browser/ui/list_model/list_model.h"

// Accessibility ID for the dialog asking users to confirm the export of their
// passwords.
extern NSString* const kPasswordSettingsExportConfirmViewId;

// Accessibility ID for the switch controlling the "Offer to Save Passwords"
// setting.
extern NSString* const kPasswordSettingsSavePasswordSwitchTableViewId;
extern NSString* const kPasswordSettingsManagedSavePasswordSwitchTableViewId;

// Accessibility ID for the UITableView in Password Settings.
extern NSString* const kPasswordsSettingsTableViewId;

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSTANTS_H_
