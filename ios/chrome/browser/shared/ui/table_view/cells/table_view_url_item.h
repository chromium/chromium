// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@class CrURL;
@class FaviconAttributes;
@class FaviconView;

// TableViewURLItem contains the model data for a TableView URL cell.
@interface TableViewURLItem : TableViewItem

// The title of the page at `URL`.
@property(nonatomic, readwrite, copy) NSString* title;
// Line break mode for the title. Default is NSLineBreakByTruncatingTail.
@property(nonatomic, assign) NSLineBreakMode titleLineBreakMode;
// CrURL from which the cell will retrieve a favicon and display the host name.
@property(nonatomic, readwrite, strong) CrURL* URL;
// Custom third row text. This is not shown if it is empty or if the second row
// is empty.
@property(nonatomic, readwrite, copy) NSString* thirdRowText;
// Detail text to be displayed instead of the URL.
@property(nonatomic, copy) NSString* detailText;
// Attributes for the favicon.
@property(nonatomic, strong) FaviconAttributes* faviconAttributes;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_URL_ITEM_H_
