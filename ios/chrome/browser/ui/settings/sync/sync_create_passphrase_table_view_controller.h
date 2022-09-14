// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_SYNC_CREATE_PASSPHRASE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_SYNC_CREATE_PASSPHRASE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"

// Controller to allow user to specify encryption passphrase for Sync.
@interface SyncCreatePassphraseTableViewController
    : SyncEncryptionPassphraseTableViewController
@end

@interface SyncCreatePassphraseTableViewController (UsedForTesting)
@property(nonatomic, readonly) UITextField* confirmPassphrase;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_SYNC_CREATE_PASSPHRASE_TABLE_VIEW_CONTROLLER_H_
