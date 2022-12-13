// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TRANSLATE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TRANSLATE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class PrefService;
@protocol SnackbarCommands;

// Controller for the UI that allows the user to control Translate settings.
@interface TranslateTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// SnackbarCommands handler.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// `prefs` must not be nil.
- (instancetype)initWithPrefs:(PrefService*)prefs NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TRANSLATE_TABLE_VIEW_CONTROLLER_H_
