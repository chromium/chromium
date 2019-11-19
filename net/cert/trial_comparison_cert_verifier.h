// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_TRIAL_COMPARISON_CERT_VERIFIER_H_
#define NET_CERT_TRIAL_COMPARISON_CERT_VERIFIER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verifier.h"

namespace net {
class CertVerifyProc;

// TrialComparisonCertVerifier is a CertVerifier that can be used to compare
// the results between two different CertVerifyProcs. The results are reported
// back to the caller via a ReportCallback, allowing the caller to further
// examine the differences.
class NET_EXPORT TrialComparisonCertVerifier : public CertVerifier {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum TrialComparisonResult {
    kInvalid = 0,
    kEqual = 1,
    kPrimaryValidSecondaryError = 2,
    kPrimaryErrorSecondaryValid = 3,
    kBothValidDifferentDetails = 4,
    kBothErrorDifferentDetails = 5,
    kIgnoredMacUndesiredRevocationChecking = 6,
    kIgnoredMultipleEVPoliciesAndOneMatchesRoot = 7,
    kIgnoredDifferentPathReVerifiesEquivalent = 8,
    kIgnoredLocallyTrustedLeaf = 9,
    kIgnoredConfigurationChanged = 10,
    kMaxValue = kIgnoredConfigurationChanged
  };

  using ReportCallback = base::RepeatingCallback<void(
      const std::string& hostname,
      const scoped_refptr<X509Certificate>& unverified_cert,
      bool enable_rev_checking,
      bool require_rev_checking_local_anchors,
      bool enable_sha1_local_anchors,
      bool disable_symantec_enforcement,
      const net::CertVerifyResult& primary_result,
      const net::CertVerifyResult& trial_result)>;

  // Create a new TrialComparisonCertVerifier. Initially, no trial
  // verifications will actually be performed; that is, calls to Verify() will
  // be dispatched to the underlying |primary_verify_proc|. This can be changed
  // by calling set_trial_allowed().
  //
  // When trial verifications are enabled, calls to Verify() will first call
  // into |primary_verify_proc| to verify. The result of this verification will
  // be immediately returned to the caller of Verify, allowing them to proceed.
  // However, the verifier will continue in the background, attempting to
  // verify the same RequestParams using |trial_verify_proc|. If there are
  // differences in the results, they will be reported via |report_callback|,
  // allowing the creator to receive information about differences.
  //
  // If the caller abandons the CertVerifier::Request prior to the primary
  // verification completed, no trial verification will be done. However, once
  // the primary verifier has returned, the trial verifications will continue,
  // provided that the underlying configuration has not been changed by
  // calling SetConfig().
  //
  // Note that there may be multiple calls to both |primary_verify_proc| and
  // |trial_verify_proc|, using different parameters to account for platform
  // differences.
  //
  // TODO(rsleevi): Make the types distinct, to guarantee that
  // |primary_verify_proc| is a System CertVerifyProc, and |trial_verify_proc|
  // is the Builtin CertVerifyProc.
  TrialComparisonCertVerifier(scoped_refptr<CertVerifyProc> primary_verify_proc,
                              scoped_refptr<CertVerifyProc> trial_verify_proc,
                              ReportCallback report_callback);

  ~TrialComparisonCertVerifier() override;

  void set_trial_allowed(bool allowed) { allowed_ = allowed; }
  bool trial_allowed() const { return allowed_; }

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             CertVerifyResult* verify_result,
             CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const NetLogWithSource& net_log) override;
  void SetConfig(const Config& config) override;

 private:
  class Job;
  friend class Job;

  CertVerifier* primary_verifier() const { return primary_verifier_.get(); }
  CertVerifier* primary_reverifier() const { return primary_reverifier_.get(); }
  CertVerifier* trial_verifier() const { return trial_verifier_.get(); }
  CertVerifier* revocation_trial_verifier() const {
    return revocation_trial_verifier_.get();
  }

  void RemoveJob(Job* job_ptr);

  // Whether the trial is allowed.
  bool allowed_ = false;
  // Callback that reports are sent to.
  ReportCallback report_callback_;

  CertVerifier::Config config_;

  std::unique_ptr<CertVerifier> primary_verifier_;
  std::unique_ptr<CertVerifier> primary_reverifier_;
  std::unique_ptr<CertVerifier> trial_verifier_;
  // Similar to |trial_verifier_|, except configured to always check
  // revocation information.
  std::unique_ptr<CertVerifier> revocation_trial_verifier_;

  std::set<std::unique_ptr<Job>, base::UniquePtrComparator> jobs_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(TrialComparisonCertVerifier);
};

}  // namespace net

#endif  // NET_CERT_TRIAL_COMPARISON_CERT_VERIFIER_H_
