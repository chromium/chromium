// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_trusted_vault_utils.h"

#import "components/trusted_vault/local_recovery_factor.h"
#import "components/trusted_vault/trusted_vault_histograms.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"

namespace {
trusted_vault::TrustedVaultRecoveryFactorRegistrationStateForUMA
CWVConvertTrustedVaultState(CWVTrustedVaultState state) {
  switch (state) {
    case CWVTrustedVaultStateAlreadyRegisteredV0:
      return trusted_vault::TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAlreadyRegisteredV0;
    case CWVTrustedVaultStateLocalKeysAreStale:
      return trusted_vault::TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kLocalKeysAreStale;
    case CWVTrustedVaultStateThrottledClientSide:
      return trusted_vault::TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kThrottledClientSide;
    case CWVTrustedVaultStateAttemptingRegistrationWithNewKeyPair:
      return trusted_vault::TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAttemptingRegistrationWithNewKeyPair;
    case CWVTrustedVaultStateAttemptingRegistrationWithExistingKeyPair:
      return trusted_vault::TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAttemptingRegistrationWithExistingKeyPair;
    case CWVTrustedVaultStateAttemptingRegistrationWithPersistentAuthError:
      // TODO(crbug.com/40257503): remove CWV version of this bucket.
      return trusted_vault::TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kDeprecatedAttemptingRegistrationWithPersistentAuthError;
    case CWVTrustedVaultStateAlreadyRegisteredV1:
      return trusted_vault::TrustedVaultRecoveryFactorRegistrationStateForUMA::
          kAlreadyRegisteredV1;
  }
}
}  // namespace

@implementation CWVTrustedVaultUtils

+ (void)logTrustedVaultDidUpdateState:(CWVTrustedVaultState)state {
  trusted_vault::RecordTrustedVaultRecoveryFactorRegistrationState(
      trusted_vault::LocalRecoveryFactorType::kPhysicalDevice,
      trusted_vault::SecurityDomainId::kChromeSync,
      CWVConvertTrustedVaultState(state));
}

+ (void)logTrustedVaultDidReceiveHTTPStatusCode:(NSInteger)statusCode {
  trusted_vault::RecordTrustedVaultURLFetchResponse(
      trusted_vault::SecurityDomainId::kChromeSync,
      trusted_vault::TrustedVaultURLFetchReasonForUMA::kUnspecified, statusCode,
      /*net_error=*/0);
}

+ (void)logTrustedVaultDidFailKeyDistribution:(NSError*)error {
  // TODO(crbug.com/40204010): Check to see if any UMA logging needs to occur.
}

@end
