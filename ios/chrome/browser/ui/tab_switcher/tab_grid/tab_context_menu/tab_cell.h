// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CELL_H_

#import <UIKit/UIKit.h>

@class GridItemIdentifier;

// UICollectionViewCell that represents a tab cell.
@interface TabCell : UICollectionViewCell

// Unique identifier for the cell's contents. This is used to ensure that
// updates in an asynchronous callback are only made if the item is the same.
@property(nonatomic, strong) GridItemIdentifier* itemIdentifier;

@property(nonatomic, readonly) UIDragPreviewParameters* dragPreviewParameters;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CELL_H_
