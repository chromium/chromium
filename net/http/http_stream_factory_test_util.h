// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_TEST_UTIL_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_TEST_UTIL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
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

namespace net {

class HttpStream;
class WebSocketHandshakeStreamBase;
class BidirectionalStreamImpl;

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

// Single-use HttpStreamRequest::Delegate that can wait for completion and
// records the results. CHECKs if there are multiple calls to methods signalling
// completion.
class MockHttpStreamRequestDelegate : public HttpStreamRequest::Delegate {
 public:
  MockHttpStreamRequestDelegate();

  MockHttpStreamRequestDelegate(const MockHttpStreamRequestDelegate&) = delete;
  MockHttpStreamRequestDelegate& operator=(
      const MockHttpStreamRequestDelegate&) = delete;

  ~MockHttpStreamRequestDelegate() override;

  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override;
  void OnWebSocketHandshakeStreamReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override;
  void OnBidirectionalStreamImplReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override;
  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const ProxyInfo& used_proxy_info,
                      ResolveErrorInfo resolve_error_info) override;
  void OnCertificateError(int status, const SSLInfo& ssl_info) override;
  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override;
  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) override;
  void OnQuicBroken() override;

  // Waits until the request is complete. Returns the HttpStream if one was
  // received. Otherwise, returns nullptr and fails the current test.
  std::unique_ptr<HttpStream> WaitForHttpStream();

  // Waits until the request is complete. Returns the net Error if one was
  // received. Otherwise, returns ERR_UNEXPECTED and fails the current test.
  int WaitForError();

  // Returns the proxy info received by a method signalling completion. CHECKs
  // if no such method has been invoked yet.
  const ProxyInfo& used_proxy_info() const;

  // Returns true if `this` has already observed an event indicating completion
  // - that is, one of the *Ready() methods or OnStreamFailed() has been
  // invoked.
  bool IsDone();

 private:
  std::optional<ProxyInfo> used_proxy_info_;
  std::unique_ptr<HttpStream> http_stream_;
  std::optional<int> net_error_;
  base::RunLoop done_run_loop_;
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
      NextProto alternative_protocol,
      quic::ParsedQuicVersion quic_version,
      bool is_websocket,
      bool enable_ip_based_pooling_for_h2,
      std::optional<ConnectionManagementConfig> management_config,
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
      bool is_websocket,
      bool enable_ip_based_pooling_for_h2,
      NetLog* net_log,
      NextProto alternative_protocol,
      quic::ParsedQuicVersion quic_version,
      std::optional<ConnectionManagementConfig> management_config) override;

  MockHttpStreamFactoryJob* main_job() const { return main_job_; }
  MockHttpStreamFactoryJob* alternative_job() const { return alternative_job_; }
  MockHttpStreamFactoryJob* dns_alpn_h3_job() const { return dns_alpn_h3_job_; }

  void set_use_real_jobs() { use_real_jobs_ = true; }

 private:
  raw_ptr<MockHttpStreamFactoryJob, AcrossTasksDanglingUntriaged> main_job_ =
      nullptr;
  raw_ptr<MockHttpStreamFactoryJob, AcrossTasksDanglingUntriaged>
      alternative_job_ = nullptr;
  raw_ptr<MockHttpStreamFactoryJob, AcrossTasksDanglingUntriaged>
      dns_alpn_h3_job_ = nullptr;

  // When set to true, creates real jobs, and accessors don't work.
  bool use_real_jobs_ = false;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_TEST_UTIL_H_
