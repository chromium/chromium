// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_LEAK_CHECK_SERVICE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_LEAK_CHECK_SERVICE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVLeakCheckCredential;
@protocol CWVLeakCheckServiceObserver;

// The states the LeakCheckService can be in. This mirrors:
// components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h
typedef NS_ENUM(NSInteger, CWVLeakCheckServiceState) {
  // The service is idle and there was no previous error.
  CWVLeakCheckServiceStateIdle = 0,
  // The service is checking some credentials.
  CWVLeakCheckServiceStateRunning,

  // Those below are error states. On any error the current job is aborted.
  // The error is sticky until next checkCredentials call.

  // A call to cancel aborted the running check.
  CWVLeakCheckServiceStateCanceled,
  // The user isn't signed-in to Chrome.
  CWVLeakCheckServiceStateSignedOut,
  // Error obtaining an access token.
  CWVLeakCheckServiceStateTokenRequestFailure,
  // Error in hashing/encrypting for the request.
  CWVLeakCheckServiceStateHashingFailure,
  // Error related to network.
  CWVLeakCheckServiceStateNetworkError,
  // Error related to the password leak Google service.
  CWVLeakCheckServiceStateServiceError,
  // Error related to the quota limit of the password leak Google service.
  CWVLeakCheckServiceStateQuotaLimit,
};

// A service for checking whether a credential has been leaked.
CWV_EXPORT
@interface CWVLeakCheckService : NSObject

// Current state of the service.
@property(nonatomic, readonly) CWVLeakCheckServiceState state;

- (instancetype)init NS_UNAVAILABLE;

// Adds an observer to be notified when credentials complete or state changed.
- (void)addObserver:(id<CWVLeakCheckServiceObserver>)observer;
// Removes an observer previously added via addObserver.
- (void)removeObserver:(id<CWVLeakCheckServiceObserver>)observer;
// Adds the credentials to be checked (does not dedupe).
- (void)checkCredentials:(NSArray<CWVLeakCheckCredential*>*)credentials;
// Stops all the current checks immediately.
- (void)cancel;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_LEAK_CHECK_SERVICE_H_
