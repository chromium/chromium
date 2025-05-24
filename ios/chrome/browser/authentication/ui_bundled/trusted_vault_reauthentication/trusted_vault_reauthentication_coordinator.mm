// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/trusted_vault_reauthentication/trusted_vault_reauthentication_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/trusted_vault_reauthentication/trusted_vault_reauthentication_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend_factory.h"
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

@implementation TrustedVaultReauthenticationCoordinator {
  base::OnceCallback<void(BOOL animated, ProceduralBlock cancel_done)>
      _dialogCancelCallback;
  trusted_vault::SecurityDomainId _securityDomainID;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        intent:(SigninTrustedVaultDialogIntent)intent
              securityDomainID:(trusted_vault::SecurityDomainId)securityDomainID
                       trigger:(syncer::TrustedVaultUserActionTriggerForUMA)
                                   trigger {
  DCHECK(!browser->GetProfile()->IsOffTheRecord());
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _intent = intent;
    _securityDomainID = securityDomainID;
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

- (void)start {
  [super start];
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  DCHECK(
      authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSignin));
  // TODO(crbug.com/40105436): Should test if reauth is still needed. If still
  // needed, the reauth should be really started.
  // If not, the coordinator can be closed successfuly, by calling
  // -[TrustedVaultReauthenticationCoordinator
  // reauthentificationCompletedWithSuccess:]
  self.identity = AuthenticationServiceFactory::GetForProfile(self.profile)
                      ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  __weak __typeof(self) weakSelf = self;
  void (^callback)(BOOL success, NSError* error) =
      ^(BOOL success, NSError* error) {
        [weakSelf trustedVaultDialogDoneWithSuccess:success error:error];
      };
  switch (self.intent) {
    case SigninTrustedVaultDialogIntentFetchKeys:
      _dialogCancelCallback =
          TrustedVaultClientBackendFactory::GetForProfile(self.profile)
              ->Reauthentication(self.identity, _securityDomainID,
                                 self.baseViewController, callback);
      break;
    case SigninTrustedVaultDialogIntentDegradedRecoverability:
      _dialogCancelCallback =
          TrustedVaultClientBackendFactory::GetForProfile(self.profile)
              ->FixDegradedRecoverability(self.identity, _securityDomainID,
                                          self.baseViewController, callback);
      break;
  }
}

- (void)stop {
  // This coordinator should be either showing an error dialog or the trusted
  // vault dialog.
  [self stopErrorAlertCoordinator];
  if (_dialogCancelCallback) {
    std::move(_dialogCancelCallback).Run(NO, nil);
  }
  [super stop];
  self.delegate = nil;
}

#pragma mark - Private

- (void)stopErrorAlertCoordinator {
  [self.errorAlertCoordinator stop];
  self.errorAlertCoordinator = nil;
}

- (void)trustedVaultDialogDoneWithSuccess:(BOOL)success error:(NSError*)error {
  _dialogCancelCallback.Reset();
  if (error) {
    [self displayError:error];
  } else {
    [self reauthentificationCompletedWithSuccess:success];
  }
}

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
  [self.delegate trustedVaultReauthenticationCoordinatorWantsToBeStopped:self];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, errorAlertCoordinator: %p, intent: %lu>",
                       self.class.description, self, self.errorAlertCoordinator,
                       self.intent];
}

@end
