// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_OBSERVER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@protocol CWVTrustedVaultProvider;

// An observer of CWVTrustedVaultProvider.
CWV_EXPORT
@interface CWVTrustedVaultObserver : NSObject

// This class is not meant to be instantiated directly.
- (instancetype)init NS_UNAVAILABLE;

// Call when the keys inside the vault have changed.
- (void)trustedVaultProviderDidChangeKeys:(id<CWVTrustedVaultProvider>)provider;

// Call when the recoverability of the keys has changed.
- (void)trustedVaultProviderDidChangeRecoverability:
    (id<CWVTrustedVaultProvider>)provider;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_OBSERVER_H_
