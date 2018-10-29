// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_MOST_VISITED_SHORTCUT_CELL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_MOST_VISITED_SHORTCUT_CELL_H_

#import <UIKit/UIKit.h>

@class NTPMostVisitedTileView;

// A collection view subclass that contains a most visited tile.
@interface MostVisitedShortcutCell : UICollectionViewCell

// The tile contained in the cell.
@property(nonatomic, strong) NTPMostVisitedTileView* tile;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_MOST_VISITED_SHORTCUT_CELL_H_
