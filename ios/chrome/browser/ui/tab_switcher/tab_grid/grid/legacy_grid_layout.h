// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_LEGACY_GRID_LAYOUT_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_LEGACY_GRID_LAYOUT_H_

#import <UIKit/UIKit.h>

// A collection view compositional layout that displays items in a grid.
//
// - The number of columns adapts to the available width and whether an
//     Accessibility Dynamic Type is selected.
// - Item insertions and deletions are animated by default.
@interface LegacyGridLayout : UICollectionViewFlowLayout

// Whether to animate item insertions and deletions. Defaults to YES.
@property(nonatomic, assign) BOOL animatesItemUpdates;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_LEGACY_GRID_LAYOUT_H_
