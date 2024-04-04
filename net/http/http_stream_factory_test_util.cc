// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_test_util.h"

#include <utility>

#include "net/proxy_resolution/proxy_info.h"
#include "url/scheme_host_port.h"

using ::testing::_;

namespace net {
MockHttpStreamRequestDelegate::MockHttpStreamRequestDelegate() = default;

MockHttpStreamRequestDelegate::~MockHttpStreamRequestDelegate() = default;

MockHttpStreamFactoryJob::MockHttpStreamFactoryJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpStreamFactory::StreamRequestInfo& request_info,
    RequestPriority priority,
    ProxyInfo proxy_info,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    url::SchemeHostPort destination,
    GURL origin_url,
    NextProto alternative_protocol,
    quic::ParsedQuicVersion quic_version,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log)
    : HttpStreamFactory::Job(delegate,
                             job_type,
                             session,
                             request_info,
                             priority,
                             proxy_info,
                             allowed_bad_certs,
                             std::move(destination),
                             origin_url,
                             alternative_protocol,
                             quic_version,
                             is_websocket,
                             enable_ip_based_pooling,
                             net_log) {
  DCHECK(!is_waiting());
}

MockHttpStreamFactoryJob::~MockHttpStreamFactoryJob() = default;

void MockHttpStreamFactoryJob::DoResume() {
  HttpStreamFactory::Job::Resume();
}

TestJobFactory::TestJobFactory() = default;

TestJobFactory::~TestJobFactory() = default;

std::unique_ptr<HttpStreamFactory::Job> TestJobFactory::CreateJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpStreamFactory::StreamRequestInfo& request_info,
    RequestPriority priority,
    const ProxyInfo& proxy_info,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    url::SchemeHostPort destination,
    GURL origin_url,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log,
    NextProto alternative_protocol = kProtoUnknown,
    quic::ParsedQuicVersion quic_version =
        quic::ParsedQuicVersion::Unsupported()) {
  auto job = std::make_unique<MockHttpStreamFactoryJob>(
      delegate, job_type, session, request_info, priority, proxy_info,
      allowed_bad_certs, std::move(destination), origin_url,
      alternative_protocol, quic_version, is_websocket, enable_ip_based_pooling,
      net_log);

  // Keep raw pointer to Job but pass ownership.
  switch (job_type) {
    case HttpStreamFactory::MAIN:
      main_job_ = job.get();
      break;
    case HttpStreamFactory::ALTERNATIVE:
      alternative_job_ = job.get();
      break;
    case HttpStreamFactory::DNS_ALPN_H3:
      dns_alpn_h3_job_ = job.get();
      break;
    case HttpStreamFactory::PRECONNECT:
      main_job_ = job.get();
      break;
    case HttpStreamFactory::PRECONNECT_DNS_ALPN_H3:
      main_job_ = job.get();
      break;
  }
  return job;
}

}  // namespace net
