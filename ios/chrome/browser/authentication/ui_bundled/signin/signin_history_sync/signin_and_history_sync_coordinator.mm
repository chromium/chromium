// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_history_sync/signin_and_history_sync_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_coordinator.h"
#import "ios/chrome/browser/authentication/consistency_promo_signin/coordinator/consistency_promo_signin_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/model/history_sync_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/instant_signin/instant_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_screen_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

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
  // Present FullscreenSigninCoordinator. Should be used only if there is
  // at least one identity.
  kFullscreenSignin,
  // Present HistorySyncPopupCoordinator.
  kHistorySync,
  // Last step.
  kCompleted,
};

// Converts HistorySyncResult in SigninCoordinatorResult.
SigninCoordinatorResult HistorySyncResultToSigninCoordinatorResult(
    HistorySyncResult history_sync_result) {
  switch (history_sync_result) {
    case HistorySyncResult::kSuccess:
    case HistorySyncResult::kUserCanceled:
    case HistorySyncResult::kSkipped:
      return SigninCoordinatorResultSuccess;
    case HistorySyncResult::kPrimaryIdentityRemoved:
      return SigninCoordinatorResultInterrupted;
  }
  NOTREACHED();
}

}  // namespace

@interface SignInAndHistorySyncCoordinator () <
    FullscreenSigninCoordinatorDelegate,
    HistorySyncPopupCoordinatorDelegate>
@end

@implementation SignInAndHistorySyncCoordinator {
  // Sign-in coordinator, according to `_currentStep`.
  SigninCoordinator* _signinCoordinator;
  // Full screen signin coordinator for
  // SignInHistorySyncStep::kFullscreenSignin.
  FullscreenSigninCoordinator* _fullscreenSigninCoordinator;
  // HistorySyncPopupCoordinator for SignInHistorySyncStep::kHistorySync.
  HistorySyncPopupCoordinator* _historySyncPopupCoordinator;
  // The current step.
  SignInHistorySyncStep _currentStep;
  // Promo button used to trigger the sign-in.
  signin_metrics::PromoAction _promoAction;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<syncer::SyncService> _syncService;
  // Whether the history opt in should be optional.
  BOOL _optionalHistorySync;
  // Whether the promo should be displayed in a fullscreen modal.
  BOOL _fullscreenPromo;
  ChangeProfileContinuationProvider _continuationProvider;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                  contextStyle:(SigninContextStyle)contextStyle
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction
           optionalHistorySync:(BOOL)optionalHistorySync
               fullscreenPromo:(BOOL)fullscreenPromo
          continuationProvider:
              (const ChangeProfileContinuationProvider&)continuationProvider {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                              contextStyle:contextStyle
                               accessPoint:accessPoint];
  if (self) {
    CHECK(continuationProvider);
    _continuationProvider = continuationProvider;
    _optionalHistorySync = optionalHistorySync;
    _fullscreenPromo = fullscreenPromo;
    _promoAction = promoAction;
    _currentStep = SignInHistorySyncStep::kStart;
  }
  return self;
}

- (void)dealloc {
  CHECK(!_signinCoordinator, base::NotFatalUntil::M145)
      << base::SysNSStringToUTF8([self description]);
  CHECK(!_historySyncPopupCoordinator, base::NotFatalUntil::M145)
      << base::SysNSStringToUTF8([self description]);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  _authenticationService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  _syncService = SyncServiceFactory::GetForProfile(self.profile);
  [self presentNextStepWithPreviousResult:SigninCoordinatorResultSuccess];
}

#pragma mark - AnimatedCoordinator

- (void)stopAnimated:(BOOL)animated {
  [self stopSigninCoordinatorAnimated:animated];
  [self stopHistorySyncPopupCoordinatorAnimated:animated];
  _syncService = nullptr;
  _authenticationService = nullptr;
  [self stopFullscreenSigninCoordinator];
  [super stopAnimated:animated];
}

#pragma mark - BuggyAuthenticationViewOwner

- (BOOL)viewWillPersist {
  // As the current coordinator has no view, its view can has disappeared only
  // if its signin coordinator view may have disappeared.
  return !_signinCoordinator || _signinCoordinator.viewWillPersist;
}

#pragma mark - HistorySyncPopupCoordinatorDelegate

- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(HistorySyncResult)result {
  CHECK_EQ(coordinator, _historySyncPopupCoordinator,
           base::NotFatalUntil::M145);
  [self stopHistorySyncPopupCoordinatorAnimated:YES];
  SigninCoordinatorResult signinResult =
      HistorySyncResultToSigninCoordinatorResult(result);
  [self presentNextStepWithPreviousResult:signinResult];
}

#pragma mark - Private

- (void)stopFullscreenSigninCoordinator {
  [_fullscreenSigninCoordinator stop];
  _fullscreenSigninCoordinator.delegate = nil;
  _fullscreenSigninCoordinator = nil;
}

- (void)stopHistorySyncPopupCoordinatorAnimated:(BOOL)animated {
  [_historySyncPopupCoordinator stopAnimated:animated];
  _historySyncPopupCoordinator.delegate = nil;
  _historySyncPopupCoordinator = nil;
}

- (void)stopSigninCoordinatorAnimated:(BOOL)animated {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

// Moves to the next step and presents the coordinator of that next step.
- (void)presentNextStepWithPreviousResult:
    (SigninCoordinatorResult)previousResult {
  CHECK(!_signinCoordinator) << base::SysNSStringToUTF8([self description]);
  CHECK(!_historySyncPopupCoordinator)
      << base::SysNSStringToUTF8([self description]);
  switch (previousResult) {
    case SigninCoordinatorResultSuccess:
    case SigninCoordinatorResultDisabled:
      _currentStep = [self nextStep];
      break;
    case SigninCoordinatorProfileSwitch:
    case SigninCoordinatorResultInterrupted:
    case SigninCoordinatorResultCanceledByUser:
      _currentStep = SignInHistorySyncStep::kCompleted;
      break;
    case SigninCoordinatorUINotAvailable:
      // SigninAndHistorySyncController presents its child coordinators
      // directly and does not use `ShowSigninCommand`.
      NOTREACHED();
  }
  if (_currentStep != SignInHistorySyncStep::kCompleted) {
    [self createAndPresentStepChildCoordinator];
    return;
  }
  // If there are no steps remaining, call delegate to stop presenting
  // coordinators.
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
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
  id<SystemIdentity> completionIdentity = identity;
  [self runCompletionWithSigninResult:result
                   completionIdentity:completionIdentity];
}

// Creates the current step coordinator according to `_currentStep`.
- (void)createAndPresentStepChildCoordinator {
  CHECK(!_fullscreenSigninCoordinator, base::NotFatalUntil::M148);
  CHECK(!_signinCoordinator, base::NotFatalUntil::M148);
  CHECK(!_historySyncPopupCoordinator, base::NotFatalUntil::M148);
  switch (_currentStep) {
    case SignInHistorySyncStep::kFullscreenSignin: {
      _fullscreenSigninCoordinator = [[FullscreenSigninCoordinator alloc]
                 initWithBaseViewController:self.baseViewController
                                    browser:self.browser
                             screenProvider:[[SigninScreenProvider alloc] init]
                               contextStyle:self.contextStyle
                                accessPoint:self.accessPoint
          changeProfileContinuationProvider:_continuationProvider];
      _fullscreenSigninCoordinator.delegate = self;
      [_fullscreenSigninCoordinator start];
      return;
    }
    case SignInHistorySyncStep::kBottomSheetSignin: {
      _signinCoordinator = [[ConsistencyPromoSigninCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser
                        contextStyle:self.contextStyle
                         accessPoint:self.accessPoint
                prepareChangeProfile:nil
                continuationProvider:_continuationProvider];
      __weak __typeof(self) weakSelf = self;
      _signinCoordinator.signinCompletion =
          ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
            id<SystemIdentity>) {
            [weakSelf currentSigninStepDidFinishWithCoordinator:coordinator
                                                         result:result];
          };
      [_signinCoordinator start];
      return;
    }
    case SignInHistorySyncStep::kInstantSignin: {
      _signinCoordinator = [[InstantSigninCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser
                            identity:nil
                        contextStyle:self.contextStyle
                         accessPoint:self.accessPoint
                         promoAction:_promoAction
                continuationProvider:_continuationProvider];
      __weak __typeof(self) weakSelf = self;
      _signinCoordinator.signinCompletion =
          ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
            id<SystemIdentity>) {
            [weakSelf currentSigninStepDidFinishWithCoordinator:coordinator
                                                         result:result];
          };
      [_signinCoordinator start];
      return;
    }
    case SignInHistorySyncStep::kHistorySync: {
      if (history_sync::GetSkipReason(_syncService, _authenticationService,
                                      self.profile->GetPrefs(),
                                      _optionalHistorySync) !=
          history_sync::HistorySyncSkipReason::kNone) {
        [self
            presentNextStepWithPreviousResult:SigninCoordinatorResultDisabled];
      } else {
        _historySyncPopupCoordinator = [[HistorySyncPopupCoordinator alloc]
            initWithBaseViewController:self.baseViewController
                               browser:self.browser
                         showUserEmail:NO
                     signOutIfDeclined:NO
                            isOptional:_optionalHistorySync
                          contextStyle:self.contextStyle
                           accessPoint:self.accessPoint];
        _historySyncPopupCoordinator.delegate = self;
        [_historySyncPopupCoordinator start];
      }
      return;
    }
    case SignInHistorySyncStep::kStart:
    case SignInHistorySyncStep::kCompleted:
      break;
  }
  NOTREACHED() << base::SysNSStringToUTF8([self description]);
}

// Stops the child coordinator and prepares the next step to present.
- (void)
    currentSigninStepDidFinishWithCoordinator:(SigninCoordinator*)coordinator
                                       result:(SigninCoordinatorResult)result {
  // TODO(crbug.com/40929259): Turn into CHECK.
  DUMP_WILL_BE_CHECK_EQ(_signinCoordinator, coordinator)
      << base::SysNSStringToUTF8([self description]);
  DUMP_WILL_BE_CHECK(!_historySyncPopupCoordinator)
      << base::SysNSStringToUTF8([self description]);
  [self stopSigninCoordinatorAnimated:YES];
  [self presentNextStepWithPreviousResult:result];
}

// Returns the next step for `_currentStep`.
- (SignInHistorySyncStep)nextStep {
  switch (_currentStep) {
    case SignInHistorySyncStep::kStart: {
      signin::IdentityManager* identityManager =
          IdentityManagerFactory::GetForProfile(self.profile);
      bool hasIdentitiesOnDevice =
          !identityManager->GetAccountsOnDevice().empty();
      if (_fullscreenPromo) {
        return SignInHistorySyncStep::kFullscreenSignin;
      } else if (hasIdentitiesOnDevice) {
        return SignInHistorySyncStep::kBottomSheetSignin;
      }
      return SignInHistorySyncStep::kInstantSignin;
    }
    case SignInHistorySyncStep::kInstantSignin:
    case SignInHistorySyncStep::kBottomSheetSignin:
    case SignInHistorySyncStep::kFullscreenSignin:
      return SignInHistorySyncStep::kHistorySync;
    case SignInHistorySyncStep::kHistorySync:
      return SignInHistorySyncStep::kCompleted;
    case SignInHistorySyncStep::kCompleted:
      break;
  }
  NOTREACHED() << base::SysNSStringToUTF8([self description]);
}

#pragma mark - FullscreenSigninCoordinatorDelegate

- (void)fullscreenSigninCoordinatorWantsToBeStopped:
            (FullscreenSigninCoordinator*)coordinator
                                             result:(SigninCoordinatorResult)
                                                        result {
  CHECK_EQ(_fullscreenSigninCoordinator, coordinator);
  [self stopFullscreenSigninCoordinator];
  [self presentNextStepWithPreviousResult:result];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, signinCoordinator: %@, "
                        "historySyncPopupCoordinator: %@, currentStep: %d, "
                       @"accessPoint %d, promoAction %d>",
                       self.class.description, self, _signinCoordinator,
                       _historySyncPopupCoordinator,
                       static_cast<int>(_currentStep),
                       static_cast<int>(self.accessPoint),
                       static_cast<int>(_promoAction)];
}

@end
