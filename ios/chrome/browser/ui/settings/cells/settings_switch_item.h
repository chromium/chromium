// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SWITCH_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SWITCH_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// SettingsSwitchItem is a model class that uses SettingsSwitchCell.
@interface SettingsSwitchItem : TableViewItem
// The filename for the leading icon.  If empty, no icon will be shown.
@property(nonatomic, copy) NSString* iconImageName;

// The text to display.
@property(nonatomic, copy) NSString* text;

// The current state of the switch.
@property(nonatomic, assign, getter=isOn) BOOL on;

// Whether or not the switch is enabled.  Disabled switches are automatically
// drawn as in the "off" state, with dimmed text.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

// SettingsSwitchCell implements a UITableViewCell subclass containing an icon,
// a text label, and a switch.
// If the preferred content size category is an accessibility category, the
// switch is displayed below the label. Otherwise, it is on the trailing side.
@interface SettingsSwitchCell : UITableViewCell

// UILabel corresponding to |text| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// The switch view.
@property(nonatomic, readonly, strong) UISwitch* switchView;

// Returns the default text color used for the given |state|.
+ (UIColor*)defaultTextColorForState:(UIControlState)state;

// Sets the image that should be displayed at the leading edge of the cell. If
// set to nil, the icon will be hidden and the remaining content will expand to
// fill the full width of the cell.
- (void)setIconImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SWITCH_ITEM_H_
