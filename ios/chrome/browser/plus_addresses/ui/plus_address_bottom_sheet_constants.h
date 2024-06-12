// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the bottom sheet's description used to locate
// the description in automation.
extern NSString* const kPlusAddressSheetDescriptionAccessibilityIdentifier;

// Accessibility identifier for the bottom sheet's error message used to locate
// the error message in automation.
extern NSString* const kPlusAddressSheetErrorMessageAccessibilityIdentifier;

// The margin to be shown under the user's primary email address, and above the
// reserved plus address element in the bottom sheet.
extern const CGFloat kPlusAddressSheetPrimaryAddressBottomMargin;

// The margin to be shown above the image, to prevent the top of the bottom
// sheet content being too close to the top of the sheet.
extern const CGFloat kPlusAddressSheetBeforeImageTopMargin;

// The margin to be shown below the image, to prevent the extra space which may
// cause content overflow.
extern const CGFloat kPlusAddressSheetAfterImageMargin;

// The desired size of the image at the top of the bottom sheet content in
// `PointSize`.
extern const CGFloat kPlusAddressSheetImageSize;

// The desired size of the branded image width at the top of the bottom sheet
// content. `kImageSize` can not be used as branded image size uses different
// dimensions and unit.
extern const CGFloat kPlusAddressSheetBrandedImageWidth;

// The margin to be added below the content. This is hidden under action buttons
// stack and is used to prevent content being hidden under buttons border.
extern const CGFloat kPlusAddressSheetScrollViewBottomInsets;

// The table view corner radius.
extern const CGFloat kPlusAddressSheetTableViewCellCornerRadius;

// The cell height in the table view.
extern const CGFloat kPlusAddressSheetTableViewCellHeight;

// The image size for both the plus address and the refresh icon.
extern const CGFloat kPlusAddressSheetCellImageSize;

// Branding icon view size.
extern const CGFloat kPlusAddressSheetBrandingIconContainerViewSize;

// Branding icon view corner radius.
extern const CGFloat kPlusAddressSheetBrandingIconContainerViewCornerRadius;

// Branding icon view shadow radius.
extern const CGFloat kPlusAddressSheetBrandingIconContainerViewShadowRadius;

// Branding icon view shadow opacity.
extern const CGFloat kPlusAddressSheetBrandingIconContainerViewShadowOpacity;

// Branding icon size that sits inside the entire view.
extern const CGFloat kPlusAddressSheetBrandingIconSize;

// Bottom padding for the branding icon view.
extern const CGFloat kPlusAddressSheetBrandingIconContainerViewBottomPadding;

// Top padding for the branding icon view.
extern const CGFloat kPlusAddressSheetBrandingIconContainerViewTopPadding;

// Enum specifying the URL the bottom sheet should open.
enum class PlusAddressURLType {
  kErrorReport = 0,
  kManagement = 1,
};

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSTANTS_H_
