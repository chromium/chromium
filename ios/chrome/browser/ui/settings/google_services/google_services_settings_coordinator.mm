// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"

#import "base/apple/foundation_util.h"
#import "components/google/core/common/google_util.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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

@implementation GoogleServicesSettingsCoordinator {
  ParcelTrackingSettingsCoordinator* _parcelTrackingSettingsCoordinator;
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
  GoogleServicesSettingsViewController* viewController =
      [[GoogleServicesSettingsViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  viewController.presentationDelegate = self;
  viewController.forcedSigninEnabled =
      self.authService->GetServiceStatus() ==
      AuthenticationService::ServiceStatus::SigninForcedByPolicy;
  self.viewController = viewController;
  self.mediator = [[GoogleServicesSettingsMediator alloc]
      initWithIdentityManager:IdentityManagerFactory::GetForProfile(
                                  self.browser->GetProfile())
              userPrefService:self.browser->GetProfile()->GetPrefs()
             localPrefService:GetApplicationContext()->GetLocalState()];
  self.mediator.consumer = viewController;
  self.mediator.authService = self.authService;
  self.mediator.commandHandler = self;
  viewController.modelDelegate = self.mediator;
  viewController.serviceDelegate = self.mediator;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  viewController.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  viewController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  viewController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  viewController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  DCHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.signOutCoordinator stop];
  _signOutCoordinator = nil;
  [_parcelTrackingSettingsCoordinator stop];
  _parcelTrackingSettingsCoordinator = nil;
}

#pragma mark - Private

- (void)authenticationFlowDidComplete {
  DCHECK(self.authenticationFlow);
  self.authenticationFlow = nil;
  [self.googleServicesSettingsViewController allowUserInteraction];
}

#pragma mark - Properties

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForProfile(
      self.browser->GetProfile());
}

- (GoogleServicesSettingsViewController*)googleServicesSettingsViewController {
  return base::apple::ObjCCast<GoogleServicesSettingsViewController>(
      self.viewController);
}

- (signin::IdentityManager*)identityManager {
  return IdentityManagerFactory::GetForProfile(self.browser->GetProfile());
}

#pragma mark - GoogleServicesSettingsCommandHandler

- (void)showSignOutFromTargetRect:(CGRect)targetRect
                       completion:
                           (signin_ui::SignoutCompletionCallback)completion {
  DCHECK(completion);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.browser->GetProfile());
  BOOL isSyncConsentGiven =
      syncService &&
      syncService->GetUserSettings()->IsInitialSyncFeatureSetupComplete();
  BOOL shouldClearDataOnSignOut =
      self.authService->ShouldClearDataForSignedInPeriodOnSignOut();

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
  } else if (shouldClearDataOnSignOut) {
    // If `kIdentityDiscAccountMenu` is enabled, signing out may also cause tabs
    // to be closed, see `MainControllerAuthenticationServiceDelegate::
    //    ClearBrowsingDataForSignedinPeriod`.
    NSString* clearDataMessage =
        base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)
            ? l10n_util::GetNSString(
                  IDS_IOS_SIGNOUT_AND_DISALLOW_SIGNIN_CLOSES_TABS_AND_CLEARS_DATA_MESSAGE_WITH_MANAGED_ACCOUNT)
            : l10n_util::GetNSString(
                  IDS_IOS_SIGNOUT_AND_DISALLOW_SIGNIN_CLEARS_DATA_MESSAGE_WITH_MANAGED_ACCOUNT);
    self.signOutCoordinator.attributedMessage = [[NSAttributedString alloc]
        initWithString:clearDataMessage
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
                  // TODO(crbug.com/40066949): Simplify once kSync becomes
                  // unreachable or is deleted from the codebase. See
                  // ConsentLevel::kSync documentation for details.
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

- (void)showParcelTrackingSettingsPage {
  _parcelTrackingSettingsCoordinator =
      [[ParcelTrackingSettingsCoordinator alloc]
          initWithBaseNavigationController:_baseNavigationController
                                   browser:self.browser];
  [_parcelTrackingSettingsCoordinator start];
}

// Displays the option to keep or clear data for a syncing user.
- (void)showDataRetentionOptionsWithTargetRect:(CGRect)targetRect
                                    completion:
                                        (signin_ui::SignoutCompletionCallback)
                                            completion {
  DCHECK(completion);
  self.signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            rect:targetRect
                            view:self.viewController.view
        forceSnackbarOverToolbar:NO
                      withSource:signin_metrics::ProfileSignout::
                                     kUserClickedSignoutSettings];
  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  self.signoutActionSheetCoordinator.delegate = self;
  self.signoutActionSheetCoordinator.signoutCompletion = ^(BOOL success) {
    if (completion)
      completion(success);
    [weakSelf.signoutActionSheetCoordinator stop];
    weakSelf.signoutActionSheetCoordinator = nil;
  };
  [self.signoutActionSheetCoordinator start];
}

// Signs the user out of Chrome, only clears data for managed accounts.
- (void)signOutWithCompletion:(signin_ui::SignoutCompletionCallback)completion {
  DCHECK(completion);
  [self.googleServicesSettingsViewController preventUserInteraction];
  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  self.authService->SignOut(
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
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
