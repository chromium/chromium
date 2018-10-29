// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_SWITCH_ITEM_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_SWITCH_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// CollectionViewSwitchItem is the model class corresponding to
// CollectionViewSwitchCell.
@interface CollectionViewSwitchItem : CollectionViewItem

// TODO(crbug.com/891299) remove when all collection and table views are fixed
// for dynamic types.
// Set to YES to use dynamic font types.
@property(nonatomic, assign) BOOL useScaledFont;

// The text to display.
@property(nonatomic, copy) NSString* text;

// The current state of the switch.
@property(nonatomic, assign, getter=isOn) BOOL on;

// Whether or not the switch is enabled.  Disabled switches are automatically
// drawn as in the "off" state, with dimmed text.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

// CollectionViewSwitchCell implements a UICollectionViewCell subclass
// containing a text label and a switch.
@interface CollectionViewSwitchCell : MDCCollectionViewCell

// UILabel corresponding to |text| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// The switch view.
@property(nonatomic, readonly, strong) UISwitch* switchView;

// Returns the default text color used for the given |state|.
+ (UIColor*)defaultTextColorForState:(UIControlState)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_SWITCH_ITEM_H_
