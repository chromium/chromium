// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_manager.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/signin/system_identity_interaction_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AddAccountSigninManager ()

// Presenting view controller.
@property(nonatomic, strong) UIViewController* baseViewController;
// The coordinator's manager that handles interactions to add identities.
@property(nonatomic, strong) id<SystemIdentityInteractionManager>
    identityInteractionManager;
// Indicates that the add account sign-in flow was interrupted.
@property(nonatomic, readwrite) BOOL signinInterrupted;

@end

@implementation AddAccountSigninManager {
  // YES if the add account if done, and the delegate has been called.
  BOOL _addAccountFlowDone;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                identityInteractionManager:
                    (id<SystemIdentityInteractionManager>)
                        identityInteractionManager {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _identityInteractionManager = identityInteractionManager;
  }
  return self;
}

- (void)showSigninWithDefaultUserEmail:(NSString*)defaultUserEmail {
  DCHECK(!_addAccountFlowDone);
  DCHECK(self.identityInteractionManager);
  __weak AddAccountSigninManager* weakSelf = self;
  [self.identityInteractionManager
      startAuthActivityWithViewController:self.baseViewController
                                userEmail:defaultUserEmail
                               completion:^(id<SystemIdentity> identity,
                                            NSError* error) {
                                 [weakSelf
                                     operationCompletedWithIdentity:identity
                                                              error:error];
                               }];
}

- (void)interruptAddAccountAnimated:(BOOL)animated
                         completion:(ProceduralBlock)completion {
  self.signinInterrupted = YES;
  __weak __typeof(self) weakSelf = self;
  [self.identityInteractionManager
      cancelAuthActivityAnimated:animated
                      completion:^() {
                        // If `identityInteractionManager` completion callback
                        // has not been called yet, the add account needs to be
                        // fully done by calling:
                        // `operationCompletedWithIdentity:error:`, before
                        // calling `completion` See crbug.com/1227658.
                        [weakSelf operationCompletedWithIdentity:nil error:nil];
                        if (completion) {
                          completion();
                        }
                      }];
}

#pragma mark - Private

// Handles the reauthentication or add account operation or displays an alert
// if the flow is interrupted by a sign-in error.
- (void)operationCompletedWithIdentity:(id<SystemIdentity>)identity
                                 error:(NSError*)error {
  if (_addAccountFlowDone) {
    // When the dialog is interrupted, this method can be called twice.
    // See: `interruptAddAccountAnimated:completion:`.
    return;
  }
  DCHECK(self.identityInteractionManager);
  _addAccountFlowDone = YES;
  self.identityInteractionManager = nil;
  SigninCoordinatorResult signinResult = SigninCoordinatorResultSuccess;
  if (self.signinInterrupted) {
    signinResult = SigninCoordinatorResultInterrupted;
    identity = nil;
  } else if (error) {
    // Filter out errors handled internally by `identity`.
    if (ShouldHandleSigninError(error)) {
      [self.delegate addAccountSigninManagerFailedWithError:error];
      return;
    }
    signinResult = SigninCoordinatorResultCanceledByUser;
    identity = nil;
  }

  [self.delegate addAccountSigninManagerFinishedWithSigninResult:signinResult
                                                        identity:identity];
}

@end
