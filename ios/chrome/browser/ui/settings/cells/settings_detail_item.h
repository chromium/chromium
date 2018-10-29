// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_DETAIL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_DETAIL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// SettingsDetailItem is a model class that uses SettingsDetailCell.
@interface SettingsDetailItem : TableViewItem

// The filename for the leading icon.  If empty, no icon will be shown.
@property(nonatomic, copy) NSString* iconImageName;

// The background color of the cell.
@property(nonatomic, strong) UIColor* cellBackgroundColor;

// The accessory type to display on the trailing edge of the cell.
@property(nonatomic) UITableViewCellAccessoryType accessoryType;

// The main text string.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

@end

// SettingsDetailCell implements an UITableViewCell subclass containing an
// optional leading icon and two text labels: a "main" label and a "detail"
// label. The two labels are laid out side-by-side and fill the full width of
// the cell. Labels are truncated as needed to fit in the cell.
@interface SettingsDetailCell : UITableViewCell

// UILabels corresponding to |text| and |detailText| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// Sets the image that should be displayed at the leading edge of the cell. If
// set to nil, the icon will be hidden and the text labels will expand to fill
// the full width of the cell.
- (void)setIconImage:(UIImage*)image;

// The amount of horizontal space to provide to each of the labels, Exposed for
// testing. When the preferred ContentSize category chosen by the user isn't one
// of the accessibility categories, these values are determined with the
// following logic:
//
// - If there is sufficient room (after accounting for margins) for the full
//   width of each label, use the current width of each label.
// - If not, use the current width of the main label and a clipped width for the
//   detail label.
// - Unless the main label wants more than 75% of the available width and the
//   detail label wants 25% or less of the available width, in which case use a
//   clipped width for the main label and the current width of the detail label.
// - If both labels want more width than their guaranteed minimums (75% and
//   25%), use the guaranteed minimum amount for each.
//
// If the PreferredContentSizeCategory is an Accessibility category, those
// values aren't used. The content is displayed on two labels, one above the
// other, without limitation on the number of lines used by the labels.
@property(nonatomic, readonly) CGFloat textLabelTargetWidth;
@property(nonatomic, readonly) CGFloat detailTextLabelTargetWidth;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_DETAIL_ITEM_H_
