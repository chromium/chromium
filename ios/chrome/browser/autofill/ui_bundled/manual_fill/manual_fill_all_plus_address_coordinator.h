// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ALL_PLUS_ADDRESS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ALL_PLUS_ADDRESS_COORDINATOR_H_

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_coordinator.h"

@class ManualFillAllPlusAddressCoordinator;

// Delegate for the coordinator actions.
@protocol ManualFillAllPlusAddressCoordinatorDelegate <NSObject>

// Requests the delegate to dismiss the coordinator.
- (void)manualFillAllPlusAddressCoordinatorWantsToBeDismissed:
    (ManualFillAllPlusAddressCoordinator*)coordinator;

// Requests the delegate to dismiss the coordinator and then open the manage
// plus address view.
- (void)dismissManualFillAllPlusAddressAndOpenManagePlusAddress;

@end

// Creates and manages a view controller to present all the plus addresses. The
// view contains a search bar and any selected plus address would be filled in
// the current field of the active web state.
@interface ManualFillAllPlusAddressCoordinator : FallbackCoordinator

// The delegate for this coordinator.
@property(nonatomic, weak) id<ManualFillAllPlusAddressCoordinatorDelegate>
    manualFillAllPlusAddressCoordinatorDelegate;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_ALL_PLUS_ADDRESS_COORDINATOR_H_
