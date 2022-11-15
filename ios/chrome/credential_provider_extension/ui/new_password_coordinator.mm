// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/new_password_coordinator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/credential_provider_extension/password_util.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_mediator.h"
#import "ios/chrome/credential_provider_extension/ui/new_password_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewPasswordCoordinator () <NewPasswordViewControllerDelegate>

// Base view controller from where `viewController` is presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The view controller of this coordinator.
@property(nonatomic, strong) UINavigationController* viewController;

// The extension context for the credential provider.
@property(nonatomic, weak) ASCredentialProviderExtensionContext* context;

// The mediator for this coordinator.
@property(nonatomic, strong) NewPasswordMediator* mediator;

// The service identifiers this password is being created for.
@property(nonatomic, strong)
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers;

// The existing credentials to check for whether a new credential already
// exists.
@property(nonatomic, weak) id<CredentialStore> existingCredentials;

@end

@implementation NewPasswordCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
                       context:(ASCredentialProviderExtensionContext*)context
            serviceIdentifiers:
                (NSArray<ASCredentialServiceIdentifier*>*)serviceIdentifiers
           existingCredentials:(id<CredentialStore>)existingCredentials {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _context = context;
    _serviceIdentifiers = serviceIdentifiers;
    _existingCredentials = existingCredentials;
  }
  return self;
}

- (void)start {
  self.mediator = [[NewPasswordMediator alloc]
      initWithUserDefaults:app_group::GetGroupUserDefaults()
         serviceIdentifier:self.serviceIdentifiers.firstObject];
  self.mediator.existingCredentials = self.existingCredentials;
  self.mediator.context = self.context;

  NewPasswordViewController* newPasswordViewController =
      [[NewPasswordViewController alloc] init];
  newPasswordViewController.delegate = self;
  newPasswordViewController.credentialHandler = self.mediator;
  newPasswordViewController.navigationItem.prompt =
      PromptForServiceIdentifiers(self.serviceIdentifiers);

  self.mediator.uiHandler = newPasswordViewController;

  NSString* identifier = self.serviceIdentifiers.firstObject.identifier;
  NSURL* url = identifier ? [NSURL URLWithString:identifier] : nil;
  newPasswordViewController.currentHost = url ? url.host : @"";

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
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
}

@end
