// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_TABLE_VIEW_TABLE_VIEW_CELLS_CONSTANTS_H_
#define IOS_CHROME_COMMON_UI_TABLE_VIEW_TABLE_VIEW_CELLS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// The minimum height for a TableViewHeaderFooterView.
extern const CGFloat kTableViewHeaderFooterViewHeight;

// The minimum height for a TableViewCell.
extern const CGFloat kChromeTableViewCellHeight;

// The horizontal spacing between views and the container view of a cell.
extern const CGFloat kTableViewHorizontalSpacing;

// The vertical spacing for a cell containing only one label.
extern const CGFloat kTableViewOneLabelCellVerticalSpacing;

// The vertical spacing for a cell containing one label and one sub label.
extern const CGFloat kTableViewTwoLabelsCellVerticalSpacing;

// The vertical spacing between views and the container view of a cell.
extern const CGFloat kTableViewVerticalSpacing;

// The large vertical spacing between views and the container view of a cell.
extern const CGFloat kTableViewLargeVerticalSpacing;

// The horizontal spacing between subviews within the container view.
extern const CGFloat kTableViewSubViewHorizontalSpacing;

// Animation duration for highlighting selected section header.
extern const CGFloat kTableViewCellSelectionAnimationDuration;

// Setting the font size to 0 for a custom preferred font lets iOS manage
// sizing.
extern const CGFloat kUseDefaultFontSize;

// Spacing between text label and cell contentView.
extern const CGFloat kTableViewLabelVerticalTopSpacing;

// The width taken by the accessory view when it is displayed.
extern const CGFloat kTableViewAccessoryWidth;

// Size of the icon image.
extern const CGFloat kTableViewIconImageSize;

// Padding used between the image and the text labels.
extern const CGFloat kTableViewImagePadding;

// Padding used between the trailing content and the trailing of the
// contentView.
extern const CGFloat kTableViewTrailingContentPadding;

// A masked password string(e.g. "••••••••").
extern NSString* const kMaskedPassword;

// The accessibility identifier of the info button of the
// TableViewInfoButtonCell.
extern NSString* const kTableViewCellInfoButtonViewId;

// The accessibility identifier of the TableViewTabsSearchSuggestedHistoryItem.
extern NSString* const kTableViewTabsSearchSuggestedHistoryItemId;

// Accessibility identifier for the badge icon.
extern NSString* const kTableViewURLCellFaviconBadgeViewID;

// Accessibility identifier for the metadata image view.
extern NSString* const kTableViewURLCellMetadataImageID;

// Returns a padding according to the width of the current device.
extern CGFloat HorizontalPadding();

// Accessibility identifier for UMA checkbox in the FRE and in Google services
// settings.
extern NSString* const kImproveChromeItemAccessibilityIdentifier;

// Accessibility identifier for TableViewActivityIndicatorHeaderFooterView.
extern NSString* const kTableViewActivityIndicatorHeaderFooterViewId;

#endif  // IOS_CHROME_COMMON_UI_TABLE_VIEW_TABLE_VIEW_CELLS_CONSTANTS_H_
