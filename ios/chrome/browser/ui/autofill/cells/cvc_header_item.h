// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_CVC_HEADER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_CVC_HEADER_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"

// Item to represent and configure a CVCHeaderCell
@interface CVCHeaderItem : TableViewHeaderFooterItem

// The instructions text to display.
@property(nonatomic, copy) NSString* instructionsText;

@end

// Header view of the CVC Prompt.
@interface CVCHeaderView : UITableViewHeaderFooterView

// The label displaying instructions.
@property(nonatomic, readonly, strong) UILabel* instructionsLabel;

@end
#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_CVC_HEADER_ITEM_H_
