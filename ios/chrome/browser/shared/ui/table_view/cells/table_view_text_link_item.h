// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_LINK_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_LINK_ITEM_H_

#import <UIKit/UIKit.h>

#include <vector>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

class GURL;
@class CrURL;
@class TableViewTextLinkCell;

// Delegate for TableViewTextLinkCell.
@protocol TableViewTextLinkCellDelegate <NSObject>
// Notifies the delegate that `URL` should be opened.
- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(CrURL*)URL;
@end

// TableViewTextLinkItem contains the model data for a TableViewTextLinkCell.
@interface TableViewTextLinkItem : TableViewItem
// Text being stored by this item.
@property(nonatomic, readwrite, strong) NSString* text;
// URL links being stored by this item.
@property(nonatomic, assign) std::vector<GURL> linkURLs;
// Character range for the links in `linkURLs`. Order should match order in
// `linkURLs`.
@property(nonatomic, strong) NSArray* linkRanges;

@end

// TableViewCell that displays a text label that might contain a link.
@interface TableViewTextLinkCell : TableViewCell
// The text to display.
@property(nonatomic, readonly, strong) UITextView* textView;
// Delegate for the TableViewTextLinkCell. Is notified when a link is
// tapped.
@property(nonatomic, weak) id<TableViewTextLinkCellDelegate> delegate;
// Sets the cell's label with the appropriate urls and ranges.
- (void)setText:(NSString*)text
       linkURLs:(std::vector<GURL>)linkURLS
     linkRanges:(NSArray*)linkRanges;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_LINK_ITEM_H_
