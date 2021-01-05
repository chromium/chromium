// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_android.h"

#include <memory>
#include <vector>

#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verify_proc_android.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/internal/test_helpers.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::ByMove;
using ::testing::Return;
using ::testing::_;

namespace net {

namespace {

// A CertNetFetcher::Request whose WaitForResult() method always
// immediately returns the |error| and |bytes| provided in its
// constructor.
class TestCertNetFetcherRequest : public CertNetFetcher::Request {
 public:
  TestCertNetFetcherRequest(Error error, const std::vector<uint8_t>& bytes)
      : error_(error), bytes_(bytes) {}
  ~TestCertNetFetcherRequest() override {}

  void WaitForResult(Error* error, std::vector<uint8_t>* bytes) override {
    *error = error_;
    *bytes = bytes_;
  }

 private:
  Error error_;
  std::vector<uint8_t> bytes_;
};

class MockCertNetFetcher : public CertNetFetcher {
 public:
  MockCertNetFetcher() {}

  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD3(FetchCaIssuers, std::unique_ptr<Request>(const GURL&, int, int));
  MOCK_METHOD3(FetchCrl, std::unique_ptr<Request>(const GURL&, int, int));
  MOCK_METHOD3(FetchOcsp, std::unique_ptr<Request>(const GURL&, int, int));

 private:
  ~MockCertNetFetcher() override {}
};

std::unique_ptr<CertNetFetcher::Request> CreateMockRequestFromX509Certificate(
    Error error,
    const scoped_refptr<X509Certificate>& cert) {
  base::StringPiece der =
      x509_util::CryptoBufferAsStringPiece(cert->cert_buffer());
  return std::make_unique<TestCertNetFetcherRequest>(
      error, std::vector<uint8_t>(der.data(), der.data() + der.length()));
}

std::unique_ptr<CertNetFetcher::Request> CreateMockRequestWithError(
    Error error) {
  return std::make_unique<TestCertNetFetcherRequest>(error,
                                                     std::vector<uint8_t>({}));
}

std::unique_ptr<CertNetFetcher::Request>
CreateMockRequestWithInvalidCertificate() {
  return std::make_unique<TestCertNetFetcherRequest>(
      OK, std::vector<uint8_t>({1, 2, 3}));
}

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
    scoped_refptr<X509Certificate>* result) {
  std::string der;
  ::testing::AssertionResult r =
      ReadTestPem("net/data/cert_issuer_source_aia_unittest/" + file_name,
                  "CERTIFICATE", &der);
  if (!r)
    return r;
  *result = X509Certificate::CreateFromBytes(der.data(), der.length());
  if (!result) {
    return ::testing::AssertionFailure()
           << "X509Certificate::CreateFromBytes() failed";
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult ReadTestAIARoot(
    scoped_refptr<X509Certificate>* result) {
  return ReadTestCert("root.pem", result);
}

::testing::AssertionResult CreateCertificateChainFromFiles(
    const std::vector<std::string>& files,
    scoped_refptr<X509Certificate>* result) {
  scoped_refptr<X509Certificate> leaf;
  ::testing::AssertionResult r = ReadTestCert(files[0], &leaf);
  if (!r)
    return r;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediate_buffers;
  for (size_t i = 1; i < files.size(); i++) {
    scoped_refptr<X509Certificate> intermediate;
    r = ReadTestCert(files[i], &intermediate);
    if (!r)
      return r;
    intermediate_buffers.push_back(bssl::UpRef(intermediate->cert_buffer()));
  }
  *result = X509Certificate::CreateFromBuffer(bssl::UpRef(leaf->cert_buffer()),
                                              std::move(intermediate_buffers));
  return ::testing::AssertionSuccess();
}

// A test fixture for testing CertVerifyProcAndroid AIA fetching. It creates,
// sets up, and shuts down a MockCertNetFetcher for CertVerifyProcAndroid to
// use, and enables the field trial for AIA fetching.
class CertVerifyProcAndroidTestWithAIAFetching : public testing::Test {
 public:
  void SetUp() override {
    fetcher_ = base::MakeRefCounted<MockCertNetFetcher>();
  }

  void TearDown() override {
    // Ensure that mock expectations are checked, since the CertNetFetcher is
    // global and leaky.
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(fetcher_.get()));
  }

 protected:
  ::testing::AssertionResult SetUpTestRoot() {
    ::testing::AssertionResult r = ReadTestAIARoot(&root_);
    if (!r)
      return r;
    scoped_test_root_.reset(new ScopedTestRoot(root_.get()));
    return ::testing::AssertionSuccess();
  }

  scoped_refptr<MockCertNetFetcher> fetcher_;
  const CertificateList empty_cert_list_;

 private:
  scoped_refptr<X509Certificate> root_;
  std::unique_ptr<ScopedTestRoot> scoped_test_root_;
};

}  // namespace

// Tests that if the proper intermediates are supplied in the server-sent chain,
// no AIA fetch occurs.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       NoFetchIfProperIntermediatesSupplied) {
  ASSERT_TRUE(SetUpTestRoot());
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> leaf;
  ASSERT_TRUE(
      CreateCertificateChainFromFiles({"target_one_aia.pem", "i.pem"}, &leaf));
  CertVerifyResult verify_result;
  EXPECT_EQ(
      OK,
      proc->Verify(leaf.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

// Tests that if the certificate does not contain an AIA URL, no AIA fetch
// occurs.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, NoAIAURL) {
  ASSERT_TRUE(SetUpTestRoot());
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> cert;
  ASSERT_TRUE(ReadTestCert("target_no_aia.pem", &cert));
  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(cert.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

// Tests that if a certificate contains one file:// URL and one http:// URL,
// there are two fetches, with the latter resulting in a successful
// verification.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, OneFileAndOneHTTPURL) {
  ASSERT_TRUE(SetUpTestRoot());
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> cert;
  ASSERT_TRUE(ReadTestCert("target_file_and_http_aia.pem", &cert));
  scoped_refptr<X509Certificate> intermediate;
  ASSERT_TRUE(ReadTestCert("i2.pem", &intermediate));

  // Expect two fetches: the file:// URL (which returns an error), and the
  // http:// URL that returns a valid intermediate signed by |root_|. Though the
  // intermediate itself contains an AIA URL, it should not be fetched because
  // |root_| is in the test trust store.
  EXPECT_CALL(*fetcher_, FetchCaIssuers(GURL("file:///dev/null"), _, _))
      .WillOnce(Return(
          ByMove(CreateMockRequestWithError(ERR_DISALLOWED_URL_SCHEME))));
  EXPECT_CALL(*fetcher_,
              FetchCaIssuers(GURL("http://url-for-aia2/I2.foo"), _, _))
      .WillOnce(Return(
          ByMove(CreateMockRequestFromX509Certificate(OK, intermediate))));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      OK,
      proc->Verify(cert.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

// Tests that if an AIA request returns the wrong intermediate, certificate
// verification should fail.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       UnsuccessfulVerificationWithLeafOnly) {
  ASSERT_TRUE(SetUpTestRoot());
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> cert;
  ASSERT_TRUE(ReadTestCert("target_one_aia.pem", &cert));
  const scoped_refptr<X509Certificate> bad_intermediate =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");

  EXPECT_CALL(*fetcher_, FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(
          ByMove(CreateMockRequestFromX509Certificate(OK, bad_intermediate))));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(cert.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

// Tests that if an AIA request returns an error, certificate verification
// should fail.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       UnsuccessfulVerificationWithLeafOnlyAndErrorOnFetch) {
  ASSERT_TRUE(SetUpTestRoot());
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> cert;
  ASSERT_TRUE(ReadTestCert("target_one_aia.pem", &cert));

  EXPECT_CALL(*fetcher_, FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(ByMove(CreateMockRequestWithError(ERR_FAILED))));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(cert.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

// Tests that if an AIA request returns an unparseable cert, certificate
// verification should fail.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       UnsuccessfulVerificationWithLeafOnlyAndUnparseableFetch) {
  ASSERT_TRUE(SetUpTestRoot());
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> cert;
  ASSERT_TRUE(ReadTestCert("target_one_aia.pem", &cert));

  EXPECT_CALL(*fetcher_, FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(ByMove(CreateMockRequestWithInvalidCertificate())));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(cert.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

// Tests that if a certificate has two HTTP AIA URLs, they are both fetched. If
// one serves an unrelated certificate and one serves a proper intermediate, the
// latter should be used to build a valid chain.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, TwoHTTPURLs) {
  ASSERT_TRUE(SetUpTestRoot());
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> cert;
  ASSERT_TRUE(ReadTestCert("target_two_aia.pem", &cert));
  scoped_refptr<X509Certificate> intermediate;
  ASSERT_TRUE(ReadTestCert("i2.pem", &intermediate));
  scoped_refptr<X509Certificate> unrelated;
  ASSERT_TRUE(ReadTestCert("target_three_aia.pem", &unrelated));

  // Expect two fetches, the first of which returns an unrelated certificate
  // that is not useful in chain-building, and the second of which returns a
  // valid intermediate signed by |root_|. Though the intermediate itself
  // contains an AIA URL, it should not be fetched because |root_| is in the
  // trust store.
  EXPECT_CALL(*fetcher_, FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(
          Return(ByMove(CreateMockRequestFromX509Certificate(OK, unrelated))));
  EXPECT_CALL(*fetcher_,
              FetchCaIssuers(GURL("http://url-for-aia2/I2.foo"), _, _))
      .WillOnce(Return(
          ByMove(CreateMockRequestFromX509Certificate(OK, intermediate))));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      OK,
      proc->Verify(cert.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

// Tests that if an intermediate is fetched via AIA, and the intermediate itself
// has an AIA URL, that URL is fetched if necessary.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       AIAFetchForFetchedIntermediate) {
  // Do not set up the test root to be trusted. If the test root were trusted,
  // then the intermediate i2.pem would not require an AIA fetch. With the test
  // root untrusted, i2.pem does not verify and so it will trigger an AIA fetch.
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> cert;
  ASSERT_TRUE(ReadTestCert("target_one_aia.pem", &cert));
  scoped_refptr<X509Certificate> intermediate;
  ASSERT_TRUE(ReadTestCert("i2.pem", &intermediate));
  scoped_refptr<X509Certificate> root;
  ASSERT_TRUE(ReadTestAIARoot(&root));

  // Expect two fetches, the first of which returns an intermediate that itself
  // has an AIA URL.
  EXPECT_CALL(*fetcher_, FetchCaIssuers(GURL("http://url-for-aia/I.cer"), _, _))
      .WillOnce(Return(
          ByMove(CreateMockRequestFromX509Certificate(OK, intermediate))));
  EXPECT_CALL(*fetcher_,
              FetchCaIssuers(GURL("http://url-for-aia/Root.cer"), _, _))
      .WillOnce(Return(ByMove(CreateMockRequestFromX509Certificate(OK, root))));

  CertVerifyResult verify_result;
  // This chain results in an AUTHORITY_INVALID root because |root_| is not
  // trusted.
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(cert.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

// Tests that if a certificate contains six AIA URLs, only the first five are
// fetched, since the maximum number of fetches per Verify() call is five.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, MaxAIAFetches) {
  ASSERT_TRUE(SetUpTestRoot());
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> cert;
  ASSERT_TRUE(ReadTestCert("target_six_aia.pem", &cert));

  EXPECT_CALL(*fetcher_, FetchCaIssuers(_, _, _))
      .WillOnce(Return(ByMove(CreateMockRequestWithError(ERR_FAILED))))
      .WillOnce(Return(ByMove(CreateMockRequestWithError(ERR_FAILED))))
      .WillOnce(Return(ByMove(CreateMockRequestWithError(ERR_FAILED))))
      .WillOnce(Return(ByMove(CreateMockRequestWithError(ERR_FAILED))))
      .WillOnce(Return(ByMove(CreateMockRequestWithError(ERR_FAILED))));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(cert.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

// Tests that if the supplied chain contains an intermediate with an AIA URL,
// that AIA URL is fetched if necessary.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, FetchForSuppliedIntermediate) {
  // Do not set up the test root to be trusted. If the test root were trusted,
  // then the intermediate i.pem would not require an AIA fetch. With the test
  // root untrusted, i.pem does not verify and so it will trigger an AIA fetch.
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_);
  scoped_refptr<X509Certificate> leaf;
  ASSERT_TRUE(
      CreateCertificateChainFromFiles({"target_one_aia.pem", "i.pem"}, &leaf));
  scoped_refptr<X509Certificate> root;
  ASSERT_TRUE(ReadTestAIARoot(&root));

  EXPECT_CALL(*fetcher_,
              FetchCaIssuers(GURL("http://url-for-aia/Root.cer"), _, _))
      .WillOnce(Return(ByMove(CreateMockRequestFromX509Certificate(OK, root))));

  CertVerifyResult verify_result;
  // This chain results in an AUTHORITY_INVALID root because |root_| is not
  // trusted.
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(leaf.get(), "target", /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
                   empty_cert_list_, &verify_result, NetLogWithSource()));
}

}  // namespace net
