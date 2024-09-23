// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_DELEGATE_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_DELEGATE_H_

#import <Foundation/Foundation.h>
#import "url/gurl.h"

enum class PlusAddressURLType;
// A delegate that wraps service calls backing the plus_address bottom sheet UI.
@protocol PlusAddressBottomSheetDelegate

// Asks this delegate to reserve a plus address.
- (void)reservePlusAddress;

// Asks this delegate to confirm use of a reserved plus address. Intended to be
// called only after `reservePlusAddress` succeeds.
- (void)confirmPlusAddress;

// Asks the delegate for the user's primary email address.
- (NSString*)primaryEmailAddress;

// Asks the delegate to open the URL for `PlusAddressUrlType` on new tab.
- (void)openNewTab:(PlusAddressURLType)type;

// Asks the delegate if the refresh functionality is enabled.
- (BOOL)isRefreshEnabled;

// Informs the delegate that the refresh button has been tapped by the user.
- (void)didTapRefreshButton;

// Informs the delegate whether to show the notice in the view or not.
- (BOOL)shouldShowNotice;

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_BOTTOM_SHEET_DELEGATE_H_
