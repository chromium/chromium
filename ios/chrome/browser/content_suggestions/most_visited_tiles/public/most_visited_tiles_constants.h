// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_PUBLIC_MOST_VISITED_TILES_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_PUBLIC_MOST_VISITED_TILES_CONSTANTS_H_

#import <UIKit/UIKit.h>

// The space between the icon and the title of a tile in the Most Visited
// collection.
CGFloat MostVisitedIconTitleSpacing();

// The size of the container for a Most Visited Tile's favicon.
CGFloat MostVisitedIconContainerSize();

// Insets for the Most Visited collection in a container.
extern const NSDirectionalEdgeInsets kMostVisitedContainerInsets;

// The corner radius to give the MVT image background rounded square corners.
extern const CGFloat kMostVisitedTileImageContainerSquareCornerRadius;

// Size of the favicon or icon in a most visited tile.
extern const CGFloat kMostVisitedTileIconSize;

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_PUBLIC_MOST_VISITED_TILES_CONSTANTS_H_
