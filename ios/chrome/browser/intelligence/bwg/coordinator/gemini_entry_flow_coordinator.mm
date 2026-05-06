// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_entry_flow_coordinator.h"

#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

@implementation GeminiEntryFlowCoordinator {
  // The sign-in coordinator presented when the user is signed out.
  SigninCoordinator* _signinCoordinator;
  // The startup state for the Gemini session.
  GeminiStartupState* _startupState;
  // The sign-in access point for metrics.
  signin_metrics::AccessPoint _accessPoint;
  // Whether to show a snackbar on ineligible completion.
  BOOL _showSnackbarOnCompletion;
  // Called with the final result of the flow.
  GeminiEntryFlowCompletion _completion;
  // The account menu coordinator for switching accounts when the current
  // account is ineligible due to Gemini policy restriction.
  AccountMenuCoordinator* _accountMenuCoordinator;
}

#pragma mark - ChromeCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
                       browser:(Browser*)browser
                  startupState:(GeminiStartupState*)startupState
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
      showSnackbarOnCompletion:(BOOL)showSnackbarOnCompletion
                    completion:(GeminiEntryFlowCompletion)completion {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _startupState = startupState;
    _accessPoint = accessPoint;
    _showSnackbarOnCompletion = showSnackbarOnCompletion;
    _completion = [completion copy];
  }
  return self;
}

- (void)start {
  [super start];

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());

  // If the user is already signed in, proceed to the next step.
  if (authService->HasPrimaryIdentity()) {
    [self evaluateEligibilityAndRoute];
    return;
  }

  // User is signed out, present sign-in.
  [self presentSignIn];
}

- (void)stop {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
  [self stopAccountMenu];
  [super stop];
}

#pragma mark - AccountMenuCoordinatorDelegate

- (void)accountMenuCoordinatorWantsToBeStopped:
    (AccountMenuCoordinator*)coordinator {
  [self stopAccountMenu];

  // Re-check eligibility after the account switch.
  GeminiService* geminiService =
      GeminiServiceFactory::GetForProfile(self.browser->GetProfile());
  if (geminiService && geminiService->IsProfileEligibleForGemini()) {
    // TODO(crbug.com/507509815): Page eligibility check.
    [self finishWithResult:kGeminiEntryFlowResultSuccess];
    return;
  }

  [self finishWithResult:kGeminiEntryFlowResultAccountIneligibleByGemini];
}

#pragma mark - Private

// Presents the sign-in sheet.
- (void)presentSignIn {
  _signinCoordinator = [SigninCoordinator
      signinAndHistorySyncCoordinatorWithBaseViewController:
          self.baseViewController
                                                    browser:self.browser
                                               contextStyle:SigninContextStyle::
                                                                kDefault
                                                accessPoint:_accessPoint
                                                promoAction:
                                                    signin_metrics::PromoAction::
                                                        PROMO_ACTION_NO_SIGNIN_PROMO
                                        optionalHistorySync:YES
                                            fullscreenPromo:NO
                                       continuationProvider:
                                           DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        [weakSelf signinDidFinishWithResult:result];
      };
  [_signinCoordinator start];
}

// Called when sign-in completes or is cancelled.
- (void)signinDidFinishWithResult:(SigninCoordinatorResult)result {
  [_signinCoordinator stop];
  _signinCoordinator = nil;

  if (result != SigninCoordinatorResultSuccess) {
    [self finishWithResult:kGeminiEntryFlowResultCancelled];
    return;
  }

  // Sign-in succeeded, check eligibility.
  [self evaluateEligibilityAndRoute];
}

// Completes the flow and calls the completion block.
- (void)finishWithResult:(GeminiEntryFlowResult)result {
  if (_completion) {
    _completion(result);
  }
}

// Evaluates profile eligibility and routes to the appropriate outcome.
// Uses the currently available eligibility data. If the workspace policy
// check hasn't completed yet, the service returns conservative defaults
// (personal accounts treated as eligible, managed accounts as ineligible).
- (void)evaluateEligibilityAndRoute {
  GeminiService* geminiService =
      GeminiServiceFactory::GetForProfile(self.browser->GetProfile());

  if (!geminiService) {
    [self finishWithResult:kGeminiEntryFlowResultUnknown];
    return;
  }

  // Trigger the workspace policy check if it hasn't started yet.
  geminiService->CheckGeminiEnterpriseEligibilityIfNeeded();

  std::optional<gemini::IneligibilityReasons> result =
      geminiService->GeminiIneligibilityForProfile();

  // Eligible — check page availability.
  if (!result.has_value()) {
    // TODO(crbug.com/507509815): Page eligibility check.
    [self finishWithResult:kGeminiEntryFlowResultSuccess];
    return;
  }

  // Ineligible — show snackbar if enabled.
  [self showIneligibleSnackbarIfNeeded];

  // Enterprise policy restriction.
  if (result.value().chrome_enterprise) {
    [self finishWithResult:kGeminiEntryFlowResultAccountIneligibleByEnterprise];
    return;
  }

  // Gemini policy restriction (workspace) — present account menu
  // for switching accounts.
  if (result.value().workspace) {
    [self presentAccountMenu];
    return;
  }

  // Remaining ineligibility (account capability or other).
  [self finishWithResult:kGeminiEntryFlowResultUnknown];
}

// Shows the ineligible account snackbar if showSnackbarOnCompletion is YES.
- (void)showIneligibleSnackbarIfNeeded {
  if (!_showSnackbarOnCompletion) {
    return;
  }
  id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  SnackbarMessage* message = [[SnackbarMessage alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_AI_HUB_INELIGIBLE_ACCOUNT_SNACKBAR)];
  [snackbarHandler showSnackbarMessage:message];
}

// Presents the account menu for switching to a different account.
- (void)presentAccountMenu {
  _accountMenuCoordinator = [[AccountMenuCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      anchorView:self.baseViewController.view
                     accessPoint:AccountMenuAccessPoint::kGeminiEntryFlow
                             URL:GURL()];
  _accountMenuCoordinator.delegate = self;
  [_accountMenuCoordinator start];
}

// Stops and releases the account menu coordinator.
- (void)stopAccountMenu {
  [_accountMenuCoordinator stop];
  _accountMenuCoordinator = nil;
}

@end
