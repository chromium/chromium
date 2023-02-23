// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVSyncController;

// Delegate of CWVSyncController.
@protocol CWVSyncControllerDelegate<NSObject>

@optional

// Called when sync has started. Check |syncController|'s |passphraseNeeded|
// property to see if |unlockWithPassphrase:| is necessary.
- (void)syncControllerDidStartSync:(CWVSyncController*)syncController;

// Called after sync has stopped.
- (void)syncControllerDidStopSync:(CWVSyncController*)syncController;

// Called whenever the state of sync internals updates.
// Specifically, CWVSyncController properties like |currentIdentity|,
// |passphraseNeeded|, |trustedVaultKeysRequired|, and
// |trustedVaultRecoverabilityDegraded| may have changed.
- (void)syncControllerDidUpdateState:(CWVSyncController*)syncController;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DELEGATE_H_
