// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_ADDRESS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_ADDRESS_COORDINATOR_H_

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/plus_address_coordinator_delegate.h"

@class ManualFillPlusAddressMediator;

namespace autofill {
class AutofillProfile;
}  // namespace autofill

// Delegate for the coordinator actions.
// TODO(crbug.com/40577448): revise delegate method names.
@protocol AddressCoordinatorDelegate <FallbackCoordinatorDelegate,
                                      PlusAddressCoordinatorDelegate>

// Opens the details of the given address in edit mode. `offerMigrateToAccount`
// indicates whether or not the option to migrate the address to the account
// should be available in the details page.
- (void)openAddressDetailsInEditMode:(autofill::AutofillProfile)address
               offerMigrateToAccount:(BOOL)offerMigrateToAccount;

// Opens the address settings.
- (void)openAddressSettings;

@end

// Creates and manages a view controller to present addresses to the user.
// Any selected address field will be sent to the current field in the active
// web state.
@interface AddressCoordinator : FallbackCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
             manualFillPlusAddressMediator:
                 (ManualFillPlusAddressMediator*)manualFillPlusAddressMediator
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler
                    showAutofillFormButton:(BOOL)showAutofillFormButton
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler
    NS_UNAVAILABLE;

// The delegate for this coordinator. Delegate class extends
// FallbackCoordinatorDelegate, and replaces super class delegate.
@property(nonatomic, weak) id<AddressCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_ADDRESS_COORDINATOR_H_
