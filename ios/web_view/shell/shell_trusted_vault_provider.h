// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_SHELL_SHELL_TRUSTED_VAULT_PROVIDER_H_
#define IOS_WEB_VIEW_SHELL_SHELL_TRUSTED_VAULT_PROVIDER_H_

#import <ChromeWebView/ChromeWebView.h>

#import "ios/web_view/shell/shell_auth_service.h"

NS_ASSUME_NONNULL_BEGIN

// Provides trusted vault functions to ChromeWebView.
@interface ShellTrustedVaultProvider : NSObject <CWVTrustedVaultProvider>

- (instancetype)initWithAuthService:(ShellAuthService*)authService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)showFetchKeysFlowForIdentity:(CWVIdentity*)identity
                  fromViewController:(UIViewController*)viewController;
- (void)showFixDegradedRecoverabilityFlowForIdentity:(CWVIdentity*)identity
                                  fromViewController:
                                      (UIViewController*)viewController;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_SHELL_SHELL_TRUSTED_VAULT_PROVIDER_H_
