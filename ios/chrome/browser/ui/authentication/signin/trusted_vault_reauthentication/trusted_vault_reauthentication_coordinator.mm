// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/trusted_vault_reauthentication/trusted_vault_reauthentication_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service_utils.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/trusted_vault_client_backend.h"
#import "ios/chrome/browser/signin/trusted_vault_client_backend_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using base::SysNSStringToUTF16;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

@interface TrustedVaultReauthenticationCoordinator ()

@property(nonatomic, strong) AlertCoordinator* errorAlertCoordinator;
@property(nonatomic, strong) id<SystemIdentity> identity;
@property(nonatomic, assign) SigninTrustedVaultDialogIntent intent;

@end

@implementation TrustedVaultReauthenticationCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        intent:(SigninTrustedVaultDialogIntent)intent
                       trigger:(syncer::TrustedVaultUserActionTriggerForUMA)
                                   trigger {
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _intent = intent;
    switch (intent) {
      case SigninTrustedVaultDialogIntentFetchKeys:
        syncer::RecordKeyRetrievalTrigger(trigger);
        break;
      case SigninTrustedVaultDialogIntentDegradedRecoverability:
        syncer::RecordRecoverabilityDegradedFixTrigger(trigger);
        break;
    }
  }
  return self;
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  BOOL animated;
  __weak __typeof(self) weakSelf = self;
  void (^cancelCompletion)(void) = ^() {
    // The reauthentication callback is dropped when the dialog is canceled.
    // The completion block has to be called explicitly.
    SigninCompletionInfo* completionInfo =
        [SigninCompletionInfo signinCompletionInfoWithIdentity:nil];
    [weakSelf
        runCompletionCallbackWithSigninResult:SigninCoordinatorResultInterrupted
                               completionInfo:completionInfo];
    if (completion) {
      completion();
    }
  };
  switch (action) {
    case SigninCoordinatorInterrupt::UIShutdownNoDismiss:
      // TrustedVaultClientBackend doesn't support no dismiss. Therefore there
      // is nothing to do. It will be just deallocated when the service will
      // be shutdown.
      if (self.errorAlertCoordinator) {
        [self.errorAlertCoordinator stop];
        self.errorAlertCoordinator = nil;
      }
      cancelCompletion();
      return;
    case SigninCoordinatorInterrupt::DismissWithoutAnimation:
      animated = NO;
      break;
    case SigninCoordinatorInterrupt::DismissWithAnimation:
      animated = YES;
      break;
  }
  if (self.errorAlertCoordinator) {
    CHECK(!self.errorAlertCoordinator.noInteractionAction);
    self.errorAlertCoordinator.noInteractionAction = cancelCompletion;
    [self.errorAlertCoordinator stop];
    self.errorAlertCoordinator = nil;
  } else {
    TrustedVaultClientBackendFactory::GetForBrowserState(
        self.browser->GetBrowserState())
        ->CancelDialog(animated, cancelCompletion);
  }
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  DCHECK(
      authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSignin));
  // TODO(crbug.com/1019685): Should test if reauth is still needed. If still
  // needed, the reauth should be really started.
  // If not, the coordinator can be closed successfuly, by calling
  // -[TrustedVaultReauthenticationCoordinator
  // reauthentificationCompletedWithSuccess:]
  self.identity = AuthenticationServiceFactory::GetForBrowserState(
                      self.browser->GetBrowserState())
                      ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  __weak __typeof(self) weakSelf = self;
  void (^callback)(BOOL success, NSError* error) =
      ^(BOOL success, NSError* error) {
        if (error) {
          [weakSelf displayError:error];
        } else {
          [weakSelf reauthentificationCompletedWithSuccess:success];
        }
      };
  switch (self.intent) {
    case SigninTrustedVaultDialogIntentFetchKeys:
      TrustedVaultClientBackendFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->Reauthentication(self.identity, self.baseViewController, callback);
      break;
    case SigninTrustedVaultDialogIntentDegradedRecoverability:
      TrustedVaultClientBackendFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->FixDegradedRecoverability(self.identity, self.baseViewController,
                                      callback);
      break;
  }
}

#pragma mark - Private

- (void)displayError:(NSError*)error {
  DCHECK(error);
  NSString* title = GetNSString(IDS_IOS_SIGN_TRUSTED_VAULT_ERROR_DIALOG_TITLE);
  NSString* errorCode = [NSString stringWithFormat:@"%ld", error.code];
  NSString* message =
      GetNSStringF(IDS_IOS_SIGN_TRUSTED_VAULT_ERROR_DIALOG_MESSAGE,
                   SysNSStringToUTF16(errorCode));
  self.errorAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message];
  __weak __typeof(self) weakSelf = self;
  [self.errorAlertCoordinator
      addItemWithTitle:GetNSString(IDS_OK)
                action:^{
                  [weakSelf reauthentificationCompletedWithSuccess:NO];
                }
                 style:UIAlertActionStyleDefault];
  [self.errorAlertCoordinator start];
}

- (void)reauthentificationCompletedWithSuccess:(BOOL)success {
  DCHECK(self.identity);
  SigninCoordinatorResult result = success
                                       ? SigninCoordinatorResultSuccess
                                       : SigninCoordinatorResultCanceledByUser;
  SigninCompletionInfo* completionInfo = [SigninCompletionInfo
      signinCompletionInfoWithIdentity:success ? self.identity : nil];
  [self runCompletionCallbackWithSigninResult:result
                               completionInfo:completionInfo];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, errorAlertCoordinator: %p, intent: %lu>",
                       self.class.description, self, self.errorAlertCoordinator,
                       self.intent];
}

@end
