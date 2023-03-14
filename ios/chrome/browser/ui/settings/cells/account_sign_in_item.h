// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_ACCOUNT_SIGN_IN_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_ACCOUNT_SIGN_IN_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Item to display a non-personnalized sign-in cell.
@interface AccountSignInItem : TableViewItem

// Subtitle for the sign-in cell (optional).
@property(nonatomic, strong) NSString* detailText;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_ACCOUNT_SIGN_IN_ITEM_H_
