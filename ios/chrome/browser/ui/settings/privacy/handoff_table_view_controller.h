// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_HANDOFF_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_HANDOFF_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"


// This View Controller is responsible for managing the settings related to
// Handoff.
@interface HandoffTableViewController : SettingsRootTableViewController

// The designated initializer. `profile` must not be nil.
- (instancetype)initWithProfile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_HANDOFF_TABLE_VIEW_CONTROLLER_H_
