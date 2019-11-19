// Copyright 2018 The Chromium Authors. All rights reserved.
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

// Called when sync fails. |error|'s code is a CWVSyncError.
// May need to call |stopSyncAndClearIdentity| and try starting again later.
- (void)syncController:(CWVSyncController*)syncController
      didFailWithError:(NSError*)error;

// Called after sync has stopped.
- (void)syncControllerDidStopSync:(CWVSyncController*)syncController;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_DELEGATE_H_
