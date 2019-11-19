// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/address_coordinator.h"

#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AddressCoordinator () <AddressListDelegate,
                                  PersonalDataManagerObserver> {
  // Personal data manager to be observed.
  autofill::PersonalDataManager* _personalDataManager;

  // C++ to ObjC bridge for PersonalDataManagerObserver.
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge>
      _personalDataManagerObserver;
}

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

- (instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState
          injectionHandler:(ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState
                          injectionHandler:injectionHandler];
  if (self) {
    _addressViewController = [[AddressViewController alloc] init];
    _addressViewController.contentInsetsAlwaysEqualToSafeArea = YES;

    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
    DCHECK(_personalDataManager);

    _personalDataManagerObserver.reset(
        new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_personalDataManagerObserver.get());

    std::vector<autofill::AutofillProfile*> profiles =
        _personalDataManager->GetProfilesToSuggest();

    _addressMediator =
        [[ManualFillAddressMediator alloc] initWithProfiles:profiles];
    _addressMediator.navigationDelegate = self;
    _addressMediator.contentInjector = self.injectionHandler;
    _addressMediator.consumer = _addressViewController;
  }
  return self;
}

- (void)dealloc {
  if (_personalDataManager) {
    _personalDataManager->RemoveObserver(_personalDataManagerObserver.get());
  }
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.addressViewController;
}

#pragma mark - AddressListDelegate

- (void)openAddressSettings {
  __weak id<AddressCoordinatorDelegate> delegate = self.delegate;
  [self dismissIfNecessaryThenDoCompletion:^{
    [delegate openAddressSettings];
    if (IsIPadIdiom()) {
      // Settings close the popover but don't send a message to reopen it.
      [delegate fallbackCoordinatorDidDismissPopover:self];
    }
  }];
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  std::vector<autofill::AutofillProfile*> profiles =
      _personalDataManager->GetProfilesToSuggest();

  [self.addressMediator reloadWithProfiles:profiles];
}

@end
