// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"

NSString* const kInactiveTabButtonSectionIdentifier =
    @"InactiveTabSectionIdentifier";
NSString* const kGridOpenTabsSectionIdentifier = @"OpenTabsSectionIdentifier";
NSString* const kSuggestedActionsSectionIdentifier =
    @"SuggestedActionsSectionIdentifier";

NSString* const kInactiveTabsButtonAccessibilityIdentifier =
    @"InactiveTabsButtonAccessibilityIdentifier";

// Accessibility identifier prefix of a grid cell.
NSString* const kGridCellIdentifierPrefix = @"GridCellIdentifierPrefix";

// Accessibility identifier prefix of a grid cell.
NSString* const kGroupGridCellIdentifierPrefix =
    @"GroupGridCellIdentifierPrefix";

// Accessibility identifier for the close button in a grid cell.
NSString* const kGridCellCloseButtonIdentifier =
    @"GridCellCloseButtonIdentifier";

// Accessibility identifier for the background of the grid.
NSString* const kGridBackgroundIdentifier = @"GridBackgroundIdentifier";

// Accessibility identifier for the grid section header.
NSString* const kGridSectionHeaderIdentifier = @"GridSectionHeaderIdentifier";

// Accessibility identifier for the suggested actions cell.
NSString* const kSuggestedActionsGridCellIdentifier =
    @"SuggestedActionsGridCellIdentifier";

// Grid styling.
NSString* const kGridBackgroundColor = @"grid_background_color";

const CGFloat kReorderingInactiveCellOpacity = 0.80;
const CGFloat kReorderingActiveCellScale = 1.15;

// GridHeader styling.
const int kGridHeaderTitleColor = 0xFFFFFF;
const int kGridHeaderValueColor = 0xEBEBF5;
const CGFloat kGridHeaderContentSpacing = 4.0f;

// GridCell dimensions.
const CGFloat kGridCellCornerRadius = 16.0f;
const CGFloat kGridCellIconCornerRadius = 3.0f;
const CGFloat kGroupGridCellCornerRadius = 12.0f;
const CGFloat kGroupGridFaviconViewCornerRadius = 3.0f;

// The cell header contains the icon, title, and close button.
const CGFloat kGridCellHeaderHeight = 32.0f;
const CGFloat kGridCellHeaderAccessibilityHeight = 108.0f;
const CGFloat kGridCellHeaderLeadingInset = 9.0f;
const CGFloat kGridCellCloseTapTargetWidthHeight = 32.0f;
const CGFloat kGridCellCloseButtonContentInset = 8.5f;
const CGFloat kGridCellCloseButtonTopSpacing = 16.0f;
const CGFloat kGridCellTitleLabelContentInset = 4.0f;
const CGFloat kGridCellIconDiameter = 16.0f;
const CGFloat kGridCellSelectIconContentInset = 4.0f;
const CGFloat kGridCellSelectIconTopSpacing = 3.5f;
const CGFloat kGridCellSelectIconSize = 25.0f;
const CGFloat kGridCellSelectionRingGapWidth = 2.0f;
const CGFloat kGridCellSelectionRingTintWidth = 5.0f;

const CGFloat kGridCellPriceDropTopSpacing = 10.0f;
const CGFloat kGridCellPriceDropLeadingSpacing = 10.0f;
const CGFloat kGridCellPriceDropTrailingSpacing = 10.0f;
