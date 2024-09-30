// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_history_sync/signin_and_history_sync_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/instant_signin/instant_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"

namespace {

// List of steps to do the sign-in flow.
enum class SignInHistorySyncStep {
  // Initial step.
  kStart,
  // Present InstantSigninCoordinator. Should be used only if there is no
  // identities.
  kInstantSignin,
  // Present ConsistencyPromoSigninCoordinator. Should be used only if there is
  // at least one identity.
  kBottomSheetSignin,
  // Present HistorySyncPopupCoordinator.
  kHistorySync,
  // Last step.
  kCompleted,
};

}  // namespace

@interface SignInAndHistorySyncCoordinator () <
    HistorySyncPopupCoordinatorDelegate>
@end

@implementation SignInAndHistorySyncCoordinator {
  // Sign-in or history sync coordinator, according to `_currentStep`.
  InterruptibleChromeCoordinator* _childCoordinator;
  // The current step.
  SignInHistorySyncStep _currentStep;
  // Promo button used to trigger the sign-in.
  signin_metrics::PromoAction _promoAction;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                               accessPoint:accessPoint];
  if (self) {
    _promoAction = promoAction;
    _currentStep = SignInHistorySyncStep::kStart;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_childCoordinator) << base::SysNSStringToUTF8([self description]);
}

- (void)start {
  [super start];
  [self presentNextStepWithPreviousResult:SigninCoordinatorResultSuccess];
}

- (void)stop {
  if (_currentStep != SignInHistorySyncStep::kCompleted) {
    [self interruptWithAction:SigninCoordinatorInterrupt::UIShutdownNoDismiss
                   completion:nil];
  }
  [super stop];
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  // TODO(crbug.com/40929259): Turn into CHECK.
  DUMP_WILL_BE_CHECK(_childCoordinator)
      << base::SysNSStringToUTF8([self description]);
  // Interrupt `_childCoordinator` which will trigger the end of this
  // coordinator. Its callback will triggered.
  [_childCoordinator interruptWithAction:action completion:completion];
}

#pragma mark - HistorySyncPopupCoordinatorDelegate

- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(SigninCoordinatorResult)result {
  [self currentStepDidFinishWithResult:result];
}

#pragma mark - Private

// Moves to the next step and presents the coordinator of that next step.
- (void)presentNextStepWithPreviousResult:
    (SigninCoordinatorResult)previousResult {
  CHECK(!_childCoordinator) << base::SysNSStringToUTF8([self description]);
  switch (previousResult) {
    case SigninCoordinatorResultSuccess:
    case SigninCoordinatorResultDisabled:
      _currentStep = [self nextStep];
      break;
    case SigninCoordinatorResultInterrupted:
    case SigninCoordinatorResultCanceledByUser:
      _currentStep = SignInHistorySyncStep::kCompleted;
      break;
  }
  if (_currentStep != SignInHistorySyncStep::kCompleted) {
    _childCoordinator = [self createPresentStepChildCoordinator];
    // TODO(crbug.com/40929259): Turn into CHECK.
    DUMP_WILL_BE_CHECK(_childCoordinator)
        << base::SysNSStringToUTF8([self description]);
    [_childCoordinator start];
    return;
  }
  // If there are no steps remaining, call delegate to stop presenting
  // coordinators.
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  SigninCoordinatorResult result;
  if (previousResult == SigninCoordinatorResultInterrupted) {
    result = SigninCoordinatorResultInterrupted;
    // If a SigninCoordinator is interrupted, the completion info should not
    // contain a identity even if a sign-in has been completed successfully.
    identity = nil;
  } else if (identity) {
    result = SigninCoordinatorResultSuccess;
  } else {
    result = SigninCoordinatorResultCanceledByUser;
  }
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:identity];
  [self runCompletionCallbackWithSigninResult:result
                               completionInfo:completionInfo];
}

// Creates the current step coordinator according to `_currentStep`.
- (InterruptibleChromeCoordinator*)createPresentStepChildCoordinator {
  switch (_currentStep) {
    case SignInHistorySyncStep::kBottomSheetSignin: {
      SigninCoordinator* coordinator =
          [[ConsistencyPromoSigninCoordinator alloc]
              initWithBaseViewController:self.baseViewController
                                 browser:self.browser
                             accessPoint:self.accessPoint];
      __weak __typeof(self) weakSelf = self;
      coordinator.signinCompletion =
          ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
            [weakSelf currentStepDidFinishWithResult:result];
          };
      return coordinator;
    }
    case SignInHistorySyncStep::kInstantSignin: {
      SigninCoordinator* coordinator = [[InstantSigninCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser
                            identity:nil
                         accessPoint:self.accessPoint
                         promoAction:_promoAction];
      __weak __typeof(self) weakSelf = self;
      coordinator.signinCompletion =
          ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
            [weakSelf currentStepDidFinishWithResult:result];
          };
      return coordinator;
    }
    case SignInHistorySyncStep::kHistorySync: {
      HistorySyncPopupCoordinator* coordinator =
          [[HistorySyncPopupCoordinator alloc]
              initWithBaseViewController:self.baseViewController
                                 browser:self.browser
                           showUserEmail:NO
                       signOutIfDeclined:NO
                              isOptional:YES
                             accessPoint:self.accessPoint];
      coordinator.delegate = self;
      return coordinator;
    }
    case SignInHistorySyncStep::kStart:
    case SignInHistorySyncStep::kCompleted:
      break;
  }
  NOTREACHED() << base::SysNSStringToUTF8([self description]);
}

// Stops the child coordinator and prepares the next step to present.
- (void)currentStepDidFinishWithResult:(SigninCoordinatorResult)result {
  // TODO(crbug.com/40929259): Turn into CHECK.
  DUMP_WILL_BE_CHECK(_childCoordinator)
      << base::SysNSStringToUTF8([self description]);
  [_childCoordinator stop];
  _childCoordinator = nil;
  [self presentNextStepWithPreviousResult:result];
}

// Returns the next step for `_currentStep`.
- (SignInHistorySyncStep)nextStep {
  switch (_currentStep) {
    case SignInHistorySyncStep::kStart: {
      ChromeAccountManagerService* accountManagerService =
          ChromeAccountManagerServiceFactory::GetForProfile(
              self.browser->GetProfile());
      if (accountManagerService->HasIdentities()) {
        return SignInHistorySyncStep::kBottomSheetSignin;
      }
      return SignInHistorySyncStep::kInstantSignin;
    }
    case SignInHistorySyncStep::kInstantSignin:
    case SignInHistorySyncStep::kBottomSheetSignin:
      return SignInHistorySyncStep::kHistorySync;
    case SignInHistorySyncStep::kHistorySync:
      return SignInHistorySyncStep::kCompleted;
    case SignInHistorySyncStep::kCompleted:
      break;
  }
  NOTREACHED() << base::SysNSStringToUTF8([self description]);
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, childcoordinator: %@, currentStep: %d, "
                       @"accessPoint %d, promoAction %d>",
                       self.class.description, self, _childCoordinator,
                       static_cast<int>(_currentStep),
                       static_cast<int>(self.accessPoint),
                       static_cast<int>(_promoAction)];
}

@end
