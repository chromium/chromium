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
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_mediator.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/identity_docs_coordinator.h"
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
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface AutofillAndPasswordsCoordinator () <
    AutofillAndPasswordsTableViewControllerDelegate,
    PasswordsCoordinatorDelegate,
    IdentityDocsCoordinatorDelegate,
    TravelInfoCoordinatorDelegate>
@end

@implementation AutofillAndPasswordsCoordinator {
  AutofillAndPasswordsTableViewController* _viewController;
  AutofillAndPasswordsMediator* _mediator;
  PasswordsCoordinator* _passwordsCoordinator;
  IdentityDocsCoordinator* _identityDocsCoordinator;
  TravelInfoCoordinator* _travelInfoCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _viewController = [[AutofillAndPasswordsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;

  autofill::EntityDataManager* entityDataManager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(
          self.browser->GetProfile());

  _mediator = [[AutofillAndPasswordsMediator alloc]
      initWithUserPrefService:self.browser->GetProfile()->GetPrefs()
            entityDataManager:entityDataManager];
  _mediator.consumer = _viewController;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _passwordsCoordinator.delegate = nil;
  [_passwordsCoordinator stop];
  _passwordsCoordinator = nil;

  _identityDocsCoordinator.delegate = nil;
  [_identityDocsCoordinator stop];
  _identityDocsCoordinator = nil;

  _travelInfoCoordinator.delegate = nil;
  [_travelInfoCoordinator stop];
  _travelInfoCoordinator = nil;

  [_mediator disconnect];
  _mediator = nil;

  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - AutofillAndPasswordsTableViewControllerDelegate

- (void)autofillAndPasswordsTableViewControllerDidRemove:
    (AutofillAndPasswordsTableViewController*)controller {
  [self.delegate autofillAndPasswordsCoordinatorDidRemove:self];
}

- (void)autofillAndPasswordsTableViewControllerDidSelectPasswords:
    (AutofillAndPasswordsTableViewController*)controller {
  if (_passwordsCoordinator) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("Options_ShowPasswordManager"));
  base::UmaHistogramEnumeration(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kChromeSettings);

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

  // TODO(crbug.com/500341282): Add missing metric.

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

  // TODO(crbug.com/500341282): Add missing metric.

  _travelInfoCoordinator = [[TravelInfoCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  _travelInfoCoordinator.delegate = self;
  [_travelInfoCoordinator start];
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

#pragma mark - IdentityDocsCoordinatorDelegate

- (void)identityDocsCoordinatorDidRemove:(IdentityDocsCoordinator*)coordinator {
  CHECK_EQ(_identityDocsCoordinator, coordinator);
  _identityDocsCoordinator.delegate = nil;
  [_identityDocsCoordinator stop];
  _identityDocsCoordinator = nil;
}

#pragma mark - TravelInfoCoordinatorDelegate

- (void)travelInfoCoordinatorDidRemove:(TravelInfoCoordinator*)coordinator {
  CHECK_EQ(_travelInfoCoordinator, coordinator);
  _travelInfoCoordinator.delegate = nil;
  [_travelInfoCoordinator stop];
  _travelInfoCoordinator = nil;
}

@end
