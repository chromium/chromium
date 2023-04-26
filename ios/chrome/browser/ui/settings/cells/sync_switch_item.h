// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SYNC_SWITCH_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SYNC_SWITCH_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// SyncSwitchItem is a model class that uses TableViewSwitchCell.
@interface SyncSwitchItem : TableViewItem

// The text to display.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

// The current state of the switch.
@property(nonatomic, assign, getter=isOn) BOOL on;

// Whether or not the switch is enabled.  Disabled switches are automatically
// drawn as in the "off" state, with dimmed text.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// UserSelectableType for the item.
@property(nonatomic, assign) NSInteger dataType;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SYNC_SWITCH_ITEM_H_
