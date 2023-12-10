// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the bottom sheet's description used to locate
// the description in automation.
extern NSString* const kPlusAddressModalDescriptionAccessibilityIdentifier;

// The margin to be shown under the user's primary email address, and above the
// reserved plus address element in the bottom sheet.
extern const CGFloat kPrimaryAddressBottomMargin;

// The margin to be shown above the image, to prevent the top of the bottom
// sheet content being too close to the top of the sheet.
extern const CGFloat kBeforeImageTopMargin;

// The desired size of the image at the top of the bottom sheet content.
extern const CGFloat kImageSize;

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_CONSTANTS_H_
