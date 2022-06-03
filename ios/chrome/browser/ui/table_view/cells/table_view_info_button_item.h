// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// TableViewInfoButtonItem is a model class that uses TableViewInfoButtonCell.
@interface TableViewInfoButtonItem : TableViewItem

// The UIImage for the leading image. If nil, no image will be shown.
@property(nonatomic, strong) UIImage* image;

// Tint color for the icon.
@property(nonatomic, strong) UIColor* tintColor;

// The main text string.
@property(nonatomic, copy) NSString* text;

// The color of the main text.
@property(nonatomic, strong) UIColor* textColor;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

// The color of the detail text.
@property(nonatomic, strong) UIColor* detailTextColor;

// The status text string.
@property(nonatomic, copy) NSString* statusText;

// The accessibility hint text string.
@property(nonatomic, copy) NSString* accessibilityHint;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_ITEM_H_
