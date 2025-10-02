// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_test_util.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/http_stream.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "url/scheme_host_port.h"

using ::testing::_;

namespace net {

MockHttpStreamRequestDelegate::MockHttpStreamRequestDelegate() = default;

MockHttpStreamRequestDelegate::~MockHttpStreamRequestDelegate() = default;

void MockHttpStreamRequestDelegate::OnStreamReady(
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<HttpStream> stream) {
  CHECK(!IsDone());

  used_proxy_info_ = used_proxy_info;
  http_stream_ = std::move(stream);
  done_run_loop_.Quit();
}

void MockHttpStreamRequestDelegate::OnWebSocketHandshakeStreamReady(
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<WebSocketHandshakeStreamBase> stream) {
  NOTREACHED();
}

void MockHttpStreamRequestDelegate::OnBidirectionalStreamImplReady(
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<BidirectionalStreamImpl> stream) {
  NOTREACHED();
}

void MockHttpStreamRequestDelegate::OnStreamFailed(
    int status,
    const NetErrorDetails& net_error_details,
    const ProxyInfo& used_proxy_info,
    ResolveErrorInfo resolve_error_info) {
  CHECK(!IsDone());

  net_error_ = status;
  used_proxy_info_ = used_proxy_info;
  done_run_loop_.Quit();
}

void MockHttpStreamRequestDelegate::OnCertificateError(
    int status,
    const SSLInfo& ssl_info) {
  NOTREACHED();
}

void MockHttpStreamRequestDelegate::OnNeedsProxyAuth(
    const HttpResponseInfo& proxy_response,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  NOTREACHED();
}

void MockHttpStreamRequestDelegate::OnNeedsClientAuth(
    SSLCertRequestInfo* cert_info) {
  NOTREACHED();
}

void MockHttpStreamRequestDelegate::OnQuicBroken() {
  CHECK(!IsDone());
}

std::unique_ptr<HttpStream> MockHttpStreamRequestDelegate::WaitForHttpStream() {
  done_run_loop_.Run();
  EXPECT_TRUE(http_stream_);
  return std::move(http_stream_);
}

int MockHttpStreamRequestDelegate::WaitForError() {
  done_run_loop_.Run();
  if (!net_error_) {
    ADD_FAILURE() << "StreamRequest unexpectedly succeeded";
    return ERR_UNEXPECTED;
  }
  return *net_error_;
}

const ProxyInfo& MockHttpStreamRequestDelegate::used_proxy_info() const {
  CHECK(used_proxy_info_);
  return *used_proxy_info_;
}

bool MockHttpStreamRequestDelegate::IsDone() {
  return done_run_loop_.AnyQuitCalled();
}

MockHttpStreamFactoryJob::MockHttpStreamFactoryJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpStreamFactory::StreamRequestInfo& request_info,
    RequestPriority priority,
    ProxyInfo proxy_info,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    url::SchemeHostPort destination,
    NextProto alternative_protocol,
    quic::ParsedQuicVersion quic_version,
    bool is_websocket,
    bool enable_ip_based_pooling_for_h2,
    std::optional<ConnectionManagementConfig> management_config,
    NetLog* net_log)
    : HttpStreamFactory::Job(delegate,
                             job_type,
                             session,
                             request_info,
                             priority,
                             proxy_info,
                             allowed_bad_certs,
                             std::move(destination),
                             alternative_protocol,
                             quic_version,
                             is_websocket,
                             enable_ip_based_pooling_for_h2,
                             management_config,
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
    bool is_websocket,
    bool enable_ip_based_pooling_for_h2,
    NetLog* net_log,
    NextProto alternative_protocol = NextProto::kProtoUnknown,
    quic::ParsedQuicVersion quic_version =
        quic::ParsedQuicVersion::Unsupported(),
    std::optional<ConnectionManagementConfig> management_config =
        std::nullopt) {
  if (use_real_jobs_) {
    return std::make_unique<HttpStreamFactory::Job>(
        delegate, job_type, session, request_info, priority, proxy_info,
        allowed_bad_certs, std::move(destination), alternative_protocol,
        quic_version, is_websocket, enable_ip_based_pooling_for_h2,
        std::move(management_config), net_log);
  }

  auto job = std::make_unique<MockHttpStreamFactoryJob>(
      delegate, job_type, session, request_info, priority, proxy_info,
      allowed_bad_certs, std::move(destination), alternative_protocol,
      quic_version, is_websocket, enable_ip_based_pooling_for_h2,
      management_config, net_log);

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
