// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_COLLECTION_VIEW_LAYOUT_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_COLLECTION_VIEW_LAYOUT_H_

#import <UIKit/UIKit.h>

/// A custom compositional layout for the most visited tiles collection view.
@interface MostVisitedTilesCollectionViewLayout
    : UICollectionViewCompositionalLayout

/// Initializer with `count` number of items in the collection.
- (instancetype)initWithItemCount:(NSUInteger)count NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_COLLECTION_VIEW_LAYOUT_H_
