// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSTANTS_H_

#include <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

// Identifier for the section containing the inactive tab button.
extern NSString* const kInactiveTabButtonSectionIdentifier;
// Identifier for the tabs section.
extern NSString* const kGridOpenTabsSectionIdentifier;
// Identifier for the section containing the suggested actions.
extern NSString* const kSuggestedActionsSectionIdentifier;

// Accessibility identifier for the Inactive Tabs button (entry point).
extern NSString* const kInactiveTabsButtonAccessibilityIdentifier;

// Accessibility identifier prefix of a grid cell. To reference a specific cell,
// concatenate `kGridCellIdentifierPrefix` with the index of the cell. For
// example, [NSString stringWithFormat:@"%@%d", kGridCellIdentifierPrefix,
// index].
extern NSString* const kGridCellIdentifierPrefix;

// Accessibility identifier prefix of a group grid cell. To reference a specific
// cell, concatenate `kGroupGridCellIdentifierPrefix` with the index of the
// cell. For example, [NSString stringWithFormat:@"%@%d",
// kGroupGridCellIdentifierPrefix, index].
extern NSString* const kGroupGridCellIdentifierPrefix;

// Accessibility identifier for the close button in a grid cell.
extern NSString* const kGridCellCloseButtonIdentifier;

// Accessibility identifier for the background of the grid.
extern NSString* const kGridBackgroundIdentifier;

// Accessibility identifier for the grid section header.
extern NSString* const kGridSectionHeaderIdentifier;

// Accessibility identifier for the suggested actions cell.
extern NSString* const kSuggestedActionsGridCellIdentifier;

// Grid styling.
extern NSString* const kGridBackgroundColor;

// GridReorderingLayout.
// Opacity for cells that aren't being moved.
extern const CGFloat kReorderingInactiveCellOpacity;
// Scale for the cell that is being moved.
extern const CGFloat kReorderingActiveCellScale;

// GridHeader styling.
// The GridHeader title label Color.
extern const int kGridHeaderTitleColor;
// The GridHeader value label Color.
extern const int kGridHeaderValueColor;
// The space between different labels inside the GridHeader.
extern const CGFloat kGridHeaderContentSpacing;

// GridCell dimensions.
extern const CGFloat kGridCellCornerRadius;
extern const CGFloat kGridCellIconCornerRadius;
extern const CGFloat kGroupGridCellCornerRadius;
extern const CGFloat kGroupGridFaviconViewCornerRadius;
// The cell header contains the icon, title, and close button.
extern const CGFloat kGridCellHeaderHeight;
extern const CGFloat kGridCellHeaderAccessibilityHeight;
extern const CGFloat kGridCellHeaderLeadingInset;
extern const CGFloat kGridCellCloseTapTargetWidthHeight;
extern const CGFloat kGridCellCloseButtonContentInset;
extern const CGFloat kGridCellCloseButtonTopSpacing;
extern const CGFloat kGridCellTitleLabelContentInset;
extern const CGFloat kGridCellIconDiameter;
extern const CGFloat kGridCellSelectIconContentInset;
extern const CGFloat kGridCellSelectIconTopSpacing;
extern const CGFloat kGridCellSelectIconSize;
extern const CGFloat kGridCellSelectionRingGapWidth;
extern const CGFloat kGridCellSelectionRingTintWidth;

// PriceCardView constants
extern const CGFloat kGridCellPriceDropTopSpacing;
extern const CGFloat kGridCellPriceDropLeadingSpacing;
extern const CGFloat kGridCellPriceDropTrailingSpacing;

typedef NS_ENUM(NSUInteger, GridCellState) {
  GridCellStateNotEditing = 1,
  GridCellStateEditingUnselected,
  GridCellStateEditingSelected,
};

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSTANTS_H_
