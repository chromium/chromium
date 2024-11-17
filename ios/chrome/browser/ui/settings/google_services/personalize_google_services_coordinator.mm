// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_coordinator.h"

#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_view_controller.h"

using DismissViewCallback = SystemIdentityManager::DismissViewCallback;

@interface PersonalizeGoogleServicesCoordinator () <
    PersonalizeGoogleServicesViewControllerPresentationDelegate,
    PersonalizeGoogleServicesCommandHandler>
@end

@implementation PersonalizeGoogleServicesCoordinator {
  PersonalizeGoogleServicesViewController* _viewController;
  // Dismiss callback for Web and app setting details view.
  DismissViewCallback _dismissWebAndAppSettingDetailsCallback;
  // Dismiss callback for Linked Google services settings details view.
  DismissViewCallback _dismissLinkedGoogleServicesSettingsDetailsCallback;
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
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
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
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  _dismissLinkedGoogleServicesSettingsDetailsCallback =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentLinkedServicesSettingsDetailsController(
              identity, _viewController, /*animated=*/YES, base::DoNothing());
}

@end
