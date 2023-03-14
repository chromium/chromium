// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@protocol TableViewInfoButtonItemDelegate;

// TableViewInfoButtonItem is a model class that uses TableViewInfoButtonCell.
@interface TableViewInfoButtonItem : TableViewItem

// The leading icon.  If empty, no icon will be shown.
@property(nonatomic, copy) UIImage* iconImage;

// The background color of the icon.
@property(nonatomic, strong) UIColor* iconBackgroundColor;

// The corner radius of the UIImage view.
@property(nonatomic, assign) CGFloat iconCornerRadius;

// The tint color of the icon.
@property(nonatomic, strong) UIColor* iconTintColor;

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

// Boolean for if the info button is hidden.
@property(nonatomic, assign) BOOL infoButtonIsHidden;

// Accessibility delegate for custom accessibility actions.
@property(nonatomic, weak) id<TableViewInfoButtonItemDelegate>
    accessibilityDelegate;

// Boolean for if the accessibility activation point should be on the button of
// cell. The default value is YES. If value is changed to NO, the activation
// point will be on the center of the cell.
@property(nonatomic, assign) BOOL accessibilityActivationPointOnButton;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_ITEM_H_
