// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewDetailIconItem is a model class that uses TableViewDetailIconCell.
@interface TableViewDetailIconItem : TableViewItem

// The leading icon. If empty, no icon will be shown.
@property(nonatomic, copy) UIImage* iconImage;

// The background color of the icon.
@property(nonatomic, strong) UIColor* iconBackgroundColor;

// The corner radius of the UIImage view.
@property(nonatomic, assign) CGFloat iconCornerRadius;

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

// If set to YES, a kBlue600Color dot will be shown in the Cell. The
// notification dot is only supported when `textLayoutConstraintAxis` is set to
// `UILayoutConstraintAxisHorizontal`.
@property(nonatomic, assign) BOOL showNotificationDot;

@end

// TableViewDetailIconCell implements an TableViewCell subclass containing an
// optional leading icon and two text labels: a "main" label and a "detail"
// label. The layout of the two labels is based on `textLayoutConstraintAxis`
// defined as either (1) horizontally laid out side-by-side and filling the full
// width of the cell or (2) vertically laid out and filling the full height of
// the cell. Labels are truncated as needed to fit in the cell.
@interface TableViewDetailIconCell : TableViewCell

// UILabel corresponding to `text` from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// The layout constraint axis of the text labels within the cell. Defaults
// to a horizontal, edge aligned layout.
@property(nonatomic, readwrite, assign)
    UILayoutConstraintAxis textLayoutConstraintAxis;

// Sets the `image` that should be displayed at the leading edge of the cell
// with a `tintColor`. If set to nil, the icon will be hidden and the text
// labels will expand to fill the full width of the cell. The image view will be
// configured with a `backgroundColor` and a `cornerRadius`.
- (void)setIconImage:(UIImage*)image
           tintColor:(UIColor*)tintColor
     backgroundColor:(UIColor*)backgroundColor
        cornerRadius:(CGFloat)cornerRadius;

// Sets the detail text. `detailText` can be nil (or empty) to hide the detail
// text.
- (void)setDetailText:(NSString*)detailText;

// Sets whether or not to show the notification dot. The notification dot is
// only supported when `textLayoutConstraintAxis` is set to
// `UILayoutConstraintAxisHorizontal`. If the notification dot is activated
// while the axis is vertical, the app will crash through DCHECK.
- (void)setShowNotificationDot:(BOOL)showNotificationDot;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_ICON_ITEM_H_
