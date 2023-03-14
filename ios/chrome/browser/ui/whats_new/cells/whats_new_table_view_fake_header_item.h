// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_FAKE_HEADER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_FAKE_HEADER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// WhatsNewTableViewFakeHeaderItem renders a custom header for What's New table
// view items.
@interface WhatsNewTableViewFakeHeaderItem : TableViewItem

// Main text to be displayed.
@property(nonatomic, strong) NSString* text;

@end

// WhatsNewTableViewFakeHeaderCell that displays a text label. The text
// label is displayed on one line.
@interface WhatsNewTableViewFakeHeaderCell : TableViewCell

// The text to display.
@property(nonatomic, strong) UILabel* headerLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_FAKE_HEADER_ITEM_H_
