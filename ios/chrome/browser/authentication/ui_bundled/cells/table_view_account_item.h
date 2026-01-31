// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_ACCOUNT_ITEM_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_ACCOUNT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@protocol SystemIdentity;

typedef NS_ENUM(NSInteger, TableViewAccountMode) {
  // The cell can be tappable, and the colors are not dimmed.
  TableViewAccountModeEnabled,
  // The cell is not tappable, and the colors are not dimmed.
  TableViewAccountModeNonTappable,
};

enum class TableViewAccountDetailImage {
  // No details
  kNone,
  // There is an error with the account.
  kError,
  // The account is managed.
  kManaged,
};

// Item for account avatar, used everywhere an account cell is shown.
@interface TableViewAccountItem : TableViewItem

// Those properties should be set before the cell is displayed because
// updates to these will not be reflected after it is shown.
@property(nonatomic, strong) UIImage* image;
@property(nonatomic, copy) NSString* text;
@property(nonatomic, copy) NSString* detailText;

// The detail image to be shown for the account.
@property(nonatomic, assign) TableViewAccountDetailImage detailImage;

// The default value is TableViewAccountModeEnabled.
@property(nonatomic, assign) TableViewAccountMode mode;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_ACCOUNT_ITEM_H_
