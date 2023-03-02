// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_TRIAL_COMPARISON_CERT_VERIFIER_UTIL_H_
#define NET_CERT_TRIAL_COMPARISON_CERT_VERIFIER_UTIL_H_

#include "net/base/net_export.h"
#include "net/cert/cert_verify_result.h"

namespace net {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TrialComparisonResult {
  kInvalid = 0,
  kEqual = 1,
  kPrimaryValidSecondaryError = 2,
  kPrimaryErrorSecondaryValid = 3,
  kBothValidDifferentDetails = 4,
  kBothErrorDifferentDetails = 5,
  kIgnoredMacUndesiredRevocationChecking = 6,
  kIgnoredMultipleEVPoliciesAndOneMatchesRoot = 7,
  kIgnoredDifferentPathReVerifiesEquivalent = 8,
  // Deprecated: kIgnoredLocallyTrustedLeaf = 9,
  kIgnoredConfigurationChanged = 10,
  kIgnoredSHA1SignaturePresent = 11,
  kIgnoredWindowsRevCheckingEnabled = 12,
  kIgnoredBothAuthorityInvalid = 13,
  kIgnoredBothKnownRoot = 14,
  kIgnoredBuiltinAuthorityInvalidPlatformSymantec = 15,
  kIgnoredLetsEncryptExpiredRoot = 16,
  kIgnoredAndroidErrorDatePriority = 17,
  kMaxValue = kIgnoredAndroidErrorDatePriority,
};

NET_EXPORT_PRIVATE bool CertVerifyResultEqual(const CertVerifyResult& a,
                                              const CertVerifyResult& b);

NET_EXPORT_PRIVATE TrialComparisonResult
IsSynchronouslyIgnorableDifference(int primary_error,
                                   const CertVerifyResult& primary_result,
                                   int trial_error,
                                   const CertVerifyResult& trial_result,
                                   bool sha1_local_anchors_enabled);

}  // namespace net

#endif  // NET_CERT_TRIAL_COMPARISON_CERT_VERIFIER_UTIL_H_
