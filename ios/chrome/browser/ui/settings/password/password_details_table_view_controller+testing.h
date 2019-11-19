// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/ui/settings/password/password_details_table_view_controller.h"

// TODO(crbug.com/943523): Refactor the PasswordTableViewController and
// PasswordsSettingsTestCase to remove this Category file.
@interface PasswordDetailsTableViewController (Testing)

// Allows to replace a |reauthenticationModule| for a fake one in integration
// tests, where the testing code cannot control the creation of the
// controller.
- (void)setReauthenticationModule:
    (id<ReauthenticationProtocol>)reauthenticationModule;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_TESTING_H_
