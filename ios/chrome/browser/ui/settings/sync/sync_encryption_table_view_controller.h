// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_SYNC_ENCRYPTION_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_SYNC_ENCRYPTION_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class Browser;

// Controller to allow user to specify encryption passphrase for Sync.
@interface SyncEncryptionTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// Designated initializer. `profile` must not be nil.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SYNC_SYNC_ENCRYPTION_TABLE_VIEW_CONTROLLER_H_
