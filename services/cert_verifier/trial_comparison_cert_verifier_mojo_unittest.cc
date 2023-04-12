// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/trial_comparison_cert_verifier_mojo.h"

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/der/encode_values.h"
#include "net/der/parse_values.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "net/cert/internal/trust_store_mac.h"
#endif
#if BUILDFLAG(USE_NSS_CERTS)
#include <nss.h>
#include "net/cert/internal/trust_store_nss.h"
#endif

namespace {

struct ReceivedReport {
  std::string hostname;
  scoped_refptr<net::X509Certificate> unverified_cert;
  bool enable_rev_checking;
  bool require_rev_checking_local_anchors;
  bool enable_sha1_local_anchors;
  bool disable_symantec_enforcement;
  std::vector<uint8_t> stapled_ocsp;
  std::vector<uint8_t> sct_list;
  net::CertVerifyResult primary_result;
  net::CertVerifyResult trial_result;
  cert_verifier::mojom::CertVerifierDebugInfoPtr debug_info;
};

class FakeReportClient
    : public cert_verifier::mojom::TrialComparisonCertVerifierReportClient {
 public:
  explicit FakeReportClient(
      mojo::PendingReceiver<
          cert_verifier::mojom::TrialComparisonCertVerifierReportClient>
          report_client_receiver)
      : receiver_(this, std::move(report_client_receiver)) {}

  // TrialComparisonCertVerifierReportClient implementation:
  void SendTrialReport(
      const std::string& hostname,
      const scoped_refptr<net::X509Certificate>& unverified_cert,
      bool enable_rev_checking,
      bool require_rev_checking_local_anchors,
      bool enable_sha1_local_anchors,
      bool disable_symantec_enforcement,
      const std::vector<uint8_t>& stapled_ocsp,
      const std::vector<uint8_t>& sct_list,
      const net::CertVerifyResult& primary_result,
      const net::CertVerifyResult& trial_result,
      cert_verifier::mojom::CertVerifierDebugInfoPtr debug_info) override {
    ReceivedReport report;
    report.hostname = hostname;
    report.unverified_cert = unverified_cert;
    report.enable_rev_checking = enable_rev_checking;
    report.require_rev_checking_local_anchors =
        require_rev_checking_local_anchors;
    report.enable_sha1_local_anchors = enable_sha1_local_anchors;
    report.disable_symantec_enforcement = disable_symantec_enforcement;
    report.stapled_ocsp = stapled_ocsp;
    report.sct_list = sct_list;
    report.primary_result = primary_result;
    report.trial_result = trial_result;
    report.debug_info = std::move(debug_info);
    reports_.push_back(std::move(report));

    run_loop_.Quit();
  }

  const std::vector<ReceivedReport>& reports() const { return reports_; }

  void WaitForReport() { run_loop_.Run(); }

 private:
  mojo::Receiver<cert_verifier::mojom::TrialComparisonCertVerifierReportClient>
      receiver_;

  std::vector<ReceivedReport> reports_;
  base::RunLoop run_loop_;
};

class NotCalledCertVerifyProc : public net::CertVerifyProc {
 public:
  NotCalledCertVerifyProc()
      : net::CertVerifyProc(net::CRLSet::BuiltinCRLSet()) {}

  bool SupportsAdditionalTrustAnchors() const override { return false; }

 protected:
  ~NotCalledCertVerifyProc() override = default;

 private:
  int VerifyInternal(net::X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     const net::CertificateList& additional_trust_anchors,
                     net::CertVerifyResult* verify_result,
                     const net::NetLogWithSource& net_log) override {
    ADD_FAILURE() << "NotCalledCertVerifyProc was called!";
    return net::ERR_UNEXPECTED;
  }
};

class NotCalledProcFactory : public net::CertVerifyProcFactory {
 public:
  scoped_refptr<net::CertVerifyProc> CreateCertVerifyProc(
      scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
      const ImplParams& impl_params) override {
    return base::MakeRefCounted<NotCalledCertVerifyProc>();
  }

 protected:
  ~NotCalledProcFactory() override = default;
};

}  // namespace

TEST(TrialComparisonCertVerifierMojoTest, SendReportDebugInfo) {
  base::test::TaskEnvironment scoped_task_environment;

  scoped_refptr<net::X509Certificate> unverified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  scoped_refptr<net::X509Certificate> chain1 =
      net::CreateCertificateChainFromFile(net::GetTestCertsDirectory(),
                                          "x509_verify_results.chain.pem",
                                          net::X509Certificate::FORMAT_AUTO);
  scoped_refptr<net::X509Certificate> chain2 =
      net::CreateCertificateChainFromFile(net::GetTestCertsDirectory(),
                                          "multi-root-chain1.pem",
                                          net::X509Certificate::FORMAT_AUTO);
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = chain1;
  net::CertVerifyResult trial_result;
  trial_result.verified_cert = chain2;

  base::Time time = base::Time::Now();

#if BUILDFLAG(IS_MAC)
  constexpr int kExpectedTrustDebugInfo = 0xABCD;
  auto* mac_trust_debug_info =
      net::TrustStoreMac::ResultDebugData::GetOrCreate(&trial_result);
  ASSERT_TRUE(mac_trust_debug_info);
  mac_trust_debug_info->UpdateTrustDebugInfo(
      kExpectedTrustDebugInfo, net::TrustStoreMac::TrustImplType::kSimple);
#endif
#if BUILDFLAG(USE_NSS_CERTS)
  net::TrustStoreNSS::ResultDebugData::Create(
      false, net::TrustStoreNSS::ResultDebugData::SlotFilterType::kDontFilter,
      &primary_result);
  net::TrustStoreNSS::ResultDebugData::Create(
      true,
      net::TrustStoreNSS::ResultDebugData::SlotFilterType::
          kAllowSpecifiedUserSlot,
      &trial_result);
#endif
  absl::optional<int64_t> chrome_root_store_version = absl::nullopt;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  chrome_root_store_version = 42;
#endif

  net::der::GeneralizedTime der_time;
  der_time.year = 2019;
  der_time.month = 9;
  der_time.day = 27;
  der_time.hours = 22;
  der_time.minutes = 11;
  der_time.seconds = 8;
  net::CertVerifyProcBuiltinResultDebugData::Create(
      &trial_result, time, der_time, chrome_root_store_version);

  mojo::PendingRemote<
      cert_verifier::mojom::TrialComparisonCertVerifierReportClient>
      report_client_remote;
  FakeReportClient report_client(
      report_client_remote.InitWithNewPipeAndPassReceiver());
  cert_verifier::TrialComparisonCertVerifierMojo tccvm(
      true, {}, std::move(report_client_remote),
      base::MakeRefCounted<NotCalledProcFactory>(), nullptr, {});

  tccvm.OnSendTrialReport("example.com", unverified_cert, false, false, false,
                          false, "stapled ocsp", "sct list", primary_result,
                          trial_result);

  report_client.WaitForReport();

  ASSERT_EQ(1U, report_client.reports().size());
  const ReceivedReport& report = report_client.reports()[0];
  EXPECT_TRUE(
      unverified_cert->EqualsIncludingChain(report.unverified_cert.get()));
  EXPECT_TRUE(
      chain1->EqualsIncludingChain(report.primary_result.verified_cert.get()));
  EXPECT_TRUE(
      chain2->EqualsIncludingChain(report.trial_result.verified_cert.get()));
  EXPECT_EQ("stapled ocsp", std::string(report.stapled_ocsp.begin(),
                                        report.stapled_ocsp.end()));
  EXPECT_EQ("sct list",
            std::string(report.sct_list.begin(), report.sct_list.end()));

  ASSERT_TRUE(report.debug_info);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(kExpectedTrustDebugInfo,
            report.debug_info->mac_combined_trust_debug_info);
  EXPECT_EQ(
      cert_verifier::mojom::CertVerifierDebugInfo::MacTrustImplType::kSimple,
      report.debug_info->mac_trust_impl);
#endif
#if BUILDFLAG(USE_NSS_CERTS)
  EXPECT_EQ(NSS_GetVersion(), report.debug_info->nss_version);

  ASSERT_TRUE(report.debug_info->primary_nss_debug_info);
  EXPECT_EQ(
      false,
      report.debug_info->primary_nss_debug_info->ignore_system_trust_settings);
  EXPECT_EQ(
      cert_verifier::mojom::TrustStoreNSSDebugInfo::SlotFilterType::kDontFilter,
      report.debug_info->primary_nss_debug_info->slot_filter_type);

  ASSERT_TRUE(report.debug_info->trial_nss_debug_info);
  EXPECT_EQ(
      true,
      report.debug_info->trial_nss_debug_info->ignore_system_trust_settings);
  EXPECT_EQ(cert_verifier::mojom::TrustStoreNSSDebugInfo::SlotFilterType::
                kAllowSpecifiedUserSlot,
            report.debug_info->trial_nss_debug_info->slot_filter_type);
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  ASSERT_TRUE(report.debug_info->chrome_root_store_debug_info);
  EXPECT_EQ(chrome_root_store_version.value(),
            report.debug_info->chrome_root_store_debug_info
                ->chrome_root_store_version);
#endif

  EXPECT_EQ(time, report.debug_info->trial_verification_time);
  EXPECT_EQ("20190927221108Z", report.debug_info->trial_der_verification_time);
}

TEST(TrialComparisonCertVerifierMojoTest, ObserverIsCalledOnCRSUpdate) {
  base::test::TaskEnvironment scoped_task_environment;

  mojo::PendingRemote<
      cert_verifier::mojom::TrialComparisonCertVerifierReportClient>
      report_client_remote;
  FakeReportClient report_client(
      report_client_remote.InitWithNewPipeAndPassReceiver());
  cert_verifier::TrialComparisonCertVerifierMojo tccvm(
      true, {}, std::move(report_client_remote),
      base::MakeRefCounted<NotCalledProcFactory>(), nullptr, {});

  net::CertVerifierObserverCounter observer_(&tccvm);
  EXPECT_EQ(observer_.change_count(), 0u);
  tccvm.UpdateVerifyProcData(nullptr, {});
  // Observer is called twice since the TrialComparisonCertVerifier currently
  // forwards notifications from both the primary and secondary verifiers.
  EXPECT_EQ(observer_.change_count(), 2u);
}
