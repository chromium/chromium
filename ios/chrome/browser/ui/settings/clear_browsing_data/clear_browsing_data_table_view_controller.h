// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol ApplicationCommands;
class Browser;
@protocol ClearBrowsingDataUIDelegate;

// TableView for clearing browsing data (including history,
// cookies, caches, passwords, and autofill).
@interface ClearBrowsingDataTableViewController
    : SettingsRootTableViewController <UIAdaptivePresentationControllerDelegate>

// Initializers. `browser` can't be nil.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Must be call before the view is deallocated.
- (void)stop;

// Prepares view controller so that -dismissViewControllerAnimated dismisses it.
// Call this method before dismissing view controller.
- (void)prepareForDismissal;

// Local Dispatcher for this ClearBrowsingDataTableView.
@property(nonatomic, weak) id<ClearBrowsingDataUIDelegate> delegate;

// The dispatcher used by this ViewController.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_TABLE_VIEW_CONTROLLER_H_
