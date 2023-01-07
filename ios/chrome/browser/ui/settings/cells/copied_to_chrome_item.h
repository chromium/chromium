// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_COPIED_TO_CHROME_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_COPIED_TO_CHROME_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// Item that configures a CopiedToChromeCell.
@interface CopiedToChromeItem : TableViewItem
@end

// A cell indicating that the credit card has been copied to Chrome. Includes a
// button to clear the copy.
@interface CopiedToChromeCell : TableViewCell

// Text label displaying the item's text.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Button to clear the copy.
@property(nonatomic, readonly, strong) UIButton* button;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_COPIED_TO_CHROME_ITEM_H_
