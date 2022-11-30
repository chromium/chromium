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

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_
