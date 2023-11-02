// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_TRUSTED_VAULT_REAUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_TRUSTED_VAULT_REAUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_H_

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

namespace syncer {
enum class TrustedVaultUserActionTriggerForUMA;
}  // namespace syncer

// Coordinates the Trusted Vault re-authentication dialog. Trusted Valut is
// managed by IOSTrustedValueClient.
@interface TrustedVaultReauthenticationCoordinator : SigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// `viewController` presents the sign-in.
// `intent` Dialog to present.
// `trigger` UI elements where the trusted vault reauth has been triggered.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        intent:(SigninTrustedVaultDialogIntent)intent
                       trigger:
                           (syncer::TrustedVaultUserActionTriggerForUMA)trigger
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_TRUSTED_VAULT_REAUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_H_
