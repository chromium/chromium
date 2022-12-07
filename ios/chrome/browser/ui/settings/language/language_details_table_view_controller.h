// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_DETAILS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_DETAILS_TABLE_VIEW_CONTROLLER_H_

#include <string>

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@class LanguageDetailsTableViewController;
@class LanguageItem;

// Protocol used by LanguageDetailsTableViewController to communicate to its
// delegate.
@protocol LanguageDetailsTableViewControllerDelegate

// Informs the delegate that user selected whether or not to offer Translate for
// `languageCode`.
- (void)languageDetailsTableViewController:
            (LanguageDetailsTableViewController*)tableViewController
                   didSelectOfferTranslate:(BOOL)offerTranslate
                              languageCode:(const std::string&)languageCode;

@end

// Controller for the UI that allows the user to choose whether or not Translate
// should be offered for a given language.
@interface LanguageDetailsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// The designated initializer. `languageItem` and `delegate` must not be nil.
// `delegate` will not be retained.
- (instancetype)initWithLanguageItem:(LanguageItem*)languageItem
                            delegate:
                                (id<LanguageDetailsTableViewControllerDelegate>)
                                    delegate NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_LANGUAGE_LANGUAGE_DETAILS_TABLE_VIEW_CONTROLLER_H_
