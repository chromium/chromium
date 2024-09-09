// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_PLUS_ADDRESS_LIST_NAVIGATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_PLUS_ADDRESS_LIST_NAVIGATOR_H_

// Object to navigate different views in manual fallback's plus address list.
@protocol PlusAddressListNavigator

// Requests to open the bottom sheet to create a new plus address.
- (void)openCreatePlusAddressSheet;

// Requests to open the list of all plus addresses.
- (void)openAllPlusAddressList;

// Requests to open the manage plus address page.
- (void)openManagePlusAddress;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_PLUS_ADDRESS_LIST_NAVIGATOR_H_
