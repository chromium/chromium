// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/coordinator/credential_import_coordinator.h"

#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/credential_exchange/coordinator/credential_import_mediator.h"
#import "ios/chrome/browser/credential_exchange/ui/credential_import_view_controller.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface CredentialImportCoordinator () <CredentialImportMediatorDelegate>
@end

@implementation CredentialImportCoordinator {
  // Handles interaction with the model.
  CredentialImportMediator* _mediator;

  // Token received from the OS during app launch needed to receive credentials.
  NSUUID* _UUID;

  // Presents the `_viewController` controlled by this coordinator.
  UINavigationController* _navigationController;

  // The view controller for the import flow.
  CredentialImportViewController* _viewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      UUID:(NSUUID*)UUID {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _UUID = UUID;
  }
  return self;
}

- (void)start {
  _viewController = [[CredentialImportViewController alloc] init];
  std::string email = IdentityManagerFactory::GetForProfile(self.profile)
                          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                          .email;
  _mediator = [[CredentialImportMediator alloc] initWithUUID:_UUID
                                                    delegate:self
                                                   userEmail:std::move(email)];
  _mediator.consumer = _viewController;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.navigationBarHidden = NO;
}

#pragma mark - CredentialImportMediatorDelegate

- (void)showImportScreen {
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

@end
