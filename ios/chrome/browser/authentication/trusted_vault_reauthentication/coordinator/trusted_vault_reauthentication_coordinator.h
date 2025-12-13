// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
namespace syncer {
enum class TrustedVaultUserActionTriggerForUMA;
}  // namespace syncer
namespace trusted_vault {
enum class SecurityDomainId;
enum class TrustedVaultUserActionTriggerForUMA;
}  // namespace trusted_vault
typedef NS_ENUM(NSUInteger, SigninTrustedVaultDialogIntent);
@protocol TrustedVaultReauthenticationCoordinatorDelegate;

// Intent for TrustedVaultReauthenticationCoordinator to display either
// the reauthentication or degraded recoverability dialog.
typedef NS_ENUM(NSUInteger, SigninTrustedVaultDialogIntent) {
  // Show reauthentication dialog for fetch keys.
  SigninTrustedVaultDialogIntentFetchKeys,
  // Show reauthentication degraded recoverability dialog (to enroll additional
  // recovery factors).
  SigninTrustedVaultDialogIntentDegradedRecoverability,
};

// Coordinates the Trusted Vault re-authentication dialog. Trusted Valut is
// managed by IOSTrustedValueClient.
@interface TrustedVaultReauthenticationCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<TrustedVaultReauthenticationCoordinatorDelegate>
    delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// `viewController` presents the sign-in.
// `intent` Dialog to present.
// `securityDomainID` Identifies a particular security domain.
// `trigger` UI elements where the trusted vault reauth has been triggered.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        intent:(SigninTrustedVaultDialogIntent)intent
              securityDomainID:(trusted_vault::SecurityDomainId)securityDomainID
                       trigger:
                           (trusted_vault::TrustedVaultUserActionTriggerForUMA)
                               trigger NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_H_
