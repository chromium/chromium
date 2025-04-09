// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_coordinator.h"

#import "base/apple/foundation_util.h"
#import "components/google/core/common/google_util.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_command_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
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
// View controller presented by this coordinator.
@property(nonatomic, strong, readonly)
    GoogleServicesSettingsViewController* googleServicesSettingsViewController;
// Action sheets that provides options for sign out.
@property(nonatomic, strong) ActionSheetCoordinator* signOutCoordinator;
@end

@implementation GoogleServicesSettingsCoordinator

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
                                  self.profile)
              userPrefService:self.profile->GetPrefs()
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
  [self.mediator disconnect];
  _signOutCoordinator = nil;
  [self dismissSignoutCoordinator];
}

#pragma mark - Private

// Callback for the sign-out confirmation button.
// Dismisses the alert coordinator and sign-out.
- (void)
    onSignoutConfirmationTappedWithTargetRect:(CGRect)targetRect
                                   completion:
                                       (signin_ui::SignoutCompletionCallback)
                                           completion {
  [self dismissSignoutCoordinator];
  [self signOutWithCompletion:completion];
}

- (void)dismissSignoutCoordinator {
  [self.signOutCoordinator stop];
  self.signOutCoordinator = nil;
}

#pragma mark - Properties

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForProfile(self.profile);
}

- (GoogleServicesSettingsViewController*)googleServicesSettingsViewController {
  return base::apple::ObjCCast<GoogleServicesSettingsViewController>(
      self.viewController);
}

#pragma mark - GoogleServicesSettingsCommandHandler

- (void)showSignOutFromTargetRect:(CGRect)targetRect
                       completion:
                           (signin_ui::SignoutCompletionCallback)completion {
  DCHECK(completion);
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
  if (shouldClearDataOnSignOut) {
    // If `kIdentityDiscAccountMenu` is enabled, signing out may also cause tabs
    // to be closed, see `MainControllerAuthenticationServiceDelegate::
    //    ClearBrowsingDataForSignedinPeriod`.
    NSString* clearDataMessage =
        IsIdentityDiscAccountMenuEnabled()
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
                  [weakSelf
                      onSignoutConfirmationTappedWithTargetRect:targetRect
                                                     completion:completion];
                }
                 style:UIAlertActionStyleDestructive];

  [self.signOutCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                     action:^{
                                       [weakSelf dismissSignoutCoordinator];
                                       completion(NO, nil);
                                     }
                                      style:UIAlertActionStyleCancel];
  [self.signOutCoordinator start];
}

// Signs the user out of Chrome, only clears data for managed accounts.
- (void)signOutWithCompletion:(signin_ui::SignoutCompletionCallback)completion {
  DCHECK(completion);
  [self.googleServicesSettingsViewController preventUserInteraction];
  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  signin::ProfileSignoutRequest(
      signin_metrics::ProfileSignout::kUserDisabledAllowChromeSignIn)
      .SetCompletionCallback(base::BindOnce(^(SceneState* scene_state) {
        [weakSelf.googleServicesSettingsViewController allowUserInteraction];
        completion(YES, scene_state);
      }))
      .Run(self.browser);
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
