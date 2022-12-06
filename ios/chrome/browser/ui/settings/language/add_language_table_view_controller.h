// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_ADD_LANGUAGE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_ADD_LANGUAGE_TABLE_VIEW_CONTROLLER_H_

#include <string>

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@class AddLanguageTableViewController;
@protocol LanguageSettingsDataSource;

// Protocol used by AddLanguageTableViewController to communicate to its
// delegate.
@protocol AddLanguageTableViewControllerDelegate

// Informs the delegate that user selected a language with the given code.
- (void)addLanguageTableViewController:
            (AddLanguageTableViewController*)tableViewController
                 didSelectLanguageCode:(const std::string&)languageCode;

@end

// Controller for the UI that allows the user to select a supported language to
// add to the accept languages list.
@interface AddLanguageTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// The designated initializer. `dataSource` and `delegate` must not be nil.
// `delegate` will not be retained.
- (instancetype)initWithDataSource:(id<LanguageSettingsDataSource>)dataSource
                          delegate:(id<AddLanguageTableViewControllerDelegate>)
                                       delegate NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Called when the list of supported languages changes so that the view
// controller can update its model from `dataSource`.
- (void)supportedLanguagesListChanged;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_ADD_LANGUAGE_TABLE_VIEW_CONTROLLER_H_
