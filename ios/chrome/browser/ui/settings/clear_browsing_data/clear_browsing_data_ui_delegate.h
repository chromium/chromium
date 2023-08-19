// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_UI_DELEGATE_H_

#include "base/ios/block_types.h"

@class ClearBrowsingDataTableViewController;
class GURL;

@protocol ClearBrowsingDataUIDelegate

// Opens URL in a new non-incognito tab and dismisses the clear browsing data
// view.
- (void)clearBrowsingDataTableViewController:
            (ClearBrowsingDataTableViewController*)controller
                              wantsToOpenURL:(const GURL&)URL;
// Notifies the delegate that Clear Browsing Data wants to be dismissed.
- (void)clearBrowsingDataTableViewControllerWantsDismissal:
    (ClearBrowsingDataTableViewController*)controller;

// Called when the view controller is removed from its parent.
- (void)clearBrowsingDataTableViewControllerWasRemoved:
    (ClearBrowsingDataTableViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_UI_DELEGATE_H_
