// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_PLUS_ADDRESS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_PLUS_ADDRESS_COORDINATOR_DELEGATE_H_

// Delegate for the plus address fallback actions.
@protocol PlusAddressCoordinatorDelegate <NSObject>

// Opens the bottom sheet to create plus address.
- (void)openCreatePlusAddressSheet;

// Opens the all plus addresses picker.
- (void)openAllPlusAddressesPicker;

// Opens the manage page address page in a new tab.
- (void)openManagePlusAddress;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_PLUS_ADDRESS_COORDINATOR_DELEGATE_H_
