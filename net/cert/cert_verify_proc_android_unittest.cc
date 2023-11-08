// Copyright 2017 The Chromium Authors
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
#include "net/cert/mock_cert_net_fetcher.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_builder.h"
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

const char kHostname[] = "example.com";
const GURL kRootURL("http://aia.test/root");
const GURL kIntermediateURL("http://aia.test/intermediate");

std::unique_ptr<CertNetFetcher::Request>
CreateMockRequestWithInvalidCertificate() {
  return MockCertNetFetcherRequest::Create(std::vector<uint8_t>({1, 2, 3}));
}

// A test fixture for testing CertVerifyProcAndroid AIA fetching. It creates,
// sets up, and shuts down a MockCertNetFetcher for CertVerifyProcAndroid to
// use, and enables the field trial for AIA fetching.
class CertVerifyProcAndroidTestWithAIAFetching : public testing::Test {
 public:
  void SetUp() override {
    fetcher_ = base::MakeRefCounted<MockCertNetFetcher>();

    // Generate a certificate chain with AIA pointers. Tests can modify these
    // if testing a different scenario.
    std::tie(leaf_, intermediate_, root_) = CertBuilder::CreateSimpleChain3();
    root_->SetCaIssuersUrl(kRootURL);
    intermediate_->SetCaIssuersUrl(kRootURL);
    leaf_->SetCaIssuersUrl(kIntermediateURL);
    leaf_->SetSubjectAltName(kHostname);
  }

  void TearDown() override {
    // Ensure that mock expectations are checked, since the CertNetFetcher is
    // global and leaky.
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(fetcher_.get()));
  }

  scoped_refptr<X509Certificate> LeafOnly() {
    return leaf_->GetX509Certificate();
  }

  scoped_refptr<X509Certificate> LeafWithIntermediate() {
    return leaf_->GetX509CertificateChain();
  }

 protected:
  void TrustTestRoot() {
    scoped_test_root_.Reset({root_->GetX509Certificate()});
  }

  scoped_refptr<MockCertNetFetcher> fetcher_;
  std::unique_ptr<CertBuilder> root_;
  std::unique_ptr<CertBuilder> intermediate_;
  std::unique_ptr<CertBuilder> leaf_;

 private:
  ScopedTestRoot scoped_test_root_;
};

}  // namespace

// Tests that if the proper intermediates are supplied in the server-sent chain,
// no AIA fetch occurs.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       NoFetchIfProperIntermediatesSupplied) {
  TrustTestRoot();
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());
  CertVerifyResult verify_result;
  EXPECT_EQ(OK, proc->Verify(LeafWithIntermediate().get(), kHostname,
                             /*ocsp_response=*/std::string(),
                             /*sct_list=*/std::string(), 0, &verify_result,
                             NetLogWithSource()));
}

// Tests that if the certificate does not contain an AIA URL, no AIA fetch
// occurs.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, NoAIAURL) {
  leaf_->SetCaIssuersAndOCSPUrls(/*ca_issuers_urls=*/{}, /*ocsp_urls=*/{});
  TrustTestRoot();
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());
  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(LeafOnly().get(), kHostname, /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, &verify_result,
                   NetLogWithSource()));
}

// Tests that if a certificate contains one file:// URL and one http:// URL,
// there are two fetches, with the latter resulting in a successful
// verification.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, OneFileAndOneHTTPURL) {
  const GURL kFileURL("file:///dev/null");
  leaf_->SetCaIssuersAndOCSPUrls(
      /*ca_issuers_urls=*/{kFileURL, kIntermediateURL},
      /*ocsp_urls=*/{});
  TrustTestRoot();
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());

  // Expect two fetches: the file:// URL (which returns an error), and the
  // http:// URL that returns a valid intermediate signed by |root_|. Though the
  // intermediate itself contains an AIA URL, it should not be fetched because
  // |root_| is in the test trust store.
  EXPECT_CALL(*fetcher_, FetchCaIssuers(kFileURL, _, _))
      .WillOnce(Return(ByMove(
          MockCertNetFetcherRequest::Create(ERR_DISALLOWED_URL_SCHEME))));
  EXPECT_CALL(*fetcher_, FetchCaIssuers(kIntermediateURL, _, _))
      .WillOnce(Return(ByMove(
          MockCertNetFetcherRequest::Create(intermediate_->GetCertBuffer()))));

  CertVerifyResult verify_result;
  EXPECT_EQ(OK, proc->Verify(LeafOnly().get(), kHostname,
                             /*ocsp_response=*/std::string(),
                             /*sct_list=*/std::string(), 0, &verify_result,
                             NetLogWithSource()));
}

// Tests that if an AIA request returns the wrong intermediate, certificate
// verification should fail.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       UnsuccessfulVerificationWithLeafOnly) {
  TrustTestRoot();
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());
  const scoped_refptr<X509Certificate> bad_intermediate =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");

  EXPECT_CALL(*fetcher_, FetchCaIssuers(kIntermediateURL, _, _))
      .WillOnce(Return(ByMove(
          MockCertNetFetcherRequest::Create(bad_intermediate->cert_buffer()))));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(LeafOnly().get(), kHostname, /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, &verify_result,
                   NetLogWithSource()));
}

// Tests that if an AIA request returns an error, certificate verification
// should fail.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       UnsuccessfulVerificationWithLeafOnlyAndErrorOnFetch) {
  TrustTestRoot();
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());

  EXPECT_CALL(*fetcher_, FetchCaIssuers(kIntermediateURL, _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(ERR_FAILED))));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(LeafOnly().get(), kHostname, /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, &verify_result,
                   NetLogWithSource()));
}

// Tests that if an AIA request returns an unparseable cert, certificate
// verification should fail.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       UnsuccessfulVerificationWithLeafOnlyAndUnparseableFetch) {
  TrustTestRoot();
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());

  EXPECT_CALL(*fetcher_, FetchCaIssuers(kIntermediateURL, _, _))
      .WillOnce(Return(ByMove(CreateMockRequestWithInvalidCertificate())));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(LeafOnly().get(), kHostname, /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, &verify_result,
                   NetLogWithSource()));
}

// Tests that if a certificate has two HTTP AIA URLs, they are both fetched. If
// one serves an unrelated certificate and one serves a proper intermediate, the
// latter should be used to build a valid chain.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, TwoHTTPURLs) {
  const GURL kUnrelatedURL("http://aia.test/unrelated");
  leaf_->SetCaIssuersAndOCSPUrls(
      /*ca_issuers_urls=*/{kUnrelatedURL, kIntermediateURL},
      /*ocsp_urls=*/{});
  scoped_refptr<X509Certificate> unrelated =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(unrelated);

  TrustTestRoot();
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());

  // Expect two fetches, the first of which returns an unrelated certificate
  // that is not useful in chain-building, and the second of which returns a
  // valid intermediate signed by |root_|. Though the intermediate itself
  // contains an AIA URL, it should not be fetched because |root_| is in the
  // trust store.
  EXPECT_CALL(*fetcher_, FetchCaIssuers(kUnrelatedURL, _, _))
      .WillOnce(Return(
          ByMove(MockCertNetFetcherRequest::Create(unrelated->cert_buffer()))));
  EXPECT_CALL(*fetcher_, FetchCaIssuers(kIntermediateURL, _, _))
      .WillOnce(Return(ByMove(
          MockCertNetFetcherRequest::Create(intermediate_->GetCertBuffer()))));

  CertVerifyResult verify_result;
  EXPECT_EQ(OK, proc->Verify(LeafOnly().get(), kHostname,
                             /*ocsp_response=*/std::string(),
                             /*sct_list=*/std::string(), 0, &verify_result,
                             NetLogWithSource()));
}

// Tests that if an intermediate is fetched via AIA, and the intermediate itself
// has an AIA URL, that URL is fetched if necessary.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching,
       AIAFetchForFetchedIntermediate) {
  // Do not set up the test root to be trusted. If the test root were trusted,
  // then the intermediate would not require an AIA fetch. With the test root
  // untrusted, the intermediate does not verify and so it will trigger an AIA
  // fetch.
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());

  // Expect two fetches, the first of which returns an intermediate that itself
  // has an AIA URL.
  EXPECT_CALL(*fetcher_, FetchCaIssuers(kIntermediateURL, _, _))
      .WillOnce(Return(ByMove(
          MockCertNetFetcherRequest::Create(intermediate_->GetCertBuffer()))));
  EXPECT_CALL(*fetcher_, FetchCaIssuers(kRootURL, _, _))
      .WillOnce(Return(
          ByMove(MockCertNetFetcherRequest::Create(root_->GetCertBuffer()))));

  CertVerifyResult verify_result;
  // This chain results in an AUTHORITY_INVALID root because |root_| is not
  // trusted.
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(LeafOnly().get(), kHostname, /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, &verify_result,
                   NetLogWithSource()));
}

// Tests that if a certificate contains six AIA URLs, only the first five are
// fetched, since the maximum number of fetches per Verify() call is five.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, MaxAIAFetches) {
  leaf_->SetCaIssuersAndOCSPUrls(
      /*ca_issuers_urls=*/{GURL("http://aia.test/1"), GURL("http://aia.test/2"),
                           GURL("http://aia.test/3"), GURL("http://aia.test/4"),
                           GURL("http://aia.test/5"),
                           GURL("http://aia.test/6")},
      /*ocsp_urls=*/{});
  TrustTestRoot();
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());

  EXPECT_CALL(*fetcher_, FetchCaIssuers(_, _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(ERR_FAILED))))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(ERR_FAILED))))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(ERR_FAILED))))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(ERR_FAILED))))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(ERR_FAILED))));

  CertVerifyResult verify_result;
  EXPECT_EQ(
      ERR_CERT_AUTHORITY_INVALID,
      proc->Verify(LeafOnly().get(), kHostname, /*ocsp_response=*/std::string(),
                   /*sct_list=*/std::string(), 0, &verify_result,
                   NetLogWithSource()));
}

// Tests that if the supplied chain contains an intermediate with an AIA URL,
// that AIA URL is fetched if necessary.
TEST_F(CertVerifyProcAndroidTestWithAIAFetching, FetchForSuppliedIntermediate) {
  // Do not set up the test root to be trusted. If the test root were trusted,
  // then the intermediate would not require an AIA fetch. With the test root
  // untrusted, the intermediate does not verify and so it will trigger an AIA
  // fetch.
  scoped_refptr<CertVerifyProcAndroid> proc =
      base::MakeRefCounted<CertVerifyProcAndroid>(fetcher_,
                                                  CRLSet::BuiltinCRLSet());

  EXPECT_CALL(*fetcher_, FetchCaIssuers(kRootURL, _, _))
      .WillOnce(Return(
          ByMove(MockCertNetFetcherRequest::Create(root_->GetCertBuffer()))));

  CertVerifyResult verify_result;
  // This chain results in an AUTHORITY_INVALID root because |root_| is not
  // trusted.
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID,
            proc->Verify(LeafWithIntermediate().get(), kHostname,
                         /*ocsp_response=*/std::string(),
                         /*sct_list=*/std::string(), 0, &verify_result,
                         NetLogWithSource()));
}

}  // namespace net
