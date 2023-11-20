// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_COORDINATOR_PLUS_ADDRESS_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_COORDINATOR_PLUS_ADDRESS_BOTTOM_SHEET_COORDINATOR_H_

#import "components/plus_addresses/plus_address_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "url/origin.h"

@protocol BrowserCoordinatorCommands;
@protocol PlusAddressBottomSheetCoordinatorDelegate;

// The coordinator responsible for creating the bottom sheet's view controller,
// and eventually a mediator.
@interface PlusAddressBottomSheetCoordinator : ChromeCoordinator

// `viewController` is the VC used to present the bottom sheet.
// TODO(b/311204704): create a mediator that will orchestrate the data
// operations via the `PlusAddressService`. This will entail reserving a
// plus_address, showing a preview of it, confirming via service call when the
// primary button is clicked, and then running a callback to fill the field from
// which the bottom sheet was triggered.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser;

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_COORDINATOR_PLUS_ADDRESS_BOTTOM_SHEET_COORDINATOR_H_
