// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_edit_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/browser/data_quality/autofill_data_util.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_country_selection_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/cells/country_item.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

@interface AutofillProfileEditCoordinator () <
    AutofillCountrySelectionTableViewControllerDelegate,
    AutofillProfileEditMediatorDelegate,
    UIAdaptivePresentationControllerDelegate>

@end

@implementation AutofillProfileEditCoordinator {
  std::unique_ptr<autofill::AutofillProfile> _autofillProfile;

  // The mediator for the view controller attatched to this coordinator.
  AutofillProfileEditMediator* _mediator;

  // The view controller attached to this coordinator.
  AutofillSettingsProfileEditTableViewController* _viewController;

  AutofillProfileEditTableViewController* _sharedViewController;

  // Default NO. Yes when the country selection view has been presented.
  BOOL _isCountrySelectorPresented;

  // If YES, a button is shown asking the user to migrate the account.
  BOOL _showMigrateToAccountButton;
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
          self.profile->GetOriginalProfile());

  _mediator = [[AutofillProfileEditMediator alloc]
         initWithDelegate:self
      personalDataManager:personalDataManager
          autofillProfile:_autofillProfile.get()
        isMigrationPrompt:NO
         addManualAddress:NO];

  _viewController = [[AutofillSettingsProfileEditTableViewController alloc]
                      initWithDelegate:_mediator
      shouldShowMigrateToAccountButton:_showMigrateToAccountButton
                             userEmail:[self userEmail]];
  _sharedViewController = [[AutofillProfileEditTableViewController alloc]
      initWithDelegate:_mediator
             userEmail:[self userEmail]
            controller:_viewController
          settingsView:YES
      addManualAddress:NO];
  _mediator.consumer = _sharedViewController;
  _viewController.handler = _sharedViewController;
  _viewController.snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  _viewController.applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  if (self.openInEditMode) {
    [_viewController editButtonPressed];
  }

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
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
  if (_isCountrySelectorPresented) {
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
                             settingsView:YES
              previousViewControllerTitle:nil];
  [self.baseNavigationController
      pushViewController:autofillCountrySelectionTableViewController
                animated:YES];
  _isCountrySelectorPresented = YES;
}

- (void)didSaveProfile {
  NOTREACHED();
}

#pragma mark - AutofillCountrySelectionTableViewControllerDelegate

- (void)didSelectCountry:(CountryItem*)selectedCountry {
  [self.baseNavigationController popViewControllerAnimated:YES];
  _isCountrySelectorPresented = NO;
  [_mediator didSelectCountry:selectedCountry];
}

- (void)dismissCountryViewController {
  NOTREACHED();
}

#pragma mark - Private

- (NSString*)userEmail {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  CHECK(authenticationService);
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  return identity ? identity.userEmail : nil;
}

@end
