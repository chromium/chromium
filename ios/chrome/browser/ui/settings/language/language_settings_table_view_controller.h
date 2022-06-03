// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/language/language_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol LanguageSettingsDataSource;
@protocol LanguageSettingsCommands;

// Controller for the UI that allows the user to change language settings such
// as the ordered list of accept languages and their Translate preferences.
@interface LanguageSettingsTableViewController
    : SettingsRootTableViewController <LanguageSettingsConsumer,
                                       SettingsControllerProtocol>

// The designated initializer. |dataSource| and |commandHandler| must not be
// nil. |commandHandler| will not be retained.
- (instancetype)initWithDataSource:(id<LanguageSettingsDataSource>)dataSource
                    commandHandler:(id<LanguageSettingsCommands>)commandHandler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_SETTINGS_TABLE_VIEW_CONTROLLER_H_
