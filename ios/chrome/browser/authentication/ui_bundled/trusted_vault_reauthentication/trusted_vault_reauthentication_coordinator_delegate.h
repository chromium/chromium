// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_TRUSTED_VAULT_REAUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_TRUSTED_VAULT_REAUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class TrustedVaultReauthenticationCoordinator;

@protocol TrustedVaultReauthenticationCoordinatorDelegate <NSObject>

- (void)trustedVaultReauthenticationCoordinatorWantsToBeStopped:
    (TrustedVaultReauthenticationCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_TRUSTED_VAULT_REAUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_DELEGATE_H_
