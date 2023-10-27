// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/cert_issuer_source_aia.h"

#include <memory>

#include "base/files/file_util.h"
#include "net/cert/internal/test_helpers.h"
#include "net/cert/mock_cert_net_fetcher.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "url/gurl.h"

namespace net {

namespace {

using ::testing::ByMove;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

::testing::AssertionResult ReadTestPem(const std::string& file_name,
                                       const std::string& block_name,
                                       std::string* result) {
  const PemBlockMapping mappings[] = {
      {block_name.c_str(), result},
  };

  return ReadTestDataFromPemFile(file_name, mappings);
}

::testing::AssertionResult ReadTestCert(
    const std::string& file_name,
    std::shared_ptr<const bssl::ParsedCertificate>* result) {
  std::string der;
  ::testing::AssertionResult r =
      ReadTestPem("net/data/cert_issuer_source_aia_unittest/" + file_name,
                  "CERTIFICATE", &der);
  if (!r)
    return r;
  bssl::CertErrors errors;
  *result = bssl::ParsedCertificate::Create(x509_util::CreateCryptoBuffer(der),
                                            {}, &errors);
  if (!*result) {
    return ::testing::AssertionFailure()
           << "bssl::ParsedCertificate::Create() failed:\n"
           << errors.ToDebugString();
  }
  return ::testing::AssertionSuccess();
}

// CertIssuerSourceAia does not return results for SyncGetIssuersOf.
TEST(CertIssuerSourceAiaTest, NoSyncResults) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_two_aia.pem", &cert));

  // No methods on |mock_fetcher| should be called.
  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
  CertIssuerSourceAia aia_source(mock_fetcher);
  bssl::ParsedCertificateList issuers;
  aia_source.SyncGetIssuersOf(cert.get(), &issuers);
  EXPECT_EQ(0U, issuers.size());
}

// If the AuthorityInfoAccess extension is not present, AsyncGetIssuersOf should
// synchronously indicate no results.
TEST(CertIssuerSourceAiaTest, NoAia) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_no_aia.pem", &cert));

  // No methods on |mock_fetcher| should be called.
  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> request;
  aia_source.AsyncGetIssuersOf(cert.get(), &request);
  EXPECT_EQ(nullptr, request);
}

// If the AuthorityInfoAccess extension only contains non-HTTP URIs,
// AsyncGetIssuersOf should create a Request object. The URL scheme check is
// part of the specific CertNetFetcher implementation, this tests that we handle
// ERR_DISALLOWED_URL_SCHEME properly. If FetchCaIssuers is modified to fail
// synchronously in that case, this test will be more interesting.
TEST(CertIssuerSourceAiaTest, FileAia) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_file_aia.pem", &cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
  EXPECT_CALL(*mock_fetcher, FetchCaIssuers(GURL("file:///dev/null"), _, _))
      .WillOnce(Return(ByMove(
          MockCertNetFetcherRequest::Create(ERR_DISALLOWED_URL_SCHEME))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  // No results.
  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  EXPECT_TRUE(result_certs.empty());
}

// If the AuthorityInfoAccess extension contains an invalid URL,
// AsyncGetIssuersOf should synchronously indicate no results.
TEST(CertIssuerSourceAiaTest, OneInvalidURL) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_invalid_url_aia.pem", &cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> request;
  aia_source.AsyncGetIssuersOf(cert.get(), &request);
  EXPECT_EQ(nullptr, request);
}

// AuthorityInfoAccess with a single HTTP url pointing to a single DER cert.
TEST(CertIssuerSourceAiaTest, OneAia) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_one_aia.pem", &cert));
  std::shared_ptr<const bssl::ParsedCertificate> intermediate_cert;
  ASSERT_TRUE(ReadTestCert("i.pem", &intermediate_cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(
          intermediate_cert->cert_buffer()))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(1u, result_certs.size());
  ASSERT_EQ(result_certs.front()->der_cert(), intermediate_cert->der_cert());

  result_certs.clear();
  cert_source_request->GetNext(&result_certs);
  EXPECT_TRUE(result_certs.empty());
}

// AuthorityInfoAccess with two URIs, one a FILE, the other a HTTP.
// Simulate a ERR_DISALLOWED_URL_SCHEME for the file URL. If FetchCaIssuers is
// modified to synchronously reject disallowed schemes, this test will be more
// interesting.
TEST(CertIssuerSourceAiaTest, OneFileOneHttpAia) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_file_and_http_aia.pem", &cert));
  std::shared_ptr<const bssl::ParsedCertificate> intermediate_cert;
  ASSERT_TRUE(ReadTestCert("i2.pem", &intermediate_cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  EXPECT_CALL(*mock_fetcher, FetchCaIssuers(GURL("file:///dev/null"), _, _))
      .WillOnce(Return(ByMove(
          MockCertNetFetcherRequest::Create(ERR_DISALLOWED_URL_SCHEME))));

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia2/I2.foo"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(
          intermediate_cert->cert_buffer()))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(1u, result_certs.size());
  ASSERT_EQ(result_certs.front()->der_cert(), intermediate_cert->der_cert());

  cert_source_request->GetNext(&result_certs);
  EXPECT_EQ(1u, result_certs.size());
}

// AuthorityInfoAccess with two URIs, one is invalid, the other HTTP.
TEST(CertIssuerSourceAiaTest, OneInvalidOneHttpAia) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_invalid_and_http_aia.pem", &cert));
  std::shared_ptr<const bssl::ParsedCertificate> intermediate_cert;
  ASSERT_TRUE(ReadTestCert("i2.pem", &intermediate_cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia2/I2.foo"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(
          intermediate_cert->cert_buffer()))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(1u, result_certs.size());
  EXPECT_EQ(result_certs.front()->der_cert(), intermediate_cert->der_cert());

  // No more results.
  result_certs.clear();
  cert_source_request->GetNext(&result_certs);
  EXPECT_EQ(0u, result_certs.size());
}

// AuthorityInfoAccess with two HTTP urls, each pointing to a single DER cert.
// One request completes, results are retrieved, then the next request completes
// and the results are retrieved.
TEST(CertIssuerSourceAiaTest, TwoAiaCompletedInSeries) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_two_aia.pem", &cert));
  std::shared_ptr<const bssl::ParsedCertificate> intermediate_cert;
  ASSERT_TRUE(ReadTestCert("i.pem", &intermediate_cert));
  std::shared_ptr<const bssl::ParsedCertificate> intermediate_cert2;
  ASSERT_TRUE(ReadTestCert("i2.pem", &intermediate_cert2));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(
          intermediate_cert->cert_buffer()))));

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia2/I2.foo"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(
          intermediate_cert2->cert_buffer()))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  // GetNext() should return intermediate_cert followed by intermediate_cert2.
  // They are returned in two separate batches.
  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(1u, result_certs.size());
  EXPECT_EQ(result_certs.front()->der_cert(), intermediate_cert->der_cert());

  result_certs.clear();
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(1u, result_certs.size());
  EXPECT_EQ(result_certs.front()->der_cert(), intermediate_cert2->der_cert());

  // No more results.
  result_certs.clear();
  cert_source_request->GetNext(&result_certs);
  EXPECT_EQ(0u, result_certs.size());
}

// AuthorityInfoAccess with a single HTTP url pointing to a single DER cert,
// CertNetFetcher request fails.
TEST(CertIssuerSourceAiaTest, OneAiaHttpError) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_one_aia.pem", &cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  // HTTP request returns with an error.
  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(ERR_FAILED))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  // No results.
  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(0u, result_certs.size());
}

// AuthorityInfoAccess with a single HTTP url pointing to a single DER cert,
// CertNetFetcher request completes, but the DER cert fails to parse.
TEST(CertIssuerSourceAiaTest, OneAiaParseError) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_one_aia.pem", &cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  // HTTP request returns invalid certificate data.
  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(
          std::vector<uint8_t>({1, 2, 3, 4, 5})))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  // No results.
  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(0u, result_certs.size());
}

// AuthorityInfoAccess with two HTTP urls, each pointing to a single DER cert.
// One request fails.
TEST(CertIssuerSourceAiaTest, TwoAiaCompletedInSeriesFirstFails) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_two_aia.pem", &cert));
  std::shared_ptr<const bssl::ParsedCertificate> intermediate_cert2;
  ASSERT_TRUE(ReadTestCert("i2.pem", &intermediate_cert2));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  // Request for I.cer completes first, but fails.
  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(
          ByMove(MockCertNetFetcherRequest::Create(ERR_INVALID_RESPONSE))));

  // Request for I2.foo succeeds.
  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia2/I2.foo"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(
          intermediate_cert2->cert_buffer()))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  // GetNext() should return intermediate_cert2.
  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(1u, result_certs.size());
  EXPECT_EQ(result_certs.front()->der_cert(), intermediate_cert2->der_cert());

  // No more results.
  result_certs.clear();
  cert_source_request->GetNext(&result_certs);
  EXPECT_EQ(0u, result_certs.size());
}

// AuthorityInfoAccess with two HTTP urls, each pointing to a single DER cert.
// First request completes, result is retrieved, then the second request fails.
TEST(CertIssuerSourceAiaTest, TwoAiaCompletedInSeriesSecondFails) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_two_aia.pem", &cert));
  std::shared_ptr<const bssl::ParsedCertificate> intermediate_cert;
  ASSERT_TRUE(ReadTestCert("i.pem", &intermediate_cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  // Request for I.cer completes first.
  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(
          intermediate_cert->cert_buffer()))));

  // Request for I2.foo fails.
  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia2/I2.foo"), _, _))
      .WillOnce(Return(
          ByMove(MockCertNetFetcherRequest::Create(ERR_INVALID_RESPONSE))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  // GetNext() should return intermediate_cert.
  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(1u, result_certs.size());
  EXPECT_EQ(result_certs.front()->der_cert(), intermediate_cert->der_cert());

  // No more results.
  result_certs.clear();
  cert_source_request->GetNext(&result_certs);
  EXPECT_EQ(0u, result_certs.size());
}

// AuthorityInfoAccess with six HTTP URLs.  kMaxFetchesPerCert is 5, so the
// sixth URL should be ignored.
TEST(CertIssuerSourceAiaTest, MaxFetchesPerCert) {
  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_six_aia.pem", &cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  std::vector<uint8_t> bad_der({1, 2, 3, 4, 5});

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(bad_der))));

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia2/I2.foo"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(bad_der))));

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia3/I3.foo"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(bad_der))));

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia4/I4.foo"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(bad_der))));

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia5/I5.foo"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(bad_der))));

  // Note that the sixth URL (http://url-for-aia6/I6.foo) will not be requested.

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  // GetNext() will not get any certificates (since the first 5 fail to be
  // parsed, and the sixth URL is not attempted).
  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(0u, result_certs.size());
}

// AuthorityInfoAccess that returns a certs-only CMS message containing two
// certificates.
TEST(CertIssuerSourceAiaTest, CertsOnlyCmsMessage) {
  base::FilePath cert_path =
      GetTestCertsDirectory().AppendASCII("google.binary.p7b");
  std::string cert_data;
  ASSERT_TRUE(base::ReadFileToString(cert_path, &cert_data));

  std::shared_ptr<const bssl::ParsedCertificate> cert;
  ASSERT_TRUE(ReadTestCert("target_one_aia.pem", &cert));

  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  EXPECT_CALL(*mock_fetcher,
              FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(
          std::vector<uint8_t>(cert_data.begin(), cert_data.end())))));

  CertIssuerSourceAia aia_source(mock_fetcher);
  std::unique_ptr<bssl::CertIssuerSource::Request> cert_source_request;
  aia_source.AsyncGetIssuersOf(cert.get(), &cert_source_request);
  ASSERT_NE(nullptr, cert_source_request);

  bssl::ParsedCertificateList result_certs;
  cert_source_request->GetNext(&result_certs);
  ASSERT_EQ(2u, result_certs.size());

  // The fingerprint of the Google certificate used in the parsing tests.
  SHA256HashValue google_parse_fingerprint = {
      {0xf6, 0x41, 0xc3, 0x6c, 0xfe, 0xf4, 0x9b, 0xc0, 0x71, 0x35, 0x9e,
       0xcf, 0x88, 0xee, 0xd9, 0x31, 0x7b, 0x73, 0x8b, 0x59, 0x89, 0x41,
       0x6a, 0xd4, 0x01, 0x72, 0x0c, 0x0a, 0x4e, 0x2e, 0x63, 0x52}};
  // The fingerprint for the Thawte SGC certificate
  SHA256HashValue thawte_parse_fingerprint = {
      {0x10, 0x85, 0xa6, 0xf4, 0x54, 0xd0, 0xc9, 0x11, 0x98, 0xfd, 0xda,
       0xb1, 0x1a, 0x31, 0xc7, 0x16, 0xd5, 0xdc, 0xd6, 0x8d, 0xf9, 0x1c,
       0x03, 0x9c, 0xe1, 0x8d, 0xca, 0x9b, 0xeb, 0x3c, 0xde, 0x3d}};
  EXPECT_EQ(google_parse_fingerprint, X509Certificate::CalculateFingerprint256(
                                          result_certs[0]->cert_buffer()));
  EXPECT_EQ(thawte_parse_fingerprint, X509Certificate::CalculateFingerprint256(
                                          result_certs[1]->cert_buffer()));
  result_certs.clear();
  cert_source_request->GetNext(&result_certs);
  EXPECT_TRUE(result_certs.empty());
}

}  // namespace

}  // namespace net
