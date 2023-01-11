// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/ssl_certificate_error_job.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"

namespace net {

namespace {

const char kMockHostname[] = "mock.ssl.cert.error.request";

class MockJobInterceptor : public URLRequestInterceptor {
 public:
  MockJobInterceptor() = default;

  MockJobInterceptor(const MockJobInterceptor&) = delete;
  MockJobInterceptor& operator=(const MockJobInterceptor&) = delete;

  ~MockJobInterceptor() override = default;

  // URLRequestJobFactory::ProtocolHandler implementation:
  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const override {
    return std::make_unique<SSLCertificateErrorJob>(request);
  }
};

}  // namespace

SSLCertificateErrorJob::SSLCertificateErrorJob(URLRequest* request)
    : URLRequestJob(request) {}

SSLCertificateErrorJob::~SSLCertificateErrorJob() = default;

void SSLCertificateErrorJob::Start() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SSLCertificateErrorJob::NotifyError,
                                weak_factory_.GetWeakPtr()));
}

void SSLCertificateErrorJob::AddUrlHandler() {
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor("https", kMockHostname,
                                 std::make_unique<MockJobInterceptor>());
}

GURL SSLCertificateErrorJob::GetMockUrl() {
  return GURL(base::StringPrintf("https://%s", kMockHostname));
}

void SSLCertificateErrorJob::NotifyError() {
  SSLInfo info;
  info.cert_status = CERT_STATUS_DATE_INVALID;
  NotifySSLCertificateError(net::ERR_CERT_DATE_INVALID, info, true);
}

}  // namespace net
