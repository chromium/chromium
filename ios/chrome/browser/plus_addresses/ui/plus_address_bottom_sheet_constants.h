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

// Accessibility identifier for the bottom sheet's notice message used to locate
// the notice message in automation.
extern NSString* const kPlusAddressSheetNoticeMessageAccessibilityIdentifier;

// Accessibility identifier for the plus address label.
extern NSString* const kPlusAddressLabelAccessibilityIdentifier;

// Accessibility identifier for the refresh button in the bottom sheet.
extern NSString* const kPlusAddressRefreshButtonAccessibilityIdentifier;

// The margin to be shown under the user's primary email address, and above the
// reserved plus address element in the bottom sheet.
extern const CGFloat kPlusAddressSheetPrimaryAddressBottomMargin;

// The margin to be shown above the image, to prevent the top of the bottom
// sheet content being too close to the top of the sheet.
extern const CGFloat kPlusAddressSheetBeforeImageTopMargin;

// The margin to be shown below the image, to prevent the extra space which may
// cause content overflow.
extern const CGFloat kPlusAddressSheetAfterImageMargin;

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
  // A bug reporting URL for plus addresses.
  kErrorReport = 0,
  // A plus address management surface on accounts.google.com.
  kManagement = 1,
  // A help center page to learn more about plus addresses.
  kLearnMore = 2,
};

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSTANTS_H_
