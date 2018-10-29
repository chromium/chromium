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

// The error domain for sync errors.
FOUNDATION_EXPORT CWV_EXPORT NSErrorDomain const CWVSyncErrorDomain;

// Possible error codes during syncing.
typedef NS_ENUM(NSInteger, CWVSyncError) {
  // No error.
  CWVSyncErrorNone = 0,
  // The credentials supplied to GAIA were either invalid, or the locally
  // cached credentials have expired.
  CWVSyncErrorInvalidGAIACredentials = -100,
  // The GAIA user is not authorized to use the service.
  CWVSyncErrorUserNotSignedUp = -200,
  // Could not connect to server to verify credentials. This could be in
  // response to either failure to connect to GAIA or failure to connect to
  // the service needing GAIA tokens during authentication.
  CWVSyncErrorConnectionFailed = -300,
  // The service is not available; try again later.
  CWVSyncErrorServiceUnavailable = -400,
  // The requestor of the authentication step cancelled the request
  // prior to completion.
  CWVSyncErrorRequestCanceled = -500,
  // Indicates the service responded to a request, but we cannot
  // interpret the response.
  CWVSyncErrorUnexpectedServiceResponse = -600,
};

CWV_EXPORT
// Used to manage syncing for autofill and password data. Usage:
// 1. Call |startSyncWithIdentity:dataSource:| to start syncing with identity.
// 2. Call |stopSyncAndClearIdentity| to stop syncing.
// It is necessary to call |startSyncWithIdentity:dataSource:| once per cold app
// launch to keep |currentIdentity| syncing. Remember to set the |delegate| to
// listen to sync start and stop events.
@interface CWVSyncController : NSObject

// The delegate of CWVSyncController.
@property(nonatomic, weak, nullable) id<CWVSyncControllerDelegate> delegate;

// The user who is syncing.
@property(nonatomic, readonly, nullable) CWVIdentity* currentIdentity;

// Whether or not a passphrase is needed to access sync data. Not meaningful
// until |currentIdentity| is set and |syncControllerDidStartSync:| callback in
// is invoked in |delegate|.
@property(nonatomic, readonly, getter=isPassphraseNeeded) BOOL passphraseNeeded;

- (instancetype)init NS_UNAVAILABLE;

// Start syncing with |identity|. |dataSource| is used to obtain access tokens.
// |identity| will be persisted as |currentIdentity| and continue syncing until
// |stopSyncAndClearIdentity| is called or the app is restarted.
- (void)startSyncWithIdentity:(CWVIdentity*)identity
                   dataSource:
                       (__weak id<CWVSyncControllerDataSource>)dataSource;

// Stops syncs and nils out |currentIdentity|. This method is idempotent.
- (void)stopSyncAndClearIdentity;

// If |passphraseNeeded| is |YES|. Call this to unlock the sync data.
// Only call after calling |startSyncWithIdentity:dataSource:| and receiving
// |syncControllerDidStartSync:| callback in |delegate|.
// No op if |passphraseNeeded| is |NO|. Returns |YES| if successful.
- (BOOL)unlockWithPassphrase:(NSString*)passphrase;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SYNC_CONTROLLER_H_
