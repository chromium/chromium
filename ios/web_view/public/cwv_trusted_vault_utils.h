// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_UTILS_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_UTILS_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Possible states of the trusted vault. Keep in sync with
// syncer::TrustedVaultDeviceRegistrationStateForUMA.
typedef NS_ENUM(NSInteger, CWVTrustedVaultState) {
  // TODO(crbug.com/40238423): DEPRECATED, use
  // `CWVTrustedVaultStateAlreadyRegisteredV0`.
  CWVTrustedVaultStateAlreadyRegistered = 0,
  CWVTrustedVaultStateAlreadyRegisteredV0 = 0,
  CWVTrustedVaultStateLocalKeysAreStale = 1,
  CWVTrustedVaultStateThrottledClientSide = 2,
  CWVTrustedVaultStateAttemptingRegistrationWithNewKeyPair = 3,
  CWVTrustedVaultStateAttemptingRegistrationWithExistingKeyPair = 4,
  CWVTrustedVaultStateAttemptingRegistrationWithPersistentAuthError = 5,
  CWVTrustedVaultStateAlreadyRegisteredV1 = 6,
};

// Utility methods for trusted vault.
CWV_EXPORT
@interface CWVTrustedVaultUtils : NSObject

// Call to log to UMA when trusted vault state changes.
// TODO(crbug.com/40204010): See if these functions can be implemented by a
// CWVTrustedVaultObserver instead.
+ (void)logTrustedVaultDidUpdateState:(CWVTrustedVaultState)state;

// Call to log to UMA when trusted vault receives a http status code.
// TODO(crbug.com/40204010): See if these functions can be implemented by a
// CWVTrustedVaultObserver instead.
+ (void)logTrustedVaultDidReceiveHTTPStatusCode:(NSInteger)statusCode;

// Call to log to UMA when trusted vault fails key distribution.
// TODO(crbug.com/40204010): See if these functions can be implemented by a
// CWVTrustedVaultObserver instead.
+ (void)logTrustedVaultDidFailKeyDistribution:(NSError*)error;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_UTILS_H_
