// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVIdentity;
@protocol CWVSyncControllerDataSource;
@protocol CWVSyncControllerDelegate;
@protocol CWVTrustedVaultProvider;

// Used to manage syncing for autofill and password data. Usage:
// 1. Set the |dataSource| and |delegate|.
// 2. Call |startSyncWithIdentity:| to start syncing with identity.
// 3. Call |stopSyncAndClearIdentity| to stop syncing.
CWV_EXPORT
@interface CWVSyncController : NSObject

// The trusted vault provider for CWVSyncController.
@property(class, nonatomic, weak, nullable) id<CWVTrustedVaultProvider>
    trustedVaultProvider;

// The data source of CWVSyncController.
@property(class, nonatomic, weak, nullable) id<CWVSyncControllerDataSource>
    dataSource;

// The delegate of CWVSyncController.
@property(nonatomic, weak, nullable) id<CWVSyncControllerDelegate> delegate;

// The user who is syncing.
// This property may change after |syncControllerDidUpdateState:| is invoked on
// the |delegate|.
@property(nonatomic, readonly, nullable) CWVIdentity* currentIdentity;

// Whether or not a passphrase is needed to access sync data.
// This property may change after |syncControllerDidUpdateState:| is invoked on
// the |delegate|.
@property(nonatomic, readonly, getter=isPassphraseNeeded) BOOL passphraseNeeded;

// Whether or not trusted vault keys are required to decrypt encrypted data.
// If required, UI should be presented to the user to fetch the required keys.
// This property may change after |syncControllerDidUpdateState:| is invoked on
// the |delegate|.
@property(nonatomic, readonly, getter=isTrustedVaultKeysRequired)
    BOOL trustedVaultKeysRequired;

// Whether or not trusted vault recoverability is degraded.
// Degraded recoverability refers to the state where the user is considered at
// risk of losing access to their trusted vault. In such a scenario, UI should
// be presented to allow the user to setup additional knowledge factors so that
// recoverability is better ensured.
// This property may change after |syncControllerDidUpdateState:| is invoked on
// the |delegate|.
@property(nonatomic, readonly, getter=isTrustedVaultRecoverabilityDegraded)
    BOOL trustedVaultRecoverabilityDegraded;

- (instancetype)init NS_UNAVAILABLE;

// Start syncing with |identity|.
// Call this only after receiving explicit consent from the user.
// |identity| will be persisted as |currentIdentity| and continue syncing until
// |stopSyncAndClearIdentity| is called.
// Make sure |dataSource| is set so access tokens can be fetched.
- (void)startSyncWithIdentity:(CWVIdentity*)identity;

// Stops syncs and nils out |currentIdentity|. This method is idempotent.
- (void)stopSyncAndClearIdentity;

// If |passphraseNeeded| is |YES|. Call this to unlock the sync data.
// Only call after calling |startSyncWithIdentity:| and receiving
// |syncControllerDidStartSync:| callback in |delegate|.
// No op if |passphraseNeeded| is |NO|. Returns |YES| if successful.
- (BOOL)unlockWithPassphrase:(NSString*)passphrase;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_H_
