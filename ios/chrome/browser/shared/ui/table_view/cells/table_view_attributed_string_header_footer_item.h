// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ATTRIBUTED_STRING_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ATTRIBUTED_STRING_HEADER_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

// TableViewAttributedStringHeaderFooterItem is the model class corresponding to
// TableViewAttributedStringHeaderFooterView.
@interface TableViewAttributedStringHeaderFooterItem : TableViewHeaderFooterItem

// The attributed string to display.
@property(nonatomic, copy) NSAttributedString* attributedString;

@end

// UITableViewHeaderFooterView subclass containing a single UITextView, to
// display a attributed string.
@interface TableViewAttributedStringHeaderFooterView
    : UITableViewHeaderFooterView

// Sets the `attributedString` displayed by this cell.
- (void)setAttributedString:(NSAttributedString*)attributedString;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ATTRIBUTED_STRING_HEADER_FOOTER_ITEM_H_
