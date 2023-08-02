// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"

// Accessibility identifier prefix of a grid cell.
NSString* const kGridCellIdentifierPrefix = @"GridCellIdentifierPrefix";

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

// Definition of limited width for applicable size classes. The first refers to
// the horizontal size class; the second to the vertical.
const CGFloat kGridLayoutCompactCompactLimitedWidth = 666.0f;
const CGFloat kGridLayoutCompactRegularLimitedWidth = 374.0f;
// Insets for size classes. The first refers to the horizontal size class; the
// second to the vertical.
const UIEdgeInsets kGridLayoutInsetsCompactCompact =
    UIEdgeInsets{20.0f, 20.0f, 20.0f, 20.0f};
const UIEdgeInsets kGridLayoutInsetsCompactCompactLimitedWidth =
    UIEdgeInsets{22.0f, 44.0f, 22.0f, 44.0f};
const UIEdgeInsets kGridLayoutInsetsCompactRegular =
    UIEdgeInsets{13.0f, 13.0f, 13.0f, 13.0f};
const UIEdgeInsets kGridLayoutInsetsCompactRegularLimitedWidth =
    UIEdgeInsets{28.0f, 10.0f, 28.0f, 10.0f};
const UIEdgeInsets kGridLayoutInsetsRegularCompact =
    UIEdgeInsets{32.0f, 32.0f, 32.0f, 32.0f};
const UIEdgeInsets kGridLayoutInsetsRegularRegular =
    UIEdgeInsets{28.0f, 28.0f, 28.0f, 28.0f};
// Minimum line spacing for size classes. The first refers to the horizontal
// size class; the second to the vertical.
const CGFloat kGridLayoutLineSpacingCompactCompact = 17.0f;
const CGFloat kGridLayoutLineSpacingCompactCompactLimitedWidth = 22.0f;
const CGFloat kGridLayoutLineSpacingCompactRegular = 13.0f;
const CGFloat kGridLayoutLineSpacingCompactRegularLimitedWidth = 15.0f;
const CGFloat kGridLayoutLineSpacingRegularCompact = 32.0f;
const CGFloat kGridLayoutLineSpacingRegularRegular = 14.0f;

const CGFloat kReorderingInactiveCellOpacity = 0.80;
const CGFloat kReorderingActiveCellScale = 1.15;

// GridHeader styling.
const CGFloat kGridHeaderHeight = 32.0f;
const CGFloat kGridHeaderAccessibilityHeight = 58.0f;
const int kGridHeaderTitleColor = 0xFFFFFF;
const int kGridHeaderValueColor = 0xEBEBF5;
const CGFloat kGridHeaderContentSpacing = 4.0f;

// GridCell dimensions.
const CGSize kGridCellSizeSmall = CGSize{144.0f, 168.0f};
const CGSize kGridCellSizeMedium = CGSize{168.0f, 202.0f};
const CGSize kGridCellSizeLarge = CGSize{228.0f, 256.0f};
const CGSize kGridCellSizeAccessibility = CGSize{288.0f, 336.0f};
const CGFloat kGridCellCornerRadius = 16.0f;
const CGFloat kGridCellIconCornerRadius = 3.0f;
// The cell header contains the icon, title, and close button.
const CGFloat kGridCellHeaderHeight = 32.0f;
const CGFloat kGridCellHeaderAccessibilityHeight = 108.0f;
const CGFloat kGridCellHeaderLeadingInset = 9.0f;
const CGFloat kGridCellCloseTapTargetWidthHeight = 44.0f;
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

const CGFloat kGridExpectedTopContentInset = 20.0f;
