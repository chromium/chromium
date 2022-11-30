// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_TRIAL_COMPARISON_CERT_VERIFIER_MOJO_H_
#define SERVICES_CERT_VERIFIER_TRIAL_COMPARISON_CERT_VERIFIER_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_verifier.h"
#include "services/cert_verifier/public/mojom/trial_comparison_cert_verifier.mojom.h"

namespace net {
class CertVerifyProc;
class CertVerifyProcFactory;
class CertNetFetcher;
class CertVerifyResult;
class TrialComparisonCertVerifier;
class ChromeRootStoreData;
}  // namespace net

FORWARD_DECLARE_TEST(TrialComparisonCertVerifierMojoTest, SendReportDebugInfo);

namespace cert_verifier {

// Wrapper around TrialComparisonCertVerifier that does trial configuration and
// reporting over Mojo pipes.
class TrialComparisonCertVerifierMojo
    : public net::CertVerifierWithUpdatableProc,
      public mojom::TrialComparisonCertVerifierConfigClient {
 public:
  // |initial_allowed| is the initial setting for whether the trial is allowed.
  // |config_client_receiver| is the Mojo pipe over which trial configuration
  // updates are received.
  // |report_client| is the Mojo pipe used to send trial reports.
  // |primary_verify_proc| and |trial_verify_proc| are the CertVerifyProc
  // implementations to compare.
  TrialComparisonCertVerifierMojo(
      bool initial_allowed,
      mojo::PendingReceiver<mojom::TrialComparisonCertVerifierConfigClient>
          config_client_receiver,
      mojo::PendingRemote<mojom::TrialComparisonCertVerifierReportClient>
          report_client,
      scoped_refptr<net::CertVerifyProc> primary_verify_proc,
      scoped_refptr<net::CertVerifyProcFactory> primary_verify_proc_factory,
      scoped_refptr<net::CertVerifyProc> trial_verify_proc,
      scoped_refptr<net::CertVerifyProcFactory> trial_verify_proc_factory);

  TrialComparisonCertVerifierMojo(const TrialComparisonCertVerifierMojo&) =
      delete;
  TrialComparisonCertVerifierMojo& operator=(
      const TrialComparisonCertVerifierMojo&) = delete;

  ~TrialComparisonCertVerifierMojo() override;

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override;
  void SetConfig(const Config& config) override;
  void UpdateChromeRootStoreData(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      const net::ChromeRootStoreData* root_store_data) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(::TrialComparisonCertVerifierMojoTest,
                           SendReportDebugInfo);

  // mojom::TrialComparisonCertVerifierConfigClient implementation:
  void OnTrialConfigUpdated(bool allowed) override;

  void OnSendTrialReport(
      const std::string& hostname,
      const scoped_refptr<net::X509Certificate>& unverified_cert,
      bool enable_rev_checking,
      bool require_rev_checking_local_anchors,
      bool enable_sha1_local_anchors,
      bool disable_symantec_enforcement,
      const std::string& stapled_ocsp,
      const std::string& sct_list,
      const net::CertVerifyResult& primary_result,
      const net::CertVerifyResult& trial_result);

  mojo::Receiver<mojom::TrialComparisonCertVerifierConfigClient> receiver_;

  mojo::Remote<mojom::TrialComparisonCertVerifierReportClient> report_client_;

  std::unique_ptr<net::TrialComparisonCertVerifier>
      trial_comparison_cert_verifier_;
};

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_TRIAL_COMPARISON_CERT_VERIFIER_MOJO_H_
