// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/trial_comparison_cert_verifier_mojo.h"

#include "base/containers/span.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/cert_verify_result.h"
#include "net/der/encode_values.h"
#include "net/der/parse_values.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "net/cert/cert_verify_proc_mac.h"
#include "net/cert/internal/trust_store_mac.h"
#endif
#if BUILDFLAG(IS_WIN)
#include "net/cert/cert_verify_proc_win.h"
#endif

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
  constexpr uint32_t kExpectedTrustResult = 4;
  constexpr int32_t kExpectedResultCode = -12345;
  std::vector<net::CertVerifyProcMac::ResultDebugData::CertEvidenceInfo>
      expected_status_chain;
  net::CertVerifyProcMac::ResultDebugData::CertEvidenceInfo info;
  info.status_bits = 23456;
  expected_status_chain.push_back(info);
  info.status_bits = 34567;
  info.status_codes.push_back(-4567);
  info.status_codes.push_back(-56789);
  expected_status_chain.push_back(info);
  info.status_bits = 45678;
  info.status_codes.clear();
  info.status_codes.push_back(-97261);
  expected_status_chain.push_back(info);
  net::CertVerifyProcMac::ResultDebugData::Create(
      kExpectedTrustResult, kExpectedResultCode, expected_status_chain,
      &primary_result);

  constexpr int kExpectedTrustDebugInfo = 0xABCD;
  auto* mac_trust_debug_info =
      net::TrustStoreMac::ResultDebugData::GetOrCreate(&trial_result);
  ASSERT_TRUE(mac_trust_debug_info);
  mac_trust_debug_info->UpdateTrustDebugInfo(
      kExpectedTrustDebugInfo, net::TrustStoreMac::TrustImplType::kSimple);
#endif
#if BUILDFLAG(IS_WIN)
  std::vector<uint8_t> authroot_sequence{'T', 'E', 'S', 'T'};
  net::CertVerifyProcWin::ResultDebugData::Create(time, authroot_sequence,
                                                  &primary_result);
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
      true, {}, std::move(report_client_remote), nullptr, nullptr, nullptr,
      nullptr);

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
  ASSERT_TRUE(report.debug_info->mac_platform_debug_info);
  EXPECT_EQ(kExpectedTrustResult,
            report.debug_info->mac_platform_debug_info->trust_result);
  EXPECT_EQ(kExpectedResultCode,
            report.debug_info->mac_platform_debug_info->result_code);
  ASSERT_EQ(expected_status_chain.size(),
            report.debug_info->mac_platform_debug_info->status_chain.size());
  for (size_t i = 0; i < expected_status_chain.size(); ++i) {
    EXPECT_EQ(expected_status_chain[i].status_bits,
              report.debug_info->mac_platform_debug_info->status_chain[i]
                  ->status_bits);
    EXPECT_EQ(expected_status_chain[i].status_codes,
              report.debug_info->mac_platform_debug_info->status_chain[i]
                  ->status_codes);
  }

  EXPECT_EQ(kExpectedTrustDebugInfo,
            report.debug_info->mac_combined_trust_debug_info);
  EXPECT_EQ(
      cert_verifier::mojom::CertVerifierDebugInfo::MacTrustImplType::kSimple,
      report.debug_info->mac_trust_impl);
#endif
#if BUILDFLAG(IS_WIN)
  ASSERT_TRUE(report.debug_info->win_platform_debug_info);
  EXPECT_EQ(time,
            report.debug_info->win_platform_debug_info->authroot_this_update);
  EXPECT_EQ(
      authroot_sequence,
      report.debug_info->win_platform_debug_info->authroot_sequence_number);
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
