// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_CELL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

// TableViewInfoButtonCell implements a TableViewCell subclass containing an
// icon, a text label, a detail text, a status text and an info button. If the
// preferred content size category is an accessibility category, the status text
// is displayed below the detail text, and the info button is below the status
// text. Otherwise, they are on the trailing side. Additionally, status text
// constraints are deactivated if a status text is not set. This is done to
// give detail text more available space.
@interface TableViewInfoButtonCell : TableViewCell

// UILabel displayed next to the leading image icon if there is one, otherwise
// this UILabel will be at the leading position. Corresponding to `text` from
// the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// UILabel displayed under the `textLabel` shows the description text.
// Corresponding to `detailText` from the item.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// UIButton displayed aligned to the trailing of the view.
@property(nonatomic, readonly, strong) UIButton* trailingButton;

// The customized accessibility hint text string.
@property(nonatomic, copy) NSString* customizedAccessibilityHint;

// Boolean for if accessibility activation point should be on the button when
// VoiceOver is enabled. By default, YES makes the accessibility
// activation point on the UIButton. If NO, the default activation point,
// the center, will be used.
@property(nonatomic, assign) BOOL isButtonSelectedForVoiceOver;

// Sets the `image` that should be displayed at the leading edge of the cell
// with a `tintColor`. If set to nil, the icon will be hidden and the remaining
// content will expand to fill the full width of the cell. The image view will
// be configured with a `backgroundColor` and a `cornerRadius`.
- (void)setIconImage:(UIImage*)image
           tintColor:(UIColor*)tintColor
     backgroundColor:(UIColor*)backgroundColor
        cornerRadius:(CGFloat)cornerRadius;

// Updates the padding constraints based on how many vertical text labels are
// shown. The padding will be updated only if `hasDetailText` is YES.
- (void)updatePaddingForDetailText:(BOOL)hasDetailText;

// Hides `trailingButton` and activates related constraint.
- (void)hideUIButton:(BOOL)isHidden;

// Set the status text of the button cell and adjusts constraints accordingly.
- (void)setStatusText:(NSString*)statusText;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_CELL_H_
