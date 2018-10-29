// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_COLLECTION_SHORTCUT_CELL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_COLLECTION_SHORTCUT_CELL_H_

#import <UIKit/UIKit.h>

@class NTPShortcutTileView;

// A collection view subclass that contains a collection shortcut tile.
@interface CollectionShortcutCell : UICollectionViewCell

// The tile contained in the cell.
@property(nonatomic, strong) NTPShortcutTileView* tile;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_COLLECTION_SHORTCUT_CELL_H_
