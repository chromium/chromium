// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_data_util.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/autofill/autofill_country_selection_table_view_controller.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/ui/autofill/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/autofill/cells/country_item.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_profile_edit_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AutofillProfileEditCoordinator () <
    AutofillCountrySelectionTableViewControllerDelegate,
    AutofillProfileEditMediatorDelegate,
    UIAdaptivePresentationControllerDelegate>

// The view controller attached to this coordinator.
@property(nonatomic, strong)
    AutofillSettingsProfileEditTableViewController* viewController;

@property(nonatomic, strong)
    AutofillProfileEditTableViewController* sharedViewController;

// The mediator for the view controller attatched to this coordinator.
@property(nonatomic, strong) AutofillProfileEditMediator* mediator;

// Default NO. Yes when the country selection view has been presented.
@property(nonatomic, assign) BOOL isCountrySelectorPresented;

@end

@implementation AutofillProfileEditCoordinator {
  autofill::AutofillProfile _autofillProfile;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                             profile:(const autofill::AutofillProfile&)profile {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _autofillProfile = profile;
    _isCountrySelectorPresented = NO;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // There is no personal data manager in OTR (incognito). Get the original
  // one so the user can edit the profile.
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState()->GetOriginalChromeBrowserState());

  std::string countryCode = autofill::data_util::GetCountryCodeWithFallback(
      _autofillProfile, GetApplicationContext()->GetApplicationLocale());

  self.mediator = [[AutofillProfileEditMediator alloc]
         initWithDelegate:self
      personalDataManager:personalDataManager
          autofillProfile:&_autofillProfile
              countryCode:base::SysUTF8ToNSString(countryCode)
        isMigrationPrompt:NO];

  self.viewController = [[AutofillSettingsProfileEditTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.sharedViewController = [[AutofillProfileEditTableViewController alloc]
      initWithDelegate:self.mediator
             userEmail:[self syncingUserEmail]
            controller:self.viewController
          settingsView:YES];
  self.mediator.consumer = self.sharedViewController;
  self.viewController.handler = self.sharedViewController;

  DCHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  // Never called because `self.viewController` is pushed out of the stack via
  // the navigation bar back button.
}

#pragma mark - AutofillProfileEditMediatorDelegate

- (void)autofillEditProfileMediatorDidFinish:
    (AutofillProfileEditMediator*)mediator {
  if (self.isCountrySelectorPresented) {
    // Early return because the country selection view is presented, the
    // mediator and view controller should still live.
    return;
  }

  self.sharedViewController = nil;
  self.viewController = nil;
  self.mediator = nil;
  [self.delegate
      autofillProfileEditCoordinatorTableViewControllerDidFinish:self];
}

- (void)willSelectCountryWithCurrentlySelectedCountry:(NSString*)country
                                          countryList:(NSArray<CountryItem*>*)
                                                          allCountries {
  AutofillCountrySelectionTableViewController*
      autofillCountrySelectionTableViewController =
          [[AutofillCountrySelectionTableViewController alloc]
              initWithDelegate:self
               selectedCountry:country
                  allCountries:allCountries
                  settingsView:YES];
  [self.baseNavigationController
      pushViewController:autofillCountrySelectionTableViewController
                animated:YES];
  self.isCountrySelectorPresented = YES;
}

- (void)didSaveProfile {
  NOTREACHED();
}

#pragma mark - AutofillCountrySelectionTableViewControllerDelegate

- (void)didSelectCountry:(CountryItem*)selectedCountry {
  [self.baseNavigationController popViewControllerAnimated:YES];
  self.isCountrySelectorPresented = NO;
  [self.mediator didSelectCountry:selectedCountry];
}

#pragma mark - Private

- (NSString*)syncingUserEmail {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  DCHECK(authenticationService);
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSync);
  if (identity) {
    SyncSetupService* syncSetupService =
        SyncSetupServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    if (syncSetupService->IsDataTypeActive(syncer::AUTOFILL)) {
      return identity.userEmail;
    }
  }
  return nil;
}

@end
