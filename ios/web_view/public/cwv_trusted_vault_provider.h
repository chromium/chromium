// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_PROVIDER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_PROVIDER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVIdentity;
@class CWVTrustedVaultObserver;

// Provides trusted vault functionality to the trusted vault client.
@protocol CWVTrustedVaultProvider <NSObject>

// Adds an observer of the trusted vault.
- (void)addTrustedVaultObserver:(CWVTrustedVaultObserver*)observer;

// Removes an observer of the trusted vault.
// |observer| is not guaranteed to be the same instance as the instance passed
// in |addTrustedVaultObserver:|. To ensure the correct |observer| is removed,
// you must compare them using -[NSObject isEqual:].
- (void)removeTrustedVaultObserver:(CWVTrustedVaultObserver*)observer;

// Fetch the necessary keys for the trusted vault.
// |identity| The identity whose keys are to be fetched.
// |completion| To be called whether key fetching is successful or not. NSArray
// should be a list of opaque key data whose format is already privately
// established internally. If successful, NSArray will be non-nil and NSError
// will be nil. Otherwise, NSArray will be nil and NSError will be non-nil.
- (void)fetchKeysForIdentity:(CWVIdentity*)identity
                  completion:(void (^)(NSArray<NSData*>* _Nullable,
                                       NSError* _Nullable))completion;

// Marks the local keys as out of date.
// |identity| The identity whose keys are to be marked stale.
// |completion| To be called whether or not operation succeeds. NSError will be
// nil if operation succeeds, and non-nil if operation fails.
- (void)markLocalKeysAsStaleForIdentity:(CWVIdentity*)identity
                             completion:
                                 (void (^)(NSError* _Nullable))completion;

// Computes whether or not the recoverability of the keys is degraded.
// |identity| The identity whose recoverability status is being queried.
// |completion| To be called when recoverability status is known. If the
// operation is successful, BOOL will indicate whether or not recoverability is
// degraded and the NSError will be nil. If the operation fails, the BOOL will
// be set to NO and the NSError will be non-nil to provide additional details.
- (void)isRecoverabilityDegradedForIdentity:(CWVIdentity*)identity
                                 completion:(void (^)(BOOL, NSError* _Nullable))
                                                completion;

// Clears local data belonging to |identity|, such as shared keys. This
// excludes the physical client's key pair, which remains unchanged.
- (void)clearLocalDataForForIdentity:(CWVIdentity*)identity;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_PROVIDER_H_
