// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_MANAGE_STORAGE_ALERT_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_MANAGE_STORAGE_ALERT_COMMANDS_H_

@protocol SystemIdentity;

// Commands to present the "Manage Storage" alert, which contains a preferred
// "Manage Storage" action and a no-op "Cancel" action.
@protocol ManageStorageAlertCommands <NSObject>

// Presents the "Manage Storage" alert. The "Manage Storage" action should let
// the user manage the storage for the account associated with `identity`.
- (void)showManageStorageAlertForIdentity:(id<SystemIdentity>)identity;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_MANAGE_STORAGE_ALERT_COMMANDS_H_
