// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ATTRIBUTED_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ATTRIBUTED_HEADER_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"

@class TableViewAttributedHeaderFooterView;

// TableViewAttributedHeaderFooterItem is the model class corresponding to
// TableViewAttributedHeaderFooterView.
@interface TableViewAttributedHeaderFooterItem : TableViewHeaderFooterItem

// The main text string.
@property(nonatomic, copy) NSString* text;

// The text string attributes to be applied on a specific range of text.
@property(nonatomic, assign)
    NSDictionary<NSValue*, NSDictionary*>* customTextAttributesOnRange;

@end

// UITableViewHeaderFooterView subclass containing a single UITextView. The text
// view is laid to fill the full width of the cell and it is wrapped as needed
// to fit in the cell.
@interface TableViewAttributedHeaderFooterView : UITableViewHeaderFooterView

// Sets the |text| displayed by this cell. Adds custom attributes to text if
// provided.
- (void)setText:(NSString*)text
    withCustomTextAttributes:
        (NSDictionary<NSValue*, NSDictionary*>*)customTextAttributesOnRange;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ATTRIBUTED_HEADER_FOOTER_ITEM_H_
