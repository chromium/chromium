// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_ftp_job.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "net/base/auth.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/ftp/ftp_auth_cache.h"
#include "net/ftp/ftp_response_info.h"
#include "net/ftp/ftp_transaction.h"
#include "net/ftp/ftp_transaction_factory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class MockFtpTransaction : public FtpTransaction {
 public:
  MockFtpTransaction(int start_return_value,
                     int read_return_value,
                     bool needs_auth,
                     std::vector<int> restart_return_values)
      : start_return_value_(start_return_value),
        read_return_value_(read_return_value),
        restart_return_values_(restart_return_values),
        restart_index_(0) {
    response_.needs_auth = needs_auth;
  }
  ~MockFtpTransaction() override {}

  int Start(const FtpRequestInfo* request_info,
            CompletionOnceCallback callback,
            const NetLogWithSource& net_log,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return start_return_value_;
  }

  int RestartWithAuth(const AuthCredentials& credentials,
                      CompletionOnceCallback callback) override {
    CHECK(restart_index_ < restart_return_values_.size());
    return restart_return_values_[restart_index_++];
  }

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return read_return_value_;
  }

  const FtpResponseInfo* GetResponseInfo() const override { return &response_; }

  LoadState GetLoadState() const override { return LOAD_STATE_IDLE; }

  uint64_t GetUploadProgress() const override { return 0; }

 private:
  FtpResponseInfo response_;
  int start_return_value_;
  int read_return_value_;
  std::vector<int> restart_return_values_;
  unsigned int restart_index_;

  DISALLOW_COPY_AND_ASSIGN(MockFtpTransaction);
};

class MockFtpTransactionFactory : public FtpTransactionFactory {
 public:
  MockFtpTransactionFactory(int start_return_value,
                            int read_return_value,
                            bool needs_auth,
                            std::vector<int> restart_return_values)
      : start_return_value_(start_return_value),
        read_return_value_(read_return_value),
        needs_auth_(needs_auth),
        restart_return_values_(restart_return_values) {}

  ~MockFtpTransactionFactory() override {}

  std::unique_ptr<FtpTransaction> CreateTransaction() override {
    return std::make_unique<MockFtpTransaction>(start_return_value_,
                                                read_return_value_, needs_auth_,
                                                restart_return_values_);
  }

  void Suspend(bool suspend) override {}

 private:
  int start_return_value_;
  int read_return_value_;
  bool needs_auth_;
  std::vector<int> restart_return_values_;

  DISALLOW_COPY_AND_ASSIGN(MockFtpTransactionFactory);
};

class MockURLRequestFtpJobFactory : public URLRequestJobFactory {
 public:
  MockURLRequestFtpJobFactory(int start_return_value,
                              int read_return_value,
                              bool needs_auth,
                              std::vector<int> restart_return_values)
      : auth_cache(new FtpAuthCache()),
        factory(new MockFtpTransactionFactory(start_return_value,
                                              read_return_value,
                                              needs_auth,
                                              restart_return_values)) {}

  ~MockURLRequestFtpJobFactory() override {
    delete auth_cache;
    delete factory;
  }

  URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    return new URLRequestFtpJob(request, network_delegate, factory, auth_cache);
  }

  bool IsHandledProtocol(const std::string& scheme) const override {
    return scheme == "ftp";
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return true;
  }

 private:
  FtpAuthCache* auth_cache;
  MockFtpTransactionFactory* factory;

  DISALLOW_COPY_AND_ASSIGN(MockURLRequestFtpJobFactory);
};

using UrlRequestFtpJobTest = TestWithTaskEnvironment;

TEST_F(UrlRequestFtpJobTest, HistogramLogSuccessNoAuth) {
  base::HistogramTester histograms;
  MockURLRequestFtpJobFactory url_request_ftp_job_factory(OK, OK, false, {OK});
  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_job_factory(&url_request_ftp_job_factory);
  context.Init();

  TestDelegate test_delegate;
  std::unique_ptr<URLRequest> r(context.CreateRequest(
      GURL("ftp://example.test/"), RequestPriority::DEFAULT_PRIORITY,
      &test_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  r->Start();
  test_delegate.RunUntilComplete();

  histograms.ExpectBucketCount("Net.FTP.StartResult",
                               FTPStartResult::kSuccessNoAuth, 1);
  histograms.ExpectBucketCount("Net.FTP.StartResult",
                               FTPStartResult::kSuccessAuth, 0);
  histograms.ExpectBucketCount("Net.FTP.StartResult", FTPStartResult::kFailed,
                               0);
}

TEST_F(UrlRequestFtpJobTest, HistogramLogSuccessAuth) {
  base::HistogramTester histograms;
  MockURLRequestFtpJobFactory url_request_ftp_job_factory(
      ERR_FAILED, ERR_FAILED, true, {OK});
  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_job_factory(&url_request_ftp_job_factory);
  context.Init();

  TestDelegate test_delegate;
  test_delegate.set_credentials(
      AuthCredentials(base::ASCIIToUTF16("user"), base::ASCIIToUTF16("pass")));
  std::unique_ptr<URLRequest> r(context.CreateRequest(
      GURL("ftp://example.test/"), RequestPriority::DEFAULT_PRIORITY,
      &test_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  r->Start();
  test_delegate.RunUntilComplete();

  histograms.ExpectBucketCount("Net.FTP.StartResult",
                               FTPStartResult::kSuccessNoAuth, 0);
  histograms.ExpectBucketCount("Net.FTP.StartResult",
                               FTPStartResult::kSuccessAuth, 1);
  histograms.ExpectBucketCount("Net.FTP.StartResult", FTPStartResult::kFailed,
                               0);
}

TEST_F(UrlRequestFtpJobTest, HistogramLogFailed) {
  base::HistogramTester histograms;
  MockURLRequestFtpJobFactory url_request_ftp_job_factory(
      ERR_FAILED, ERR_FAILED, false, {ERR_FAILED});
  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_job_factory(&url_request_ftp_job_factory);
  context.Init();

  TestDelegate test_delegate;
  std::unique_ptr<URLRequest> r(context.CreateRequest(
      GURL("ftp://example.test/"), RequestPriority::DEFAULT_PRIORITY,
      &test_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  r->Start();
  test_delegate.RunUntilComplete();

  histograms.ExpectBucketCount("Net.FTP.StartResult",
                               FTPStartResult::kSuccessNoAuth, 0);
  histograms.ExpectBucketCount("Net.FTP.StartResult",
                               FTPStartResult::kSuccessAuth, 0);
  histograms.ExpectBucketCount("Net.FTP.StartResult", FTPStartResult::kFailed,
                               1);
}

TEST_F(UrlRequestFtpJobTest, HistogramLogFailedInvalidAuthThenSucceed) {
  base::HistogramTester histograms;
  MockURLRequestFtpJobFactory url_request_ftp_job_factory(
      ERR_FAILED, ERR_FAILED, true, {ERR_ACCESS_DENIED, OK});
  TestNetworkDelegate network_delegate;
  TestURLRequestContext context(true);
  context.set_network_delegate(&network_delegate);
  context.set_job_factory(&url_request_ftp_job_factory);
  context.Init();

  TestDelegate test_delegate;
  test_delegate.set_credentials(
      AuthCredentials(base::ASCIIToUTF16("user"), base::ASCIIToUTF16("pass")));
  std::unique_ptr<URLRequest> r(context.CreateRequest(
      GURL("ftp://example.test/"), RequestPriority::DEFAULT_PRIORITY,
      &test_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));

  r->Start();
  test_delegate.RunUntilComplete();

  histograms.ExpectBucketCount("Net.FTP.StartResult",
                               FTPStartResult::kSuccessNoAuth, 0);
  histograms.ExpectBucketCount("Net.FTP.StartResult",
                               FTPStartResult::kSuccessAuth, 1);
  histograms.ExpectBucketCount("Net.FTP.StartResult", FTPStartResult::kFailed,
                               1);
}
}  // namespace
}  // namespace net
