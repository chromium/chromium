// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_LINK_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_LINK_HEADER_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"
#import "url/gurl.h"

@class TableViewLinkHeaderFooterView;

@protocol TableViewLinkHeaderFooterItemDelegate<NSObject>

// Notifies the delegate that the link corresponding to |URL| was tapped in
// |view|.
- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(GURL)URL;

@end

// TableViewLinkHeaderFooterItem is the model class corresponding to
// TableViewLinkHeaderFooterView.
@interface TableViewLinkHeaderFooterItem : TableViewHeaderFooterItem

// The URL to open when the cell is tapped.
@property(nonatomic, assign) GURL linkURL;

// The main text string.
@property(nonatomic, copy) NSString* text;

@end

// UITableViewHeaderFooterView subclass containing a single UITextView. The text
// view is laid to fill the full width of the cell and it is wrapped as needed
// to fit in the cell. If it contains a link, the link is correctly displayed as
// link and the delegate is notified if it is tapped.
@interface TableViewLinkHeaderFooterView : UITableViewHeaderFooterView

// Delegate to notify when the link is tapped.
@property(nonatomic, weak) id<TableViewLinkHeaderFooterItemDelegate> delegate;

// The URL to open when the link is tapped.
@property(nonatomic, assign) GURL linkURL;

// Sets the |text| displayed by this cell. If |text| is containing a link, the
// link is appropriately colored.
- (void)setText:(NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_LINK_HEADER_FOOTER_ITEM_H_
