// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_coordinator.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/credential_provider_extension/password_util.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_details_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_mediator.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_ui_handler.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/empty_credentials_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CredentialListCoordinator () <ConfirmationAlertActionHandler,
                                         CredentialListUIHandler,
                                         CredentialDetailsConsumerDelegate>

// Base view controller from where `viewController` is presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The view controller of this coordinator.
@property(nonatomic, strong) UINavigationController* viewController;

// The mediator of this coordinator.
@property(nonatomic, strong) CredentialListMediator* mediator;

// Interface for the persistent credential store.
@property(nonatomic, weak) id<CredentialStore> credentialStore;

// The extension context in which the credential list was started.
@property(nonatomic, weak) ASCredentialProviderExtensionContext* context;

// The service identifiers to prioritize in a match is found.
@property(nonatomic, strong)
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers;

// Coordinator that shows a view for the user to create a new password.
@property(nonatomic, strong) NewPasswordCoordinator* createPasswordCoordinator;

// Interface for `reauthenticationModule`, handling mostly the case when no
// hardware for authentication is available.
@property(nonatomic, weak) ReauthenticationHandler* reauthenticationHandler;

@end

@implementation CredentialListCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
               credentialStore:(id<CredentialStore>)credentialStore
                       context:(ASCredentialProviderExtensionContext*)context
            serviceIdentifiers:
                (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
       reauthenticationHandler:
           (ReauthenticationHandler*)reauthenticationHandler {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _context = context;
    _serviceIdentifiers = serviceIdentifiers;
    _credentialStore = credentialStore;
    _reauthenticationHandler = reauthenticationHandler;
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
                 context:self.context
      serviceIdentifiers:self.serviceIdentifiers];

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
      NSString* password =
          PasswordWithKeychainIdentifier(credential.keychainIdentifier);
      ASPasswordCredential* ASCredential =
          [ASPasswordCredential credentialWithUser:credential.user
                                          password:password];
      [self.context completeRequestWithSelectedCredential:ASCredential
                                        completionHandler:nil];
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
                         context:self.context
              serviceIdentifiers:self.serviceIdentifiers
             existingCredentials:self.credentialStore];
  [self.createPasswordCoordinator start];
}

#pragma mark - CredentialDetailsConsumerDelegate

- (void)navigationCancelButtonWasPressed:(UIButton*)button {
  NSError* error =
      [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                 code:ASExtensionErrorCodeUserCanceled
                             userInfo:nil];
  [self.context cancelRequestWithError:error];
}

- (void)unlockPasswordForCredential:(id<Credential>)credential
                  completionHandler:(void (^)(NSString*))completionHandler {
  [self reauthenticateIfNeededWithCompletionHandler:^(
            ReauthenticationResult result) {
    if (result != ReauthenticationResult::kFailure) {
      NSString* password =
          PasswordWithKeychainIdentifier(credential.keychainIdentifier);
      completionHandler(password);
    }
  }];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  // Finish the extension. There is no recovery from the empty credentials
  // state.
  NSError* error =
      [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                 code:ASExtensionErrorCodeUserCanceled
                             userInfo:nil];
  [self.context cancelRequestWithError:error];
}

- (void)confirmationAlertPrimaryAction {
  // No-op.
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
