// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_

// Protocol to communicate user actions from the mediator to its
// coordinator.
@protocol ManageSyncSettingsCommandHandler <NSObject>

// Opens the "Web & App Activity" dialog.
- (void)openWebAppActivityDialog;

// Open the "Personalize Google Services" page.
- (void)openPersonalizeGoogleServices;

// Opens the "Data from Chrome sync" web page.
- (void)openDataFromChromeSyncWebPage;

// If the sync feature is disabled, sign-out and display a toast.
// Otherwise, if the sync feature is enabled, presents the data options
// available when turning off Sync. `targetRect` rect in table view system
// coordinate to display the signout popover dialog.
// TODO(crbug.com/40066949): Update this comment when syncing users no longer
// exist on iOS.
- (void)signOutFromTargetRect:(CGRect)targetRect;

// Shows a dialog to warn users that addresses are not encrypted by custom
// passphrase.
- (void)showAdressesNotEncryptedDialog;

// Shows a view displaying all Google Accounts present on the current device.
// The view allows adding and removing accounts.
- (void)showAccountsPage;

// Shows https://myaccount.google.com/ for the account currently signed-in
// to Chrome. The content is displayed in a new view in the stack, i.e.
// it doesn't close the current view.
- (void)showManageYourGoogleAccount;

// Open the view to batch upload data.
- (void)openBulkUpload;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_
