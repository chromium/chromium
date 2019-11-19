// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert_net/nss_ocsp.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_proc_nss.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

// Matches the caIssuers hostname from the generated certificate.
const char kAiaHost[] = "aia-test.invalid";
// Returning a single DER-encoded cert, so the mime-type must be
// application/pkix-cert per RFC 5280.
const char kAiaHeaders[] = "HTTP/1.1 200 OK\0"
                           "Content-type: application/pkix-cert\0"
                           "\0";

class AiaResponseHandler : public URLRequestInterceptor {
 public:
  AiaResponseHandler(const std::string& headers, const std::string& cert_data)
      : headers_(headers), cert_data_(cert_data), request_count_(0) {}
  ~AiaResponseHandler() override = default;

  // URLRequestInterceptor implementation:
  URLRequestJob* MaybeInterceptRequest(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    ++const_cast<AiaResponseHandler*>(this)->request_count_;

    return new URLRequestTestJob(request, network_delegate, headers_,
                                 cert_data_, true);
  }

  int request_count() const { return request_count_; }

 private:
  std::string headers_;
  std::string cert_data_;
  int request_count_;

  DISALLOW_COPY_AND_ASSIGN(AiaResponseHandler);
};

}  // namespace

class NssHttpTest : public TestWithTaskEnvironment {
 public:
  NssHttpTest()
      : context_(false),
        handler_(NULL),
        verify_proc_(new CertVerifyProcNSS),
        verifier_(new MultiThreadedCertVerifier(verify_proc_.get())) {}
  ~NssHttpTest() override = default;

  void SetUp() override {
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(
        GetTestCertsDirectory().AppendASCII("aia-intermediate.der"),
        &file_contents));
    ASSERT_FALSE(file_contents.empty());

    // Ownership of |handler| is transferred to the URLRequestFilter, but
    // hold onto the original pointer in order to access |request_count()|.
    std::unique_ptr<AiaResponseHandler> handler(
        new AiaResponseHandler(kAiaHeaders, file_contents));
    handler_ = handler.get();

    URLRequestFilter::GetInstance()->AddHostnameInterceptor("http", kAiaHost,
                                                            std::move(handler));

    SetURLRequestContextForNSSHttpIO(&context_);
  }

  void TearDown() override {
    SetURLRequestContextForNSSHttpIO(nullptr);

    if (handler_)
      URLRequestFilter::GetInstance()->RemoveHostnameHandler("http", kAiaHost);
  }

  CertVerifier* verifier() const {
    return verifier_.get();
  }

  int request_count() const {
    return handler_->request_count();
  }

 protected:
  const CertificateList empty_cert_list_;

 private:
  TestURLRequestContext context_;
  AiaResponseHandler* handler_;
  scoped_refptr<CertVerifyProc> verify_proc_;
  std::unique_ptr<CertVerifier> verifier_;
};

// Tests that when using NSS to verify certificates that a request to fetch
// missing intermediate certificates is made successfully.
TEST_F(NssHttpTest, TestAia) {
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "aia-cert.pem"));
  ASSERT_TRUE(test_cert.get());

  scoped_refptr<X509Certificate> test_root(
      ImportCertFromFile(GetTestCertsDirectory(), "aia-root.pem"));
  ASSERT_TRUE(test_root.get());

  ScopedTestRoot scoped_root(test_root.get());

  CertVerifyResult verify_result;
  TestCompletionCallback test_callback;
  std::unique_ptr<CertVerifier::Request> request;

  int flags = 0;
  int error = verifier()->Verify(
      CertVerifier::RequestParams(test_cert, "aia-host.invalid", flags,
                                  /*ocsp_response=*/std::string(),
                                  /*sct_list=*/std::string()),
      &verify_result, test_callback.callback(), &request, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));

  error = test_callback.WaitForResult();

  EXPECT_THAT(error, IsOk());

  // Ensure that NSS made an AIA request for the missing intermediate.
  EXPECT_LT(0, request_count());
}

}  // namespace net
