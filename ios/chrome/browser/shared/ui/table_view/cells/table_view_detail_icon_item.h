// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Enum defining the different badges that can be shown.
enum class BadgeType {
  kNone,             // No badge.
  kNotificationDot,  // The blue notification dot.
  kNew,              // The "new" ("N") IPH badge.
};

// TableViewDetailIconItem is a model class for a cell with details and icon.
@interface TableViewDetailIconItem : TableViewItem

// The leading icon. If empty, no icon will be shown.
@property(nonatomic, copy) UIImage* iconImage;

// The background color of the icon.
@property(nonatomic, strong) UIColor* iconBackgroundColor;

// The tint color of the icon.
@property(nonatomic, strong) UIColor* iconTintColor;

// The main text string.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

// The layout constraint axis at which `text` and `detailText` should be
// aligned. In the case of a vertical layout, the text will adapt its font
// size to a title/subtitle style.
// Defaults to UILayoutConstraintAxisHorizontal.
@property(nonatomic, assign) UILayoutConstraintAxis textLayoutConstraintAxis;

// Selects the badge shown in the Cell. These are only supported when
// `textLayoutConstraintAxis` is set to `UILayoutConstraintAxisHorizontal`.
// Default in no badge (BadgeType::kNone).
@property(nonatomic, assign) BadgeType badgeType;

// Maximum number of lines for the `text`. Default is 1.
@property(nonatomic, assign) NSInteger textNumberOfLines;

// Maximum number of lines for the `detailText`. Value is ignored if the layout
// constraint axis is set to horizontal. 1 by default.
@property(nonatomic, assign) NSInteger detailTextNumberOfLines;

// Line break mode for the labels. Defaults to NSLineBreakByTruncatingTail.
@property(nonatomic, assign) NSLineBreakMode textLineBreakMode;
@property(nonatomic, assign) NSLineBreakMode detailTextLineBreakMode;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_
