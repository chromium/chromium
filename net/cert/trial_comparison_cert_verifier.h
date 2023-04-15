// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_TRIAL_COMPARISON_CERT_VERIFIER_H_
#define NET_CERT_TRIAL_COMPARISON_CERT_VERIFIER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verifier.h"

namespace net {
class CertVerifyProcFactory;
class CertNetFetcher;

// TrialComparisonCertVerifier is a CertVerifier that can be used to compare
// the results between two different CertVerifyProcs. The results are reported
// back to the caller via a ReportCallback, allowing the caller to further
// examine the differences.
class NET_EXPORT TrialComparisonCertVerifier
    : public CertVerifierWithUpdatableProc,
      public CertVerifier::Observer {
 public:
  using ReportCallback = base::RepeatingCallback<void(
      const std::string& hostname,
      const scoped_refptr<X509Certificate>& unverified_cert,
      bool enable_rev_checking,
      bool require_rev_checking_local_anchors,
      bool enable_sha1_local_anchors,
      bool disable_symantec_enforcement,
      const std::string& stapled_ocsp,
      const std::string& sct_list,
      const net::CertVerifyResult& primary_result,
      const net::CertVerifyResult& trial_result)>;

  // Create a new TrialComparisonCertVerifier. The `verify_proc_factory` will
  // be used to create the underlying primary and trial verifiers.
  //
  // Initially, no trial verifications will actually be performed; that is,
  // calls to Verify() will be dispatched to the underlying `primary_verifier_`
  // or `trial_verifier_` depending on the `impl_params`. This can be changed
  // by calling `set_trial_allowed()`.
  //
  // When trial verifications are enabled, calls to Verify() will first call
  // into `primary_verifier_` to verify. The result of this verification will
  // be immediately returned to the caller of Verify, allowing them to proceed.
  // However, the verifier will continue in the background, attempting to
  // verify the same RequestParams using `trial_verifier_`. If there are
  // differences in the results, they will be reported via `report_callback`,
  // allowing the creator to receive information about differences.
  //
  // If the caller abandons the CertVerifier::Request prior to the primary
  // verification completed, no trial verification will be done. However, once
  // the primary verifier has returned, the trial verifications will continue,
  // provided that the underlying configuration has not been changed by
  // calling `SetConfig()` or `UpdateVerifyProcData()`.
  //
  // Note that there may be multiple calls to both the primary CertVerifyProc
  // and trial CertVerifyProc, using different parameters to account for
  // platform differences.
  TrialComparisonCertVerifier(
      scoped_refptr<CertVerifyProcFactory> verify_proc_factory,
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      const CertVerifyProcFactory::ImplParams& impl_params,
      ReportCallback report_callback);

  TrialComparisonCertVerifier(const TrialComparisonCertVerifier&) = delete;
  TrialComparisonCertVerifier& operator=(const TrialComparisonCertVerifier&) =
      delete;

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
  void AddObserver(CertVerifier::Observer* observer) override;
  void RemoveObserver(CertVerifier::Observer* observer) override;
  void UpdateVerifyProcData(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      const CertVerifyProcFactory::ImplParams& impl_params) override;

 private:
  class Job;
  friend class Job;

  CertVerifier* primary_verifier() const { return primary_verifier_.get(); }
  CertVerifier* primary_reverifier() const { return primary_reverifier_.get(); }
  CertVerifier* trial_verifier() const { return trial_verifier_.get(); }

  void RemoveJob(Job* job_ptr);
  void NotifyJobsOfConfigChange();

  // Processes the params from the caller, updates the state of the trial and
  // returns the pair of {primary verifier params, trial verifier params} to
  // be used in creating or updating the wrapped verifiers.
  std::tuple<CertVerifyProcFactory::ImplParams,
             CertVerifyProcFactory::ImplParams>
  ProcessImplParams(const CertVerifyProcFactory::ImplParams& impl_params);

  // CertVerifier::Observer methods:
  void OnCertVerifierChanged() override;

  // Whether the trial is allowed.
  bool allowed_ = false;
  // Callback that reports are sent to.
  ReportCallback report_callback_;

  // The actual `use_chrome_root_store` value that is requested by the caller.
  // This determines which verifier's result is returned to the caller as the
  // actual verification result.
  bool actual_use_chrome_root_store_;

  CertVerifier::Config config_;

  std::unique_ptr<CertVerifierWithUpdatableProc> primary_verifier_;
  std::unique_ptr<CertVerifierWithUpdatableProc> primary_reverifier_;
  std::unique_ptr<CertVerifierWithUpdatableProc> trial_verifier_;

  std::set<std::unique_ptr<Job>, base::UniquePtrComparator> jobs_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_CERT_TRIAL_COMPARISON_CERT_VERIFIER_H_
