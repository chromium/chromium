// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_SHELL_SHELL_TRUSTED_VAULT_PROVIDER_H_
#define IOS_WEB_VIEW_SHELL_SHELL_TRUSTED_VAULT_PROVIDER_H_

#import <ChromeWebView/ChromeWebView.h>

NS_ASSUME_NONNULL_BEGIN

// Provides trusted vault functions to ChromeWebView.
@interface ShellTrustedVaultProvider : NSObject <CWVTrustedVaultProvider>
@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_SHELL_SHELL_TRUSTED_VAULT_PROVIDER_H_
