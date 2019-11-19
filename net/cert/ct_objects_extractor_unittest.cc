// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_objects_extractor.h"

#include "base/files/file_path.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/ct_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace ct {

class CTObjectsExtractorTest : public ::testing::Test {
 public:
  void SetUp() override {
    precert_chain_ =
        CreateCertificateListFromFile(GetTestCertsDirectory(),
                                      "ct-test-embedded-cert.pem",
                                      X509Certificate::FORMAT_AUTO);
    ASSERT_EQ(2u, precert_chain_.size());

    std::string der_test_cert(ct::GetDerEncodedX509Cert());
    test_cert_ = X509Certificate::CreateFromBytes(der_test_cert.data(),
                                                  der_test_cert.length());
    ASSERT_TRUE(test_cert_);

    log_ = CTLogVerifier::Create(ct::GetTestPublicKey(), "testlog");
    ASSERT_TRUE(log_);
  }

  void ExtractEmbeddedSCT(scoped_refptr<X509Certificate> cert,
                          scoped_refptr<SignedCertificateTimestamp>* sct) {
    std::string sct_list;
    ASSERT_TRUE(ExtractEmbeddedSCTList(cert->cert_buffer(), &sct_list));

    std::vector<base::StringPiece> parsed_scts;
    // Make sure the SCT list can be decoded properly
    ASSERT_TRUE(DecodeSCTList(sct_list, &parsed_scts));
    ASSERT_EQ(1u, parsed_scts.size());
    EXPECT_TRUE(DecodeSignedCertificateTimestamp(&parsed_scts[0], sct));
  }

 protected:
  CertificateList precert_chain_;
  scoped_refptr<X509Certificate> test_cert_;
  scoped_refptr<const CTLogVerifier> log_;
};

// Test that an SCT can be extracted and the extracted SCT contains the
// expected data.
TEST_F(CTObjectsExtractorTest, ExtractEmbeddedSCT) {
  scoped_refptr<ct::SignedCertificateTimestamp> sct(
      new ct::SignedCertificateTimestamp());
  ExtractEmbeddedSCT(precert_chain_[0], &sct);

  EXPECT_EQ(sct->version, SignedCertificateTimestamp::V1);
  EXPECT_EQ(ct::GetTestPublicKeyId(), sct->log_id);

  base::Time expected_timestamp =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(1365181456275);
  EXPECT_EQ(expected_timestamp, sct->timestamp);
}

TEST_F(CTObjectsExtractorTest, ExtractPrecert) {
  SignedEntryData entry;
  ASSERT_TRUE(GetPrecertSignedEntry(precert_chain_[0]->cert_buffer(),
                                    precert_chain_[1]->cert_buffer(), &entry));

  ASSERT_EQ(ct::SignedEntryData::LOG_ENTRY_TYPE_PRECERT, entry.type);
  // Should have empty leaf cert for this log entry type.
  ASSERT_TRUE(entry.leaf_certificate.empty());
  // Compare hash values of issuer spki.
  SHA256HashValue expected_issuer_key_hash;
  memcpy(expected_issuer_key_hash.data, GetDefaultIssuerKeyHash().data(), 32);
  ASSERT_EQ(expected_issuer_key_hash, entry.issuer_key_hash);
}

TEST_F(CTObjectsExtractorTest, ExtractOrdinaryX509Cert) {
  SignedEntryData entry;
  ASSERT_TRUE(GetX509SignedEntry(test_cert_->cert_buffer(), &entry));

  ASSERT_EQ(ct::SignedEntryData::LOG_ENTRY_TYPE_X509, entry.type);
  // Should have empty tbs_certificate for this log entry type.
  ASSERT_TRUE(entry.tbs_certificate.empty());
  // Length of leaf_certificate should be 718, see the CT Serialization tests.
  ASSERT_EQ(718U, entry.leaf_certificate.size());
}

// Test that the embedded SCT verifies
TEST_F(CTObjectsExtractorTest, ExtractedSCTVerifies) {
  scoped_refptr<ct::SignedCertificateTimestamp> sct(
      new ct::SignedCertificateTimestamp());
  ExtractEmbeddedSCT(precert_chain_[0], &sct);

  SignedEntryData entry;
  ASSERT_TRUE(GetPrecertSignedEntry(precert_chain_[0]->cert_buffer(),
                                    precert_chain_[1]->cert_buffer(), &entry));

  EXPECT_TRUE(log_->Verify(entry, *sct.get()));
}

// Test that an externally-provided SCT verifies over the SignedEntryData
// of a regular X.509 Certificate
TEST_F(CTObjectsExtractorTest, ComplementarySCTVerifies) {
  scoped_refptr<ct::SignedCertificateTimestamp> sct(
      new ct::SignedCertificateTimestamp());
  GetX509CertSCT(&sct);

  SignedEntryData entry;
  ASSERT_TRUE(GetX509SignedEntry(test_cert_->cert_buffer(), &entry));

  EXPECT_TRUE(log_->Verify(entry, *sct.get()));
}

// Test that the extractor can parse OCSP responses.
TEST_F(CTObjectsExtractorTest, ExtractSCTListFromOCSPResponse) {
  std::string der_subject_cert(ct::GetDerEncodedFakeOCSPResponseCert());
  scoped_refptr<X509Certificate> subject_cert =
      X509Certificate::CreateFromBytes(der_subject_cert.data(),
                                       der_subject_cert.length());
  ASSERT_TRUE(subject_cert);
  std::string der_issuer_cert(ct::GetDerEncodedFakeOCSPResponseIssuerCert());
  scoped_refptr<X509Certificate> issuer_cert =
      X509Certificate::CreateFromBytes(der_issuer_cert.data(),
                                       der_issuer_cert.length());
  ASSERT_TRUE(issuer_cert);

  std::string fake_sct_list = ct::GetFakeOCSPExtensionValue();
  ASSERT_FALSE(fake_sct_list.empty());
  std::string ocsp_response = ct::GetDerEncodedFakeOCSPResponse();

  std::string extracted_sct_list;
  EXPECT_TRUE(ct::ExtractSCTListFromOCSPResponse(
      issuer_cert->cert_buffer(), subject_cert->serial_number(), ocsp_response,
      &extracted_sct_list));
  EXPECT_EQ(extracted_sct_list, fake_sct_list);
}

// Test that the extractor honours serial number.
TEST_F(CTObjectsExtractorTest, ExtractSCTListFromOCSPResponseMatchesSerial) {
  std::string der_issuer_cert(ct::GetDerEncodedFakeOCSPResponseIssuerCert());
  scoped_refptr<X509Certificate> issuer_cert =
      X509Certificate::CreateFromBytes(der_issuer_cert.data(),
                                       der_issuer_cert.length());
  ASSERT_TRUE(issuer_cert);

  std::string ocsp_response = ct::GetDerEncodedFakeOCSPResponse();

  std::string extracted_sct_list;
  EXPECT_FALSE(ct::ExtractSCTListFromOCSPResponse(
      issuer_cert->cert_buffer(), test_cert_->serial_number(), ocsp_response,
      &extracted_sct_list));
}

// Test that the extractor honours issuer ID.
TEST_F(CTObjectsExtractorTest, ExtractSCTListFromOCSPResponseMatchesIssuer) {
  std::string der_subject_cert(ct::GetDerEncodedFakeOCSPResponseCert());
  scoped_refptr<X509Certificate> subject_cert =
      X509Certificate::CreateFromBytes(der_subject_cert.data(),
                                       der_subject_cert.length());
  ASSERT_TRUE(subject_cert);

  std::string ocsp_response = ct::GetDerEncodedFakeOCSPResponse();

  std::string extracted_sct_list;
  // Use test_cert_ for issuer - it is not the correct issuer of |subject_cert|.
  EXPECT_FALSE(ct::ExtractSCTListFromOCSPResponse(
      test_cert_->cert_buffer(), subject_cert->serial_number(), ocsp_response,
      &extracted_sct_list));
}

}  // namespace ct

}  // namespace net
