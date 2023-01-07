// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/list_model/list_item.h"

@class MDCCollectionViewCell;

// CollectionViewItem holds the model data for a given collection view item.
@interface CollectionViewItem : ListItem

- (instancetype)initWithType:(NSInteger)type NS_DESIGNATED_INITIALIZER;

// Configures the given cell with the item's information. Override this method
// to specialize. At this level, only accessibility properties are ported from
// the item to the cell.
// The cell's class must match cellClass for the given instance.
- (void)configureCell:(MDCCollectionViewCell*)cell NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_CELLS_COLLECTION_VIEW_ITEM_H_
