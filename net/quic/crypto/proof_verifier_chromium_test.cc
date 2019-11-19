// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/proof_verifier_chromium.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/x509_util.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quic/core/crypto/proof_verifier.h"
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
const char kTestChloHash[] = "CHLO hash";
const char kTestEmptySCT[] = "";
const uint16_t kTestPort = 8443;
const char kTestConfig[] = "server config bytes";
const char kLogDescription[] = "somelog";

}  // namespace

class ProofVerifierChromiumTest : public ::testing::Test {
 public:
  ProofVerifierChromiumTest()
      : verify_context_(new ProofVerifyContextChromium(0 /*cert_verify_flags*/,
                                                       NetLogWithSource())) {}

  void SetUp() override {
    EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
        .WillRepeatedly(
            Return(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS));

    scoped_refptr<const CTLogVerifier> log(
        CTLogVerifier::Create(ct::GetTestPublicKey(), kLogDescription));
    ASSERT_TRUE(log);
    log_verifiers_.push_back(log);

    ct_verifier_.reset(new MultiLogCTVerifier());
    ct_verifier_->AddLogs(log_verifiers_);

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
    source.GetProof(quic::QuicSocketAddress(), kTestHostname, kTestConfig,
                    quic::QUIC_VERSION_43, kTestChloHash,
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
    const ct::CTVerifyResult& ct_verify_result =
        proof_details->ct_verify_result;
    if (sct_expected_ok) {
      ASSERT_TRUE(ct::CheckForSingleVerifiedSCTInResult(ct_verify_result.scts,
                                                        kLogDescription));
      ASSERT_TRUE(ct::CheckForSCTOrigin(
          ct_verify_result.scts,
          ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION));
    } else {
      EXPECT_EQ(1U, ct_verify_result.scts.size());
      EXPECT_EQ(ct::SCT_STATUS_LOG_UNKNOWN, ct_verify_result.scts[0].status);
    }
  }

 protected:
  TransportSecurityState transport_security_state_;
  MockCTPolicyEnforcer ct_policy_enforcer_;

  std::unique_ptr<MultiLogCTVerifier> ct_verifier_;
  std::vector<scoped_refptr<const CTLogVerifier>> log_verifiers_;
  std::unique_ptr<quic::ProofVerifyContext> verify_context_;
  std::unique_ptr<quic::ProofVerifyDetails> details_;
  std::string error_details_;
  std::vector<std::string> certs_;
  CertVerifyResult dummy_result_;
  scoped_refptr<X509Certificate> test_cert_;
};

TEST_F(ProofVerifierChromiumTest, VerifyProof) {
  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);
}

// Tests that the quic::ProofVerifier fails verification if certificate
// verification fails.
TEST_F(ProofVerifierChromiumTest, FailsIfCertFails) {
  MockCertVerifier dummy_verifier;
  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
}

// Valid SCT, but invalid signature.
TEST_F(ProofVerifierChromiumTest, ValidSCTList) {
  // Use different certificates for SCT tests.
  ASSERT_NO_FATAL_FAILURE(GetSCTTestCertificates(&certs_));

  MockCertVerifier cert_verifier;

  ProofVerifierChromium proof_verifier(&cert_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, ct::GetSCTListForTesting(), kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
  CheckSCT(/*sct_expected_ok=*/true);
}

// Invalid SCT and signature.
TEST_F(ProofVerifierChromiumTest, InvalidSCTList) {
  // Use different certificates for SCT tests.
  ASSERT_NO_FATAL_FAILURE(GetSCTTestCertificates(&certs_));

  MockCertVerifier cert_verifier;
  ProofVerifierChromium proof_verifier(&cert_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, ct::GetSCTListWithInvalidSCT(), kTestEmptySCT,
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
  CheckSCT(/*sct_expected_ok=*/false);
}

// Tests that the quic::ProofVerifier doesn't verify certificates if the config
// signature fails.
TEST_F(ProofVerifierChromiumTest, FailsIfSignatureFails) {
  FailsTestCertVerifier cert_verifier;
  ProofVerifierChromium proof_verifier(&cert_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, kTestConfig, verify_context_.get(),
      &error_details_, &details_, std::move(callback));
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kCTAndPKPHost, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {kCTAndPKPHost});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kCTAndPKPHost, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_TRUE(verify_details->pkp_bypassed);
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_FAILURE, status);

  histograms.ExpectUniqueSample(
      kHistogramName,
      static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS), 1);
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
    ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                         &transport_security_state_,
                                         ct_verifier_.get(), {kTestHostname});

    std::unique_ptr<DummyProofVerifierCallback> callback(
        new DummyProofVerifierCallback);
    quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
        kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
        kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
        verify_context_.get(), &error_details_, &details_, std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    histograms.ExpectTotalCount(kHistogramName, 0);
  }
  // Now test that the histogram is recorded for public roots.
  {
    dummy_result_.is_issued_by_known_root = true;
    MockCertVerifier dummy_verifier;
    dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);
    ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                         &transport_security_state_,
                                         ct_verifier_.get(), {});

    std::unique_ptr<DummyProofVerifierCallback> callback(
        new DummyProofVerifierCallback);
    quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
        kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
        kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
        verify_context_.get(), &error_details_, &details_, std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    histograms.ExpectUniqueSample(
        kHistogramName,
        static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS),
        1);
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {kTestHostname});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kCTAndPKPHost, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
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
    ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                         &transport_security_state_,
                                         ct_verifier_.get(), {kTestHostname});

    std::unique_ptr<DummyProofVerifierCallback> callback(
        new DummyProofVerifierCallback);
    quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
        kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
        kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
        verify_context_.get(), &error_details_, &details_, std::move(callback));
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
                                         &transport_security_state_,
                                         ct_verifier_.get(), {});

    std::unique_ptr<DummyProofVerifierCallback> callback(
        new DummyProofVerifierCallback);
    quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
        kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
        kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
        verify_context_.get(), &error_details_, &details_, std::move(callback));
    ASSERT_EQ(quic::QUIC_SUCCESS, status);

    // The histogram should have been recorded with the CT compliance status.
    histograms.ExpectUniqueSample(
        kHistogramName,
        static_cast<int>(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS),
        1);
  }
}

// Tests that when CT is required but the connection is not compliant, the
// relevant flag is set in the CTVerifyResult.
TEST_F(ProofVerifierChromiumTest, CTRequirementsFlagNotMet) {
  dummy_result_.is_issued_by_known_root = true;
  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  // Set up CT.
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_.SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));
  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_NOT_DIVERSE_SCTS));

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));

  // The flag should be set in the CTVerifyResult.
  ProofVerifyDetailsChromium* proof_details =
      reinterpret_cast<ProofVerifyDetailsChromium*>(details_.get());
  const ct::CTVerifyResult& ct_verify_result = proof_details->ct_verify_result;
  EXPECT_TRUE(ct_verify_result.policy_compliance_required);
}

// Tests that when CT is required and the connection is compliant, the relevant
// flag is set in the CTVerifyResult.
TEST_F(ProofVerifierChromiumTest, CTRequirementsFlagMet) {
  dummy_result_.is_issued_by_known_root = true;
  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  // Set up CT.
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_.SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));
  EXPECT_CALL(ct_policy_enforcer_, CheckCompliance(_, _, _))
      .WillRepeatedly(
          Return(ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS));

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));

  // The flag should be set in the CTVerifyResult.
  ProofVerifyDetailsChromium* proof_details =
      reinterpret_cast<ProofVerifyDetailsChromium*>(details_.get());
  const ct::CTVerifyResult& ct_verify_result = proof_details->ct_verify_result;
  EXPECT_TRUE(ct_verify_result.policy_compliance_required);
}

TEST_F(ProofVerifierChromiumTest, UnknownRootRejected) {
  dummy_result_.is_issued_by_known_root = false;

  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
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
                                       &transport_security_state_,
                                       ct_verifier_.get(), {kTestHostname});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyProof(
      kTestHostname, kTestPort, kTestConfig, quic::QUIC_VERSION_43,
      kTestChloHash, certs_, kTestEmptySCT, GetTestSignature(),
      verify_context_.get(), &error_details_, &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(dummy_result_.cert_status,
            verify_details->cert_verify_result.cert_status);
}

// Tests that the VerifyCertChain verifies certificates.
TEST_F(ProofVerifierChromiumTest, VerifyCertChain) {
  MockCertVerifier dummy_verifier;
  dummy_verifier.AddResultForCert(test_cert_.get(), dummy_result_, OK);

  ProofVerifierChromium proof_verifier(&dummy_verifier, &ct_policy_enforcer_,
                                       &transport_security_state_,
                                       ct_verifier_.get(), {});

  std::unique_ptr<DummyProofVerifierCallback> callback(
      new DummyProofVerifierCallback);
  quic::QuicAsyncStatus status = proof_verifier.VerifyCertChain(
      kTestHostname, certs_, /*ocsp_response=*/std::string(),
      /*cert_sct=*/std::string(), verify_context_.get(), &error_details_,
      &details_, std::move(callback));
  ASSERT_EQ(quic::QUIC_SUCCESS, status);

  ASSERT_TRUE(details_.get());
  ProofVerifyDetailsChromium* verify_details =
      static_cast<ProofVerifyDetailsChromium*>(details_.get());
  EXPECT_EQ(0u, verify_details->cert_verify_result.cert_status);
}

}  // namespace test
}  // namespace net
