// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_test_util.h"

#include <utility>

#include "net/proxy_resolution/proxy_info.h"

using ::testing::_;

namespace net {
MockHttpStreamRequestDelegate::MockHttpStreamRequestDelegate() = default;

MockHttpStreamRequestDelegate::~MockHttpStreamRequestDelegate() = default;

MockHttpStreamFactoryJob::MockHttpStreamFactoryJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    ProxyInfo proxy_info,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    HostPortPair destination,
    GURL origin_url,
    NextProto alternative_protocol,
    quic::ParsedQuicVersion quic_version,
    const ProxyServer& alternative_proxy_server,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log)
    : HttpStreamFactory::Job(delegate,
                             job_type,
                             session,
                             request_info,
                             priority,
                             proxy_info,
                             server_ssl_config,
                             proxy_ssl_config,
                             destination,
                             origin_url,
                             alternative_protocol,
                             quic_version,
                             alternative_proxy_server,
                             is_websocket,
                             enable_ip_based_pooling,
                             net_log) {
  DCHECK(!is_waiting());
}

MockHttpStreamFactoryJob::~MockHttpStreamFactoryJob() = default;

TestJobFactory::TestJobFactory()
    : main_job_(nullptr),
      alternative_job_(nullptr),
      override_main_job_url_(false) {}

TestJobFactory::~TestJobFactory() = default;

std::unique_ptr<HttpStreamFactory::Job> TestJobFactory::CreateMainJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const ProxyInfo& proxy_info,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    HostPortPair destination,
    GURL origin_url,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log) {
  if (override_main_job_url_)
    origin_url = main_job_alternative_url_;

  auto main_job = std::make_unique<MockHttpStreamFactoryJob>(
      delegate, job_type, session, request_info, priority, proxy_info,
      SSLConfig(), SSLConfig(), destination, origin_url, kProtoUnknown,
      quic::UnsupportedQuicVersion(), ProxyServer(), is_websocket,
      enable_ip_based_pooling, net_log);

  // Keep raw pointer to Job but pass ownership.
  main_job_ = main_job.get();

  return std::move(main_job);
}

std::unique_ptr<HttpStreamFactory::Job> TestJobFactory::CreateAltSvcJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const ProxyInfo& proxy_info,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    HostPortPair destination,
    GURL origin_url,
    NextProto alternative_protocol,
    quic::ParsedQuicVersion quic_version,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log) {
  auto alternative_job = std::make_unique<MockHttpStreamFactoryJob>(
      delegate, job_type, session, request_info, priority, proxy_info,
      SSLConfig(), SSLConfig(), destination, origin_url, alternative_protocol,
      quic_version, ProxyServer(), is_websocket, enable_ip_based_pooling,
      net_log);

  // Keep raw pointer to Job but pass ownership.
  alternative_job_ = alternative_job.get();

  return std::move(alternative_job);
}

std::unique_ptr<HttpStreamFactory::Job> TestJobFactory::CreateAltProxyJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const ProxyInfo& proxy_info,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    HostPortPair destination,
    GURL origin_url,
    const ProxyServer& alternative_proxy_server,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log) {
  auto alternative_job = std::make_unique<MockHttpStreamFactoryJob>(
      delegate, job_type, session, request_info, priority, proxy_info,
      SSLConfig(), SSLConfig(), destination, origin_url, kProtoUnknown,
      quic::UnsupportedQuicVersion(), alternative_proxy_server, is_websocket,
      enable_ip_based_pooling, net_log);

  // Keep raw pointer to Job but pass ownership.
  alternative_job_ = alternative_job.get();

  return std::move(alternative_job);
}

}  // namespace net
