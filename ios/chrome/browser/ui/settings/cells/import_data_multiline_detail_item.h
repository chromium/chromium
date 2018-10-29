// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_IMPORT_DATA_MULTILINE_DETAIL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_IMPORT_DATA_MULTILINE_DETAIL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// ImportDataMultilineDetailItem is a model class that uses
// ImportDataMultilineDetailCell.
@interface ImportDataMultilineDetailItem : CollectionViewItem

// The accessory type to display on the trailing edge of the cell.
@property(nonatomic) MDCCollectionViewCellAccessoryType accessoryType;

// The main text string.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

@end

// ImportDataMultilineDetailCell implements an MDCCollectionViewCell
// subclass containing two text labels: a "main" label and a "detail" label.
// The two labels are laid out on top of each other. The detail text can span
// multiple lines.
// This is to be used with a ImportDataMultilineDetailItem.
@interface ImportDataMultilineDetailCell : MDCCollectionViewCell

// UILabels corresponding to |text| and |detailText| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_IMPORT_DATA_MULTILINE_DETAIL_ITEM_H_
