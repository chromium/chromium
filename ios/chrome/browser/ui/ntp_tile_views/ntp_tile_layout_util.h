// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_TILE_LAYOUT_UTIL_H_
#define IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_TILE_LAYOUT_UTIL_H_

#import <UIKit/UIKit.h>

// Vertical spacing between rows of tiles.
extern const int kNtpTilesVerticalSpacing;
// Vertical spacing between columns of tiles.
extern const int kNtpTilesHorizontalSpacingRegular;
extern const int kNtpTilesHorizontalSpacingCompact;

// For font size < UIContentSizeCategoryExtraExtraExtraLarge.
extern const CGSize kNtpTileViewSizeSmall;
// For font size == UIContentSizeCategoryExtraExtraExtraLarge.
extern const CGSize kNtpTileViewSizeMedium;
// For font size == UIContentSizeCategoryAccessibilityMedium.
extern const CGSize kNtpTileViewSizeLarge;
// For font size > UIContentSizeCategoryAccessibilityMedium.
extern const CGSize kNtpTileViewSizeExtraLarge;

// Returns the vertical spacing between columns of tiles under
// |trait_collection|.
CGFloat NtpTilesHorizontalSpacing(UITraitCollection* trait_collection);

// Returns the size of most visited cell based on |category|.
CGSize MostVisitedCellSize(UIContentSizeCategory category);

// Returns x-offset in order to have the tiles centered in a view with a
// |width| under |environment|.
CGFloat CenteredTilesMarginForWidth(UITraitCollection* trait_collection,
                                    CGFloat width);

#endif  // IOS_CHROME_BROWSER_UI_NTP_TILE_VIEWS_NTP_TILE_LAYOUT_UTIL_H_
