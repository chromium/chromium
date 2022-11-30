// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_VOICE_SEARCH_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_VOICE_SEARCH_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class PrefService;

// Table view controller for the voice search language selection.
@interface VoiceSearchTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// The designated initializer.
- (instancetype)initWithPrefs:(PrefService*)prefs NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_VOICE_SEARCH_TABLE_VIEW_CONTROLLER_H_
