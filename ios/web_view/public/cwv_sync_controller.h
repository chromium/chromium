// Copyright 2018 The Chromium Authors. All rights reserved.
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
@property(nonatomic, readonly, nullable) CWVIdentity* currentIdentity;

// Whether or not a passphrase is needed to access sync data. Not meaningful
// until |currentIdentity| is set and |syncControllerDidStartSync:| callback in
// is invoked in |delegate|.
@property(nonatomic, readonly, getter=isPassphraseNeeded) BOOL passphraseNeeded;

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
