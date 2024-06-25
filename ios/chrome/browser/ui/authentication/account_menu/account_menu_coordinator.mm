// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"

@interface AccountMenuCoordinator () <
    AccountMenuViewControllerPresentationDelegate,
    UIAdaptivePresentationControllerDelegate,
    UINavigationControllerDelegate>
@end

@implementation AccountMenuCoordinator {
  AccountMenuViewController* _viewController;
  UINavigationController* _navigationController;
  AuthenticationService* _authenticationService;
  // Dismiss callback for account details view.
  SystemIdentityManager::DismissViewCallback
      _accountDetailsControllerDismissCallback;
  // The coordinators for the "Edit account list"
  AccountsCoordinator* _accountsCoordinator;
}

- (void)start {
  [super start];

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  _authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);

  _viewController = [[AccountMenuViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.delegate = self;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.delegate = self;

  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  if (idiom == UIUserInterfaceIdiomPad) {
    _navigationController.modalPresentationStyle = UIModalPresentationPopover;
    _navigationController.popoverPresentationController.sourceView =
        self.anchorView;
    _navigationController.popoverPresentationController
        .permittedArrowDirections = UIPopoverArrowDirectionUp;
  }

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  // TODO(crbug.com/336719423): Change condition to CHECK(_viewController). But
  // firt inform the parent coordinator at didTapClose that this view was
  // dismissed.
  if (!_viewController) {
    return;
  }
  if (!_accountDetailsControllerDismissCallback.is_null()) {
    std::move(_accountDetailsControllerDismissCallback).Run(/*animated=*/false);
  }
  [self stopAccountsCoordinator];
  [_navigationController dismissViewControllerAnimated:YES completion:nil];
  _authenticationService = nil;
  _viewController.delegate = nil;
  _viewController = nil;
  _navigationController.delegate = nil;
  _navigationController = nil;
  [super stop];
}

#pragma mark - AccountMenuPresentationDelegate

- (void)didTapManageYourGoogleAccount {
  __weak __typeof(self) weakSelf = self;
  _accountDetailsControllerDismissCallback =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentAccountDetailsController(
              _authenticationService->GetPrimaryIdentity(
                  signin::ConsentLevel::kSignin),
              _viewController,
              /*animated=*/YES,
              base::BindOnce(
                  [](__typeof(self) strongSelf) {
                    [strongSelf resetAccountDetailsControllerDismissCallback];
                  },
                  weakSelf));
}

- (void)didTapEditAccountList {
  _accountsCoordinator = [[AccountsCoordinator alloc]
      initWithBaseViewController:_navigationController
                         browser:self.browser
       closeSettingsOnAddAccount:NO];
  _accountsCoordinator.signoutDismissalByParentCoordinator = YES;
  [_accountsCoordinator start];
}

- (void)viewControllerWantsToBeClosed {
  [self stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate acountMenuCoordinatorShouldStop:self];
}

#pragma mark - Private

- (void)stopAccountsCoordinator {
  [_accountsCoordinator stop];
  _accountsCoordinator = nil;
}

- (void)resetAccountDetailsControllerDismissCallback {
  _accountDetailsControllerDismissCallback.Reset();
}

@end
