// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_LAYOUT_UTIL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_LAYOUT_UTIL_H_

#import <UIKit/UIKit.h>

// Vertical spacing between rows of tiles.
extern const int kContentSuggestionsTilesVerticalSpacing;
// Vertical spacing between columns of tiles.
extern const int kContentSuggestionsTilesHorizontalSpacingRegular;
extern const int kContentSuggestionsTilesHorizontalSpacingCompact;

// For font size < UIContentSizeCategoryExtraExtraExtraLarge.
extern const CGSize kContentSuggestionsTileViewSizeSmall;
// For font size == UIContentSizeCategoryExtraExtraExtraLarge.
extern const CGSize kContentSuggestionsTileViewSizeMedium;
// For font size == UIContentSizeCategoryAccessibilityMedium.
extern const CGSize kContentSuggestionsTileViewSizeLarge;
// For font size > UIContentSizeCategoryAccessibilityMedium.
extern const CGSize kContentSuggestionsTileViewSizeExtraLarge;

// Returns the vertical spacing between columns of tiles under
// `trait_collection`.
CGFloat ContentSuggestionsTilesHorizontalSpacing(UITraitCollection* trait_collection);

// Returns the size of most visited cell based on `category`.
CGSize MostVisitedCellSize(UIContentSizeCategory category);

// Returns x-offset in order to have the tiles centered in a view with a
// `width` under `environment`.
CGFloat CenteredTilesMarginForWidth(UITraitCollection* trait_collection,
                                    CGFloat width);

// Returns horizontal space needed to show the Most Visited tiles.
CGFloat MostVisitedTilesContentHorizontalSpace(
    UITraitCollection* trait_collection);

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_LAYOUT_UTIL_H_
