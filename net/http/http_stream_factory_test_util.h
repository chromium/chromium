// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_TEST_UTIL_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_factory_job.h"
#include "net/http/http_stream_factory_job_controller.h"
#include "net/http/http_stream_request.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/scheme_host_port.h"

using testing::_;
using testing::Invoke;

namespace net {

class HttpStreamFactoryPeer {
 public:
  static void AddJobController(
      HttpStreamFactory* factory,
      std::unique_ptr<HttpStreamFactory::JobController> job_controller) {
    factory->job_controller_set_.insert(std::move(job_controller));
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

  MockHttpStreamRequestDelegate(const MockHttpStreamRequestDelegate&) = delete;
  MockHttpStreamRequestDelegate& operator=(
      const MockHttpStreamRequestDelegate&) = delete;

  ~MockHttpStreamRequestDelegate() override;

  // std::unique_ptr is not copyable and therefore cannot be mocked.
  MOCK_METHOD2(OnStreamReadyImpl,
               void(const ProxyInfo& used_proxy_info, HttpStream* stream));

  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    OnStreamReadyImpl(used_proxy_info, stream.get());
  }

  // std::unique_ptr is not copyable and therefore cannot be mocked.
  void OnBidirectionalStreamImplReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override {}

  // std::unique_ptr is not copyable and therefore cannot be mocked.
  void OnWebSocketHandshakeStreamReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override {}

  MOCK_METHOD4(OnStreamFailed,
               void(int status,
                    const NetErrorDetails& net_error_details,
                    const ProxyInfo& used_proxy_info,
                    ResolveErrorInfo resolve_error_info));

  MOCK_METHOD2(OnCertificateError, void(int status, const SSLInfo& ssl_info));

  MOCK_METHOD3(OnNeedsProxyAuth,
               void(const HttpResponseInfo& proxy_response,
                    const ProxyInfo& used_proxy_info,
                    HttpAuthController* auth_controller));

  MOCK_METHOD1(OnNeedsClientAuth, void(SSLCertRequestInfo* cert_info));

  MOCK_METHOD0(OnQuicBroken, void());

  // `switching_info` is not copyable and therefore cannot be mocked.
  MOCK_METHOD1(OnSwitchesToHttpStreamPoolImpl,
               void(HttpStreamPoolSwitchingInfo& switching_info));

  void OnSwitchesToHttpStreamPool(
      HttpStreamPoolSwitchingInfo switching_info) override {
    OnSwitchesToHttpStreamPoolImpl(switching_info);
  }
};

class MockHttpStreamFactoryJob : public HttpStreamFactory::Job {
 public:
  MockHttpStreamFactoryJob(
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
      NetLog* net_log);

  ~MockHttpStreamFactoryJob() override;

  MOCK_METHOD0(Resume, void());

  MOCK_METHOD0(Orphan, void());

  void DoResume();
};

// JobFactory for creating MockHttpStreamFactoryJobs.
class TestJobFactory : public HttpStreamFactory::JobFactory {
 public:
  TestJobFactory();
  ~TestJobFactory() override;

  std::unique_ptr<HttpStreamFactory::Job> CreateJob(
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
      NextProto alternative_protocol,
      quic::ParsedQuicVersion quic_version) override;

  MockHttpStreamFactoryJob* main_job() const { return main_job_; }
  MockHttpStreamFactoryJob* alternative_job() const { return alternative_job_; }
  MockHttpStreamFactoryJob* dns_alpn_h3_job() const { return dns_alpn_h3_job_; }

 private:
  raw_ptr<MockHttpStreamFactoryJob, AcrossTasksDanglingUntriaged> main_job_ =
      nullptr;
  raw_ptr<MockHttpStreamFactoryJob, AcrossTasksDanglingUntriaged>
      alternative_job_ = nullptr;
  raw_ptr<MockHttpStreamFactoryJob, AcrossTasksDanglingUntriaged>
      dns_alpn_h3_job_ = nullptr;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_TEST_UTIL_H_
