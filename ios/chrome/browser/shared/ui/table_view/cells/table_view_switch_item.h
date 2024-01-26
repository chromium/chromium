// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_SWITCH_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_SWITCH_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewSwitchItem is a model class that uses TableViewSwitchCell.
@interface TableViewSwitchItem : TableViewItem

// The leading icon.  If empty, no icon will be shown.
@property(nonatomic, copy) UIImage* iconImage;

// The background color of the icon.
@property(nonatomic, strong) UIColor* iconBackgroundColor;

// The corner radius of the UIImage view.
@property(nonatomic, assign) CGFloat iconCornerRadius;

// The border width of the UIImage view. Is zero (no border) by default.
@property(nonatomic, assign) CGFloat iconBorderWidth;

// The tint color of the icon.
@property(nonatomic, strong) UIColor* iconTintColor;

// The text to display.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

// The current state of the switch.
@property(nonatomic, assign, getter=isOn) BOOL on;

// Whether or not the switch is enabled.  Disabled switches are automatically
// drawn as in the "off" state, with dimmed text.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_SWITCH_ITEM_H_
