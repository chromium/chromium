// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_SWITCH_CELL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_SWITCH_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

// A TableViewCell that contains an icon, a text label, a detail text and a
// switch. If the preferred content size category is an accessibility category,
// the switch is displayed below the label. Otherwise, it is on the trailing
// side.
@interface TableViewSwitchCell : TableViewCell

// UILabel corresponding to `text` from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// UILabel corresponding to `detailText` from the item.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// The switch view.
@property(nonatomic, readonly, strong) UISwitch* switchView;

// Configures the cell with its `title`, `subtitle` and whether the switch is
// `enabled` and `on`.
- (void)configureCellWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                 switchEnabled:(BOOL)enabled
                            on:(BOOL)on;

// Sets the `image` that should be displayed at the leading edge of the cell
// with a `tintColor`. If set to nil, the icon will be hidden and the text
// labels will expand to fill the full width of the cell. The image view will be
// configured with a `backgroundColor` and a `cornerRadius`.
- (void)setIconImage:(UIImage*)image
           tintColor:(UIColor*)tintColor
     backgroundColor:(UIColor*)backgroundColor
        cornerRadius:(CGFloat)cornerRadius
         borderWidth:(CGFloat)borderWidth;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_SWITCH_CELL_H_
