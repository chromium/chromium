// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/credential_provider_extension/passkey_util.h"
#import "ios/chrome/credential_provider_extension/password_util.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_mediator.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_ui_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"
#import "ios/chrome/credential_provider_extension/ui/empty_credentials_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_coordinator.h"

@interface CredentialListCoordinator () <ConfirmationAlertActionHandler,
                                         CredentialListUIHandler,
                                         CredentialDetailsConsumerDelegate,
                                         NewPasswordCoordinatorDelegate>

// Base view controller from where `viewController` is presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The view controller of this coordinator.
@property(nonatomic, strong) UINavigationController* viewController;

// The mediator of this coordinator.
@property(nonatomic, strong) CredentialListMediator* mediator;

// Interface for the persistent credential store.
@property(nonatomic, weak) id<CredentialStore> credentialStore;

// The service identifiers to prioritize in a match is found.
@property(nonatomic, strong)
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers;

// Information about a passkey credential request.
@property(nonatomic, strong)
    ASPasskeyCredentialRequestParameters* requestParameters API_AVAILABLE(
        ios(17.0));

// Coordinator that shows a view for the user to create a new password.
@property(nonatomic, strong) NewPasswordCoordinator* createPasswordCoordinator;

// Interface for `reauthenticationModule`, handling mostly the case when no
// hardware for authentication is available.
@property(nonatomic, weak) ReauthenticationHandler* reauthenticationHandler;

// The handler to use when a credential is selected.
@property(nonatomic, weak) id<CredentialResponseHandler>
    credentialResponseHandler;

@end

@implementation CredentialListCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
               credentialStore:(id<CredentialStore>)credentialStore
            serviceIdentifiers:
                (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
       reauthenticationHandler:(ReauthenticationHandler*)reauthenticationHandler
     credentialResponseHandler:
         (id<CredentialResponseHandler>)credentialResponseHandler {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _serviceIdentifiers = serviceIdentifiers;
    _credentialStore = credentialStore;
    _reauthenticationHandler = reauthenticationHandler;
    _credentialResponseHandler = credentialResponseHandler;
  }
  return self;
}

- (void)start {
  CredentialListViewController* credentialListViewController =
      [[CredentialListViewController alloc] init];
  self.mediator = [[CredentialListMediator alloc]
               initWithConsumer:credentialListViewController
                      UIHandler:self
                credentialStore:self.credentialStore
             serviceIdentifiers:self.serviceIdentifiers
      credentialResponseHandler:self.credentialResponseHandler];

  self.viewController = [[UINavigationController alloc]
      initWithRootViewController:credentialListViewController];
  self.viewController.modalPresentationStyle =
      UIModalPresentationCurrentContext;
  [self.baseViewController presentViewController:self.viewController
                                        animated:NO
                                      completion:nil];
  [self.mediator fetchCredentials];
}

- (void)stop {
  if (self.createPasswordCoordinator) {
    [self.createPasswordCoordinator stop];
    self.createPasswordCoordinator = nil;
  }
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - CredentialListUIHandler

- (void)showEmptyCredentials {
  EmptyCredentialsViewController* emptyCredentialsViewController =
      [[EmptyCredentialsViewController alloc] init];
  emptyCredentialsViewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  emptyCredentialsViewController.actionHandler = self;
  [self.viewController presentViewController:emptyCredentialsViewController
                                    animated:YES
                                  completion:nil];
}

- (void)userSelectedCredential:(id<Credential>)credential {
  [self reauthenticateIfNeededWithCompletionHandler:^(
            ReauthenticationResult result) {
    if (result != ReauthenticationResult::kFailure) {
      if (!credential.isPasskey) {
        ASPasswordCredential* passwordCredential =
            [ASPasswordCredential credentialWithUser:credential.username
                                            password:credential.password];
        [self.credentialResponseHandler
            userSelectedPassword:passwordCredential];
      } else if (@available(iOS 17.0, *)) {
        // TODO(crbug.com/330355124): Handle
        // self.requestParameters.userVerificationPreference.

        [self.credentialResponseHandler
            userSelectedPasskey:credential
                 clientDataHash:self.requestParameters.clientDataHash
             allowedCredentials:self.allowedCredentials
                     allowRetry:YES];
      }
    }
  }];
}

- (void)showDetailsForCredential:(id<Credential>)credential {
  CredentialDetailsViewController* detailsViewController =
      [[CredentialDetailsViewController alloc] init];
  detailsViewController.delegate = self;
  [detailsViewController presentCredential:credential];

  [self.viewController pushViewController:detailsViewController animated:YES];
}

- (void)showCreateNewPasswordUI {
  self.createPasswordCoordinator = [[NewPasswordCoordinator alloc]
      initWithBaseViewController:self.viewController
              serviceIdentifiers:self.serviceIdentifiers
             existingCredentials:self.credentialStore
       credentialResponseHandler:self.credentialResponseHandler];
  self.createPasswordCoordinator.delegate = self;
  [self.createPasswordCoordinator start];
}

- (NSArray<NSData*>*)allowedCredentials {
  if (@available(iOS 17.0, *)) {
    return self.requestParameters.allowedCredentials;
  } else {
    return nil;
  }
}

- (BOOL)isRequestingPasskey {
  if (@available(iOS 17.0, *)) {
    return self.requestParameters != nil;
  } else {
    return NO;
  }
}

#pragma mark - CredentialDetailsConsumerDelegate

- (void)navigationCancelButtonWasPressed:(UIButton*)button {
  [self.credentialResponseHandler
      userCancelledRequestWithErrorCode:ASExtensionErrorCodeUserCanceled];
}

- (void)unlockPasswordForCredential:(id<Credential>)credential
                  completionHandler:(void (^)(NSString*))completionHandler {
  [self reauthenticateIfNeededWithCompletionHandler:^(
            ReauthenticationResult result) {
    if (result != ReauthenticationResult::kFailure) {
      completionHandler(credential.password);
    }
  }];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  // Finish the extension. There is no recovery from the empty credentials
  // state.
  [self.credentialResponseHandler
      userCancelledRequestWithErrorCode:ASExtensionErrorCodeUserCanceled];
}

- (void)confirmationAlertPrimaryAction {
  // No-op.
}

#pragma mark - NewPasswordCoordinatorDelegate

- (void)dismissNewPasswordCoordinator:
    (NewPasswordCoordinator*)newPasswordCoordinator {
  [self.createPasswordCoordinator stop];
  self.createPasswordCoordinator = nil;
}

#pragma mark - Private

// Asks user for hardware reauthentication if needed.
- (void)reauthenticateIfNeededWithCompletionHandler:
    (void (^)(ReauthenticationResult))completionHandler {
  [self.reauthenticationHandler
      verifyUserWithCompletionHandler:completionHandler
      presentReminderOnViewController:self.viewController];
}

@end
