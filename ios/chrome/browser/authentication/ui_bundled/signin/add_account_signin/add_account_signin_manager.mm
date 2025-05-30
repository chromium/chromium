// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/add_account_signin/add_account_signin_manager.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity_interaction_manager.h"

namespace {
// Logs the histograms for add to account operation:
//   * Signin.AddAccountToDevice.Result
//   * Signin.AddAccountToDevice.{Interrupted|CancelledByUser|Error|Succes}.Duration
void LogAddAccountToDeviceHistograms(SigninAddAccountToDeviceResult result,
                                     base::TimeDelta duration) {
  base::UmaHistogramEnumeration("Signin.AddAccountToDevice.Result", result);
  switch (result) {
    case SigninAddAccountToDeviceResult::kInterrupted:
      base::UmaHistogramMediumTimes(
          "Signin.AddAccountToDevice.Interrupted.Duration", duration);
      break;
    case SigninAddAccountToDeviceResult::kError:
      base::UmaHistogramMediumTimes("Signin.AddAccountToDevice.Error.Duration",
                                    duration);
      break;
    case SigninAddAccountToDeviceResult::kCancelledByUser:
      base::UmaHistogramMediumTimes(
          "Signin.AddAccountToDevice.CancelledByUser.Duration", duration);
      break;
    case SigninAddAccountToDeviceResult::kSuccess:
      base::UmaHistogramMediumTimes(
          "Signin.AddAccountToDevice.Success.Duration", duration);
      break;
  }
}
}  // namespace

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
  // Timestamp of the last start of the flow to add an account to the device.
  base::TimeTicks _lastStartAddAccountToDeviceTs;
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
    CHECK(baseViewController, base::NotFatalUntil::M140);
    CHECK(prefService, base::NotFatalUntil::M140);
    CHECK(identityManager, base::NotFatalUntil::M140);
    CHECK(identityInteractionManager, base::NotFatalUntil::M140);
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
  _lastStartAddAccountToDeviceTs = base::TimeTicks::Now();
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

- (void)interruptAnimated:(BOOL)animated {
  self.signinInterrupted = YES;
  [self.identityInteractionManager cancelAuthActivityAnimated:animated];
  [self operationCompletedWithIdentity:nil error:nil];
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
  CHECK(!_lastStartAddAccountToDeviceTs.is_null());
  base::TimeDelta addAccountDuration =
      base::TimeTicks::Now() - _lastStartAddAccountToDeviceTs;
  _lastStartAddAccountToDeviceTs = base::TimeTicks();

  DCHECK(self.identityInteractionManager);
  _addAccountFlowDone = YES;
  self.identityInteractionManager = nil;

  SigninAddAccountToDeviceResult result;
  id<SystemIdentity> resultIdentity = nil;
  NSError* resultError = nil;
  if (self.signinInterrupted) {
    result = SigninAddAccountToDeviceResult::kInterrupted;
  } else if (error) {
    // Filter out errors handled internally by `identity`.
    if (ShouldHandleSigninError(error)) {
      result = SigninAddAccountToDeviceResult::kError;
      resultError = error;
    } else {
      result = SigninAddAccountToDeviceResult::kCancelledByUser;
    }
  } else {
    result = SigninAddAccountToDeviceResult::kSuccess;
    resultIdentity = identity;
  }

  LogAddAccountToDeviceHistograms(result, addAccountDuration);
  if (!self.signinInterrupted) {
    // If the coordinator interrupted the manager, it is in charge of doing
    // the cleanup.
    [self.delegate addAccountSigninManagerFinishedWithResult:result
                                                    identity:resultIdentity
                                                       error:resultError];
  }
}

@end
