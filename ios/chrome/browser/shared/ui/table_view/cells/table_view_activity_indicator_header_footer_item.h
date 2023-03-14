// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ACTIVITY_INDICATOR_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ACTIVITY_INDICATOR_HEADER_FOOTER_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
// TableViewActivityIndicatorHeaderFooterItem contains the model data for a
// TableViewActivityIndicatorHeaderFooterView.
@interface TableViewActivityIndicatorHeaderFooterItem
    : TableViewHeaderFooterItem
// Title of Header.
@property(nonatomic, readwrite, strong) NSString* text;
// Header subtitle displayed as a smaller font under title.
@property(nonatomic, readwrite, strong) NSString* subtitleText;
@end

// UITableViewHeaderFooterView that displays a text label, subtitle, and an
// activity indicator.
@interface TableViewActivityIndicatorHeaderFooterView
    : UITableViewHeaderFooterView
// Shows the text of the TableViewActivityIndicatorHeaderFooterItem.
@property(nonatomic, readwrite, strong) UILabel* titleLabel;
// Shows the subtitleText of the TableViewActivityIndicatorHeaderFooterItem.
@property(nonatomic, readwrite, strong) UILabel* subtitleLabel;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_ACTIVITY_INDICATOR_HEADER_FOOTER_ITEM_H_
