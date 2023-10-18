// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_COMMANDS_H_

// Commands protocol allowing the TabPickupSettingsTableViewController to
// interact with the coordinator layer.
@protocol TabPickupSettingsCommands

// Shows the sign in view.
- (void)showSign;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_COMMANDS_H_
