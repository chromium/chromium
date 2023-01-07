// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_TABLE_VIEW_FAVICON_TABLE_VIEW_CELL_H_
#define IOS_CHROME_COMMON_UI_TABLE_VIEW_FAVICON_TABLE_VIEW_CELL_H_

#import <UIKit/UIKit.h>

@class FaviconView;
@class TableViewURLCellFaviconBadgeView;

@interface FaviconTableViewCell : UITableViewCell

// The image view that is displayed on the leading edge of the cell. This
// contains a favicon composited on top of an off-white background.
@property(nonatomic, readonly, strong) FaviconView* faviconView;

// The image view used to display the favicon badge.
@property(nonatomic, readonly, strong)
    TableViewURLCellFaviconBadgeView* faviconBadgeView;

// The text to display.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// The detail text to display.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// Identifier use to match an object with its cell.
@property(nonatomic, strong) NSString* uniqueIdentifier;

@end

#endif  // IOS_CHROME_COMMON_UI_TABLE_VIEW_FAVICON_TABLE_VIEW_CELL_H_
