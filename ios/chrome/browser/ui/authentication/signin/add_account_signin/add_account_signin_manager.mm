// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_manager.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity_interaction_manager.h"

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
  // The user pref service.
  raw_ptr<PrefService> _prefService;
  // The identity manager.
  raw_ptr<signin::IdentityManager> _identityManager;
  // YES if the add account if done, and the delegate has been called.
  BOOL _addAccountFlowDone;
}

#pragma mark - Public

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
                   prefService:(PrefService*)prefService
               identityManager:(signin::IdentityManager*)identityManager
    identityInteractionManager:
        (id<SystemIdentityInteractionManager>)identityInteractionManager {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _prefService = prefService;
    _identityManager = identityManager;
    _identityInteractionManager = identityInteractionManager;
  }
  return self;
}

- (void)showSigninWithIntent:(AddAccountSigninIntent)signinIntent {
  DCHECK(!_addAccountFlowDone);
  DCHECK(self.identityInteractionManager);

  CoreAccountInfo primaryAccount =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  NSString* userEmail = nil;
  switch (signinIntent) {
    case AddAccountSigninIntent::kPrimaryAccountReauth:
      DUMP_WILL_BE_CHECK(!primaryAccount.IsEmpty())
          << base::SysNSStringToUTF8([self description]);
      userEmail = base::SysUTF8ToNSString(primaryAccount.email);
      break;
    case AddAccountSigninIntent::kAddAccount:
      // The user wants to add a new account, don't pre-fill any email.
      break;
    case AddAccountSigninIntent::kResignin:
      DUMP_WILL_BE_CHECK(primaryAccount.IsEmpty())
          << base::SysNSStringToUTF8([self description]);
      std::string userEmailString =
          _prefService->GetString(prefs::kGoogleServicesLastSignedInUsername);
      // Note(crbug/1443096): Gracefully handle an empty `userEmailString` by
      // showing the sign-in screen without a prefilled email.
      if (!userEmailString.empty()) {
        userEmail = base::SysUTF8ToNSString(userEmailString);
      }
      break;
  }

  __weak AddAccountSigninManager* weakSelf = self;
  [self.identityInteractionManager
      startAuthActivityWithViewController:self.baseViewController
                                userEmail:userEmail
                               completion:^(id<SystemIdentity> identity,
                                            NSError* error) {
                                 [weakSelf
                                     operationCompletedWithIdentity:identity
                                                              error:error];
                               }];
}

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  self.signinInterrupted = YES;
  switch (action) {
    case SigninCoordinatorInterrupt::UIShutdownNoDismiss:
      // IdentityInteractionManager doesn't support interrupt with no dismiss.
      // We need to stop with no animation to make sure dealloc are done with
      // CHECK failures.
      // When the interrupt is called with `NoDismiss`, the completion block
      // needs to be synchronous. So we can't wait for the cancel completion
      // block from `IdentityInteractionManager` to be called, to call the
      // interrupt completion block.
      // See crbug.com/1455216.
      [self.identityInteractionManager cancelAuthActivityAnimated:NO
                                                       completion:nil];
      [self operationCompletedWithIdentity:nil error:nil];
      if (completion) {
        completion();
      }
      break;
    case SigninCoordinatorInterrupt::DismissWithoutAnimation:
    case SigninCoordinatorInterrupt::DismissWithAnimation: {
      __weak __typeof(self) weakSelf = self;
      BOOL animated =
          action == SigninCoordinatorInterrupt::DismissWithAnimation;
      [self.identityInteractionManager
          cancelAuthActivityAnimated:animated
                          completion:^() {
                            // If `identityInteractionManager` completion
                            // callback has not been called yet, the add account
                            // needs to be fully done by calling:
                            // `operationCompletedWithIdentity:error:`, before
                            // calling `completion` See crbug.com/1227658.
                            [weakSelf operationCompletedWithIdentity:nil
                                                               error:nil];
                            if (completion) {
                              completion();
                            }
                          }];
      break;
    }
  }
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
