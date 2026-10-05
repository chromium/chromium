// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_UTILS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_UTILS_H_

#import <UIKit/UIKit.h>

@class MostVisitedTilesCollectionView;
@class NewTabPageColorPalette;

/// Block extracting a `UIColor` from a  `NewTabPageColorPalette`.
typedef UIColor* (^PaletteColorProvider)(NewTabPageColorPalette*);

// Whether the top of feed sync promo has met the criteria to be shown.
bool ShouldShowTopOfFeedSyncPromo();

/// Generates a `UIButtonConfigurationUpdateHandler` that will color its button
/// correctly for the current NTP theming status.
/// - `unthemedTintColor` is the button's foreground tint color when there is no
///   theme set (color or image)
/// - `paletteBackgroundColorProvider` provides the desired background color
///   of the button. It will be passed the current palette, or nil if there is
///   no theme set.
/// - `imageBlurEffectStyleOverride` if set, overrides the default background
///   blur style for when the NTP background is an image.
UIButtonConfigurationUpdateHandler CreateThemedButtonConfigurationUpdateHandler(
    UIColor* unthemedTintColor,
    PaletteColorProvider paletteBackgroundColorProvider,
    UIBlurEffectStyle imageBlurEffectStyleOverride =
        UIBlurEffectStyleSystemMaterial);

/// Creates and returns a container view wrapping `collectionView` with standard
/// NTP card background styling, corner radius, and bottom padding. Returns nil
/// if `collectionView` is nil.
UIView* CreateMostVisitedContainerView(
    MostVisitedTilesCollectionView* collectionView,
    BOOL hasBackground);

/// Calculates the layout height of Most Visited Tiles given its inner
/// collection view, falling back to system layout fitting on `fallbackView` or
/// `collectionView.intrinsicContentSize` if `collectionView` is nil or has not
/// calculated its content size.
CGFloat MostVisitedContainerHeight(
    MostVisitedTilesCollectionView* collectionView,
    UIView* fallbackView = nil);

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_UTILS_H_
