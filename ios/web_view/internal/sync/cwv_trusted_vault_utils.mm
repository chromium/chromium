// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_trusted_vault_utils.h"

#import "components/sync/service/trusted_vault_histograms.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
syncer::TrustedVaultDeviceRegistrationStateForUMA CWVConvertTrustedVaultState(
    CWVTrustedVaultState state) {
  switch (state) {
    case CWVTrustedVaultStateAlreadyRegisteredV0:
      return syncer::TrustedVaultDeviceRegistrationStateForUMA::
          kAlreadyRegisteredV0;
    case CWVTrustedVaultStateLocalKeysAreStale:
      return syncer::TrustedVaultDeviceRegistrationStateForUMA::
          kLocalKeysAreStale;
    case CWVTrustedVaultStateThrottledClientSide:
      return syncer::TrustedVaultDeviceRegistrationStateForUMA::
          kThrottledClientSide;
    case CWVTrustedVaultStateAttemptingRegistrationWithNewKeyPair:
      return syncer::TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair;
    case CWVTrustedVaultStateAttemptingRegistrationWithExistingKeyPair:
      return syncer::TrustedVaultDeviceRegistrationStateForUMA::
          kAttemptingRegistrationWithExistingKeyPair;
    case CWVTrustedVaultStateAttemptingRegistrationWithPersistentAuthError:
      // TODO(crbug.com/1418027): remove CWV version of this bucket.
      return syncer::TrustedVaultDeviceRegistrationStateForUMA::
          kDeprecatedAttemptingRegistrationWithPersistentAuthError;
    case CWVTrustedVaultStateAlreadyRegisteredV1:
      return syncer::TrustedVaultDeviceRegistrationStateForUMA::
          kAlreadyRegisteredV1;
  }
}
}  // namespace

@implementation CWVTrustedVaultUtils

+ (void)logTrustedVaultDidUpdateState:(CWVTrustedVaultState)state {
  syncer::RecordTrustedVaultDeviceRegistrationState(
      CWVConvertTrustedVaultState(state));
}

+ (void)logTrustedVaultDidReceiveHTTPStatusCode:(NSInteger)statusCode {
  syncer::RecordTrustedVaultURLFetchResponse(
      statusCode, /*net_error=*/0,
      syncer::TrustedVaultURLFetchReasonForUMA::kUnspecified);
}

+ (void)logTrustedVaultDidFailKeyDistribution:(NSError*)error {
  // TODO(crbug.com/1266130): Check to see if any UMA logging needs to occur.
}

@end
