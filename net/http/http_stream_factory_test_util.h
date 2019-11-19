// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_TEST_UTIL_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_TEST_UTIL_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "net/base/proxy_server.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_factory_job.h"
#include "net/http/http_stream_factory_job_controller.h"
#include "net/http/http_stream_request.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/next_proto.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Invoke;

namespace net {

class HttpStreamFactoryPeer {
 public:
  static void AddJobController(
      HttpStreamFactory* factory,
      HttpStreamFactory::JobController* job_controller) {
    factory->job_controller_set_.insert(base::WrapUnique(job_controller));
  }

  static bool IsJobControllerDeleted(HttpStreamFactory* factory) {
    return factory->job_controller_set_.empty();
  }

  static HttpStreamFactory::JobFactory* GetDefaultJobFactory(
      HttpStreamFactory* factory) {
    return factory->job_factory_.get();
  }
};

// This delegate does nothing when called.
class MockHttpStreamRequestDelegate : public HttpStreamRequest::Delegate {
 public:
  MockHttpStreamRequestDelegate();

  ~MockHttpStreamRequestDelegate() override;

  // std::unique_ptr is not copyable and therefore cannot be mocked.
  MOCK_METHOD3(OnStreamReadyImpl,
               void(const SSLConfig& used_ssl_config,
                    const ProxyInfo& used_proxy_info,
                    HttpStream* stream));

  void OnStreamReady(const SSLConfig& used_ssl_config,
                     const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    OnStreamReadyImpl(used_ssl_config, used_proxy_info, stream.get());
  }

  // std::unique_ptr is not copyable and therefore cannot be mocked.
  void OnBidirectionalStreamImplReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override {}

  // std::unique_ptr is not copyable and therefore cannot be mocked.
  void OnWebSocketHandshakeStreamReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override {}

  MOCK_METHOD4(OnStreamFailed,
               void(int status,
                    const NetErrorDetails& net_error_details,
                    const SSLConfig& used_ssl_config,
                    const ProxyInfo& used_proxy_info));

  MOCK_METHOD3(OnCertificateError,
               void(int status,
                    const SSLConfig& used_ssl_config,
                    const SSLInfo& ssl_info));

  MOCK_METHOD4(OnNeedsProxyAuth,
               void(const HttpResponseInfo& proxy_response,
                    const SSLConfig& used_ssl_config,
                    const ProxyInfo& used_proxy_info,
                    HttpAuthController* auth_controller));

  MOCK_METHOD2(OnNeedsClientAuth,
               void(const SSLConfig& used_ssl_config,
                    SSLCertRequestInfo* cert_info));

  MOCK_METHOD0(OnQuicBroken, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHttpStreamRequestDelegate);
};

class MockHttpStreamFactoryJob : public HttpStreamFactory::Job {
 public:
  MockHttpStreamFactoryJob(HttpStreamFactory::Job::Delegate* delegate,
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
                           NetLog* net_log);

  ~MockHttpStreamFactoryJob() override;

  MOCK_METHOD0(Resume, void());

  MOCK_METHOD0(Orphan, void());
};

// JobFactory for creating MockHttpStreamFactoryJobs.
class TestJobFactory : public HttpStreamFactory::JobFactory {
 public:
  TestJobFactory();
  ~TestJobFactory() override;

  std::unique_ptr<HttpStreamFactory::Job> CreateMainJob(
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
      NetLog* net_log) override;

  std::unique_ptr<HttpStreamFactory::Job> CreateAltSvcJob(
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
      NetLog* net_log) override;

  std::unique_ptr<HttpStreamFactory::Job> CreateAltProxyJob(
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
      NetLog* net_log) override;

  MockHttpStreamFactoryJob* main_job() const { return main_job_; }
  MockHttpStreamFactoryJob* alternative_job() const { return alternative_job_; }

  void UseDifferentURLForMainJob(GURL url) {
    override_main_job_url_ = true;
    main_job_alternative_url_ = url;
  }

 private:
  MockHttpStreamFactoryJob* main_job_;
  MockHttpStreamFactoryJob* alternative_job_;
  bool override_main_job_url_;
  GURL main_job_alternative_url_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_TEST_UTIL_H_
