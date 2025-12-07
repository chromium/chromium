// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_BUILTIN_H_
#define NET_CERT_CERT_VERIFY_PROC_BUILTIN_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/network_time/time_tracker/time_tracker.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verify_proc.h"

namespace net {

class CertNetFetcher;
class CRLSet;
class CTPolicyEnforcer;
class CTVerifier;
class SystemTrustStore;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(NetCertVerifier1QwacResult)
enum class Verify1QwacResult {
  kNotQwac = 0,
  kInconsistentBits = 1,
  kFailedVerification = 2,
  kValid1Qwac = 3,
  kMaxValue = kValid1Qwac,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:NetCertVerifier1QwacResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(NetCertVerifier2QwacBindingResult)
enum class Verify2QwacBindingResult {
  kOtherError = 0,
  kValid2QwacBinding = 1,
  kBindingParsingError = 2,
  kBindingSignatureInvalid = 3,
  kTlsCertNotBound = 4,
  kCertLeafParsingError = 5,
  kCertNotQwac = 6,
  kCertInconsistentBits = 7,
  kCertNameInvalid = 8,
  kCertDateInvalid = 9,
  kCertAuthorityInvalid = 10,
  kCertInvalid = 11,
  kCertWeakKey = 12,
  kCertNameConstraintViolation = 13,
  kCertOtherError = 14,

  kMaxValue = kCertOtherError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:NetCertVerifier2QwacBindingResult)

// TODO(crbug.com/41276779): This is not how other cert_verify_proc_*.h are
// implemented -- they expose the type in the header. Use a consistent style
// here too.
NET_EXPORT scoped_refptr<CertVerifyProc> CreateCertVerifyProcBuiltin(
    scoped_refptr<CertNetFetcher> net_fetcher,
    scoped_refptr<CRLSet> crl_set,
    std::unique_ptr<CTVerifier> ct_verifier,
    scoped_refptr<CTPolicyEnforcer> ct_policy_enforcer,
    std::unique_ptr<SystemTrustStore> system_trust_store,
    const CertVerifyProc::InstanceParams& instance_params,
    std::optional<network_time::TimeTracker> time_tracker);

// Returns the time limit used by CertVerifyProcBuiltin. Intended for test use.
NET_EXPORT_PRIVATE base::TimeDelta
GetCertVerifyProcBuiltinTimeLimitForTesting();

}  // namespace net

#endif  // NET_CERT_CERT_VERIFY_PROC_BUILTIN_H_
