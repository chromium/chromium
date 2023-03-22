// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSTANTS_H_

#include <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

// Accessibility identifier prefix of a grid cell. To reference a specific cell,
// concatenate `kGridCellIdentifierPrefix` with the index of the cell. For
// example, [NSString stringWithFormat:@"%@%d", kGridCellIdentifierPrefix,
// index].
extern NSString* const kGridCellIdentifierPrefix;

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

// PlusSignCell styling
extern NSString* const kPlusSignCellBackgroundColor;
extern NSString* const kPlusSignCellBackgroundDarkColor;

// GridLayout.
// Extra-small screens require a slightly different layout configuration (e.g.,
// margins) even though they may be categorized into the same size class as
// larger screens. These screens are determined to have a "limited width" in
// their size class by the definition below. The first size class refers to the
// horizontal; the second to the vertical.
extern const CGFloat kGridLayoutCompactCompactLimitedWidth;
extern const CGFloat kGridLayoutCompactRegularLimitedWidth;
// Insets for size classes. The first refers to the horizontal size class; the
// second to the vertical.
extern const UIEdgeInsets kGridLayoutInsetsCompactCompact;
extern const UIEdgeInsets kGridLayoutInsetsCompactCompactLimitedWidth;
extern const UIEdgeInsets kGridLayoutInsetsCompactRegular;
extern const UIEdgeInsets kGridLayoutInsetsCompactRegularLimitedWidth;
extern const UIEdgeInsets kGridLayoutInsetsRegularCompact;
extern const UIEdgeInsets kGridLayoutInsetsRegularRegular;
// Minimum line spacing for size classes. The first refers to the horizontal
// size class; the second to the vertical.
extern const CGFloat kGridLayoutLineSpacingCompactCompact;
extern const CGFloat kGridLayoutLineSpacingCompactCompactLimitedWidth;
extern const CGFloat kGridLayoutLineSpacingCompactRegular;
extern const CGFloat kGridLayoutLineSpacingCompactRegularLimitedWidth;
extern const CGFloat kGridLayoutLineSpacingRegularCompact;
extern const CGFloat kGridLayoutLineSpacingRegularRegular;

// GridReorderingLayout.
// Opacity for cells that aren't being moved.
extern const CGFloat kReorderingInactiveCellOpacity;
// Scale for the cell that is being moved.
extern const CGFloat kReorderingActiveCellScale;

// GridHeader styling.
// The GridHeader height.
extern const CGFloat kGridHeaderHeight;
extern const CGFloat kGridHeaderAccessibilityHeight;
// The GridHeader title label Color.
extern const int kGridHeaderTitleColor;
// The GridHeader value label Color.
extern const int kGridHeaderValueColor;
// The space between different labels inside the GridHeader.
extern const CGFloat kGridHeaderContentSpacing;

// GridCell dimensions.
extern const CGSize kGridCellSizeSmall;
extern const CGSize kGridCellSizeMedium;
extern const CGSize kGridCellSizeLarge;
extern const CGSize kGridCellSizeAccessibility;
extern const CGFloat kGridCellCornerRadius;
extern const CGFloat kGridCellIconCornerRadius;
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

// Horizontal distance from the center of the plus sign image to the trailing of
// the tab grid.
extern const CGFloat kPlusSignImageTrailingCenterDistance;
// Threshold after which the thumb strip's plus sign button should be hidden.
extern const CGFloat kScrollThresholdForPlusSignButtonHide;
// Vertical distance from the center of the plus sign image and the top of the
// tab grid.
extern const CGFloat kPlusSignImageYCenterConstant;
// With of the plus sign button.
extern const CGFloat kPlusSignButtonWidth;
// Expected collection view top content inset. If mismatch, the extra space
// is communicated to the plus sign button to align + sign correctly.
extern const CGFloat kGridExpectedTopContentInset;
// Height of the view that is revealed. The thumb strip has a height equal to a
// small grid cell + edge insets (top and bottom) from thumb strip layout.
extern const CGFloat kThumbStripHeight;

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CONSTANTS_H_
