// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/url_request_mock_data_job.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request_filter.h"

namespace net {
namespace {

const char kMockHostname[] = "mock.data";

// Gets the data from URL of the form:
// scheme://kMockHostname/?data=abc&repeat_count=nnn.
std::string GetDataFromRequest(const URLRequest& request) {
  std::string value;
  if (!GetValueForKeyInQuery(request.url(), "data", &value))
    return "default_data";
  return value;
}

// Gets the numeric repeat count from URL of the form:
// scheme://kMockHostname/?data=abc&repeat_count=nnn.
int GetRepeatCountFromRequest(const URLRequest& request) {
  std::string value;
  if (!GetValueForKeyInQuery(request.url(), "repeat", &value))
    return 1;

  int repeat_count;
  if (!base::StringToInt(value, &repeat_count))
    return 1;

  DCHECK_GT(repeat_count, 0);

  return repeat_count;
}

// Gets the requestcert flag from URL.
bool GetRequestClientCertificate(const URLRequest& request) {
  std::string ignored_value;
  return GetValueForKeyInQuery(request.url(), "requestcert", &ignored_value);
}

GURL GetMockUrl(const std::string& scheme,
                const std::string& hostname,
                const std::string& data,
                int data_repeat_count,
                bool request_client_certificate) {
  DCHECK_GT(data_repeat_count, 0);
  std::string url(scheme + "://" + hostname + "/");
  url.append("?data=");
  url.append(data);
  url.append("&repeat=");
  url.append(base::NumberToString(data_repeat_count));
  if (request_client_certificate)
    url += "&requestcert=1";
  return GURL(url);
}

class MockJobInterceptor : public URLRequestInterceptor {
 public:
  MockJobInterceptor() = default;
  ~MockJobInterceptor() override = default;

  // URLRequestInterceptor implementation
  URLRequestJob* MaybeInterceptRequest(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    return new URLRequestMockDataJob(request, network_delegate,
                                     GetDataFromRequest(*request),
                                     GetRepeatCountFromRequest(*request),
                                     GetRequestClientCertificate(*request));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockJobInterceptor);
};

}  // namespace

URLRequestMockDataJob::URLRequestMockDataJob(URLRequest* request,
                                             NetworkDelegate* network_delegate,
                                             const std::string& data,
                                             int data_repeat_count,
                                             bool request_client_certificate)
    : URLRequestJob(request, network_delegate),
      data_offset_(0),
      request_client_certificate_(request_client_certificate) {
  DCHECK_GT(data_repeat_count, 0);
  for (int i = 0; i < data_repeat_count; ++i) {
    data_.append(data);
  }
}

void URLRequestMockDataJob::OverrideResponseHeaders(
    const std::string& headers) {
  headers_ = headers;
}

void URLRequestMockDataJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestMockDataJob::StartAsync,
                                weak_factory_.GetWeakPtr()));
}

URLRequestMockDataJob::~URLRequestMockDataJob() = default;

int URLRequestMockDataJob::ReadRawData(IOBuffer* buf, int buf_size) {
  int bytes_read =
      std::min(static_cast<size_t>(buf_size), data_.length() - data_offset_);
  memcpy(buf->data(), data_.c_str() + data_offset_, bytes_read);
  data_offset_ += bytes_read;
  return bytes_read;
}

void URLRequestMockDataJob::ContinueWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key) {
  DCHECK(request_client_certificate_);
  NotifyHeadersComplete();
}

// Public virtual version.
void URLRequestMockDataJob::GetResponseInfo(HttpResponseInfo* info) {
  // Forward to private const version.
  GetResponseInfoConst(info);
}

// Private const version.
void URLRequestMockDataJob::GetResponseInfoConst(HttpResponseInfo* info) const {
  // Send back mock headers.
  std::string raw_headers;
  if (headers_.has_value()) {
    raw_headers = headers_.value();
  } else {
    raw_headers.append(
        "HTTP/1.1 200 OK\n"
        "Content-type: text/plain\n");
    raw_headers.append(base::StringPrintf("Content-Length: %1d\n",
                                          static_cast<int>(data_.length())));
  }
  info->headers = base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(raw_headers));
}

void URLRequestMockDataJob::StartAsync() {
  if (!request_)
    return;

  set_expected_content_size(data_.length());
  if (request_client_certificate_) {
    auto request_all = base::MakeRefCounted<SSLCertRequestInfo>();
    NotifyCertificateRequested(request_all.get());
    return;
  }
  NotifyHeadersComplete();
}

// static
void URLRequestMockDataJob::AddUrlHandler() {
  return AddUrlHandlerForHostname(kMockHostname);
}

// static
void URLRequestMockDataJob::AddUrlHandlerForHostname(
    const std::string& hostname) {
  // Add |hostname| to URLRequestFilter for HTTP and HTTPS.
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor("http", hostname,
                                 std::make_unique<MockJobInterceptor>());
  filter->AddHostnameInterceptor("https", hostname,
                                 std::make_unique<MockJobInterceptor>());
}

// static
GURL URLRequestMockDataJob::GetMockHttpUrl(const std::string& data,
                                           int repeat_count) {
  return GetMockHttpUrlForHostname(kMockHostname, data, repeat_count);
}

// static
GURL URLRequestMockDataJob::GetMockHttpsUrl(const std::string& data,
                                            int repeat_count) {
  return GetMockHttpsUrlForHostname(kMockHostname, data, repeat_count);
}

GURL URLRequestMockDataJob::GetMockUrlForClientCertificateRequest() {
  return GetMockUrl("https", kMockHostname, "data", 1, true);
}

// static
GURL URLRequestMockDataJob::GetMockHttpUrlForHostname(
    const std::string& hostname,
    const std::string& data,
    int repeat_count) {
  return GetMockUrl("http", hostname, data, repeat_count, false);
}

// static
GURL URLRequestMockDataJob::GetMockHttpsUrlForHostname(
    const std::string& hostname,
    const std::string& data,
    int repeat_count) {
  return GetMockUrl("https", hostname, data, repeat_count, false);
}

}  // namespace net
