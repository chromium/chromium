// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_

// Protocol to communicate user actions from the mediator to its coordinator.
@protocol GoogleServicesSettingsCommandHandler <NSObject>

// Presents the sign-in dialog to the user.
- (void)showSignIn;

// Opens the account setting view.
- (void)openAccountSettings;

// Opens the manage sync settings view.
- (void)openManageSyncSettings;

// Opens the "Manage Your Google Account" view.
- (void)openManageGoogleAccount;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COMMAND_HANDLER_H_
