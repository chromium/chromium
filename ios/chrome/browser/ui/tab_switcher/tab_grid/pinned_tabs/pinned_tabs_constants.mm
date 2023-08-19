// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"

// Pinned view dimensions.
const CGFloat kPinnedViewDragEnabledHeight = 94.0f;
const CGFloat kPinnedViewDefaultHeight = 68.0f;
const CGFloat kPinnedViewCornerRadius = 25.0f;

// Pinned view constraints.
const CGFloat kPinnedViewHorizontalPadding = 6.0f;
const CGFloat kPinnedViewBottomPadding = 8.0f;
const CGFloat kPinnedViewMaxWidthInPercent = 0.5f;

// Pinned view animations.
const NSTimeInterval kPinnedViewFadeInTime = 0.2;
const NSTimeInterval kPinnedViewDragAnimationTime = 0.2;
const NSTimeInterval kPinnedViewMoveAnimationTime = 0.1;
const NSTimeInterval kPinnedViewInsetAnimationTime = 0.2;
const NSTimeInterval kPinnedViewPopAnimationTime = 0.2;

// Pinned cell identifier.
NSString* const kPinnedCellIdentifier = @"PinnedCellIdentifier";

// Pinned View identifier.
NSString* const kPinnedViewIdentifier = @"PinnedViewIdentifier";

// Pinned cell dimensions.
const CGFloat kPinnedCellHeight = 36.0f;
const CGFloat kPinnedCellMaxWidth = 168.0f;
const CGFloat kPinnedCellMinWidth = 90.0f;
const CGFloat kPinnedCellInteritemSpacing = 8.0f;
const CGFloat kPinnedCellPopInitialScale = 0.5f;

// Pinned cell constraints.
const CGFloat kPinnedCellCornerRadius = 13.0f;
const CGFloat kPinnedCellHorizontalPadding = 8.0f;
const CGFloat kPinnedCellTitleLeadingPadding = 4.0f;
const CGFloat kPinnedCellSnapshotTopPadding = 32.0f;
const CGFloat kPinnedCellFaviconWidth = 16.0f;
const CGFloat kPinnedCellFaviconSymbolPointSize = 18.0f;
const CGFloat kPinnedCellFaviconContainerWidth = 16.0f;
const CGFloat kPinnedCellFaviconBorderWidth = 1.5f;
const CGFloat kPinnedCellFaviconContainerCornerRadius = 9.0f;
const CGFloat kPinnedCellFaviconCornerRadius = 3.0f;
const CGFloat kPinnedCellSelectionRingGapWidth = 2.0f;
const CGFloat kPinnedCellSelectionRingTintWidth = 3.0f;
const CGFloat kPinnedCellSelectionRingPadding =
    kPinnedCellSelectionRingTintWidth + kPinnedCellSelectionRingGapWidth;

// Pinned cell collection view layout constraints.
const CGFloat kPinnedCellVerticalLayoutInsets = 16.0f;
const CGFloat kPinnedCellHorizontalLayoutInsets = 16.0f;

// Pinned cell title label fader dimensions.
const CGFloat kPinnedCellFaderGradientWidth = 16.0f;
