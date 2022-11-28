// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"

#import "base/mac/foundation_util.h"
#import "components/google/core/common/google_util.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/driver/sync_service_utils.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface GoogleServicesSettingsCoordinator () <
    GoogleServicesSettingsCommandHandler,
    GoogleServicesSettingsViewControllerPresentationDelegate,
    SignoutActionSheetCoordinatorDelegate>

// Google services settings mediator.
@property(nonatomic, strong) GoogleServicesSettingsMediator* mediator;
// Returns the authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;
// Manages the authentication flow for a given identity.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// Manages user's Google identities.
@property(nonatomic, assign, readonly) signin::IdentityManager* identityManager;
// View controller presented by this coordinator.
@property(nonatomic, strong, readonly)
    GoogleServicesSettingsViewController* googleServicesSettingsViewController;
// Action sheets that provides options for sign out.
@property(nonatomic, strong) ActionSheetCoordinator* signOutCoordinator;
@property(nonatomic, strong)
    SignoutActionSheetCoordinator* signoutActionSheetCoordinator;
@end

@implementation GoogleServicesSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ([super initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  GoogleServicesSettingsViewController* viewController =
      [[GoogleServicesSettingsViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  viewController.presentationDelegate = self;
  viewController.forcedSigninEnabled =
      self.authService->GetServiceStatus() ==
      AuthenticationService::ServiceStatus::SigninForcedByPolicy;
  self.viewController = viewController;
  self.mediator = [[GoogleServicesSettingsMediator alloc]
      initWithUserPrefService:self.browser->GetBrowserState()->GetPrefs()
             localPrefService:GetApplicationContext()->GetLocalState()];
  self.mediator.consumer = viewController;
  self.mediator.authService = self.authService;
  self.mediator.identityManager = IdentityManagerFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.mediator.commandHandler = self;
  viewController.modelDelegate = self.mediator;
  viewController.serviceDelegate = self.mediator;
  viewController.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
  DCHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

#pragma mark - Private

- (void)authenticationFlowDidComplete {
  DCHECK(self.authenticationFlow);
  self.authenticationFlow = nil;
  [self.googleServicesSettingsViewController allowUserInteraction];
}

#pragma mark - Properties

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

- (GoogleServicesSettingsViewController*)googleServicesSettingsViewController {
  return base::mac::ObjCCast<GoogleServicesSettingsViewController>(
      self.viewController);
}

- (signin::IdentityManager*)identityManager {
  return IdentityManagerFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

#pragma mark - GoogleServicesSettingsCommandHandler

- (void)showSignOutFromTargetRect:(CGRect)targetRect
                       completion:(signin_ui::CompletionCallback)completion {
  DCHECK(completion);
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  BOOL isSyncConsentGiven =
      syncSetupService && syncSetupService->IsFirstSetupComplete();

  self.signOutCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:nil
                         message:nil
                            rect:targetRect
                            view:self.viewController.view];
  // Because setting `title` to nil automatically forces the title-style text on
  // `message` in the UIAlertController, the attributed message below
  // specifically denotes the font style to apply.
  if (isSyncConsentGiven) {
    self.signOutCoordinator.attributedMessage = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(
                           IDS_IOS_SIGNOUT_DIALOG_MESSAGE_WITH_SYNC)
            attributes:@{
              NSFontAttributeName :
                  [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
            }];
    [self.signOutCoordinator updateAttributedText];
  }

  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  [self.signOutCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)
                action:^{
                  if (!weakSelf) {
                    return;
                  }
                  // Provide additional data retention options if the user is
                  // syncing their data.
                  if (weakSelf.identityManager->HasPrimaryAccount(
                          signin::ConsentLevel::kSync)) {
                    [weakSelf
                        showDataRetentionOptionsWithTargetRect:targetRect
                                                    completion:completion];
                    return;
                  }
                  [weakSelf signOutWithCompletion:completion];
                }
                 style:UIAlertActionStyleDestructive];

  [self.signOutCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  weakSelf.signOutCoordinator = nil;
                  completion(NO);
                }
                 style:UIAlertActionStyleCancel];
  [self.signOutCoordinator start];
}

// Displays the option to keep or clear data for a syncing user.
- (void)showDataRetentionOptionsWithTargetRect:(CGRect)targetRect
                                    completion:(signin_ui::CompletionCallback)
                                                   completion {
  DCHECK(completion);
  self.signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            rect:targetRect
                            view:self.viewController.view
                      withSource:signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS];
  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  self.signoutActionSheetCoordinator.delegate = self;
  self.signoutActionSheetCoordinator.completion = ^(BOOL success) {
    if (completion)
      completion(success);
    [weakSelf.signoutActionSheetCoordinator stop];
    weakSelf.signoutActionSheetCoordinator = nil;
  };
  [self.signoutActionSheetCoordinator start];
}

// Signs the user out of Chrome, only clears data for managed accounts.
- (void)signOutWithCompletion:(signin_ui::CompletionCallback)completion {
  DCHECK(completion);
  [self.googleServicesSettingsViewController preventUserInteraction];
  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  self.authService->SignOut(
      signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
      /*force_clear_browsing_data=*/NO, ^{
        if (!weakSelf) {
          return;
        }
        [weakSelf.googleServicesSettingsViewController allowUserInteraction];
        completion(YES);
      });
}

#pragma mark - GoogleServicesSettingsViewControllerPresentationDelegate

- (void)googleServicesSettingsViewControllerDidRemove:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate googleServicesSettingsCoordinatorDidRemove:self];
}

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [self.googleServicesSettingsViewController preventUserInteraction];
}

- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [self.googleServicesSettingsViewController allowUserInteraction];
}

@end
