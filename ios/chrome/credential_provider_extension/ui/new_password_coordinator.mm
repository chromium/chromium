// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_coordinator.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_coordinator+Testing.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/credential_provider_extension/password_util.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_mediator.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/ui_util.h"

@interface NewPasswordCoordinator () <NewPasswordViewControllerDelegate>

// Base view controller from where `viewController` is presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The view controller of this coordinator.
@property(nonatomic, strong) UINavigationController* viewController;

// The mediator for this coordinator.
@property(nonatomic, strong) NewPasswordMediator* mediator;

// The service identifiers this password is being created for.
@property(nonatomic, strong)
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers;

// The existing credentials to check for whether a new credential already
// exists.
@property(nonatomic, weak) id<CredentialStore> existingCredentials;

// The handler to use when a credential is selected.
@property(nonatomic, weak) id<CredentialResponseHandler>
    credentialResponseHandler;

@end

@implementation NewPasswordCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
            serviceIdentifiers:
                (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
           existingCredentials:(id<CredentialStore>)existingCredentials
     credentialResponseHandler:
         (id<CredentialResponseHandler>)credentialResponseHandler {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _serviceIdentifiers = serviceIdentifiers;
    _existingCredentials = existingCredentials;
    _credentialResponseHandler = credentialResponseHandler;
  }
  return self;
}

- (void)start {
  self.mediator = [[NewPasswordMediator alloc]
      initWithUserDefaults:app_group::GetGroupUserDefaults()
         serviceIdentifier:self.serviceIdentifiers.firstObject];
  self.mediator.existingCredentials = self.existingCredentials;
  self.mediator.credentialResponseHandler = self.credentialResponseHandler;

  NewPasswordViewController* newPasswordViewController =
      [[NewPasswordViewController alloc] init];
  newPasswordViewController.delegate = self;
  newPasswordViewController.credentialHandler = self.mediator;
  newPasswordViewController.navigationItem.prompt =
      PromptForServiceIdentifiers(self.serviceIdentifiers);

  self.mediator.uiHandler = newPasswordViewController;

  NSString* identifier = self.serviceIdentifiers.firstObject.identifier;
  // `url` can be nil when it's malformed or nil.
  // > "Linked on or after iOS 17, this method parses URLString according to RFC
  // 3986. Linked before iOS 17, this method parses URLString according to RFCs
  // 1738 and 1808."
  // https://developer.apple.com/documentation/foundation/nsurl/1572047-urlwithstring
  NSURL* url = identifier ? [NSURL URLWithString:identifier] : nil;
  // `url.host` can be nil when it does not conform to RFC 1808.
  // https://developer.apple.com/documentation/foundation/nsurl/1413640-host
  newPasswordViewController.currentHost = url && url.host ? url.host : @"";

  self.viewController = [[UINavigationController alloc]
      initWithRootViewController:newPasswordViewController];
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
}

#pragma mark - NewPasswordViewControllerDelegate

- (void)navigationCancelButtonWasPressedInNewPasswordViewController:
    (NewPasswordViewController*)viewController {
  [self.delegate dismissNewPasswordCoordinator:self];
}

@end
