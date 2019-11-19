// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_TABLE_VIEW_CLEAR_BROWSING_DATA_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_TABLE_VIEW_CLEAR_BROWSING_DATA_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

enum class BrowsingDataRemoveMask;

// TableViewClearBrowsingDataItem contains the model data for a
// TableViewTextCell in addition a BrowsingDataRemoveMask property.
@interface TableViewClearBrowsingDataItem : TableViewItem

@property(nonatomic, copy) NSString* imageName;
@property(nonatomic, copy) NSString* text;
@property(nonatomic, copy) NSString* detailText;
@property(nonatomic, copy) NSString* optionalText;

// Whether or not this item is checked.
@property(nonatomic, assign) BOOL checked;

// Background color for the cell's backgroundView when self.checked == YES
// Is copied to the cell's highlightedBackgroundColor
@property(nonatomic, strong) UIColor* checkedBackgroundColor;

// Mask of the data to be cleared.
@property(nonatomic, assign) BrowsingDataRemoveMask dataTypeMask;

// Pref name associated with the item.
@property(nonatomic, assign) const char* prefName;

@end

// TableViewClearBrowsingDataCell implements an TableViewCell subclass
// containing a leading image icon and three text labels: a "title" label, a
// "detail" label, and an optional third label in case we need more description
// for the item. All three labels are laid out one after the other vertically
// and fill the full width of the cell.
@interface TableViewClearBrowsingDataCell : TableViewCell

@property(nonatomic, strong) UILabel* textLabel;
@property(nonatomic, strong) UILabel* detailTextLabel;
@property(nonatomic, strong) UILabel* optionalTextLabel;

// Whether or not this cell is checked.
@property(nonatomic, assign) BOOL checked;

// Background color for this cell's backgroundView when highlighted
@property(nonatomic, strong) UIColor* highlightedBackgroundColor;

- (void)setImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_TABLE_VIEW_CLEAR_BROWSING_DATA_ITEM_H_
