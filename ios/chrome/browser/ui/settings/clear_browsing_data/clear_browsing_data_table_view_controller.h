// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

namespace ios {
class ChromeBrowserState;
}

@protocol ApplicationCommands;
@protocol ClearBrowsingDataLocalCommands;

// TableView for clearing browsing data (including history,
// cookies, caches, passwords, and autofill).
@interface ClearBrowsingDataTableViewController
    : SettingsRootTableViewController <UIAdaptivePresentationControllerDelegate>

// Initializers. |browserState| can't be nil.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// Prepares view controller so that -dismissViewControllerAnimated dismisses it.
// Call this method before dismissing view controller.
- (void)prepareForDismissal;

// Local Dispatcher for this ClearBrowsingDataTableView.
@property(nonatomic, weak) id<ClearBrowsingDataLocalCommands> localDispatcher;

// The dispatcher used by this ViewController.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_CLEAR_BROWSING_DATA_TABLE_VIEW_CONTROLLER_H_
