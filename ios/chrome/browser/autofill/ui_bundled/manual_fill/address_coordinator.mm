// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/address_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/address_list_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/address_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_address_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ui/base/device_form_factor.h"

@interface AddressCoordinator () <AddressListDelegate>

// The view controller presented above the keyboard where the user can select
// a field from one of their addresses.
@property(nonatomic, strong) AddressViewController* addressViewController;

// Fetches and filters the addresses for the view controller.
@property(nonatomic, strong) ManualFillAddressMediator* addressMediator;

@end

@implementation AddressCoordinator

// Property tagged dynamic because it overrides super class delegate with and
// extension of the super delegate type (i.e. AddressCoordinatorDelegate extends
// FallbackCoordinatorDelegate)
@dynamic delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler
                    showAutofillFormButton:(BOOL)showAutofillFormButton {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                          injectionHandler:injectionHandler];
  if (self) {
    _addressViewController = [[AddressViewController alloc] init];

    ChromeBrowserState* browserState =
        self.browser->GetBrowserState()->GetOriginalChromeBrowserState();

    // Service must use regular browser state, even if the Browser has an
    // OTR browser state.
    autofill::PersonalDataManager* personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
    CHECK(personalDataManager);

    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForBrowserState(browserState);
    CHECK(authenticationService);

    _addressMediator = [[ManualFillAddressMediator alloc]
        initWithPersonalDataManager:personalDataManager
             showAutofillFormButton:showAutofillFormButton
              authenticationService:authenticationService];
    _addressMediator.navigationDelegate = self;
    _addressMediator.contentInjector = super.injectionHandler;
    _addressMediator.consumer = _addressViewController;
  }
  return self;
}

- (void)stop {
  [super stop];
  [_addressMediator disconnect];
  _addressMediator = nil;

  _addressViewController = nil;
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.addressViewController;
}

#pragma mark - AddressListDelegate

- (void)openAddressSettings {
  __weak __typeof(self) weakSelf = self;
  [self dismissIfNecessaryThenDoCompletion:^{
    [weakSelf.delegate openAddressSettings];
  }];
}

- (void)openAddressDetailsInEditMode:(const autofill::AutofillProfile*)address
               offerMigrateToAccount:(BOOL)offerMigrateToAccount {
  __weak __typeof(self) weakSelf = self;
  [self dismissIfNecessaryThenDoCompletion:^{
    [weakSelf.delegate openAddressDetailsInEditMode:address
                              offerMigrateToAccount:offerMigrateToAccount];
  }];
}

@end
