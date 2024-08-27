// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/multi_threaded_cert_verifier.h"

#include <memory>

#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;
using testing::_;
using testing::DoAll;
using testing::Return;

namespace net {

class ChromeRootStoreData;
class CertNetFetcher;

namespace {

void FailTest(int /* result */) {
  FAIL();
}

class MockCertVerifyProc : public CertVerifyProc {
 public:
  MockCertVerifyProc() : CertVerifyProc(CRLSet::BuiltinCRLSet()) {}
  MOCK_METHOD7(VerifyInternal,
               int(X509Certificate*,
                   const std::string&,
                   const std::string&,
                   const std::string&,
                   int,
                   CertVerifyResult*,
                   const NetLogWithSource&));
  MOCK_CONST_METHOD0(SupportsAdditionalTrustAnchors, bool());

 private:
  ~MockCertVerifyProc() override = default;
};

ACTION(SetCertVerifyResult) {
  X509Certificate* cert = arg0;
  CertVerifyResult* result = arg5;
  result->Reset();
  result->verified_cert = cert;
  result->cert_status = CERT_STATUS_COMMON_NAME_INVALID;
}

ACTION(SetCertVerifyRevokedResult) {
  X509Certificate* cert = arg0;
  CertVerifyResult* result = arg5;
  result->Reset();
  result->verified_cert = cert;
  result->cert_status = CERT_STATUS_REVOKED;
}

class SwapWithNewProcFactory : public CertVerifyProcFactory {
 public:
  explicit SwapWithNewProcFactory(scoped_refptr<CertVerifyProc> new_mock_proc)
      : mock_verify_proc_(std::move(new_mock_proc)) {}

  scoped_refptr<net::CertVerifyProc> CreateCertVerifyProc(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      const CertVerifyProc::ImplParams& impl_params,
      const CertVerifyProc::InstanceParams& instance_params) override {
    return mock_verify_proc_;
  }

 protected:
  ~SwapWithNewProcFactory() override = default;
  scoped_refptr<CertVerifyProc> mock_verify_proc_;
};

}  // namespace

class MultiThreadedCertVerifierTest : public TestWithTaskEnvironment {
 public:
  MultiThreadedCertVerifierTest()
      : mock_verify_proc_(base::MakeRefCounted<MockCertVerifyProc>()),
        mock_new_verify_proc_(base::MakeRefCounted<MockCertVerifyProc>()),
        verifier_(std::make_unique<MultiThreadedCertVerifier>(
            mock_verify_proc_,
            base::MakeRefCounted<SwapWithNewProcFactory>(
                mock_new_verify_proc_))) {
    EXPECT_CALL(*mock_verify_proc_, SupportsAdditionalTrustAnchors())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_verify_proc_, VerifyInternal(_, _, _, _, _, _, _))
        .WillRepeatedly(
            DoAll(SetCertVerifyResult(), Return(ERR_CERT_COMMON_NAME_INVALID)));
  }
  ~MultiThreadedCertVerifierTest() override = default;

 protected:
  scoped_refptr<MockCertVerifyProc> mock_verify_proc_;
  // The new verify_proc_ swapped in if the proc is updated.
  scoped_refptr<MockCertVerifyProc> mock_new_verify_proc_;
  std::unique_ptr<MultiThreadedCertVerifier> verifier_;
};

// Tests that the callback of a canceled request is never made.
TEST_F(MultiThreadedCertVerifierTest, CancelRequest) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());

  int error;
  CertVerifyResult verify_result;
  std::unique_ptr<CertVerifier::Request> request;

  error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, base::BindOnce(&FailTest), &request, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  ASSERT_TRUE(request);
  request.reset();

  // Issue a few more requests to the worker pool and wait for their
  // completion, so that the task of the canceled request (which runs on a
  // worker thread) is likely to complete by the end of this test.
  TestCompletionCallback callback;
  for (int i = 0; i < 5; ++i) {
    error = verifier_->Verify(
        CertVerifier::RequestParams(test_cert, "www2.example.com", 0,
                                    /*ocsp_response=*/std::string(),
                                    /*sct_list=*/std::string()),
        &verify_result, callback.callback(), &request, NetLogWithSource());
    ASSERT_THAT(error, IsError(ERR_IO_PENDING));
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
  }
}

// Tests that the callback of a request is never made if the |verifier_| itself
// is deleted.
TEST_F(MultiThreadedCertVerifierTest, DeleteVerifier) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());

  int error;
  CertVerifyResult verify_result;
  std::unique_ptr<CertVerifier::Request> request;

  error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, base::BindOnce(&FailTest), &request, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  ASSERT_TRUE(request);
  verifier_.reset();

  RunUntilIdle();
}

namespace {

struct CertVerifyResultHelper {
  void FailTest(int /* result */) { FAIL(); }
  std::unique_ptr<CertVerifier::Request> request;
};

}  // namespace

// The same as the above "DeleteVerifier" test, except the callback provided
// will own the CertVerifier::Request as allowed by the CertVerifier contract.
// This is a regression test for https://crbug.com/1157562.
TEST_F(MultiThreadedCertVerifierTest, DeleteVerifierCallbackOwnsResult) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());

  int error;
  CertVerifyResult verify_result;
  std::unique_ptr<CertVerifyResultHelper> result_helper =
      std::make_unique<CertVerifyResultHelper>();
  CertVerifyResultHelper* result_helper_ptr = result_helper.get();
  CompletionOnceCallback callback = base::BindOnce(
      &CertVerifyResultHelper::FailTest, std::move(result_helper));

  error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, std::move(callback), &result_helper_ptr->request,
      NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  ASSERT_TRUE(result_helper_ptr->request);
  verifier_.reset();

  RunUntilIdle();
}

// Tests that a canceled request is not leaked.
TEST_F(MultiThreadedCertVerifierTest, CancelRequestThenQuit) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), test_cert.get());

  int error;
  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;

  {
    // Because shutdown intentionally doesn't join worker threads, memory may
    // be leaked if the main thread shuts down before the worker thread
    // completes. In particular MultiThreadedCertVerifier calls
    // base::WorkerPool::PostTaskAndReply(), which leaks its "relay" when it
    // can't post the reply back to the origin thread. See
    // https://crbug.com/522514
    ANNOTATE_SCOPED_MEMORY_LEAK;
    error = verifier_->Verify(
        CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                    /*ocsp_response=*/std::string(),
                                    /*sct_list=*/std::string()),
        &verify_result, callback.callback(), &request, NetLogWithSource());
  }
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);
  request.reset();
  // Destroy |verifier_| by going out of scope.
}

// Tests propagation of configuration options into CertVerifyProc flags
TEST_F(MultiThreadedCertVerifierTest, ConvertsConfigToFlags) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  const struct TestConfig {
    bool CertVerifier::Config::*config_ptr;
    int expected_flag;
  } kTestConfig[] = {
      {&CertVerifier::Config::enable_rev_checking,
       CertVerifyProc::VERIFY_REV_CHECKING_ENABLED},
      {&CertVerifier::Config::require_rev_checking_local_anchors,
       CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS},
      {&CertVerifier::Config::enable_sha1_local_anchors,
       CertVerifyProc::VERIFY_ENABLE_SHA1_LOCAL_ANCHORS},
      {&CertVerifier::Config::disable_symantec_enforcement,
       CertVerifyProc::VERIFY_DISABLE_SYMANTEC_ENFORCEMENT},
  };
  for (const auto& test_config : kTestConfig) {
    CertVerifier::Config config;
    config.*test_config.config_ptr = true;

    verifier_->SetConfig(config);

    EXPECT_CALL(*mock_verify_proc_,
                VerifyInternal(_, _, _, _, test_config.expected_flag, _, _))
        .WillRepeatedly(
            DoAll(SetCertVerifyRevokedResult(), Return(ERR_CERT_REVOKED)));

    CertVerifyResult verify_result;
    TestCompletionCallback callback;
    std::unique_ptr<CertVerifier::Request> request;
    int error = verifier_->Verify(
        CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                    /*ocsp_response=*/std::string(),
                                    /*sct_list=*/std::string()),
        &verify_result, callback.callback(), &request, NetLogWithSource());
    ASSERT_THAT(error, IsError(ERR_IO_PENDING));
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_TRUE(IsCertificateError(error));
    EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

    testing::Mock::VerifyAndClearExpectations(mock_verify_proc_.get());
  }
}

// Tests propagation of CertVerifier flags into CertVerifyProc flags
TEST_F(MultiThreadedCertVerifierTest, ConvertsFlagsToFlags) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  EXPECT_CALL(
      *mock_verify_proc_,
      VerifyInternal(_, _, _, _, CertVerifyProc::VERIFY_DISABLE_NETWORK_FETCHES,
                     _, _))
      .WillRepeatedly(
          DoAll(SetCertVerifyRevokedResult(), Return(ERR_CERT_REVOKED)));

  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com",
                                  CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);
  error = callback.WaitForResult();
  EXPECT_TRUE(IsCertificateError(error));
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  testing::Mock::VerifyAndClearExpectations(mock_verify_proc_.get());
}

// Tests swapping in new Chrome Root Store Data.
TEST_F(MultiThreadedCertVerifierTest, VerifyProcChangeChromeRootStore) {
  CertVerifierObserverCounter observer_counter(verifier_.get());

  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  EXPECT_EQ(observer_counter.change_count(), 0u);

  EXPECT_CALL(*mock_new_verify_proc_, VerifyInternal(_, _, _, _, _, _, _))
      .WillRepeatedly(
          DoAll(SetCertVerifyRevokedResult(), Return(ERR_CERT_REVOKED)));
  verifier_->UpdateVerifyProcData(nullptr, {}, {});

  EXPECT_EQ(observer_counter.change_count(), 1u);

  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);
  error = callback.WaitForResult();
  EXPECT_TRUE(IsCertificateError(error));
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  testing::Mock::VerifyAndClearExpectations(mock_verify_proc_.get());
  testing::Mock::VerifyAndClearExpectations(mock_new_verify_proc_.get());
}

// Tests swapping out a new proc while a request is pending still uses
// the old proc for the old request.
TEST_F(MultiThreadedCertVerifierTest, VerifyProcChangeRequest) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "ok_cert.pem"));
  ASSERT_TRUE(test_cert);

  CertVerifyResult verify_result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier_->Verify(
      CertVerifier::RequestParams(test_cert, "www.example.com", 0,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, callback.callback(), &request, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);
  verifier_->UpdateVerifyProcData(nullptr, {}, {});
  error = callback.WaitForResult();
  EXPECT_TRUE(IsCertificateError(error));
  EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));

  testing::Mock::VerifyAndClearExpectations(mock_verify_proc_.get());
  testing::Mock::VerifyAndClearExpectations(mock_new_verify_proc_.get());
}

}  // namespace net
