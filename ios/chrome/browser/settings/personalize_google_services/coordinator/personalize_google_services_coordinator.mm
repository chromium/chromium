// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/personalize_google_services/coordinator/personalize_google_services_coordinator.h"

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/user_metrics.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/signin/reauth/coordinator/signin_reauth_coordinator.h"
#import "ios/chrome/browser/settings/personalize_google_services/ui/personalize_google_services_command_handler.h"
#import "ios/chrome/browser/settings/personalize_google_services/ui/personalize_google_services_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

using DismissViewCallback = SystemIdentityManager::DismissViewCallback;

namespace {

enum class ActionAfterReauth {
  // Do nothing.
  kNone,
  // Open the web app activity dialog.
  kOpenWebAppAcvityDialog,
  // Open the linked google service dialog.
  kOpenLinkedGoogleServicesDialog,
};

}  // namespace

@interface PersonalizeGoogleServicesCoordinator () <
    PersonalizeGoogleServicesViewControllerPresentationDelegate,
    PersonalizeGoogleServicesCommandHandler,
    SigninReauthCoordinatorDelegate>
@end

@implementation PersonalizeGoogleServicesCoordinator {
  PersonalizeGoogleServicesViewController* _viewController;
  // Dismiss callback for Web and app setting details view.
  DismissViewCallback _dismissWebAndAppSettingDetailsCallback;
  // Dismiss callback for Linked Google services settings details view.
  DismissViewCallback _dismissLinkedGoogleServicesSettingsDetailsCallback;
  SigninReauthCoordinator* _reauthCoordinator;
  // What to do after a successful signin reauth.
  ActionAfterReauth _actionAfterReauth;
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
  _viewController = [[PersonalizeGoogleServicesViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.presentationDelegate = self;
  _viewController.handler = self;

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;

  [self stopReauthCoordinator];

  if (!_dismissLinkedGoogleServicesSettingsDetailsCallback.is_null()) {
    std::move(_dismissLinkedGoogleServicesSettingsDetailsCallback)
        .Run(/*animated*/ false);
  }

  if (!_dismissWebAndAppSettingDetailsCallback.is_null()) {
    std::move(_dismissWebAndAppSettingDetailsCallback).Run(/*animated*/ false);
  }
}

#pragma mark - PersonalizeGoogleServicesViewControllerPresentationDelegate

- (void)personalizeGoogleServicesViewcontrollerDidRemove:
    (PersonalizeGoogleServicesViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate personalizeGoogleServicesCoordinatorWasRemoved:self];
}

#pragma mark - PersonalizeGoogleServicesCommandHandler

- (void)openWebAppActivityDialog {
  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_GoogleActivityControlsClicked"));
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!identity.hasValidAuth) {
    [self openReauthCoordinatorWithAction:ActionAfterReauth::
                                              kOpenWebAppAcvityDialog];
    return;
  }

  _dismissWebAndAppSettingDetailsCallback =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentWebAndAppSettingDetailsController(identity, _viewController,
                                                     /*animated=*/YES,
                                                     base::DoNothing());
}

- (void)openLinkedGoogleServicesDialog {
  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_LinkedGoogleServicesClicked"));
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!identity.hasValidAuth) {
    [self openReauthCoordinatorWithAction:ActionAfterReauth::
                                              kOpenLinkedGoogleServicesDialog];
    return;
  }

  _dismissLinkedGoogleServicesSettingsDetailsCallback =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentLinkedServicesSettingsDetailsController(
              identity, _viewController, /*animated=*/YES, base::DoNothing());
}

#pragma mark - Private

- (void)stopReauthCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

- (void)openReauthCoordinatorWithAction:(ActionAfterReauth)action {
  if (_reauthCoordinator.viewWillPersist) {
    return;
  }
  [self stopReauthCoordinator];
  _actionAfterReauth = action;

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);
  CoreAccountInfo account =
      identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (account.IsEmpty()) {
    // A sign-out was triggered in the meantime, don't do anything.
    return;
  }
  _reauthCoordinator = [[SigninReauthCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                         account:account
               reauthAccessPoint:signin_metrics::ReauthAccessPoint::
                                     kAccountSettings];
  _reauthCoordinator.delegate = self;
  [_reauthCoordinator start];
}

#pragma mark - SigninReauthCoordinatorDelegate

- (void)reauthFinishedWithResult:(ReauthResult)result
                          gaiaID:(const GaiaId*)gaiaID {
  [self stopReauthCoordinator];
  if (result != ReauthResult::kSuccess) {
    return;
  }
  switch (_actionAfterReauth) {
    case ActionAfterReauth::kOpenWebAppAcvityDialog:
      [self openWebAppActivityDialog];
      break;
    case ActionAfterReauth::kOpenLinkedGoogleServicesDialog:
      [self openLinkedGoogleServicesDialog];
      break;
    case ActionAfterReauth::kNone:
      break;
  }
}

@end
