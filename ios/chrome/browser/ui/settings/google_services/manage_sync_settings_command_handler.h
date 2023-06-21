// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_

// Protocol to communicate user actions from the mediator to its coordinator.
@protocol ManageSyncSettingsCommandHandler <NSObject>

// Opens the "Web & App Activity" dialog.
- (void)openWebAppActivityDialog;

// Opens the "Data from Chrome sync" web page.
- (void)openDataFromChromeSyncWebPage;

// Presents the data options available when turning off Sync.
// `targetRect` rect in table view system coordinate to display the signout
// popover dialog.
- (void)showTurnOffSyncOptionsFromTargetRect:(CGRect)targetRect;

// Signs out.
- (void)signOut;

// Shows a view displaying all Google Accounts present on the current device.
// The view allows adding and removing accounts.
- (void)showAccountsPage;

// Shows https://myaccount.google.com/ for the account currently signed-in
// to Chrome. The content is displayed in a new view in the stack, i.e.
// it doesn't close the current view.
- (void)showManageYourGoogleAccount;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_
