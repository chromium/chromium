// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_log_ct_verifier.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/pem_tokenizer.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace net {

namespace {

const char kHostname[] = "example.com";
const char kLogDescription[] = "somelog";

class MultiLogCTVerifierTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_refptr<const CTLogVerifier> log(
        CTLogVerifier::Create(ct::GetTestPublicKey(), kLogDescription));
    ASSERT_TRUE(log);
    log_verifiers_.push_back(log);

    verifier_.reset(new MultiLogCTVerifier());
    verifier_->AddLogs(log_verifiers_);
    std::string der_test_cert(ct::GetDerEncodedX509Cert());
    chain_ = X509Certificate::CreateFromBytes(
        der_test_cert.data(),
        der_test_cert.length());
    ASSERT_TRUE(chain_.get());

    embedded_sct_chain_ =
        CreateCertificateChainFromFile(GetTestCertsDirectory(),
                                       "ct-test-embedded-cert.pem",
                                       X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(embedded_sct_chain_.get());
  }

  bool CheckForEmbeddedSCTInNetLog(const TestNetLog& net_log) {
    auto entries = net_log.GetEntries();
    if (entries.size() != 2)
      return false;

    auto embedded_scts =
        GetOptionalStringValueFromParams(entries[0], "embedded_scts");
    if (!embedded_scts || embedded_scts->empty())
      return false;

    const NetLogEntry& parsed = entries[1];
    const base::ListValue* scts;
    if (!GetListValueFromParams(parsed, "scts", &scts) ||
        scts->GetSize() != 1) {
      return false;
    }

    const base::DictionaryValue* the_sct;
    if (!scts->GetDictionary(0, &the_sct))
      return false;

    std::string origin;
    if (!the_sct->GetString("origin", &origin))
      return false;
    if (origin != "Embedded in certificate")
      return false;

    std::string verification_status;
    if (!the_sct->GetString("verification_status", &verification_status))
      return false;
    if (verification_status != "Verified")
      return false;

    return true;
  }

  // Returns true if |chain| is a certificate with embedded SCTs that can be
  // successfully extracted.
  bool VerifySinglePrecertificateChain(scoped_refptr<X509Certificate> chain) {
    SignedCertificateTimestampAndStatusList scts;
    verifier_->Verify(kHostname, chain.get(), base::StringPiece(),
                      base::StringPiece(), &scts, NetLogWithSource());
    return !scts.empty();
  }

  // Returns true if |chain| is a certificate with a single embedded SCT that
  // can be successfully extracted and matched to the test log indicated by
  // |kLogDescription|.
  bool CheckPrecertificateVerification(scoped_refptr<X509Certificate> chain) {
    SignedCertificateTimestampAndStatusList scts;
    TestNetLog test_net_log;
    NetLogWithSource net_log = NetLogWithSource::Make(
        &test_net_log, NetLogSourceType::SSL_CONNECT_JOB);
    verifier_->Verify(kHostname, chain.get(), base::StringPiece(),
                      base::StringPiece(), &scts, net_log);
    return ct::CheckForSingleVerifiedSCTInResult(scts, kLogDescription) &&
           ct::CheckForSCTOrigin(
               scts, ct::SignedCertificateTimestamp::SCT_EMBEDDED) &&
           CheckForEmbeddedSCTInNetLog(test_net_log);
  }

  // Histogram-related helper methods
  int GetValueFromHistogram(const std::string& histogram_name,
                            int sample_index) {
    base::Histogram* histogram = static_cast<base::Histogram*>(
        base::StatisticsRecorder::FindHistogram(histogram_name));

    if (histogram == nullptr)
      return 0;

    std::unique_ptr<base::HistogramSamples> samples =
        histogram->SnapshotSamples();
    return samples->GetCount(sample_index);
  }

  int NumEmbeddedSCTsInHistogram() {
    return GetValueFromHistogram("Net.CertificateTransparency.SCTOrigin",
                                 ct::SignedCertificateTimestamp::SCT_EMBEDDED);
  }

  int NumValidSCTsInStatusHistogram() {
    return GetValueFromHistogram("Net.CertificateTransparency.SCTStatus",
                                 ct::SCT_STATUS_OK);
  }

 protected:
  std::unique_ptr<MultiLogCTVerifier> verifier_;
  scoped_refptr<X509Certificate> chain_;
  scoped_refptr<X509Certificate> embedded_sct_chain_;
  std::vector<scoped_refptr<const CTLogVerifier>> log_verifiers_;
};

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCT) {
  ASSERT_TRUE(CheckPrecertificateVerification(embedded_sct_chain_));
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCTWithPreCA) {
  scoped_refptr<X509Certificate> chain(
      CreateCertificateChainFromFile(GetTestCertsDirectory(),
                                     "ct-test-embedded-with-preca-chain.pem",
                                     X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain.get());
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest, VerifiesEmbeddedSCTWithIntermediate) {
  scoped_refptr<X509Certificate> chain(CreateCertificateChainFromFile(
      GetTestCertsDirectory(),
      "ct-test-embedded-with-intermediate-chain.pem",
      X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain.get());
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest,
       VerifiesEmbeddedSCTWithIntermediateAndPreCA) {
  scoped_refptr<X509Certificate> chain(CreateCertificateChainFromFile(
      GetTestCertsDirectory(),
      "ct-test-embedded-with-intermediate-preca-chain.pem",
      X509Certificate::FORMAT_AUTO));
  ASSERT_TRUE(chain.get());
  ASSERT_TRUE(CheckPrecertificateVerification(chain));
}

TEST_F(MultiLogCTVerifierTest, VerifiesSCTOverX509Cert) {
  std::string sct_list = ct::GetSCTListForTesting();

  SignedCertificateTimestampAndStatusList scts;
  verifier_->Verify(kHostname, chain_.get(), base::StringPiece(), sct_list,
                    &scts, NetLogWithSource());
  ASSERT_TRUE(ct::CheckForSingleVerifiedSCTInResult(scts, kLogDescription));
  ASSERT_TRUE(ct::CheckForSCTOrigin(
      scts, ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION));
}

TEST_F(MultiLogCTVerifierTest, IdentifiesSCTFromUnknownLog) {
  std::string sct_list = ct::GetSCTListWithInvalidSCT();
  SignedCertificateTimestampAndStatusList scts;

  verifier_->Verify(kHostname, chain_.get(), base::StringPiece(), sct_list,
                    &scts, NetLogWithSource());
  EXPECT_EQ(1U, scts.size());
  EXPECT_EQ("", scts[0].sct->log_description);
  EXPECT_EQ(ct::SCT_STATUS_LOG_UNKNOWN, scts[0].status);
}

TEST_F(MultiLogCTVerifierTest, CountsValidSCTsInStatusHistogram) {
  int num_valid_scts = NumValidSCTsInStatusHistogram();

  ASSERT_TRUE(VerifySinglePrecertificateChain(embedded_sct_chain_));

  EXPECT_EQ(num_valid_scts + 1, NumValidSCTsInStatusHistogram());
}

TEST_F(MultiLogCTVerifierTest, CountsInvalidSCTsInStatusHistogram) {
  std::string sct_list = ct::GetSCTListWithInvalidSCT();
  SignedCertificateTimestampAndStatusList scts;

  int num_valid_scts = NumValidSCTsInStatusHistogram();
  int num_invalid_scts = GetValueFromHistogram(
      "Net.CertificateTransparency.SCTStatus", ct::SCT_STATUS_LOG_UNKNOWN);

  verifier_->Verify(kHostname, chain_.get(), base::StringPiece(), sct_list,
                    &scts, NetLogWithSource());

  ASSERT_EQ(num_valid_scts, NumValidSCTsInStatusHistogram());
  ASSERT_EQ(num_invalid_scts + 1,
            GetValueFromHistogram("Net.CertificateTransparency.SCTStatus",
                                  ct::SCT_STATUS_LOG_UNKNOWN));
}

TEST_F(MultiLogCTVerifierTest, CountsSingleEmbeddedSCTInOriginsHistogram) {
  int old_embedded_count = NumEmbeddedSCTsInHistogram();
  ASSERT_TRUE(CheckPrecertificateVerification(embedded_sct_chain_));
  EXPECT_EQ(old_embedded_count + 1, NumEmbeddedSCTsInHistogram());
}

}  // namespace

}  // namespace net
