// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/proof_verifier_chromium.h"

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/cert_and_ct_verifier.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cert/x509_util.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quic/core/crypto/proof_verifier.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace net {
namespace test {

namespace {

const char kCTAndPKPHost[] = "pkp-expect-ct.preloaded.test";

// CertVerifier that will fail the test if it is ever called.
class FailsTestCertVerifier : public CertVerifier {
 public:
  FailsTestCertVerifier() {}
  ~FailsTestCertVerifier() override {}

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             CertVerifyResult* verify_result,
             CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const NetLogWithSource& net_log) override {
    ADD_FAILURE() << "CertVerifier::Verify() should not be called";
    return ERR_FAILED;
  }
  void SetConfig(const Config& config) override {}
};

// A mock CTPolicyEnforcer that returns a custom verification result.
class MockCTPolicyEnforcer : public CTPolicyEnforcer {
 public:
  MOCK_METHOD3(CheckCompliance,
               ct::CTPolicyCompliance(X509Certificate* cert,
                                      const ct::SCTList&,
                                      const NetLogWithSource&));
};

class MockRequireCTDelegate : public TransportSecurityState::RequireCTDelegate {
 public:
  MOCK_METHOD3(IsCTRequiredForHost,
               CTRequirementLevel(const std::string& host,
                                  const X509Certificate* chain,
                                  const HashValueVector& hashes));
};

class MockSCTAuditingDelegate : public SCTAuditingDelegate {
 public:
  MOCK_METHOD(bool, IsSCTAuditingEnabled, ());
  MOCK_METHOD(void,
              MaybeEnqueueReport,
              (const net::HostPortPair&,
               const net::X509Certificate*,
               const net::SignedCertificateTimestampAndStatusList&));
};

// Proof source callback which saves the signature into |signature|.
class SignatureSaver : public quic::ProofSource::Callback {
 public:
  explicit SignatureSaver(std::string* signature) : signature_(signature) {}
  ~SignatureSaver() override {}

  void Run(bool /*ok*/,
           const quic::QuicReferenceCountedPointer<
               quic::ProofSource::Chain>& /*chain*/,
           const quic::QuicCryptoProof& proof,
           std::unique_ptr<quic::ProofSource::Details> /*details*/) override {
    *signature_ = proof.signature;
  }

  std::string* signature_;
};

class DummyProofVerifierCallback : public quic::ProofVerifierCallback {
 public:
  DummyProofVerifierCallback() {}
  ~DummyProofVerifierCallback() override {}

  void Run(bool ok,
           const std::string& error_details,
           std::unique_ptr<quic::ProofVerifyDetails>* details) override {
    // Do nothing
  }
};

const char kTestHostname[] = "test.example.com";
const uint16_t kTestPort = 8443;
const char kTestConfig[] = "server config bytes";
const char kTestChloHash[] = "CHLO hash";
const char kTestEmptyOCSPResponse[] = "";
const char kTestEmptySCT[] = "";
const char kTestEmptySignature[] = "";

const char kLogDescription[] = "somelog";

// This test exercises code that does not depend on the QUIC version in use
// but that still requires a version so we just use the first one.
const quic::QuicTransportVersion kTestTransportVersion =
    quic::AllSupportedVersions().front().transport_version;

}  // namespace

// A mock ReportSenderInterface that just remembers the latest report
// URI and its NetworkIsolationKey.
class MockCertificateReportSender
    : public TransportSecurityState::ReportSenderInterface {
 public:
  MockCertificateReportSender() = default;
  ~MockCertificateReportSender() override = default;

  void Send(
      const GURL& report_uri,
      base::StringPiece content_type,
      base::StringPiece report,
      const NetworkIsolationKey& network_isolation_key,
      base::OnceCallback<void()> success_callback,
      base::OnceCallback<void(const GURL&, int, int)> error_callback) override {
    latest_report_uri_ = report_uri;
    latest_network_isolation_key_ = network_isolation_key;
  }

  const GURL& latest_report_uri() { return latest_report_uri_; }
  const NetworkIsolationKey& latest_network_isolation_key() {
    return latest_network_isolation_key_;
  }

 private:
  GURL latest_report_uri_;
  NetworkIsolationKey latest_network_isolation_key_;
};

class ProofVerifierChromiumTest : public ::testing::Test {
 public:
  ProofVerifierChromiumTest()
      : verify_context_(new ProofVerifyContextChromium(0 /*cert_verify_flags*/,
                                                       NetLogWithSource())) {}

  void SetUp() override {
    EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
        .WillRepeatedly(
            Return(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));

    static const char kTestCert[] = "quic-chain.pem";
    test_cert_ = ImportCertFromFile(GetTestCertsDirectory(), kTestCert);
    ASSERT_TRUE(test_cert_);
    certs_.clear();
    certs_.emplace_back(
        x509_util::CryptoBufferAsStringPiece(test_cert_->cert_buffer()));

    dummy_result_.verified_cert = test_cert_;
    dummy_result_.is_issued_by_known_root = true;
  }

  std::string GetTestSignature() {
    ProofSourceChromium source;
    source.Initialize(GetTestCertsDirectory().AppendASCII("quic-chain.pem"),
                      GetTestCertsDirectory().AppendASCII("quic-leaf-cert.key"),
                      base::FilePath());
    std::string signature;
    source.GetProof(quic::QuicSocketAddress(), quic::QuicSocketAddress(),
                    kTestHostname, kTestConfig, kTestTransportVersion,
                    kTestChloHash,
                    std::make_unique<SignatureSaver>(&signature));
    return signature;
  }

  void GetSCTTestCertificates(std::vector<std::string>* certs) {
    std::string der_test_cert(ct::GetDerEncodedX509Cert());
    scoped_refptr<X509Certificate> test_cert = X509Certificate::CreateFromBytes(
        der_test_cert.data(), der_test_cert.length());
    ASSERT_TRUE(test_cert.get());

    certs->clear();
    certs->emplace_back(
        x509_util::CryptoBufferAsStringPiece(test_cert->cert_buffer()));
  }

  void CheckSCT(bool sct_expected_ok) {
    ProofVerifyDetailsChromium* proof_details =
        reinterpret_cast<ProofVerifyDetailsChromium*>(details_.get());
    const CertVerifyResult& cert_verify_result =
        proof_details->cert_verify_result;
    if (sct_expected_ok) {
      EXPECT_TRUE(ct::CheckForSingleVerifiedSCTInResult(cert_verify_result.scts,
                                                        kLogDescription));
      EXPECT_TRUE(ct::CheckForSCTOrigin(
          cert_verify_result.scts,
          ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION));
    } else {
      ASSERT_EQ(1U, cert_verify_result.scts.size());
      EXPECT_EQ(ct::SCT_STATUS_LOG_UNKNOWN, cert_verify_result.scts[0].status);
    }
  }

 protected:
  TransportSecurityState transport_security_state_;
  MockCTPolicyEnforcer ct_policy_enforcer_;

  std::unique_ptr<quic::ProofVerifyContext> verify_context_;
  std::unique_ptr<quic::ProofVerifyDetails> details_;
  std::string error_details_;
  uint8_t tls_alert_;
  std::vector<std::string> certs_;
  CertVerifyResult dummy_result_;
  scoped_refptr<X509Certificate> test_cert_;
};

TEST_F(ProofVerifierChromiumTest, VerifyProof) {
  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);
}

// Tests that the quic::ProofVerifier fails verification if certificate
// verification fails.
TEST_F(ProofVerifierChromiumTest, FailsIfCertFails) {
  MockCertVerifier dummy_verifier;
  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
}

// Valid SCT and cert
TEST_F(ProofVerifierChromiumTest, ValidSCTList) {
  // Use different certificates for SCT tests.
  ASSERT_NO_FATAL_FAILURE(GetSCTTestCertificates(&certs_));

  std::string der_test_cert(ct::GetDerEncodedX509Cert());
  scoped_refptr<X509Certificate> test_cert = X509Certificate::CreateFromBytes(
      der_test_cert.data(), der_test_cert.length());
  ASSERT_TRUE(test_cert);
  CertVerifyResult dummy_result;
  dummy_result.verified_cert = test_cert;
  dummy_result.is_issued_by_known_root = true;
  auto dummy_verifier = std::make_unique<MockCertVerifier>();
  dummy_verifier->AddResultForCert(test_cert.get(), dummy_result, OK);

  // Combine the mocked cert verify result with the results of the
  // MultiLogCTVerifier.
  std::vector<scoped_refptr<const CTLogVerifier>> log_verifiers;
  scoped_refptr<const CTLogVerifier> log(
      CTLogVerifier::Create(ct::GetTestPublicKey(), kLogDescription));
  ASSERT_TRUE(log);
  log_verifiers.push_back(log);
  auto ct_verifier = std::make_unique<MultiLogCTVerifier>();
  ct_verifier->AddLogs(log_verifiers);

  CertAndCTVerifier cert_verifier(std::move(dummy_verifier),
                                  std::move(ct_verifier));

  ProofVerifierChromium proof_verifier(&cert_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse,
      ct::GetSCTListForTesting(), verify_context_.get(), &error_details_,
      &details_, &tls_alert_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);
  CheckSCT(/*sct_expected_ok=*/true);
}

// Invalid SCT, but valid cert
TEST_F(ProofVerifierChromiumTest, InvalidSCTList) {
  // Use different certificates for SCT tests.
  ASSERT_NO_FATAL_FAILURE(GetSCTTestCertificates(&certs_));

  std::string der_test_cert(ct::GetDerEncodedX509Cert());
  scoped_refptr<X509Certificate> test_cert = X509Certificate::CreateFromBytes(
      der_test_cert.data(), der_test_cert.length());
  ASSERT_TRUE(test_cert);
  CertVerifyResult dummy_result;
  dummy_result.verified_cert = test_cert;
  dummy_result.is_issued_by_known_root = true;
  auto dummy_verifier = std::make_unique<MockCertVerifier>();
  dummy_verifier->AddResultForCert(test_cert.get(), dummy_result, OK);

  // Combine the mocked cert verify result with the results of the
  // MultiLogCTVerifier.
  std::vector<scoped_refptr<const CTLogVerifier>> log_verifiers;
  scoped_refptr<const CTLogVerifier> log(
      CTLogVerifier::Create(ct::GetTestPublicKey(), kLogDescription));
  ASSERT_TRUE(log);
  log_verifiers.push_back(log);
  auto ct_verifier = std::make_unique<MultiLogCTVerifier>();
  ct_verifier->AddLogs(log_verifiers);

  CertAndCTVerifier cert_verifier(std::move(dummy_verifier),
                                  std::move(ct_verifier));

  ProofVerifierChromium proof_verifier(&cert_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse,
      ct::GetSCTListWithInvalidSCT(), verify_context_.get(), &error_details_,
      &details_, &tls_alert_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);
  CheckSCT(/*sct_expected_ok=*/false);
}

// Tests that the quic::ProofVerifier doesn't verify certificates if the config
// signature fails.
TEST_F(ProofVerifierChromiumTest, FailsIfSignatureFails) {
  FailsTestCertVerifier cert_verifier;
  ProofVerifierChromium proof_verifier(&cert_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, kTestEmptySignature,
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
}

// Tests that the certificate policy enforcer is consulted for EV
// and the certificate is allowed to be EV.
TEST_F(ProofVerifierChromiumTest, PreservesEVIfAllowed) {
  dummy_result_.cert_status = CERT_STATUS_IS_EV;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);

  // Repeat the test with VerifyCertChain.
  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);
}

// Tests that the certificate policy enforcer is consulted for EV
// and the certificate is not allowed to be EV.
TEST_F(ProofVerifierChromiumTest, StripsEVIfNotAllowed) {
  dummy_result_.cert_status = CERT_STATUS_IS_EV;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(CERT_STATUS_CT_COMPLIANCE_FAILED,
            verify_details->cert_verify_result.cert_status &
                (CERT_STATUS_CT_COMPLIANCE_FAILED | CERT_STATUS_IS_EV));

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(CERT_STATUS_CT_COMPLIANCE_FAILED,
            verify_details->cert_verify_result.cert_status &
                (CERT_STATUS_CT_COMPLIANCE_FAILED | CERT_STATUS_IS_EV));
}

// Tests that the when a certificate's EV status is stripped to EV
// non-compliance, the correct histogram is recorded.
TEST_F(ProofVerifierChromiumTest, CTEVHistogramNonCompliant) {
  const char kHistogramName[] =
      "Net.CertificateTransparency.EVCompliance2.QUIC";
  base::HistogramTester histograms;

  dummy_result_.cert_status = CERT_STATUS_IS_EV;
  dummy_result_.is_issued_by_known_root = true;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(CERT_STATUS_CT_COMPLIANCE_FAILED,
            verify_details->cert_verify_result.cert_status &
                (CERT_STATUS_CT_COMPLIANCE_FAILED | CERT_STATUS_IS_EV));

  histograms.ExpectUniqueSample(
      kHistogramName,
      static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS), 1);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(CERT_STATUS_CT_COMPLIANCE_FAILED,
            verify_details->cert_verify_result.cert_status &
                (CERT_STATUS_CT_COMPLIANCE_FAILED | CERT_STATUS_IS_EV));

  histograms.ExpectUniqueSample(
      kHistogramName,
      static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS), 2);
}

// Tests that when a connection is CT-compliant and its EV status is preserved,
// the correct histogram is recorded.
TEST_F(ProofVerifierChromiumTest, CTEVHistogramCompliant) {
  const char kHistogramName[] =
      "Net.CertificateTransparency.EVCompliance2.QUIC";
  base::HistogramTester histograms;

  dummy_result_.cert_status = CERT_STATUS_IS_EV;
  dummy_result_.is_issued_by_known_root = true;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_IS_EV);

  histograms.ExpectUniqueSample(
      kHistogramName,
      static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS), 1);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_IS_EV);

  histograms.ExpectUniqueSample(
      kHistogramName,
      static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS), 2);
}

HashValueVector MakeHashValueVector(uint8_t tag) {
  HashValue hash(HASH_VALUE_SHA256);
  memset(hash.data(), tag, hash.size());
  HashValueVector hashes;
  hashes.push_back(hash);
  return hashes;
}

TEST_F(ProofVerifierChromiumTest, IsFatalErrorNotSetForNonFatalError) {
  dummy_result_.cert_status = CERT_STATUS_DATE_INVALID;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_,
                                  ERR_CERT_DATE_INVALID);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_FALSE(verify_details->is_fatal_cert_error);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_FALSE(verify_details->is_fatal_cert_error);
}

TEST_F(ProofVerifierChromiumTest, IsFatalErrorSetForFatalError) {
  dummy_result_.cert_status = CERT_STATUS_DATE_INVALID;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_,
                                  ERR_CERT_DATE_INVALID);

  const base::Time expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);
  transport_security_state_.AddHSTS(kTestHostname, expiry, true);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->is_fatal_cert_error);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->is_fatal_cert_error);
}

// Test that PKP is enforced for certificates that chain up to known roots.
TEST_F(ProofVerifierChromiumTest, PKPEnforced) {
  dummy_result_.is_issued_by_known_root = true;
  dummy_result_.public_key_hashes = MakeHashValueVector(0x01);

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  transport_security_state_.EnableStaticPinsForTesting();
  ScopedTransportSecurityStateSource scoped_security_state_source;

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kCTAndPKPHost, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_PINNED_KEY_MISSING);
  EXPECT_FALSE(verify_details->pkp_bypassed);
  EXPECT_NE("", verify_details->pinning_failure_log);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kCTAndPKPHost, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_PINNED_KEY_MISSING);
  EXPECT_FALSE(verify_details->pkp_bypassed);
  EXPECT_NE("", verify_details->pinning_failure_log);
}

// Test |pkp_bypassed| is set when PKP is bypassed due to a local
// trust anchor
TEST_F(ProofVerifierChromiumTest, PKPBypassFlagSet) {
  dummy_result_.is_issued_by_known_root = false;
  dummy_result_.public_key_hashes = MakeHashValueVector(0x01);

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  transport_security_state_.EnableStaticPinsForTesting();
  ScopedTransportSecurityStateSource scoped_security_state_source;

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr,
                                       {kCTAndPKPHost}, NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kCTAndPKPHost, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->pkp_bypassed);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kCTAndPKPHost, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->pkp_bypassed);
}

// Test that PKP errors result in sending reports.
TEST_F(ProofVerifierChromiumTest, PKPReport) {
  NetworkIsolationKey network_isolation_key =
      NetworkIsolationKey::CreateTransient();

  MockCertificateReportSender report_sender;
  transport_security_state_.SetReportSender(&report_sender);

  HashValueVector spki_hashes;
  HashValue hash(HASH_VALUE_SHA256);
  memset(hash.data(), 0, hash.size());
  spki_hashes.push_back(hash);

  GURL report_uri("https://foo.test/");
  transport_security_state_.AddHPKP(
      kCTAndPKPHost, base::Time::Now() + base::TimeDelta::FromDays(1),
      false /* include_subdomains */, spki_hashes, report_uri);
  ScopedTransportSecurityStateSource scoped_security_state_source;

  dummy_result_.is_issued_by_known_root = true;
  dummy_result_.public_key_hashes = MakeHashValueVector(0x01);

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       network_isolation_key);

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kCTAndPKPHost, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_PINNED_KEY_MISSING);
  EXPECT_FALSE(verify_details->pkp_bypassed);
  EXPECT_NE("", verify_details->pinning_failure_log);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kCTAndPKPHost, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_PINNED_KEY_MISSING);
  EXPECT_FALSE(verify_details->pkp_bypassed);
  EXPECT_NE("", verify_details->pinning_failure_log);

  EXPECT_EQ(report_uri, report_sender.latest_report_uri());
  EXPECT_EQ(network_isolation_key,
            report_sender.latest_network_isolation_key());
}

// Test that when CT is required (in this case, by the delegate), the
// absence of CT information is a socket error.
TEST_F(ProofVerifierChromiumTest, CTIsRequired) {
  dummy_result_.is_issued_by_known_root = true;
  dummy_result_.public_key_hashes = MakeHashValueVector(0x01);

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  // Set up CT.
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_.SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(kTestHostname, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));
  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED);
}

// Test that when CT is required (in this case, by the delegate) and CT
// information is not compliant, then the CT-required histogram is recorded
// properly.
TEST_F(ProofVerifierChromiumTest, CTIsRequiredHistogramNonCompliant) {
  const char kHistogramName[] =
      "Net.CertificateTransparency.CTRequiredConnectionComplianceStatus2.QUIC";
  base::HistogramTester histograms;

  dummy_result_.is_issued_by_known_root = true;
  dummy_result_.public_key_hashes = MakeHashValueVector(0x01);

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  // Set up CT.
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_.SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(kTestHostname, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));
  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  histograms.ExpectUniqueSample(
      kHistogramName,
      static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS), 1);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  histograms.ExpectUniqueSample(
      kHistogramName,
      static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS), 2);
}

// Test that when CT is required (in this case, by the delegate) and CT
// information is compliant, then the CT-required histogram is recorded
// properly.
TEST_F(ProofVerifierChromiumTest, CTIsRequiredHistogramCompliant) {
  const char kHistogramName[] =
      "Net.CertificateTransparency.CTRequiredConnectionComplianceStatus2.QUIC";
  base::HistogramTester histograms;

  dummy_result_.is_issued_by_known_root = false;
  dummy_result_.public_key_hashes = MakeHashValueVector(0x01);

  // Set up CT.
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_.SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(kTestHostname, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));
  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  // First test that the histogram is not recorded for locally-installed roots.
  {
    MockCertVerifier dummy_verifier;
    dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);
    ProofVerifierChromium proof_verifier(
        &dummy_verifier, &ct_policy_enforcer_, &transport_security_state_,
        nullptr, {kTestHostname}, NetworkIsolationKey());

    std::unique_ptr<DummyProofVerifierCallback> callback(
        new DummyProofVerifierCallback);
    quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
        kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
        kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
        verify_context_.get(), &error_details_, &details_, std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    callback = std::make_unique<DummyProofVerifierCallback>();
    status = proof_verifier.VerifyCertChain(
        kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
        verify_context_.get(), &error_details_, &details_, &tls_alert_,
        std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    histograms.ExpectTotalCount(kHistogramName, 0);
  }
  // Now test that the histogram is recorded for public roots.
  {
    dummy_result_.is_issued_by_known_root = true;
    MockCertVerifier dummy_verifier;
    dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);
    ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                         &transport_security_state_, nullptr,
                                         {}, NetworkIsolationKey());

    std::unique_ptr<DummyProofVerifierCallback> callback(
        new DummyProofVerifierCallback);
    quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
        kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
        kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
        verify_context_.get(), &error_details_, &details_, std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    histograms.ExpectUniqueSample(
        kHistogramName,
        static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS),
        1);

    callback = std::make_unique<DummyProofVerifierCallback>();
    status = proof_verifier.VerifyCertChain(
        kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
        verify_context_.get(), &error_details_, &details_, &tls_alert_,
        std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    histograms.ExpectUniqueSample(
        kHistogramName,
        static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS),
        2);
  }
}

// Test that when CT is not required (because of a private root, in this case),
// the CT-required histogram is not recorded.
TEST_F(ProofVerifierChromiumTest, CTIsNotRequiredHistogram) {
  const char kHistogramName[] =
      "Net.CertificateTransparency.CTRequiredConnectionComplianceStatus2.QUIC";
  base::HistogramTester histograms;

  dummy_result_.is_issued_by_known_root = false;
  dummy_result_.public_key_hashes = MakeHashValueVector(0x01);

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr,
                                       {kTestHostname}, NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  histograms.ExpectTotalCount(kHistogramName, 0);
}

// Test that CT is considered even when PKP fails.
TEST_F(ProofVerifierChromiumTest, PKPAndCTBothTested) {
  dummy_result_.is_issued_by_known_root = true;
  dummy_result_.public_key_hashes = MakeHashValueVector(0x01);

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  // Set up PKP.
  transport_security_state_.EnableStaticPinsForTesting();
  ScopedTransportSecurityStateSource scoped_security_state_source;

  // Set up CT.
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_.SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(kCTAndPKPHost, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));
  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kCTAndPKPHost, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_PINNED_KEY_MISSING);
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kCTAndPKPHost, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_PINNED_KEY_MISSING);
  EXPECT_TRUE(verify_details->cert_verify_result.cert_status &
              CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED);
}

// Test that CT compliance status is recorded in a histogram.
TEST_F(ProofVerifierChromiumTest, CTComplianceStatusHistogram) {
  const char kHistogramName[] =
      "Net.CertificateTransparency.ConnectionComplianceStatus2.QUIC";
  base::HistogramTester histograms;

  dummy_result_.is_issued_by_known_root = false;

  // Set up CT.
  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS));

  // First test that the histogram is not recorded for locally-installed roots.
  {
    MockCertVerifier dummy_verifier;
    dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);
    ProofVerifierChromium proof_verifier(
        &dummy_verifier, &ct_policy_enforcer_, &transport_security_state_,
        nullptr, {kTestHostname}, NetworkIsolationKey());

    std::unique_ptr<DummyProofVerifierCallback> callback(
        new DummyProofVerifierCallback);
    quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
        kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
        kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
        verify_context_.get(), &error_details_, &details_, std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    callback = std::make_unique<DummyProofVerifierCallback>();
    status = proof_verifier.VerifyCertChain(
        kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
        verify_context_.get(), &error_details_, &details_, &tls_alert_,
        std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    // The histogram should not have been recorded.
    histograms.ExpectTotalCount(kHistogramName, 0);
  }

  // Now test that the histogram is recorded for public roots.
  {
    dummy_result_.is_issued_by_known_root = true;
    MockCertVerifier dummy_verifier;
    dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);
    ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                         &transport_security_state_, nullptr,
                                         {}, NetworkIsolationKey());

    std::unique_ptr<DummyProofVerifierCallback> callback(
        new DummyProofVerifierCallback);
    quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
        kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
        kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
        verify_context_.get(), &error_details_, &details_, std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    // The histogram should have been recorded with the CT compliance status.
    histograms.ExpectUniqueSample(
        kHistogramName,
        static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS),
        1);

    callback = std::make_unique<DummyProofVerifierCallback>();
    status = proof_verifier.VerifyCertChain(
        kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
        verify_context_.get(), &error_details_, &details_, &tls_alert_,
        std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    // The histogram should have been recorded with the CT compliance status.
    histograms.ExpectUniqueSample(
        kHistogramName,
        static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS),
        2);
  }
}

TEST_F(ProofVerifierChromiumTest, UnknownRootRejected) {
  dummy_result_.is_issued_by_known_root = false;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr, {},
                                       NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
  EXPECT_EQ(
      "Failed to verify certificate chain: net::ERR_QUIC_CERT_ROOT_NOT_KNOWN",
      error_details_);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
  EXPECT_EQ(
      "Failed to verify certificate chain: net::ERR_QUIC_CERT_ROOT_NOT_KNOWN",
      error_details_);
}

TEST_F(ProofVerifierChromiumTest, UnknownRootAcceptedWithOverride) {
  dummy_result_.is_issued_by_known_root = false;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr,
                                       {kTestHostname}, NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);
}

TEST_F(ProofVerifierChromiumTest, UnknownRootAcceptedWithWildcardOverride) {
  dummy_result_.is_issued_by_known_root = false;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_, nullptr,
                                       {""}, NetworkIsolationKey());

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  verify_details = static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);
}

// Tests that the SCTAuditingDelegate is called to enqueue SCT reports when
// verifying a good proof and cert.
TEST_F(ProofVerifierChromiumTest, SCTAuditingReportCollected) {
  MockCertVerifier cert_verifier;
  cert_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  MockSCTAuditingDelegate sct_auditing_delegate;
  EXPECT_CALL(sct_auditing_delegate, IsSCTAuditingEnabled())
      .WillRepeatedly(Return(true));
  // MaybeEnqueueReport() will be called twice: once in VerifyProof() (which
  // calls VerifyCert()) and once in VerifyCertChain().
  HostPortPair host_port_pair(kTestHostname, kTestPort);
  EXPECT_CALL(sct_auditing_delegate, MaybeEnqueueReport(host_port_pair, _, _))
      .Times(2);

  ProofVerifierChromium proof_verifier(
      &cert_verifier, &ct_policy_enforcer_, &transport_security_state_,
      &sct_auditing_delegate, {}, NetworkIsolationKey());

  auto callback = std::make_unique<DummyProofVerifierCallback>();
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, kTestTransportVersion,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  callback = std::make_unique<DummyProofVerifierCallback>();
  status = proof_verifier.VerifyCertChain(
      kTestHostname, kTestPort, certs_, kTestEmptyOCSPResponse, kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, &tls_alert_,
      std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);
}

}  // namespace test
}  // namespace net
