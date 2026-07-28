// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_coordinator.h"

#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/not_fatal_until.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo_view_mediator.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_signin_promo_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_settings_coordinator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_coordinator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/shopping_coordinator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/travel_info_coordinator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace {
// Values correspond to YourSavedInfoDataCategory in enums.xml.
// LINT.IfChange(YourSavedInfoDataCategory)
enum class YourSavedInfoDataCategory {
  kPasswordManager = 0,
  kPayments = 1,
  kContactInfo = 2,
  kIdentityDocs = 3,
  kTravel = 4,
  kShopping = 5,
  kMaxValue = kShopping,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:YourSavedInfoDataCategory)
}  // namespace

@interface AutofillAndPasswordsCoordinator () <
    AutofillAndPasswordsTableViewControllerDelegate,
    AutofillSettingsCoordinatorDelegate,
    IdentityDocsCoordinatorDelegate,
    PasswordsCoordinatorDelegate,
    SigninPromoViewMediatorDelegate,
    TravelInfoCoordinatorDelegate,
    ShoppingCoordinatorDelegate>

@end

@implementation AutofillAndPasswordsCoordinator {
  AutofillAndPasswordsTableViewController* _viewController;
  AutofillAndPasswordsMediator* _mediator;
  PasswordsCoordinator* _passwordsCoordinator;
  IdentityDocsCoordinator* _identityDocsCoordinator;
  TravelInfoCoordinator* _travelInfoCoordinator;
  ShoppingCoordinator* _shoppingCoordinator;
  AutofillSettingsCoordinator* _autofillSettingsCoordinator;

  AutofillAndPasswordsSigninPromoMediator* _signinPromoMediator;
  SigninCoordinator* _signinCoordinator;

  autofill::autofill_metrics::AutofillSettingsReferrer _referrer;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        referrer:(autofill::autofill_metrics::
                                                      AutofillSettingsReferrer)
                                                     referrer {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _referrer = referrer;
  }
  return self;
}

- (void)start {
  _viewController = [[AutofillAndPasswordsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;
  _viewController.shouldShowLevelUpPaymentMethodsWalkthroughIPH =
      self.shouldShowLevelUpPaymentMethodsWalkthroughIPH;

  ProfileIOS* profile = self.browser->GetProfile();
  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile);

  _mediator = [[AutofillAndPasswordsMediator alloc]
      initWithUserPrefService:profile->GetPrefs()
            entityDataManager:entityDataManager];
  _mediator.consumer = _viewController;

  ProfileIOS* originalProfile = profile->GetOriginalProfile();
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(originalProfile);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(originalProfile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(originalProfile);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(originalProfile);

  _signinPromoMediator = [[AutofillAndPasswordsSigninPromoMediator alloc]
      initWithAccountManagerService:accountManagerService
                        authService:authService
                    identityManager:identityManager
                        prefService:originalProfile->GetPrefs()
                        syncService:syncService];
  _signinPromoMediator.consumer = _viewController;
  _signinPromoMediator.delegate = self;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
  base::RecordAction(base::UserMetricsAction("AutofillYourSavedInfoViewed"));
  base::UmaHistogramEnumeration(
      "Autofill.YourSavedInfoSettingsPage.VisitReferrer", _referrer);
}

- (void)stop {
  [self stopSigninCoordinator];

  _passwordsCoordinator.delegate = nil;
  [_passwordsCoordinator stop];
  _passwordsCoordinator = nil;

  _identityDocsCoordinator.delegate = nil;
  [_identityDocsCoordinator stop];
  _identityDocsCoordinator = nil;

  _travelInfoCoordinator.delegate = nil;
  [_travelInfoCoordinator stop];
  _travelInfoCoordinator = nil;

  _shoppingCoordinator.delegate = nil;
  [_shoppingCoordinator stop];
  _shoppingCoordinator = nil;

  _autofillSettingsCoordinator.delegate = nil;
  [_autofillSettingsCoordinator stop];
  _autofillSettingsCoordinator = nil;

  [_mediator disconnect];
  _mediator = nil;

  _signinPromoMediator.consumer = nil;
  _signinPromoMediator.delegate = nil;
  [_signinPromoMediator disconnect];
  _signinPromoMediator = nil;

  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - AutofillAndPasswordsTableViewControllerDelegate

- (void)autofillAndPasswordsTableViewControllerDidRemove:
    (AutofillAndPasswordsTableViewController*)controller {
  [self.delegate autofillAndPasswordsCoordinatorDidRemove:self];
}

- (void)autofillAndPasswordsTableViewControllerDidLoadContent:
    (AutofillAndPasswordsTableViewController*)controller {
  [_signinPromoMediator contentDidLoad];
}

- (void)autofillAndPasswordsTableViewControllerPromoProgressStateDidChange:
    (AutofillAndPasswordsTableViewController*)controller {
  [_signinPromoMediator updateSignInPromoVisibility];
}

- (void)autofillAndPasswordsTableViewControllerDidTapSigninPromoClose:
    (AutofillAndPasswordsTableViewController*)controller {
  [_signinPromoMediator updateSignInPromoVisibility];
}

- (void)autofillAndPasswordsTableViewControllerDidSelectPasswords:
    (AutofillAndPasswordsTableViewController*)controller {
  if (_passwordsCoordinator) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("Options_ShowPasswordManager"));
  base::UmaHistogramEnumeration("PasswordManager.ManagePasswordsReferrer",
                                password_manager::ManagePasswordsReferrer::
                                    kChromeSettingsAutofillAndPasswords);
  base::UmaHistogramEnumeration(
      "Autofill.YourSavedInfoSettingsPage.CategoryLinkClick",
      YourSavedInfoDataCategory::kPasswordManager);

  _passwordsCoordinator = [[PasswordsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  _passwordsCoordinator.delegate = self;
  [_passwordsCoordinator start];
}

- (void)autofillAndPasswordsTableViewControllerDidSelectAutofillCreditCard:
    (AutofillAndPasswordsTableViewController*)controller {
  if (self.baseNavigationController.topViewController != controller) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("AutofillCreditCardsViewed"));
  base::UmaHistogramEnumeration(
      "Autofill.YourSavedInfoSettingsPage.CategoryLinkClick",
      YourSavedInfoDataCategory::kPayments);
  AutofillCreditCardTableViewController* creditCardController =
      [[AutofillCreditCardTableViewController alloc]
          initWithBrowser:self.browser];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  creditCardController.sceneHandler =
      HandlerForProtocol(dispatcher, SceneCommands);
  creditCardController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  creditCardController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  creditCardController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  [self.baseNavigationController pushViewController:creditCardController
                                           animated:YES];
}

- (void)autofillAndPasswordsTableViewControllerDidSelectAutofillProfile:
    (AutofillAndPasswordsTableViewController*)controller {
  if (self.baseNavigationController.topViewController != controller) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("AutofillAddressesViewed"));
  base::UmaHistogramEnumeration(
      "Autofill.YourSavedInfoSettingsPage.CategoryLinkClick",
      YourSavedInfoDataCategory::kContactInfo);
  AutofillProfileTableViewController* profileController =
      [[AutofillProfileTableViewController alloc] initWithBrowser:self.browser];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  profileController.sceneHandler =
      HandlerForProtocol(dispatcher, SceneCommands);
  profileController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  profileController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  profileController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  [self.baseNavigationController pushViewController:profileController
                                           animated:YES];
}

- (void)autofillAndPasswordsTableViewControllerDidSelectIdentityDocs:
    (AutofillAndPasswordsTableViewController*)controller {
  if (_identityDocsCoordinator) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Autofill.YourSavedInfoSettingsPage.CategoryLinkClick",
      YourSavedInfoDataCategory::kIdentityDocs);

  _identityDocsCoordinator = [[IdentityDocsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  _identityDocsCoordinator.delegate = self;
  [_identityDocsCoordinator start];
}

- (void)autofillAndPasswordsTableViewControllerDidSelectTravelInfo:
    (AutofillAndPasswordsTableViewController*)controller {
  if (_travelInfoCoordinator) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Autofill.YourSavedInfoSettingsPage.CategoryLinkClick",
      YourSavedInfoDataCategory::kTravel);

  _travelInfoCoordinator = [[TravelInfoCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  _travelInfoCoordinator.delegate = self;
  [_travelInfoCoordinator start];
}

- (void)autofillAndPasswordsTableViewControllerDidSelectShopping:
    (AutofillAndPasswordsTableViewController*)controller {
  if (_shoppingCoordinator) {
    return;
  }

  _shoppingCoordinator = [[ShoppingCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  _shoppingCoordinator.delegate = self;
  [_shoppingCoordinator start];
}

- (void)autofillAndPasswordsTableViewControllerDidSelectAutofillSettings:
    (AutofillAndPasswordsTableViewController*)controller {
  if (_autofillSettingsCoordinator) {
    return;
  }

  base::UmaHistogramBoolean(
      "Autofill.YourSavedInfoSettingsPage.AutofillSettingsCategoryClick", true);

  _autofillSettingsCoordinator = [[AutofillSettingsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  _autofillSettingsCoordinator.delegate = self;
  [_autofillSettingsCoordinator start];
}

#pragma mark - AutofillSettingsCoordinatorDelegate

- (void)autofillSettingsCoordinatorDidRemove:
    (AutofillSettingsCoordinator*)coordinator {
  CHECK_EQ(_autofillSettingsCoordinator, coordinator);
  _autofillSettingsCoordinator.delegate = nil;
  [_autofillSettingsCoordinator stop];
  _autofillSettingsCoordinator = nil;
}

#pragma mark - IdentityDocsCoordinatorDelegate

- (void)identityDocsCoordinatorDidRemove:(IdentityDocsCoordinator*)coordinator {
  CHECK_EQ(_identityDocsCoordinator, coordinator);
  _identityDocsCoordinator.delegate = nil;
  [_identityDocsCoordinator stop];
  _identityDocsCoordinator = nil;
}

#pragma mark - PasswordsCoordinatorDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  [self.delegate dismissPasswordManagerAfterFailedReauthentication];
}

- (void)passwordsCoordinatorDidRemove:(PasswordsCoordinator*)coordinator {
  CHECK_EQ(_passwordsCoordinator, coordinator, base::NotFatalUntil::M151);
  _passwordsCoordinator.delegate = nil;
  [_passwordsCoordinator stop];
  _passwordsCoordinator = nil;
}

#pragma mark - TravelInfoCoordinatorDelegate

- (void)travelInfoCoordinatorDidRemove:(TravelInfoCoordinator*)coordinator {
  CHECK_EQ(_travelInfoCoordinator, coordinator);
  _travelInfoCoordinator.delegate = nil;
  [_travelInfoCoordinator stop];
  _travelInfoCoordinator = nil;
}

#pragma mark - ShoppingCoordinatorDelegate

- (void)shoppingCoordinatorDidRemove:(ShoppingCoordinator*)coordinator {
  CHECK_EQ(_shoppingCoordinator, coordinator);
  _shoppingCoordinator.delegate = nil;
  [_shoppingCoordinator stop];
  _shoppingCoordinator = nil;
}

#pragma mark - SigninPromoViewMediatorDelegate

- (void)showSignin:(SigninPromoViewMediator*)mediator
           command:(ShowSigninCommand*)command {
  if (_signinCoordinator.viewWillPersist) {
    return;
  }
  [self stopSigninCoordinator];
  __weak __typeof(self) weakSelf = self;
  [command addSigninCompletion:^(SigninCoordinator* coordinator,
                                 SigninCoordinatorResult result,
                                 id<SystemIdentity> identity) {
    [weakSelf signinDidCompleteWithCoordinator:coordinator result:result];
  }];
  _signinCoordinator = [SigninCoordinator
      signinCoordinatorWithCommand:command
                           browser:signin::GetRegularBrowser(self.browser)
                baseViewController:self.baseViewController];
  [_signinCoordinator start];
}

#pragma mark - Private

// Callback invoked when the sign-in flow completes.
- (void)signinDidCompleteWithCoordinator:(SigninCoordinator*)coordinator
                                  result:(SigninCoordinatorResult)result {
  if (_signinCoordinator != coordinator) {
    return;
  }
  [_signinPromoMediator signinDidCompleteWithResult:result];
  [self stopSigninCoordinator];
}

// Stops `_signinCoordinator` and sets it to nil.
- (void)stopSigninCoordinator {
  if (_signinCoordinator) {
    [_signinCoordinator stop];
    _signinCoordinator = nil;
  }
}

@end
