// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_data_util.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_country_selection_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/country_item.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

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

// If YES, a button is shown asking the user to migrate the account.
@property(nonatomic, assign) BOOL showMigrateToAccountButton;

@end

@implementation AutofillProfileEditCoordinator {
  std::unique_ptr<autofill::AutofillProfile> _autofillProfile;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                             profile:(const autofill::AutofillProfile&)profile
              migrateToAccountButton:(BOOL)showMigrateToAccountButton {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _autofillProfile = std::make_unique<autofill::AutofillProfile>(profile);
    _isCountrySelectorPresented = NO;
    _showMigrateToAccountButton = showMigrateToAccountButton;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // There is no personal data manager in OTR (incognito). Get the original
  // one so the user can edit the profile.
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          self.browser->GetProfile()->GetOriginalProfile());

  self.mediator = [[AutofillProfileEditMediator alloc]
         initWithDelegate:self
      personalDataManager:personalDataManager
          autofillProfile:_autofillProfile.get()
        isMigrationPrompt:NO];

  self.viewController = [[AutofillSettingsProfileEditTableViewController alloc]
                      initWithDelegate:self.mediator
      shouldShowMigrateToAccountButton:self.showMigrateToAccountButton
                             userEmail:[self userEmail]];
  self.sharedViewController = [[AutofillProfileEditTableViewController alloc]
      initWithDelegate:self.mediator
             userEmail:[self userEmail]
            controller:self.viewController
          settingsView:YES];
  self.mediator.consumer = self.sharedViewController;
  self.viewController.handler = self.sharedViewController;
  self.viewController.snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  if (self.openInEditMode) {
    [self.viewController editButtonPressed];
  }

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  _sharedViewController = nil;
  _viewController = nil;
  _mediator = nil;
}

#pragma mark - AutofillProfileEditMediatorDelegate

- (void)autofillEditProfileMediatorDidFinish:
    (AutofillProfileEditMediator*)mediator {
  if (self.isCountrySelectorPresented) {
    // Early return because the country selection view is presented, the
    // mediator and view controller should still live.
    return;
  }

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
  NOTREACHED_IN_MIGRATION();
}

#pragma mark - AutofillCountrySelectionTableViewControllerDelegate

- (void)didSelectCountry:(CountryItem*)selectedCountry {
  [self.baseNavigationController popViewControllerAnimated:YES];
  self.isCountrySelectorPresented = NO;
  [self.mediator didSelectCountry:selectedCountry];
}

#pragma mark - Private

- (NSString*)userEmail {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
  CHECK(authenticationService);
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  return identity ? identity.userEmail : nil;
}

@end
