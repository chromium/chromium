// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_FLOW_LAYOUT_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_FLOW_LAYOUT_H_

#import <UIKit/UIKit.h>

// Collection view flow layout that displays items in a grid or horizontally.
// Items are square-ish. Item sizes adapt to the size classes they are shown in.
// Item deletions are animated.
@interface FlowLayout : UICollectionViewFlowLayout

// Whether to animate item insertions and deletions.
@property(nonatomic, assign) BOOL animatesItemUpdates;

// Index paths of items being inserted. Exposed for subclasses, and populated on
// this class -prepareForCollectionViewUpdates:.
@property(nonatomic, readonly)
    NSArray<NSIndexPath*>* indexPathsOfInsertingItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_FLOW_LAYOUT_H_
