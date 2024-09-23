// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/test/url_request/url_request_failed_job.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"

namespace net {

namespace {

const char kMockHostname[] = "mock.failed.request";

// String names of failure phases matching FailurePhase enum.
const char* kFailurePhase[]{
    "start",      // START
    "readsync",   // READ_SYNC
    "readasync",  // READ_ASYNC
};

static_assert(std::size(kFailurePhase) ==
                  URLRequestFailedJob::FailurePhase::MAX_FAILURE_PHASE,
              "kFailurePhase must match FailurePhase enum");

class MockJobInterceptor : public URLRequestInterceptor {
 public:
  MockJobInterceptor() = default;

  MockJobInterceptor(const MockJobInterceptor&) = delete;
  MockJobInterceptor& operator=(const MockJobInterceptor&) = delete;

  ~MockJobInterceptor() override = default;

  // URLRequestJobFactory::ProtocolHandler implementation:
  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const override {
    int net_error = OK;
    URLRequestFailedJob::FailurePhase phase =
        URLRequestFailedJob::FailurePhase::MAX_FAILURE_PHASE;
    for (size_t i = 0; i < std::size(kFailurePhase); i++) {
      std::string phase_error_string;
      if (GetValueForKeyInQuery(request->url(), kFailurePhase[i],
                                &phase_error_string)) {
        if (base::StringToInt(phase_error_string, &net_error)) {
          phase = static_cast<URLRequestFailedJob::FailurePhase>(i);
          break;
        }
      }
    }
    return std::make_unique<URLRequestFailedJob>(request, phase, net_error);
  }
};

GURL GetMockUrl(const std::string& scheme,
                const std::string& hostname,
                URLRequestFailedJob::FailurePhase phase,
                int net_error) {
  CHECK_GE(phase, URLRequestFailedJob::FailurePhase::START);
  CHECK_LE(phase, URLRequestFailedJob::FailurePhase::READ_ASYNC);
  CHECK_LT(net_error, OK);
  return GURL(scheme + "://" + hostname + "/error?" + kFailurePhase[phase] +
              "=" + base::NumberToString(net_error));
}

}  // namespace

URLRequestFailedJob::URLRequestFailedJob(URLRequest* request,
                                         FailurePhase phase,
                                         int net_error)
    : URLRequestJob(request), phase_(phase), net_error_(net_error) {
  CHECK_GE(phase, URLRequestFailedJob::FailurePhase::START);
  CHECK_LE(phase, URLRequestFailedJob::FailurePhase::READ_ASYNC);
  CHECK_LT(net_error, OK);
}

URLRequestFailedJob::URLRequestFailedJob(URLRequest* request, int net_error)
    : URLRequestFailedJob(request, START, net_error) {}

URLRequestFailedJob::~URLRequestFailedJob() = default;

void URLRequestFailedJob::Start() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestFailedJob::StartAsync,
                                weak_factory_.GetWeakPtr()));
}

int URLRequestFailedJob::ReadRawData(IOBuffer* buf, int buf_size) {
  CHECK(phase_ == READ_SYNC || phase_ == READ_ASYNC);
  if (net_error_ == ERR_IO_PENDING || phase_ == READ_SYNC)
    return net_error_;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestFailedJob::ReadRawDataComplete,
                                weak_factory_.GetWeakPtr(), net_error_));
  return ERR_IO_PENDING;
}

void URLRequestFailedJob::GetResponseInfo(HttpResponseInfo* info) {
  *info = response_info_;
}

void URLRequestFailedJob::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (net_error_ == ERR_QUIC_PROTOCOL_ERROR) {
    details->quic_connection_error = quic::QUIC_INTERNAL_ERROR;
  } else if (net_error_ == ERR_NETWORK_CHANGED) {
    details->quic_connection_error =
        quic::QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK;
  }
}

int64_t URLRequestFailedJob::GetTotalReceivedBytes() const {
  return total_received_bytes_;
}

// static
void URLRequestFailedJob::AddUrlHandler() {
  return AddUrlHandlerForHostname(kMockHostname);
}

// static
void URLRequestFailedJob::AddUrlHandlerForHostname(
    const std::string& hostname) {
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  // Add |hostname| to URLRequestFilter for HTTP and HTTPS.
  filter->AddHostnameInterceptor("http", hostname,
                                 std::make_unique<MockJobInterceptor>());
  filter->AddHostnameInterceptor("https", hostname,
                                 std::make_unique<MockJobInterceptor>());
}

// static
GURL URLRequestFailedJob::GetMockHttpUrl(int net_error) {
  return GetMockHttpUrlForHostname(net_error, kMockHostname);
}

// static
GURL URLRequestFailedJob::GetMockHttpsUrl(int net_error) {
  return GetMockHttpsUrlForHostname(net_error, kMockHostname);
}

// static
GURL URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(FailurePhase phase,
                                                         int net_error) {
  return GetMockUrl("http", kMockHostname, phase, net_error);
}

// static
GURL URLRequestFailedJob::GetMockHttpUrlForHostname(
    int net_error,
    const std::string& hostname) {
  return GetMockUrl("http", hostname, START, net_error);
}

// static
GURL URLRequestFailedJob::GetMockHttpsUrlForHostname(
    int net_error,
    const std::string& hostname) {
  return GetMockUrl("https", hostname, START, net_error);
}

void URLRequestFailedJob::StartAsync() {
  if (phase_ == START) {
    if (net_error_ != ERR_IO_PENDING) {
      NotifyStartError(net_error_);
      return;
    }
    return;
  }
  const std::string headers = "HTTP/1.1 200 OK";
  response_info_.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(headers);
  total_received_bytes_ = headers.size();
  NotifyHeadersComplete();
}

}  // namespace net
