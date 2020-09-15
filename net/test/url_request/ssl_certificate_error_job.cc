// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/ssl_certificate_error_job.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
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
  ~MockJobInterceptor() override = default;

  // URLRequestJobFactory::ProtocolHandler implementation:
  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const override {
    return std::make_unique<SSLCertificateErrorJob>(request);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockJobInterceptor);
};

}  // namespace

SSLCertificateErrorJob::SSLCertificateErrorJob(URLRequest* request)
    : URLRequestJob(request) {}

SSLCertificateErrorJob::~SSLCertificateErrorJob() = default;

void SSLCertificateErrorJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SSLCertificateErrorJob::NotifyError,
                                weak_factory_.GetWeakPtr()));
}

void SSLCertificateErrorJob::AddUrlHandler() {
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "https", kMockHostname,
      std::unique_ptr<URLRequestInterceptor>(new MockJobInterceptor()));
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
