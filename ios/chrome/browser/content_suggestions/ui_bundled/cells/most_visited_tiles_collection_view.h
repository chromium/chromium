// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_MOST_VISITED_TILES_COLLECTION_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_MOST_VISITED_TILES_COLLECTION_VIEW_H_

#import <UIKit/UIKit.h>

@class MostVisitedTilesConfig;

/// Implementation for the Most Visited Tiles so that its model can directly
/// update it.
@interface MostVisitedTilesCollectionView : UICollectionView

/// Initializes the collection view with `config` and `spacing` between the
/// cells.
- (instancetype)initWithConfig:(MostVisitedTilesConfig*)config
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame
         collectionViewLayout:(UICollectionViewLayout*)layout NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_MOST_VISITED_TILES_COLLECTION_VIEW_H_
