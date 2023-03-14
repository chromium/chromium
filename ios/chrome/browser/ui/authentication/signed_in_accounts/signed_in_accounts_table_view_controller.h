// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNED_IN_ACCOUNTS_SIGNED_IN_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNED_IN_ACCOUNTS_SIGNED_IN_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

class ChromeBrowserState;

// TableView controller to show the list of identities.
@interface SignedInAccountsTableViewController : ChromeTableViewController

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNED_IN_ACCOUNTS_SIGNED_IN_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
