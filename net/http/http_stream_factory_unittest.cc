// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_stream_factory.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/port_util.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/schemeful_site.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_proxy_delegate.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_session_pool_peer.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/quic_test_packet_printer.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
// This file can be included from net/http even though
// it is in net/websockets because it doesn't
// introduce any link dependency to net/websockets.
#include "net/websockets/websocket_handshake_stream_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::SizeIs;

using net::test::IsError;
using net::test::IsOk;

namespace base {
class Value;
}  // namespace base

namespace net {
class BidirectionalStreamImpl;
class WebSocketEndpointLockManager;
}  // namespace net

namespace net::test {

namespace {

class MockWebSocketHandshakeStream : public WebSocketHandshakeStreamBase {
 public:
  enum StreamType {
    kStreamTypeBasic,
    kStreamTypeSpdy,
  };

  explicit MockWebSocketHandshakeStream(StreamType type) : type_(type) {}

  ~MockWebSocketHandshakeStream() override = default;

  StreamType type() const { return type_; }

  // HttpStream methods
  void RegisterRequest(const HttpRequestInfo* request_info) override {}
  int InitializeStream(bool can_send_early,
                       RequestPriority priority,
                       const NetLogWithSource& net_log,
                       CompletionOnceCallback callback) override {
    return ERR_IO_PENDING;
  }
  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  CompletionOnceCallback callback) override {
    return ERR_IO_PENDING;
  }
  int ReadResponseHeaders(CompletionOnceCallback callback) override {
    return ERR_IO_PENDING;
  }
  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback) override {
    return ERR_IO_PENDING;
  }
  void Close(bool not_reusable) override {}
  bool IsResponseBodyComplete() const override { return false; }
  bool IsConnectionReused() const override { return false; }
  void SetConnectionReused() override {}
  bool CanReuseConnection() const override { return false; }
  int64_t GetTotalReceivedBytes() const override { return 0; }
  int64_t GetTotalSentBytes() const override { return 0; }
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override {
    return false;
  }
  bool GetAlternativeService(
      AlternativeService* alternative_service) const override {
    return false;
  }
  void GetSSLInfo(SSLInfo* ssl_info) override {}
  int GetRemoteEndpoint(IPEndPoint* endpoint) override {
    return ERR_UNEXPECTED;
  }
  void Drain(HttpNetworkSession* session) override {}
  void PopulateNetErrorDetails(NetErrorDetails* details) override { return; }
  void SetPriority(RequestPriority priority) override {}
  std::unique_ptr<HttpStream> RenewStreamForAuth() override { return nullptr; }
  const std::set<std::string>& GetDnsAliases() const override {
    static const base::NoDestructor<std::set<std::string>> nullset_result;
    return *nullset_result;
  }
  std::string_view GetAcceptChViaAlps() const override { return {}; }

  std::unique_ptr<WebSocketStream> Upgrade() override { return nullptr; }

  bool CanReadFromStream() const override { return true; }

  base::WeakPtr<WebSocketHandshakeStreamBase> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const StreamType type_;
  base::WeakPtrFactory<MockWebSocketHandshakeStream> weak_ptr_factory_{this};
};

// HttpStreamFactory subclass that can wait until a preconnect is complete.
class MockHttpStreamFactoryForPreconnect : public HttpStreamFactory {
 public:
  explicit MockHttpStreamFactoryForPreconnect(HttpNetworkSession* session)
      : HttpStreamFactory(session) {}
  ~MockHttpStreamFactoryForPreconnect() override = default;

  void WaitForPreconnects() {
    while (!preconnect_done_) {
      waiting_for_preconnect_ = true;
      loop_.Run();
      waiting_for_preconnect_ = false;
    }
  }

 private:
  // HttpStreamFactory methods.
  void OnPreconnectsCompleteInternal() override {
    preconnect_done_ = true;
    if (waiting_for_preconnect_) {
      loop_.QuitWhenIdle();
    }
  }

  bool preconnect_done_ = false;
  bool waiting_for_preconnect_ = false;
  base::RunLoop loop_;
};

class StreamRequester : public HttpStreamRequest::Delegate {
 public:
  explicit StreamRequester(HttpNetworkSession* session) : session_(session) {}

  StreamRequester(const StreamRequester&) = delete;
  StreamRequester& operator=(const StreamRequester&) = delete;

  void RequestStream(
      HttpStreamFactory* factory,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      bool enable_ip_based_pooling,
      bool enable_alternative_services) {
    CHECK(!request_);

    priority_ = priority;
    allowed_bad_certs_ = allowed_bad_certs;
    enable_ip_based_pooling_ = enable_ip_based_pooling;
    enable_alternative_services_ = enable_alternative_services;

    request_ =
        factory->RequestStream(request_info, priority, allowed_bad_certs, this,
                               enable_ip_based_pooling,
                               enable_alternative_services, NetLogWithSource());
  }

  void RequestStreamAndWait(
      HttpStreamFactory* factory,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      bool enable_ip_based_pooling,
      bool enable_alternative_services) {
    RequestStream(factory, request_info, priority, allowed_bad_certs,
                  enable_ip_based_pooling, enable_alternative_services);
    WaitForStream();
  }

  void RequestWebSocketHandshakeStream(
      HttpStreamFactory* factory,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      WebSocketHandshakeStreamBase::CreateHelper*
          websocket_handshake_stream_create_helper,
      bool enable_ip_based_pooling,
      bool enable_alternative_services) {
    CHECK(!request_);
    request_ = factory->RequestWebSocketHandshakeStream(
        request_info, priority, allowed_bad_certs, this,
        websocket_handshake_stream_create_helper, enable_ip_based_pooling,
        enable_alternative_services, NetLogWithSource());
  }

  void RequestBidirectionalStreamImpl(
      HttpStreamFactory* factory,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      bool enable_ip_based_pooling,
      bool enable_alternative_services) {
    CHECK(!request_);
    request_ = factory->RequestBidirectionalStreamImpl(
        request_info, priority, allowed_bad_certs, this,
        enable_ip_based_pooling, enable_alternative_services,
        NetLogWithSource());
  }

  // HttpStreamRequest::Delegate

  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    stream_done_ = true;
    if (loop_) {
      loop_->Quit();
    }
    stream_ = std::move(stream);
    used_proxy_info_ = used_proxy_info;
  }

  void OnWebSocketHandshakeStreamReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override {
    stream_done_ = true;
    if (loop_) {
      loop_->Quit();
    }
    websocket_stream_ = std::move(stream);
    used_proxy_info_ = used_proxy_info;
  }

  void OnBidirectionalStreamImplReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override {
    stream_done_ = true;
    if (loop_) {
      loop_->Quit();
    }
    bidirectional_stream_impl_ = std::move(stream);
    used_proxy_info_ = used_proxy_info;
  }

  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const ProxyInfo& used_proxy_info,
                      ResolveErrorInfo resolve_error_info) override {
    stream_done_ = true;
    if (loop_) {
      loop_->Quit();
    }
    error_status_ = status;
  }

  void OnCertificateError(int status, const SSLInfo& ssl_info) override {}

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {}

  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) override {}

  void OnQuicBroken() override {}

  void OnSwitchesToHttpStreamPool(
      HttpStreamPoolSwitchingInfo switching_info) override {
    CHECK(base::FeatureList::IsEnabled(features::kHappyEyeballsV3));
    CHECK(request_);

    request_ = session_->http_stream_pool()->RequestStream(
        this, std::move(switching_info), priority_, allowed_bad_certs_,
        enable_ip_based_pooling_, enable_alternative_services_,
        NetLogWithSource());

    if (http_stream_pool_switch_wait_closure_) {
      std::move(http_stream_pool_switch_wait_closure_).Run();
    }
  }

  void WaitForStream() {
    stream_done_ = false;
    loop_ = std::make_unique<base::RunLoop>();
    while (!stream_done_) {
      loop_->Run();
    }
    loop_.reset();
  }

  void MaybeWaitForSwitchesToHttpStreamPool() {
    if (!base::FeatureList::IsEnabled(features::kHappyEyeballsV3) ||
        switched_to_http_stream_pool_) {
      return;
    }

    CHECK(http_stream_pool_switch_wait_closure_.is_null());
    base::RunLoop run_loop;
    http_stream_pool_switch_wait_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const ProxyInfo& used_proxy_info() const { return used_proxy_info_; }

  HttpStreamRequest* request() const { return request_.get(); }

  HttpStream* stream() { return stream_.get(); }

  MockWebSocketHandshakeStream* websocket_stream() {
    return static_cast<MockWebSocketHandshakeStream*>(websocket_stream_.get());
  }

  BidirectionalStreamImpl* bidirectional_stream_impl() {
    return bidirectional_stream_impl_.get();
  }

  bool stream_done() const { return stream_done_; }
  int error_status() const { return error_status_; }

 protected:
  const raw_ptr<HttpNetworkSession> session_;

  bool switched_to_http_stream_pool_ = false;
  base::OnceClosure http_stream_pool_switch_wait_closure_;
  RequestPriority priority_ = DEFAULT_PRIORITY;
  std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;
  bool enable_ip_based_pooling_ = true;
  bool enable_alternative_services_ = true;

  bool stream_done_ = false;
  std::unique_ptr<base::RunLoop> loop_;
  std::unique_ptr<HttpStreamRequest> request_;
  std::unique_ptr<HttpStream> stream_;
  std::unique_ptr<WebSocketHandshakeStreamBase> websocket_stream_;
  std::unique_ptr<BidirectionalStreamImpl> bidirectional_stream_impl_;
  ProxyInfo used_proxy_info_;
  int error_status_ = OK;
};

class WebSocketBasicHandshakeStream : public MockWebSocketHandshakeStream {
 public:
  explicit WebSocketBasicHandshakeStream(
      std::unique_ptr<ClientSocketHandle> connection)
      : MockWebSocketHandshakeStream(kStreamTypeBasic),
        connection_(std::move(connection)) {}

  ~WebSocketBasicHandshakeStream() override {
    connection_->socket()->Disconnect();
  }

  ClientSocketHandle* connection() { return connection_.get(); }

 private:
  std::unique_ptr<ClientSocketHandle> connection_;
};

class WebSocketStreamCreateHelper
    : public WebSocketHandshakeStreamBase::CreateHelper {
 public:
  ~WebSocketStreamCreateHelper() override = default;

  std::unique_ptr<WebSocketHandshakeStreamBase> CreateBasicStream(
      std::unique_ptr<ClientSocketHandle> connection,
      bool using_proxy,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager) override {
    return std::make_unique<WebSocketBasicHandshakeStream>(
        std::move(connection));
  }
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp2Stream(
      base::WeakPtr<SpdySession> session,
      std::set<std::string> dns_aliases) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp3Stream(
      std::unique_ptr<QuicChromiumClientSession::Handle> session,
      std::set<std::string> dns_aliases) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
};

struct TestCase {
  int num_streams;
  bool ssl;
};

TestCase kTests[] = {
    {1, false},
    {2, false},
    {1, true},
    {2, true},
};

void PreconnectHelperForURL(int num_streams,
                            const GURL& url,
                            NetworkAnonymizationKey network_anonymization_key,
                            SecureDnsPolicy secure_dns_policy,
                            HttpNetworkSession* session) {
  HttpNetworkSessionPeer peer(session);
  auto mock_factory =
      std::make_unique<MockHttpStreamFactoryForPreconnect>(session);
  auto* mock_factory_ptr = mock_factory.get();
  peer.SetHttpStreamFactory(std::move(mock_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = url;
  request.load_flags = 0;
  request.network_anonymization_key = network_anonymization_key;
  request.secure_dns_policy = secure_dns_policy;
  request.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  session->http_stream_factory()->PreconnectStreams(num_streams, request);
  mock_factory_ptr->WaitForPreconnects();
}

void PreconnectHelper(const TestCase& test, HttpNetworkSession* session) {
  GURL url =
      test.ssl ? GURL("https://www.google.com") : GURL("http://www.google.com");
  PreconnectHelperForURL(test.num_streams, url, NetworkAnonymizationKey(),
                         SecureDnsPolicy::kAllow, session);
}

ClientSocketPool::GroupId GetGroupId(const TestCase& test) {
  if (test.ssl) {
    return ClientSocketPool::GroupId(
        url::SchemeHostPort(url::kHttpsScheme, "www.google.com", 443),
        PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
        SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  }
  return ClientSocketPool::GroupId(
      url::SchemeHostPort(url::kHttpScheme, "www.google.com", 80),
      PrivacyMode::PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
}

HttpStreamKey GetHttpStreamKey(const TestCase& test) {
  return GroupIdToHttpStreamKey(GetGroupId(test));
}

class CapturePreconnectsTransportSocketPool : public TransportClientSocketPool {
 public:
  explicit CapturePreconnectsTransportSocketPool(
      const CommonConnectJobParams* common_connect_job_params)
      : TransportClientSocketPool(/*max_sockets=*/0,
                                  /*max_sockets_per_group=*/0,
                                  base::TimeDelta(),
                                  ProxyChain::Direct(),
                                  /*is_for_websockets=*/false,
                                  common_connect_job_params) {}

  int last_num_streams() const { return last_num_streams_; }
  const ClientSocketPool::GroupId& last_group_id() const {
    return last_group_id_;
  }

  // Resets |last_num_streams_| and |last_group_id_| default values.
  void reset() {
    last_num_streams_ = -1;
    // Group ID that shouldn't match much.
    last_group_id_ = ClientSocketPool::GroupId(
        url::SchemeHostPort(url::kHttpsScheme,
                            "unexpected.to.conflict.with.anything.test", 9999),
        PrivacyMode::PRIVACY_MODE_ENABLED, NetworkAnonymizationKey(),
        SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false);
  }

  int RequestSocket(
      const ClientSocketPool::GroupId& group_id,
      scoped_refptr<ClientSocketPool::SocketParams> socket_params,
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      RequestPriority priority,
      const SocketTag& socket_tag,
      ClientSocketPool::RespectLimits respect_limits,
      ClientSocketHandle* handle,
      CompletionOnceCallback callback,
      const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback,
      const NetLogWithSource& net_log) override {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  int RequestSockets(
      const ClientSocketPool::GroupId& group_id,
      scoped_refptr<ClientSocketPool::SocketParams> socket_params,
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      int num_sockets,
      CompletionOnceCallback callback,
      const NetLogWithSource& net_log) override {
    last_num_streams_ = num_sockets;
    last_group_id_ = group_id;
    return OK;
  }

  void CancelRequest(const ClientSocketPool::GroupId& group_id,
                     ClientSocketHandle* handle,
                     bool cancel_connect_job) override {
    ADD_FAILURE();
  }
  void ReleaseSocket(const ClientSocketPool::GroupId& group_id,
                     std::unique_ptr<StreamSocket> socket,
                     int64_t generation) override {
    ADD_FAILURE();
  }
  void CloseIdleSockets(const char* net_log_reason_utf8) override {
    ADD_FAILURE();
  }
  int IdleSocketCount() const override {
    ADD_FAILURE();
    return 0;
  }
  size_t IdleSocketCountInGroup(
      const ClientSocketPool::GroupId& group_id) const override {
    ADD_FAILURE();
    return 0;
  }
  LoadState GetLoadState(const ClientSocketPool::GroupId& group_id,
                         const ClientSocketHandle* handle) const override {
    ADD_FAILURE();
    return LOAD_STATE_IDLE;
  }

 private:
  int last_num_streams_ = -1;
  ClientSocketPool::GroupId last_group_id_;
};

class CapturePreconnectHttpStreamPoolDelegate
    : public HttpStreamPool::TestDelegate {
 public:
  CapturePreconnectHttpStreamPoolDelegate() = default;

  CapturePreconnectHttpStreamPoolDelegate(
      const CapturePreconnectHttpStreamPoolDelegate&) = delete;
  CapturePreconnectHttpStreamPoolDelegate& operator=(
      const CapturePreconnectHttpStreamPoolDelegate&) = delete;

  ~CapturePreconnectHttpStreamPoolDelegate() override = default;

  void OnRequestStream(const HttpStreamKey& stream_key) override {}

  std::optional<int> OnPreconnect(const HttpStreamKey& stream_key,
                                  size_t num_streams) override {
    last_stream_key_ = stream_key;
    last_num_streams_ = num_streams;
    return OK;
  }

  const HttpStreamKey& last_stream_key() const { return last_stream_key_; }

  int last_num_streams() const { return last_num_streams_; }

 private:
  HttpStreamKey last_stream_key_;
  int last_num_streams_ = -1;
};

class HttpStreamFactoryTest : public TestWithTaskEnvironment,
                              public ::testing::WithParamInterface<bool> {
 public:
  HttpStreamFactoryTest() {
    if (HappyEyeballsV3Enabled()) {
      feature_list_.InitAndEnableFeature(features::kHappyEyeballsV3);
    } else {
      feature_list_.InitAndDisableFeature(features::kHappyEyeballsV3);
    }
  }

  bool HappyEyeballsV3Enabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HttpStreamFactoryTest,
                         testing::Values(true, false));

TEST_P(HttpStreamFactoryTest, PreconnectDirect) {
  for (const auto& test : kTests) {
    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateDirect());
    session_deps.http_user_agent_settings =
        std::make_unique<StaticHttpUserAgentSettings>("*", "test-ua");
    std::unique_ptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));

    if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
      auto delegate =
          std::make_unique<CapturePreconnectHttpStreamPoolDelegate>();
      CapturePreconnectHttpStreamPoolDelegate* delegate_ptr = delegate.get();
      session->http_stream_pool()->SetDelegateForTesting(std::move(delegate));
      PreconnectHelper(test, session.get());
      EXPECT_EQ(test.num_streams, delegate_ptr->last_num_streams());
      EXPECT_EQ(GetHttpStreamKey(test), delegate_ptr->last_stream_key());
    } else {
      HttpNetworkSessionPeer peer(session.get());
      CommonConnectJobParams common_connect_job_params =
          session->CreateCommonConnectJobParams();
      std::unique_ptr<CapturePreconnectsTransportSocketPool>
          owned_transport_conn_pool =
              std::make_unique<CapturePreconnectsTransportSocketPool>(
                  &common_connect_job_params);
      CapturePreconnectsTransportSocketPool* transport_conn_pool =
          owned_transport_conn_pool.get();
      auto mock_pool_manager = std::make_unique<MockClientSocketPoolManager>();
      mock_pool_manager->SetSocketPool(ProxyChain::Direct(),
                                       std::move(owned_transport_conn_pool));
      peer.SetClientSocketPoolManager(std::move(mock_pool_manager));
      PreconnectHelper(test, session.get());
      EXPECT_EQ(test.num_streams, transport_conn_pool->last_num_streams());
      EXPECT_EQ(GetGroupId(test), transport_conn_pool->last_group_id());
    }
  }
}

TEST_P(HttpStreamFactoryTest, PreconnectHttpProxy) {
  for (const auto& test : kTests) {
    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateFixedForTest(
            "http_proxy", TRAFFIC_ANNOTATION_FOR_TESTS));
    session_deps.http_user_agent_settings =
        std::make_unique<StaticHttpUserAgentSettings>("*", "test-ua");
    std::unique_ptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session.get());
    ProxyChain proxy_chain(ProxyServer::SCHEME_HTTP,
                           HostPortPair("http_proxy", 80));
    CommonConnectJobParams common_connect_job_params =
        session->CreateCommonConnectJobParams();

    auto http_proxy_pool =
        std::make_unique<CapturePreconnectsTransportSocketPool>(
            &common_connect_job_params);
    auto* http_proxy_pool_ptr = http_proxy_pool.get();
    auto mock_pool_manager = std::make_unique<MockClientSocketPoolManager>();
    mock_pool_manager->SetSocketPool(proxy_chain, std::move(http_proxy_pool));
    peer.SetClientSocketPoolManager(std::move(mock_pool_manager));
    PreconnectHelper(test, session.get());
    EXPECT_EQ(test.num_streams, http_proxy_pool_ptr->last_num_streams());
    EXPECT_EQ(GetGroupId(test), http_proxy_pool_ptr->last_group_id());
  }
}

TEST_P(HttpStreamFactoryTest, PreconnectSocksProxy) {
  for (const auto& test : kTests) {
    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateFixedForTest(
            "socks4://socks_proxy:1080", TRAFFIC_ANNOTATION_FOR_TESTS));
    session_deps.http_user_agent_settings =
        std::make_unique<StaticHttpUserAgentSettings>("*", "test-ua");
    std::unique_ptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session.get());
    ProxyChain proxy_chain(ProxyServer::SCHEME_SOCKS4,
                           HostPortPair("socks_proxy", 1080));
    CommonConnectJobParams common_connect_job_params =
        session->CreateCommonConnectJobParams();
    auto socks_proxy_pool =
        std::make_unique<CapturePreconnectsTransportSocketPool>(
            &common_connect_job_params);
    auto* socks_proxy_pool_ptr = socks_proxy_pool.get();
    auto mock_pool_manager = std::make_unique<MockClientSocketPoolManager>();
    mock_pool_manager->SetSocketPool(proxy_chain, std::move(socks_proxy_pool));
    peer.SetClientSocketPoolManager(std::move(mock_pool_manager));
    PreconnectHelper(test, session.get());
    EXPECT_EQ(test.num_streams, socks_proxy_pool_ptr->last_num_streams());
    EXPECT_EQ(GetGroupId(test), socks_proxy_pool_ptr->last_group_id());
  }
}

TEST_P(HttpStreamFactoryTest, PreconnectDirectWithExistingSpdySession) {
  for (const auto& test : kTests) {
    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateDirect());
    session_deps.http_user_agent_settings =
        std::make_unique<StaticHttpUserAgentSettings>("*", "test-ua");
    std::unique_ptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session.get());

    // Put a SpdySession in the pool.
    HostPortPair host_port_pair("www.google.com", 443);
    SpdySessionKey key(host_port_pair, PRIVACY_MODE_DISABLED,
                       ProxyChain::Direct(), SessionUsage::kDestination,
                       SocketTag(), NetworkAnonymizationKey(),
                       SecureDnsPolicy::kAllow,
                       /*disable_cert_verification_network_fetches=*/false);
    std::ignore = CreateFakeSpdySession(session->spdy_session_pool(), key);

    if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
      auto delegate =
          std::make_unique<CapturePreconnectHttpStreamPoolDelegate>();
      CapturePreconnectHttpStreamPoolDelegate* delegate_ptr = delegate.get();
      session->http_stream_pool()->SetDelegateForTesting(std::move(delegate));
      PreconnectHelper(test, session.get());
      if (test.ssl) {
        EXPECT_EQ(-1, delegate_ptr->last_num_streams());
      } else {
        EXPECT_EQ(test.num_streams, delegate_ptr->last_num_streams());
      }
    } else {
      CommonConnectJobParams common_connect_job_params =
          session->CreateCommonConnectJobParams();
      std::unique_ptr<CapturePreconnectsTransportSocketPool>
          owned_transport_conn_pool =
              std::make_unique<CapturePreconnectsTransportSocketPool>(
                  &common_connect_job_params);
      CapturePreconnectsTransportSocketPool* transport_conn_pool =
          owned_transport_conn_pool.get();
      auto mock_pool_manager = std::make_unique<MockClientSocketPoolManager>();
      mock_pool_manager->SetSocketPool(ProxyChain::Direct(),
                                       std::move(owned_transport_conn_pool));
      peer.SetClientSocketPoolManager(std::move(mock_pool_manager));
      PreconnectHelper(test, session.get());
      // We shouldn't be preconnecting if we have an existing session, which is
      // the case for https://www.google.com.
      if (test.ssl) {
        EXPECT_EQ(-1, transport_conn_pool->last_num_streams());
      } else {
        EXPECT_EQ(test.num_streams, transport_conn_pool->last_num_streams());
      }
    }
  }
}

// Verify that preconnects to unsafe ports are cancelled before they reach
// the SocketPool.
TEST_P(HttpStreamFactoryTest, PreconnectUnsafePort) {
  ASSERT_FALSE(IsPortAllowedForScheme(7, "http"));

  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  auto DoPreconnect = [&] {
    PreconnectHelperForURL(1, GURL("http://www.google.com:7"),
                           NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                           session.get());
  };

  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
    auto delegate = std::make_unique<CapturePreconnectHttpStreamPoolDelegate>();
    CapturePreconnectHttpStreamPoolDelegate* delegate_ptr = delegate.get();
    session->http_stream_pool()->SetDelegateForTesting(std::move(delegate));
    DoPreconnect();
    EXPECT_EQ(-1, delegate_ptr->last_num_streams());
  } else {
    HttpNetworkSessionPeer peer(session.get());
    CommonConnectJobParams common_connect_job_params =
        session->CreateCommonConnectJobParams();
    std::unique_ptr<CapturePreconnectsTransportSocketPool>
        owned_transport_conn_pool =
            std::make_unique<CapturePreconnectsTransportSocketPool>(
                &common_connect_job_params);
    CapturePreconnectsTransportSocketPool* transport_conn_pool =
        owned_transport_conn_pool.get();
    auto mock_pool_manager = std::make_unique<MockClientSocketPoolManager>();
    mock_pool_manager->SetSocketPool(ProxyChain::Direct(),
                                     std::move(owned_transport_conn_pool));
    peer.SetClientSocketPoolManager(std::move(mock_pool_manager));

    DoPreconnect();
    EXPECT_EQ(-1, transport_conn_pool->last_num_streams());
  }
}

// Verify that preconnects to invalid GURLs do nothing, and do not CHECK.
TEST_P(HttpStreamFactoryTest, PreconnectInvalidUrls) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  auto DoPreconnect = [&] {
    PreconnectHelperForURL(1, GURL(), NetworkAnonymizationKey(),
                           SecureDnsPolicy::kAllow, session.get());
  };

  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
    auto delegate = std::make_unique<CapturePreconnectHttpStreamPoolDelegate>();
    CapturePreconnectHttpStreamPoolDelegate* delegate_ptr = delegate.get();
    session->http_stream_pool()->SetDelegateForTesting(std::move(delegate));
    DoPreconnect();
    EXPECT_EQ(-1, delegate_ptr->last_num_streams());
  } else {
    HttpNetworkSessionPeer peer(session.get());
    CommonConnectJobParams common_connect_job_params =
        session->CreateCommonConnectJobParams();
    std::unique_ptr<CapturePreconnectsTransportSocketPool>
        owned_transport_conn_pool =
            std::make_unique<CapturePreconnectsTransportSocketPool>(
                &common_connect_job_params);
    CapturePreconnectsTransportSocketPool* transport_conn_pool =
        owned_transport_conn_pool.get();
    auto mock_pool_manager = std::make_unique<MockClientSocketPoolManager>();
    mock_pool_manager->SetSocketPool(ProxyChain::Direct(),
                                     std::move(owned_transport_conn_pool));
    peer.SetClientSocketPoolManager(std::move(mock_pool_manager));

    DoPreconnect();
    EXPECT_EQ(-1, transport_conn_pool->last_num_streams());
  }
}

// Verify that preconnects use the specified NetworkAnonymizationKey.
TEST_P(HttpStreamFactoryTest, PreconnectNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const GURL kURL("http://foo.test/");
  const SchemefulSite kSiteFoo(GURL("http://foo.test"));
  const SchemefulSite kSiteBar(GURL("http://bar.test"));
  const auto kKey1 = NetworkAnonymizationKey::CreateSameSite(kSiteFoo);
  const auto kKey2 = NetworkAnonymizationKey::CreateSameSite(kSiteBar);
  auto DoPreconnect1 = [&] {
    PreconnectHelperForURL(1, kURL, kKey1, SecureDnsPolicy::kAllow,
                           session.get());
  };
  auto DoPreconnect2 = [&] {
    PreconnectHelperForURL(2, kURL, kKey2, SecureDnsPolicy::kAllow,
                           session.get());
  };

  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
    auto delegate = std::make_unique<CapturePreconnectHttpStreamPoolDelegate>();
    CapturePreconnectHttpStreamPoolDelegate* delegate_ptr = delegate.get();
    session->http_stream_pool()->SetDelegateForTesting(std::move(delegate));

    DoPreconnect1();
    EXPECT_EQ(1, delegate_ptr->last_num_streams());
    EXPECT_EQ(kKey1,
              delegate_ptr->last_stream_key().network_anonymization_key());

    DoPreconnect2();
    EXPECT_EQ(2, delegate_ptr->last_num_streams());
    EXPECT_EQ(kKey2,
              delegate_ptr->last_stream_key().network_anonymization_key());
  } else {
    HttpNetworkSessionPeer peer(session.get());
    CommonConnectJobParams common_connect_job_params =
        session->CreateCommonConnectJobParams();
    std::unique_ptr<CapturePreconnectsTransportSocketPool>
        owned_transport_conn_pool =
            std::make_unique<CapturePreconnectsTransportSocketPool>(
                &common_connect_job_params);
    CapturePreconnectsTransportSocketPool* transport_conn_pool =
        owned_transport_conn_pool.get();
    auto mock_pool_manager = std::make_unique<MockClientSocketPoolManager>();
    mock_pool_manager->SetSocketPool(ProxyChain::Direct(),
                                     std::move(owned_transport_conn_pool));
    peer.SetClientSocketPoolManager(std::move(mock_pool_manager));

    DoPreconnect1();
    EXPECT_EQ(1, transport_conn_pool->last_num_streams());
    EXPECT_EQ(kKey1,
              transport_conn_pool->last_group_id().network_anonymization_key());

    DoPreconnect2();
    EXPECT_EQ(2, transport_conn_pool->last_num_streams());
    EXPECT_EQ(kKey2,
              transport_conn_pool->last_group_id().network_anonymization_key());
  }
}

// Verify that preconnects use the specified Secure DNS Tag.
TEST_P(HttpStreamFactoryTest, PreconnectDisableSecureDns) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  const GURL kURL("http://foo.test/");
  const SchemefulSite kSiteFoo(GURL("http://foo.test"));
  const SchemefulSite kSiteBar(GURL("http://bar.test"));
  auto DoPreconnect1 = [&] {
    PreconnectHelperForURL(1, kURL, NetworkAnonymizationKey(),
                           SecureDnsPolicy::kAllow, session.get());
  };
  auto DoPreconnect2 = [&] {
    PreconnectHelperForURL(2, kURL, NetworkAnonymizationKey(),
                           SecureDnsPolicy::kDisable, session.get());
  };

  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
    auto delegate = std::make_unique<CapturePreconnectHttpStreamPoolDelegate>();
    CapturePreconnectHttpStreamPoolDelegate* delegate_ptr = delegate.get();
    session->http_stream_pool()->SetDelegateForTesting(std::move(delegate));

    DoPreconnect1();
    EXPECT_EQ(1, delegate_ptr->last_num_streams());
    EXPECT_EQ(SecureDnsPolicy::kAllow,
              delegate_ptr->last_stream_key().secure_dns_policy());

    DoPreconnect2();
    EXPECT_EQ(2, delegate_ptr->last_num_streams());
    EXPECT_EQ(SecureDnsPolicy::kDisable,
              delegate_ptr->last_stream_key().secure_dns_policy());
  } else {
    HttpNetworkSessionPeer peer(session.get());
    CommonConnectJobParams common_connect_job_params =
        session->CreateCommonConnectJobParams();
    std::unique_ptr<CapturePreconnectsTransportSocketPool>
        owned_transport_conn_pool =
            std::make_unique<CapturePreconnectsTransportSocketPool>(
                &common_connect_job_params);
    CapturePreconnectsTransportSocketPool* transport_conn_pool =
        owned_transport_conn_pool.get();
    auto mock_pool_manager = std::make_unique<MockClientSocketPoolManager>();
    mock_pool_manager->SetSocketPool(ProxyChain::Direct(),
                                     std::move(owned_transport_conn_pool));
    peer.SetClientSocketPoolManager(std::move(mock_pool_manager));

    DoPreconnect1();
    EXPECT_EQ(1, transport_conn_pool->last_num_streams());
    EXPECT_EQ(SecureDnsPolicy::kAllow,
              transport_conn_pool->last_group_id().secure_dns_policy());

    DoPreconnect2();
    EXPECT_EQ(2, transport_conn_pool->last_num_streams());
    EXPECT_EQ(SecureDnsPolicy::kDisable,
              transport_conn_pool->last_group_id().secure_dns_policy());
  }
}

TEST_P(HttpStreamFactoryTest, JobNotifiesProxy) {
  const char* kProxyString = "PROXY bad:99; PROXY maybe:80; DIRECT";
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          kProxyString, TRAFFIC_ANNOTATION_FOR_TESTS));

  // First connection attempt fails
  StaticSocketDataProvider socket_data1;
  socket_data1.set_connect_data(MockConnect(ASYNC, ERR_ADDRESS_UNREACHABLE));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data1);

  // Second connection attempt succeeds
  StaticSocketDataProvider socket_data2;
  socket_data2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data2);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream. It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);

  // The proxy that failed should now be known to the proxy_resolution_service
  // as bad.
  const ProxyRetryInfoMap& retry_info =
      session->proxy_resolution_service()->proxy_retry_info();
  EXPECT_EQ(1u, retry_info.size());
  auto iter = retry_info.find(
      ProxyChain(ProxyUriToProxyServer("bad:99", ProxyServer::SCHEME_HTTP)));
  EXPECT_TRUE(iter != retry_info.end());
}

// This test requests a stream for an https:// URL using an HTTP proxy.
// The proxy will fail to establish a tunnel via connect, and the resolved
// proxy list includes a fallback to DIRECT.
//
// The expected behavior is that proxy fallback does NOT occur, even though the
// request might work using the fallback. This is a regression test for
// https://crbug.com/680837.
TEST_P(HttpStreamFactoryTest, NoProxyFallbackOnTunnelFail) {
  const char* kProxyString = "PROXY bad:99; DIRECT";
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          kProxyString, TRAFFIC_ANNOTATION_FOR_TESTS));

  // A 404 in response to a CONNECT will trigger
  // ERR_TUNNEL_CONNECTION_FAILED.
  MockRead data_reads[] = {
      MockRead("HTTP/1.1 404 Not Found\r\n\r\n"),
      MockRead(SYNCHRONOUS, OK),
  };

  // Simulate a failure during CONNECT to bad:99.
  StaticSocketDataProvider socket_data1(data_reads, base::span<MockWrite>());
  socket_data1.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data1);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Request a stream for an https:// URL. The exact URL doesn't matter for
  // this test, since it mocks a failure immediately when establishing a
  // tunnel through the proxy.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);

  // The stream should have failed, since the proxy server failed to
  // establish a tunnel.
  ASSERT_THAT(requester.error_status(), IsError(ERR_TUNNEL_CONNECTION_FAILED));

  // The proxy should NOT have been marked as bad.
  const ProxyRetryInfoMap& retry_info =
      session->proxy_resolution_service()->proxy_retry_info();
  EXPECT_EQ(0u, retry_info.size());
}

// List of errors that are used in the tests related to QUIC proxy.
const int quic_proxy_test_mock_errors[] = {
    ERR_PROXY_CONNECTION_FAILED,
    ERR_NAME_NOT_RESOLVED,
    ERR_ADDRESS_UNREACHABLE,
    ERR_CONNECTION_CLOSED,
    ERR_CONNECTION_TIMED_OUT,
    ERR_CONNECTION_RESET,
    ERR_CONNECTION_REFUSED,
    ERR_CONNECTION_ABORTED,
    ERR_TIMED_OUT,
    ERR_SOCKS_CONNECTION_FAILED,
    ERR_PROXY_CERTIFICATE_INVALID,
    ERR_QUIC_PROTOCOL_ERROR,
    ERR_QUIC_HANDSHAKE_FAILED,
    ERR_SSL_PROTOCOL_ERROR,
    ERR_MSG_TOO_BIG,
};

// Tests that a bad QUIC proxy is added to the list of bad proxies.
TEST_P(HttpStreamFactoryTest, QuicProxyMarkedAsBad) {
  for (int quic_proxy_test_mock_error : quic_proxy_test_mock_errors) {
    auto quic_proxy_chain =
        ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
            ProxyServer::SCHEME_QUIC, "bad", 99)});
    std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
        ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
            {quic_proxy_chain, ProxyChain::Direct()},
            TRAFFIC_ANNOTATION_FOR_TESTS);

    HttpNetworkSessionParams session_params;
    session_params.enable_quic = true;

    HttpNetworkSessionContext session_context;
    SSLConfigServiceDefaults ssl_config_service;
    HttpServerProperties http_server_properties;
    MockClientSocketFactory socket_factory;
    session_context.client_socket_factory = &socket_factory;
    MockHostResolver host_resolver;
    session_context.host_resolver = &host_resolver;
    MockCertVerifier cert_verifier;
    session_context.cert_verifier = &cert_verifier;
    TransportSecurityState transport_security_state;
    session_context.transport_security_state = &transport_security_state;
    QuicContext quic_context;
    StaticHttpUserAgentSettings http_user_agent_settings("*", "test-ua");
    session_context.http_user_agent_settings = &http_user_agent_settings;
    session_context.proxy_resolution_service = proxy_resolution_service.get();
    session_context.ssl_config_service = &ssl_config_service;
    session_context.http_server_properties = &http_server_properties;
    session_context.quic_context = &quic_context;

    host_resolver.rules()->AddRule("www.google.com", "2.3.4.5");
    host_resolver.rules()->AddRule("bad", "1.2.3.4");

    auto session =
        std::make_unique<HttpNetworkSession>(session_params, session_context);
    session->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
        true);

    StaticSocketDataProvider socket_data1;
    socket_data1.set_connect_data(
        MockConnect(ASYNC, quic_proxy_test_mock_error));
    socket_factory.AddSocketDataProvider(&socket_data1);

    // Second connection attempt succeeds.
    StaticSocketDataProvider socket_data2;
    socket_data2.set_connect_data(MockConnect(ASYNC, OK));
    socket_factory.AddSocketDataProvider(&socket_data2);

    // Now request a stream. It should succeed using the second proxy in the
    // list.
    HttpRequestInfo request_info;
    request_info.method = "GET";
    request_info.url = GURL("http://www.google.com");
    request_info.traffic_annotation =
        MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

    StreamRequester requester(session.get());
    requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                   DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                   /*enable_ip_based_pooling=*/true,
                                   /*enable_alternative_services=*/true);

    // The proxy that failed should now be known to the
    // proxy_resolution_service as bad.
    const ProxyRetryInfoMap& retry_info =
        session->proxy_resolution_service()->proxy_retry_info();
    EXPECT_EQ(1u, retry_info.size()) << quic_proxy_test_mock_error;
    EXPECT_TRUE(requester.used_proxy_info().is_direct());

    auto iter = retry_info.find(quic_proxy_chain);
    EXPECT_TRUE(iter != retry_info.end()) << quic_proxy_test_mock_error;
  }
}

// BidirectionalStreamImpl::Delegate to wait until response headers are
// received.
class TestBidirectionalDelegate : public BidirectionalStreamImpl::Delegate {
 public:
  void WaitUntilDone() { loop_.Run(); }

  const quiche::HttpHeaderBlock& response_headers() const {
    return response_headers_;
  }

 private:
  void OnStreamReady(bool request_headers_sent) override {}
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override {
    response_headers_ = response_headers.Clone();
    loop_.Quit();
  }
  void OnDataRead(int bytes_read) override { NOTREACHED_IN_MIGRATION(); }
  void OnDataSent() override { NOTREACHED_IN_MIGRATION(); }
  void OnTrailersReceived(const quiche::HttpHeaderBlock& trailers) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnFailed(int error) override { NOTREACHED_IN_MIGRATION(); }
  base::RunLoop loop_;
  quiche::HttpHeaderBlock response_headers_;
};

struct QuicTestParams {
  QuicTestParams(quic::ParsedQuicVersion quic_version,
                 bool happy_eyeballs_v3_enabled)
      : quic_version(quic_version),
        happy_eyeballs_v3_enabled(happy_eyeballs_v3_enabled) {}

  quic::ParsedQuicVersion quic_version;
  bool happy_eyeballs_v3_enabled;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const QuicTestParams& p) {
  return base::StrCat(
      {ParsedQuicVersionToString(p.quic_version), "_",
       p.happy_eyeballs_v3_enabled ? "HEv3Enabled" : "HEv3Disabled"});
}

std::vector<QuicTestParams> GetTestParams() {
  std::vector<QuicTestParams> params;
  for (const auto& quic_version : AllSupportedQuicVersions()) {
    params.emplace_back(quic_version, /*happy_eyeballs_v3_enabled=*/true);
    params.emplace_back(quic_version, /*happy_eyeballs_v3_enabled=*/false);
  }
  return params;
}

}  // namespace

TEST_P(HttpStreamFactoryTest, UsePreConnectIfNoZeroRTT) {
  for (int num_streams = 1; num_streams < 3; ++num_streams) {
    GURL url = GURL("https://www.google.com");

    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
            {ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
                ProxyServer::SCHEME_QUIC, "quic_proxy", 443)})},
            TRAFFIC_ANNOTATION_FOR_TESTS));

    // Setup params to disable preconnect, but QUIC doesn't 0RTT.
    HttpNetworkSessionParams session_params =
        SpdySessionDependencies::CreateSessionParams(&session_deps);
    session_params.enable_quic = true;

    // Set up QUIC as alternative_service.
    HttpServerProperties http_server_properties;
    const AlternativeService alternative_service(kProtoQUIC, url.host().c_str(),
                                                 url.IntPort());
    base::Time expiration = base::Time::Now() + base::Days(1);
    HostPortPair host_port_pair(alternative_service.host_port_pair());
    url::SchemeHostPort server("https", host_port_pair.host(),
                               host_port_pair.port());
    http_server_properties.SetQuicAlternativeService(
        server, NetworkAnonymizationKey(), alternative_service, expiration,
        DefaultSupportedQuicVersions());

    HttpNetworkSessionContext session_context =
        SpdySessionDependencies::CreateSessionContext(&session_deps);
    session_context.http_server_properties = &http_server_properties;

    auto session =
        std::make_unique<HttpNetworkSession>(session_params, session_context);
    HttpNetworkSessionPeer peer(session.get());
    ProxyChain proxy_chain =
        ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
            ProxyServer::SCHEME_QUIC, "quic_proxy", 443)});
    CommonConnectJobParams common_connect_job_params =
        session->CreateCommonConnectJobParams();
    auto http_proxy_pool =
        std::make_unique<CapturePreconnectsTransportSocketPool>(
            &common_connect_job_params);
    auto* http_proxy_pool_ptr = http_proxy_pool.get();
    auto mock_pool_manager = std::make_unique<MockClientSocketPoolManager>();
    mock_pool_manager->SetSocketPool(proxy_chain, std::move(http_proxy_pool));
    peer.SetClientSocketPoolManager(std::move(mock_pool_manager));
    PreconnectHelperForURL(num_streams, url, NetworkAnonymizationKey(),
                           SecureDnsPolicy::kAllow, session.get());
    EXPECT_EQ(num_streams, http_proxy_pool_ptr->last_num_streams());
  }
}

namespace {

// Return count of distinct groups in given socket pool.
int GetSocketPoolGroupCount(ClientSocketPool* pool) {
  int count = 0;
  base::Value dict = pool->GetInfoAsValue("", "");
  EXPECT_TRUE(dict.is_dict());
  const base::Value::Dict* groups = dict.GetDict().FindDict("groups");
  if (groups) {
    count = groups->size();
  }
  return count;
}

int GetHttpStreamPoolGroupCount(HttpNetworkSession* session) {
  base::Value::Dict dict = session->http_stream_pool()->GetInfoAsValue();
  const base::Value::Dict* groups = dict.FindDict("groups");
  if (groups) {
    return groups->size();
  }
  return 0;
}

int GetPoolGroupCount(HttpNetworkSession* session,
                      HttpNetworkSession::SocketPoolType pool_type,
                      const ProxyChain& proxy_chain) {
  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3) &&
      pool_type == HttpNetworkSession::NORMAL_SOCKET_POOL &&
      proxy_chain.is_direct()) {
    return GetHttpStreamPoolGroupCount(session);
  } else {
    return GetSocketPoolGroupCount(
        session->GetSocketPool(pool_type, proxy_chain));
  }
}

// Return count of distinct spdy sessions.
int GetSpdySessionCount(HttpNetworkSession* session) {
  std::unique_ptr<base::Value> value(
      session->spdy_session_pool()->SpdySessionPoolInfoToValue());
  if (!value || !value->is_list()) {
    return -1;
  }
  return value->GetList().size();
}

// Return count of sockets handed out by a given socket pool.
int GetHandedOutSocketCount(ClientSocketPool* pool) {
  base::Value dict = pool->GetInfoAsValue("", "");
  EXPECT_TRUE(dict.is_dict());
  return dict.GetDict().FindInt("handed_out_socket_count").value_or(-1);
}

int GetHttpStreamPoolHandedOutCount(HttpNetworkSession* session) {
  base::Value::Dict dict = session->http_stream_pool()->GetInfoAsValue();
  return dict.FindInt("handed_out_socket_count").value_or(-1);
}

int GetHandedOutCount(HttpNetworkSession* session,
                      HttpNetworkSession::SocketPoolType pool_type,
                      const ProxyChain& proxy_chain) {
  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3) &&
      pool_type == HttpNetworkSession::NORMAL_SOCKET_POOL &&
      proxy_chain.is_direct()) {
    return GetHttpStreamPoolHandedOutCount(session);
  } else {
    return GetHandedOutSocketCount(
        session->GetSocketPool(pool_type, proxy_chain));
  }
}

// Return count of distinct QUIC sessions.
int GetQuicSessionCount(HttpNetworkSession* session) {
  base::Value dict(session->QuicInfoToValue());
  base::Value::List* session_list = dict.GetDict().FindList("sessions");
  if (!session_list) {
    return -1;
  }
  return session_list->size();
}

TEST_P(HttpStreamFactoryTest, PrivacyModeUsesDifferentSocketPoolGroup) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  StaticSocketDataProvider socket_data_1;
  socket_data_1.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data_1);
  StaticSocketDataProvider socket_data_2;
  socket_data_2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data_2);
  StaticSocketDataProvider socket_data_3;
  socket_data_3.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data_3);

  SSLSocketDataProvider ssl_1(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_1);
  SSLSocketDataProvider ssl_2(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_2);
  SSLSocketDataProvider ssl_3(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_3);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
  ClientSocketPool* ssl_pool = session->GetSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct());

  auto GetGroupCount = [&] {
    if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
      return GetHttpStreamPoolGroupCount(session.get());
    } else {
      return GetSocketPoolGroupCount(ssl_pool);
    }
  };

  EXPECT_EQ(GetGroupCount(), 0);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.privacy_mode = PRIVACY_MODE_DISABLED;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester1(session.get());
  requester1.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);

  EXPECT_EQ(GetGroupCount(), 1);

  StreamRequester requester2(session.get());
  requester2.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);

  EXPECT_EQ(GetGroupCount(), 1);

  request_info.privacy_mode = PRIVACY_MODE_ENABLED;
  StreamRequester requester3(session.get());
  requester3.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);

  EXPECT_EQ(GetGroupCount(), 2);
}

TEST_P(HttpStreamFactoryTest, DisableSecureDnsUsesDifferentSocketPoolGroup) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  StaticSocketDataProvider socket_data_1;
  socket_data_1.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data_1);
  StaticSocketDataProvider socket_data_2;
  socket_data_2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data_2);
  StaticSocketDataProvider socket_data_3;
  socket_data_3.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data_3);

  SSLSocketDataProvider ssl_1(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_1);
  SSLSocketDataProvider ssl_2(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_2);
  SSLSocketDataProvider ssl_3(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_3);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
  ClientSocketPool* ssl_pool = session->GetSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct());

  auto GetGroupCount = [&] {
    if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
      return GetHttpStreamPoolGroupCount(session.get());
    } else {
      return GetSocketPoolGroupCount(ssl_pool);
    }
  };

  EXPECT_EQ(GetGroupCount(), 0);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.privacy_mode = PRIVACY_MODE_DISABLED;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request_info.secure_dns_policy = SecureDnsPolicy::kAllow;

  StreamRequester requester1(session.get());
  requester1.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);

  EXPECT_EQ(SecureDnsPolicy::kAllow,
            session_deps.host_resolver->last_secure_dns_policy());
  EXPECT_EQ(GetGroupCount(), 1);

  StreamRequester requester2(session.get());
  requester2.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);

  EXPECT_EQ(SecureDnsPolicy::kAllow,
            session_deps.host_resolver->last_secure_dns_policy());
  EXPECT_EQ(GetGroupCount(), 1);

  request_info.secure_dns_policy = SecureDnsPolicy::kDisable;
  StreamRequester requester3(session.get());
  requester3.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);

  EXPECT_EQ(SecureDnsPolicy::kDisable,
            session_deps.host_resolver->last_secure_dns_policy());
  EXPECT_EQ(GetGroupCount(), 2);
}

TEST_P(HttpStreamFactoryTest, GetLoadState) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestStream(session->http_stream_factory(), request_info,
                          DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                          /*enable_ip_based_pooling=*/true,
                          /*enable_alternative_services=*/true);
  requester.MaybeWaitForSwitchesToHttpStreamPool();

  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, requester.request()->GetLoadState());

  requester.WaitForStream();
}

TEST_P(HttpStreamFactoryTest, RequestHttpStream) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.  It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester.stream_done());
  ASSERT_TRUE(nullptr != requester.stream());
  EXPECT_TRUE(nullptr == requester.websocket_stream());

  EXPECT_EQ(0, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_TRUE(requester.used_proxy_info().is_direct());
}

// Test the race of SetPriority versus stream completion where SetPriority may
// be called on an HttpStreamFactory::Job after the stream has been created by
// the job.
TEST_P(HttpStreamFactoryTest, ReprioritizeAfterStreamReceived) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  session_deps.host_resolver->set_synchronous_mode(true);

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider socket_data(base::span_from_ref(mock_read),
                                       base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(SYNCHRONOUS, OK);
  ssl_socket_data.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  EXPECT_EQ(0, GetSpdySessionCount(session.get()));
  requester.RequestStream(session->http_stream_factory(), request_info, LOWEST,
                          /*allowed_bad_certs=*/{},
                          /*enable_ip_based_pooling=*/true,
                          /*enable_alternative_services=*/true);
  requester.MaybeWaitForSwitchesToHttpStreamPool();
  EXPECT_FALSE(requester.stream_done());

  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
    // When the HappyEyeballsV3 is enabled, SpdySessions never created
    // synchronously even when the mocked connects complete synchronously.
    // There is no new session at this point.
    ASSERT_EQ(0, GetSpdySessionCount(session.get()));
  } else {
    // Confirm a stream has been created by asserting that a new session
    // has been created.  (The stream is only created at the SPDY level on
    // first write, which happens after the request has returned a stream).
    ASSERT_EQ(1, GetSpdySessionCount(session.get()));
  }

  // Test to confirm that a SetPriority received after the stream is created
  // but before the request returns it does not crash.
  requester.request()->SetPriority(HIGHEST);

  requester.WaitForStream();
  EXPECT_TRUE(requester.stream_done());
  ASSERT_TRUE(requester.stream());
  EXPECT_FALSE(requester.websocket_stream());
}

TEST_P(HttpStreamFactoryTest, RequestHttpStreamOverSSL) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(base::span_from_ref(mock_read),
                                       base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester.stream_done());
  ASSERT_TRUE(nullptr != requester.stream());
  EXPECT_TRUE(nullptr == requester.websocket_stream());

  EXPECT_EQ(0, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_TRUE(requester.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestHttpStreamOverProxy) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "myproxy:8888", TRAFFIC_ANNOTATION_FOR_TESTS));

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.  It should succeed using the second proxy in the
  // list.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester.stream_done());
  ASSERT_TRUE(nullptr != requester.stream());
  EXPECT_TRUE(nullptr == requester.websocket_stream());

  EXPECT_EQ(0, GetSpdySessionCount(session.get()));
  EXPECT_EQ(0, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_EQ(1, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain(ProxyServer::SCHEME_HTTP,
                                            HostPortPair("myproxy", 8888))));
  EXPECT_EQ(0, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain(ProxyServer::SCHEME_HTTPS,
                                            HostPortPair("myproxy", 8888))));
  EXPECT_EQ(0, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
                                 ProxyChain(ProxyServer::SCHEME_HTTP,
                                            HostPortPair("myproxy", 8888))));
  EXPECT_FALSE(requester.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestWebSocketBasicHandshakeStream) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("ws://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  WebSocketStreamCreateHelper create_helper;
  requester.RequestWebSocketHandshakeStream(
      session->http_stream_factory(), request_info, DEFAULT_PRIORITY,
      /*allowed_bad_certs=*/{}, &create_helper,
      /*enable_ip_based_pooling=*/true,
      /*enable_alternative_services=*/true);
  requester.WaitForStream();
  EXPECT_TRUE(requester.stream_done());
  EXPECT_TRUE(nullptr == requester.stream());
  ASSERT_TRUE(nullptr != requester.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            requester.websocket_stream()->type());
  EXPECT_EQ(0, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_TRUE(requester.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestWebSocketBasicHandshakeStreamOverSSL) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(base::span_from_ref(mock_read),
                                       base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("wss://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  WebSocketStreamCreateHelper create_helper;
  requester.RequestWebSocketHandshakeStream(
      session->http_stream_factory(), request_info, DEFAULT_PRIORITY,
      /*allowed_bad_certs=*/{}, &create_helper,
      /*enable_ip_based_pooling=*/true,
      /*enable_alternative_services=*/true);
  requester.WaitForStream();
  EXPECT_TRUE(requester.stream_done());
  EXPECT_TRUE(nullptr == requester.stream());
  ASSERT_TRUE(nullptr != requester.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            requester.websocket_stream()->type());
  EXPECT_EQ(0, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_TRUE(requester.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestWebSocketBasicHandshakeStreamOverProxy) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateFixedForTest(
          "myproxy:8888", TRAFFIC_ANNOTATION_FOR_TESTS));

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, "HTTP/1.0 200 Connection established\r\n\r\n")};
  StaticSocketDataProvider socket_data(reads, base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("ws://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  WebSocketStreamCreateHelper create_helper;
  requester.RequestWebSocketHandshakeStream(
      session->http_stream_factory(), request_info, DEFAULT_PRIORITY,
      /*allowed_bad_certs=*/{}, &create_helper,
      /*enable_ip_based_pooling=*/true,
      /*enable_alternative_services=*/true);
  requester.WaitForStream();
  EXPECT_TRUE(requester.stream_done());
  EXPECT_TRUE(nullptr == requester.stream());
  ASSERT_TRUE(nullptr != requester.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            requester.websocket_stream()->type());
  EXPECT_EQ(0, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_EQ(0, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain(ProxyServer::SCHEME_HTTP,
                                            HostPortPair("myproxy", 8888))));
  EXPECT_EQ(1, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
                                 ProxyChain(ProxyServer::SCHEME_HTTP,
                                            HostPortPair("myproxy", 8888))));
  EXPECT_FALSE(requester.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestSpdyHttpStreamHttpsURL) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::span_from_ref(mock_read),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  HostPortPair host_port_pair("www.google.com", 443);
  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester.stream_done());
  EXPECT_TRUE(nullptr == requester.websocket_stream());
  ASSERT_TRUE(nullptr != requester.stream());

  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_TRUE(requester.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestSpdyHttpStreamHttpURL) {
  url::SchemeHostPort scheme_host_port("http", "myproxy.org", 443);
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::span_from_ref(mock_read),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps->socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.next_proto = kProtoHTTP2;
  session_deps->socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);
  session_deps->proxy_resolution_service = std::move(proxy_resolution_service);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(session_deps.get()));

  HttpServerProperties* http_server_properties =
      session->spdy_session_pool()->http_server_properties();
  EXPECT_FALSE(http_server_properties->GetSupportsSpdy(
      scheme_host_port, NetworkAnonymizationKey()));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester.stream_done());
  EXPECT_TRUE(nullptr == requester.websocket_stream());
  ASSERT_TRUE(nullptr != requester.stream());

  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(0, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_FALSE(requester.used_proxy_info().is_direct());
  EXPECT_TRUE(http_server_properties->GetSupportsSpdy(
      scheme_host_port, NetworkAnonymizationKey()));
}

// Same as above, but checks HttpServerProperties is updated using the correct
// NetworkAnonymizationKey. When/if NetworkAnonymizationKey is enabled by
// default, this should probably be merged into the above test.
TEST_P(HttpStreamFactoryTest,
       RequestSpdyHttpStreamHttpURLWithNetworkAnonymizationKey) {
  const SchemefulSite kSite1(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSite1);
  const NetworkIsolationKey kNetworkIsolationKey1(kSite1, kSite1);
  const SchemefulSite kSite2(GURL("https://bar.test/"));
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSite2);
  const NetworkIsolationKey kNetworkIsolationKey2(kSite1, kSite1);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  url::SchemeHostPort scheme_host_port("http", "myproxy.org", 443);
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::span_from_ref(mock_read),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps->socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.next_proto = kProtoHTTP2;
  session_deps->socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);
  session_deps->proxy_resolution_service = std::move(proxy_resolution_service);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(session_deps.get()));

  HttpServerProperties* http_server_properties =
      session->spdy_session_pool()->http_server_properties();
  EXPECT_FALSE(http_server_properties->GetSupportsSpdy(
      scheme_host_port, kNetworkAnonymizationKey1));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("http://www.google.com");
  request_info.load_flags = 0;
  request_info.network_isolation_key = kNetworkIsolationKey1;
  request_info.network_anonymization_key = kNetworkAnonymizationKey1;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester.stream_done());
  EXPECT_TRUE(nullptr == requester.websocket_stream());
  ASSERT_TRUE(nullptr != requester.stream());

  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(0, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_FALSE(requester.used_proxy_info().is_direct());
  EXPECT_TRUE(http_server_properties->GetSupportsSpdy(
      scheme_host_port, kNetworkAnonymizationKey1));
  // Other NetworkAnonymizationKeys should not be recorded as supporting SPDY.
  EXPECT_FALSE(http_server_properties->GetSupportsSpdy(
      scheme_host_port, NetworkAnonymizationKey()));
  EXPECT_FALSE(http_server_properties->GetSupportsSpdy(
      scheme_host_port, kNetworkAnonymizationKey2));
}

// Tests that when a new SpdySession is established, duplicated idle H2 sockets
// to the same server are closed.
TEST_P(HttpStreamFactoryTest, NewSpdySessionCloseIdleH2Sockets) {
  // Explicitly disable the HappyEyeballsV3 feature because this test relies on
  // ClientSocketPool. When HappyEyeballsV3 is enabled we immediately create
  // a SpdySession after negotiating to use HTTP/2 so there would be no idle
  // HTTP/2 sockets when the feature is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kHappyEyeballsV3);

  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  const int kNumIdleSockets = 4;
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  std::vector<std::unique_ptr<SequencedSocketData>> providers;
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.next_proto = kProtoHTTP2;
  for (int i = 0; i < kNumIdleSockets; i++) {
    auto provider =
        std::make_unique<SequencedSocketData>(reads, base::span<MockWrite>());
    provider->set_connect_data(MockConnect(ASYNC, OK));
    session_deps.socket_factory->AddSocketDataProvider(provider.get());
    providers.push_back(std::move(provider));
    session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);
  }

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  url::SchemeHostPort destination(url::kHttpsScheme, "www.google.com", 443);

  // Create some HTTP/2 sockets.
  std::vector<std::unique_ptr<ClientSocketHandle>> handles;
  for (size_t i = 0; i < kNumIdleSockets; i++) {
    auto connection = std::make_unique<ClientSocketHandle>();
    TestCompletionCallback callback;
    scoped_refptr<ClientSocketPool::SocketParams> socket_params =
        base::MakeRefCounted<ClientSocketPool::SocketParams>(
            /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>());
    ClientSocketPool::GroupId group_id(
        destination, PrivacyMode::PRIVACY_MODE_DISABLED,
        NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        /*disable_cert_network_fetches=*/false);
    int rv = connection->Init(
        group_id, socket_params, std::nullopt /* proxy_annotation_tag */,
        MEDIUM, SocketTag(), ClientSocketPool::RespectLimits::ENABLED,
        callback.callback(), ClientSocketPool::ProxyAuthCallback(),
        session->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                               ProxyChain::Direct()),
        NetLogWithSource());
    rv = callback.GetResult(rv);
    handles.push_back(std::move(connection));
  }

  // Releases handles now, and these sockets should go into the socket pool.
  handles.clear();
  EXPECT_EQ(kNumIdleSockets,
            session
                ->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                ProxyChain::Direct())
                ->IdleSocketCount());

  // Request two streams at once and make sure they use the same connection.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester1(session.get());
  StreamRequester requester2(session.get());
  requester1.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  requester2.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester1.stream_done());
  EXPECT_TRUE(requester2.stream_done());
  ASSERT_NE(nullptr, requester1.stream());
  ASSERT_NE(nullptr, requester2.stream());
  ASSERT_NE(requester1.stream(), requester2.stream());

  // Establishing the SpdySession will close idle H2 sockets.
  EXPECT_EQ(0, session
                   ->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                   ProxyChain::Direct())
                   ->IdleSocketCount());
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
}

// Regression test for https://crbug.com/706974.
TEST_P(HttpStreamFactoryTest, TwoSpdyConnects) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  SSLSocketDataProvider ssl_socket_data0(ASYNC, OK);
  ssl_socket_data0.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data0);

  MockRead reads0[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  SequencedSocketData data0(reads0, base::span<MockWrite>());
  data0.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&data0);

  SSLSocketDataProvider ssl_socket_data1(ASYNC, OK);
  ssl_socket_data1.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data1);

  SequencedSocketData data1;
  data1.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&data1);

  std::unique_ptr<HttpNetworkSession> session =
      SpdySessionDependencies::SpdyCreateSession(&session_deps);
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  // Request two streams at once and make sure they use the same connection.
  StreamRequester requester1(session.get());
  requester1.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);

  StreamRequester requester2(session.get());
  requester2.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);

  EXPECT_TRUE(requester1.stream_done());
  EXPECT_TRUE(requester2.stream_done());
  ASSERT_NE(nullptr, requester1.stream());
  ASSERT_NE(nullptr, requester2.stream());
  ASSERT_NE(requester1.stream(), requester2.stream());

  // Establishing the SpdySession will close the extra H2 socket.
  EXPECT_EQ(0, session
                   ->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                   ProxyChain::Direct())
                   ->IdleSocketCount());
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_TRUE(data0.AllReadDataConsumed());
  EXPECT_TRUE(data1.AllReadDataConsumed());
}

TEST_P(HttpStreamFactoryTest, RequestBidirectionalStreamImpl) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Explicitly disable HappyEyeballsV3 because it doesn't support bidirectional
  // streams yet.
  // TODO(crbug.com/346835898): Support bidirectional streams in
  // HappyEyeballsV3.
  scoped_feature_list.InitAndDisableFeature(features::kHappyEyeballsV3);

  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  SequencedSocketData socket_data(base::span_from_ref(mock_read),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestBidirectionalStreamImpl(
      session->http_stream_factory(), request_info, DEFAULT_PRIORITY,
      /*allowed_bad_certs=*/{},
      /*enable_ip_based_pooling=*/true,
      /*enable_alternative_services=*/true);
  requester.WaitForStream();
  EXPECT_TRUE(requester.stream_done());
  EXPECT_FALSE(requester.websocket_stream());
  ASSERT_FALSE(requester.stream());
  ASSERT_TRUE(requester.bidirectional_stream_impl());
  EXPECT_EQ(1, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_TRUE(requester.used_proxy_info().is_direct());
}

// Tests for creating an HTTP stream via QUIC.
class HttpStreamFactoryQuicTest
    : public TestWithTaskEnvironment,
      public ::testing::WithParamInterface<QuicTestParams> {
 protected:
  HttpStreamFactoryQuicTest()
      : version_(GetParam().quic_version),
        quic_context_(std::make_unique<MockQuicContext>()),
        session_deps_(ConfiguredProxyResolutionService::CreateDirect()),
        clock_(quic_context_->clock()),
        random_generator_(quic_context_->random_generator()) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    quic::QuicEnableVersion(version_);
    quic_context_->params()->supported_versions =
        quic::test::SupportedVersions(version_);
    quic_context_->params()->origins_to_force_quic_on.insert(
        HostPortPair::FromString("www.example.org:443"));
    quic_context_->AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
    session_deps_.enable_quic = true;
    session_deps_.quic_context = std::move(quic_context_);

    // Load a certificate that is valid for *.example.org
    scoped_refptr<X509Certificate> test_cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    EXPECT_TRUE(test_cert.get());
    verify_details_.cert_verify_result.verified_cert = test_cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    auto mock_crypto_client_stream_factory =
        std::make_unique<MockCryptoClientStreamFactory>();
    mock_crypto_client_stream_factory->AddProofVerifyDetails(&verify_details_);
    mock_crypto_client_stream_factory->set_handshake_mode(
        MockCryptoClientStream::CONFIRM_HANDSHAKE);
    session_deps_.quic_crypto_client_stream_factory =
        std::move(mock_crypto_client_stream_factory);

    session_deps_.http_user_agent_settings =
        std::make_unique<StaticHttpUserAgentSettings>("test-lang", "test-ua");
  }

  HttpNetworkSession* MakeSession() {
    session_ = SpdySessionDependencies::SpdyCreateSessionWithSocketFactory(
        &session_deps_, &socket_factory_);
    session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
        true);
    return session_.get();
  }

  void TearDown() override { session_.reset(); }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructInitialSettingsPacket(
      test::QuicTestPacketMaker& packet_maker,
      uint64_t packet_number) {
    return packet_maker.MakeInitialSettingsPacket(packet_number);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructAckPacket(
      test::QuicTestPacketMaker& packet_maker,
      uint64_t packet_number,
      uint64_t packet_num_received,
      uint64_t smallest_received,
      uint64_t largest_received) {
    return packet_maker.Packet(packet_number)
        .AddAckFrame(packet_num_received, smallest_received, largest_received)
        .Build();
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructConnectUdpRequestPacket(
      test::QuicTestPacketMaker& packet_maker,
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      std::string authority,
      std::string path,
      bool fin) {
    quiche::HttpHeaderBlock headers;
    headers[":scheme"] = "https";
    headers[":path"] = path;
    headers[":protocol"] = "connect-udp";
    headers[":method"] = "CONNECT";
    headers[":authority"] = authority;
    headers["user-agent"] = "test-ua";
    headers["capsule-protocol"] = "?1";
    spdy::SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
    size_t spdy_headers_frame_len;
    auto rv = packet_maker.MakeRequestHeadersPacket(
        packet_number, stream_id, fin, priority, std::move(headers),
        &spdy_headers_frame_len, /*should_include_priority_frame=*/false);
    return rv;
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructOkResponsePacket(
      test::QuicTestPacketMaker& packet_maker,
      uint64_t packet_number,
      quic::QuicStreamId stream_id,
      bool fin) {
    quiche::HttpHeaderBlock headers = packet_maker.GetResponseHeaders("200");
    size_t spdy_headers_frame_len;
    return packet_maker.MakeResponseHeadersPacket(packet_number, stream_id, fin,
                                                  std::move(headers),
                                                  &spdy_headers_frame_len);
  }

  std::unique_ptr<quic::QuicEncryptedPacket>
  ConstructAckAndClientH3DatagramPacket(
      test::QuicTestPacketMaker& packet_maker,
      uint64_t packet_number,
      uint64_t largest_received,
      uint64_t smallest_received,
      uint64_t quarter_stream_id,
      uint64_t context_id,
      std::unique_ptr<quic::QuicEncryptedPacket> packet) {
    std::string datagram;
    // Allow enough space for payload and two varint-62's.
    datagram.resize(packet->length() + 2 * 8);
    quiche::QuicheDataWriter writer(datagram.capacity(), datagram.data());
    CHECK(writer.WriteVarInt62(quarter_stream_id));
    CHECK(writer.WriteVarInt62(context_id));
    CHECK(writer.WriteBytes(packet->data(), packet->length()));
    datagram.resize(writer.length());
    return packet_maker.MakeAckAndDatagramPacket(
        packet_number, largest_received, smallest_received, datagram);
  }

  std::unique_ptr<quic::QuicEncryptedPacket> ConstructClientH3DatagramPacket(
      test::QuicTestPacketMaker& packet_maker,
      uint64_t packet_number,
      uint64_t quarter_stream_id,
      uint64_t context_id,
      std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> packets) {
    auto& builder = packet_maker.Packet(packet_number);
    for (auto& packet : packets) {
      std::string data;
      // Allow enough space for payload and two varint-62's.
      data.resize(packet->length() + 2 * 8);
      quiche::QuicheDataWriter writer(data.capacity(), data.data());
      CHECK(writer.WriteVarInt62(quarter_stream_id));
      CHECK(writer.WriteVarInt62(context_id));
      CHECK(writer.WriteBytes(packet->data(), packet->length()));
      data.resize(writer.length());
      builder.AddMessageFrame(data);
    }
    return builder.Build();
  }

  // Make a `QuicTestPacketMaker` for the current test with the given
  // characteristics.
  test::QuicTestPacketMaker MakePacketMaker(
      const std::string& host,
      quic::Perspective perspective,
      bool client_priority_uses_incremental = false,
      bool use_priority_header = false) {
    return test::QuicTestPacketMaker(
        version_, quic::QuicUtils::CreateRandomConnectionId(random_generator_),
        clock_, host, perspective, client_priority_uses_incremental,
        use_priority_header);
  }

  MockTaggingClientSocketFactory* socket_factory() { return &socket_factory_; }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  SpdySessionDependencies& session_deps() { return session_deps_; }

  quic::ParsedQuicVersion version() const { return version_; }

 private:
  quic::test::QuicFlagSaver saver_;
  const quic::ParsedQuicVersion version_;
  std::unique_ptr<MockQuicContext> quic_context_;
  SpdySessionDependencies session_deps_;
  raw_ptr<const quic::QuicClock> clock_;
  raw_ptr<quic::QuicRandom> random_generator_;
  MockTaggingClientSocketFactory socket_factory_;
  std::unique_ptr<HttpNetworkSession> session_;
  ProofVerifyDetailsChromium verify_details_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HttpStreamFactoryQuicTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

// Check that requesting an HTTP stream over a QUIC proxy sends the correct
// set of QUIC packets.
TEST_P(HttpStreamFactoryQuicTest, RequestHttpStreamOverQuicProxy) {
  static constexpr uint64_t kConnectUdpContextId = 0;
  GURL kRequestUrl("https://www.example.org");
  session_deps().proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {ProxyChain::ForIpProtection({ProxyServer::FromSchemeHostAndPort(
              ProxyServer::SCHEME_QUIC, "qproxy.example.org", 8888)})},
          TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData proxy_quic_data(version());
  quic::QuicStreamId stream_id = GetNthClientInitiatedBidirectionalStreamId(0);
  int to_proxy_packet_num = 1;
  auto to_proxy =
      MakePacketMaker("qproxy.example.org", quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/false);
  int from_proxy_packet_num = 1;
  auto from_proxy =
      MakePacketMaker("qproxy.example.org", quic::Perspective::IS_SERVER,
                      /*client_priority_uses_incremental=*/false,
                      /*use_priority_header=*/false);
  int to_endpoint_packet_num = 1;
  auto to_endpoint =
      MakePacketMaker("www.example.org", quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/true);

  // The browser sends initial settings to the proxy.
  proxy_quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                            to_proxy, to_proxy_packet_num++));

  // The browser sends CONNECT-UDP request to proxy.
  proxy_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructConnectUdpRequestPacket(
          to_proxy, to_proxy_packet_num++, stream_id, "qproxy.example.org:8888",
          "/.well-known/masque/udp/www.example.org/443/", false));

  // Proxy sends initial settings.
  proxy_quic_data.AddRead(ASYNC, ConstructInitialSettingsPacket(
                                     from_proxy, from_proxy_packet_num++));

  // Proxy responds to the CONNECT.
  proxy_quic_data.AddRead(
      ASYNC, ConstructOkResponsePacket(from_proxy, from_proxy_packet_num++,
                                       stream_id, true));
  proxy_quic_data.AddReadPauseForever();

  // The browser ACKs the OK response packet.
  proxy_quic_data.AddWrite(
      ASYNC, ConstructAckPacket(to_proxy, to_proxy_packet_num++, 1, 2, 1));

  // The browser sends initial settings to the endpoint, via proxy.
  std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> datagrams;
  datagrams.push_back(
      ConstructInitialSettingsPacket(to_endpoint, to_endpoint_packet_num++));
  proxy_quic_data.AddWrite(
      ASYNC, ConstructClientH3DatagramPacket(to_proxy, to_proxy_packet_num++,
                                             stream_id, kConnectUdpContextId,
                                             std::move(datagrams)));

  proxy_quic_data.AddSocketDataToFactory(socket_factory());

  HttpNetworkSession* session = MakeSession();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = kRequestUrl;
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session);
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);

  EXPECT_TRUE(requester.stream_done());
  EXPECT_FALSE(requester.websocket_stream());
  EXPECT_TRUE(requester.stream());
  EXPECT_FALSE(requester.used_proxy_info().is_direct());

  RunUntilIdle();

  proxy_quic_data.ExpectAllReadDataConsumed();
  proxy_quic_data.ExpectAllWriteDataConsumed();
}

// Check that requesting an HTTP stream over a two QUIC proxies sends the
// correct set of QUIC packets.
TEST_P(HttpStreamFactoryQuicTest, RequestHttpStreamOverTwoQuicProxies) {
  static constexpr uint64_t kConnectUdpContextId = 0;
  GURL kRequestUrl("https://www.example.org");
  session_deps().proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
          {
              ProxyChain::ForIpProtection(
                  {ProxyServer::FromSchemeHostAndPort(
                       ProxyServer::SCHEME_QUIC, "qproxy1.example.org", 8888),
                   ProxyServer::FromSchemeHostAndPort(
                       ProxyServer::SCHEME_QUIC, "qproxy2.example.org", 8888)}),
          },
          TRAFFIC_ANNOTATION_FOR_TESTS);

  MockQuicData proxy_quic_data(version());
  quic::QuicStreamId stream_id_0 =
      GetNthClientInitiatedBidirectionalStreamId(0);
  int to_proxy1_packet_num = 1;
  auto to_proxy1 =
      MakePacketMaker("qproxy1.example.org", quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/false);
  int from_proxy1_packet_num = 1;
  auto from_proxy1 =
      MakePacketMaker("qproxy1.example.org", quic::Perspective::IS_SERVER,
                      /*client_priority_uses_incremental=*/false,
                      /*use_priority_header=*/false);
  int to_proxy2_packet_num = 1;
  auto to_proxy2 =
      MakePacketMaker("qproxy2.example.org", quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/false);
  int from_proxy2_packet_num = 1;
  auto from_proxy2 =
      MakePacketMaker("qproxy2.example.org", quic::Perspective::IS_SERVER,
                      /*client_priority_uses_incremental=*/false,
                      /*use_priority_header=*/false);
  int to_endpoint_packet_num = 1;
  auto to_endpoint =
      MakePacketMaker("www.example.org", quic::Perspective::IS_CLIENT,
                      /*client_priority_uses_incremental=*/true,
                      /*use_priority_header=*/true);

  // The browser sends initial settings to proxy1.
  proxy_quic_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket(
                                            to_proxy1, to_proxy1_packet_num++));

  // The browser sends CONNECT-UDP request to proxy1.
  proxy_quic_data.AddWrite(
      SYNCHRONOUS,
      ConstructConnectUdpRequestPacket(
          to_proxy1, to_proxy1_packet_num++, stream_id_0,
          "qproxy1.example.org:8888",
          "/.well-known/masque/udp/qproxy2.example.org/8888/", false));

  // Proxy1 sends initial settings.
  proxy_quic_data.AddRead(ASYNC, ConstructInitialSettingsPacket(
                                     from_proxy1, from_proxy1_packet_num++));

  // Proxy1 responds to the CONNECT.
  proxy_quic_data.AddRead(
      ASYNC, ConstructOkResponsePacket(from_proxy1, from_proxy1_packet_num++,
                                       stream_id_0, true));

  // The browser ACKs the OK response packet.
  proxy_quic_data.AddWrite(
      ASYNC, ConstructAckPacket(to_proxy1, to_proxy1_packet_num++, 1, 2, 1));

  // The browser sends initial settings and a CONNECT-UDP request to proxy2 via
  // proxy1.
  std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> datagrams;
  datagrams.push_back(
      ConstructInitialSettingsPacket(to_proxy2, to_proxy2_packet_num++));
  datagrams.push_back(ConstructConnectUdpRequestPacket(
      to_proxy2, to_proxy2_packet_num++, stream_id_0,
      "qproxy2.example.org:8888",
      "/.well-known/masque/udp/www.example.org/443/", false));
  proxy_quic_data.AddWrite(
      ASYNC, ConstructClientH3DatagramPacket(to_proxy1, to_proxy1_packet_num++,
                                             stream_id_0, kConnectUdpContextId,
                                             std::move(datagrams)));

  // Proxy2 sends initial settings and an OK response to the CONNECT request,
  // via proxy1.
  datagrams.clear();
  datagrams.push_back(
      ConstructInitialSettingsPacket(from_proxy2, from_proxy2_packet_num++));
  datagrams.push_back(ConstructOkResponsePacket(
      from_proxy2, from_proxy2_packet_num++, stream_id_0, true));
  proxy_quic_data.AddRead(
      ASYNC, ConstructClientH3DatagramPacket(
                 from_proxy1, from_proxy1_packet_num++, stream_id_0,
                 kConnectUdpContextId, std::move(datagrams)));
  proxy_quic_data.AddReadPauseForever();

  // The browser ACK's the datagram from proxy1, and acks proxy2's OK response
  // packet via proxy1.
  proxy_quic_data.AddWrite(
      ASYNC,
      ConstructAckAndClientH3DatagramPacket(
          to_proxy1, to_proxy1_packet_num++, 3, 1, stream_id_0,
          kConnectUdpContextId,
          ConstructAckPacket(to_proxy2, to_proxy2_packet_num++, 1, 2, 1)));

  // The browser sends initial settings to the endpoint, via proxy2, via proxy1.
  datagrams.clear();
  std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> inner_datagrams;
  inner_datagrams.push_back(
      ConstructInitialSettingsPacket(to_endpoint, to_endpoint_packet_num++));
  datagrams.push_back(ConstructClientH3DatagramPacket(
      to_proxy2, to_proxy2_packet_num++, stream_id_0, kConnectUdpContextId,
      std::move(inner_datagrams)));
  proxy_quic_data.AddWrite(
      ASYNC, ConstructClientH3DatagramPacket(to_proxy1, to_proxy1_packet_num++,
                                             stream_id_0, kConnectUdpContextId,
                                             std::move(datagrams)));

  proxy_quic_data.AddSocketDataToFactory(socket_factory());

  HttpNetworkSession* session = MakeSession();

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = kRequestUrl;
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session);
  requester.RequestStreamAndWait(session->http_stream_factory(), request_info,
                                 DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                 /*enable_ip_based_pooling=*/true,
                                 /*enable_alternative_services=*/true);

  EXPECT_TRUE(requester.stream_done());
  EXPECT_FALSE(requester.websocket_stream());
  EXPECT_TRUE(requester.stream());
  EXPECT_FALSE(requester.used_proxy_info().is_direct());

  RunUntilIdle();

  proxy_quic_data.ExpectAllReadDataConsumed();
  proxy_quic_data.ExpectAllWriteDataConsumed();
}

class HttpStreamFactoryBidirectionalQuicTest
    : public TestWithTaskEnvironment,
      public ::testing::WithParamInterface<quic::ParsedQuicVersion> {
 protected:
  HttpStreamFactoryBidirectionalQuicTest()
      : default_url_(kDefaultUrl),
        version_(GetParam()),
        client_packet_maker_(version_,
                             quic::QuicUtils::CreateRandomConnectionId(
                                 quic_context_.random_generator()),
                             quic_context_.clock(),
                             "www.example.org",
                             quic::Perspective::IS_CLIENT),
        server_packet_maker_(version_,
                             quic::QuicUtils::CreateRandomConnectionId(
                                 quic_context_.random_generator()),
                             quic_context_.clock(),
                             "www.example.org",
                             quic::Perspective::IS_SERVER,
                             false),
        proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        ssl_config_service_(std::make_unique<SSLConfigServiceDefaults>()) {
    // Explicitly disable HappyEyeballsV3 because it doesn't support
    // bidirectional streams.
    // TODO(crbug.com/346835898): Support bidirectional streams in
    // HappyEyeballsV3.
    feature_list_.InitAndDisableFeature(features::kHappyEyeballsV3);
    FLAGS_quic_enable_http3_grease_randomness = false;
    quic_context_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
    quic::QuicEnableVersion(version_);
  }

  void TearDown() override { session_.reset(); }

  void Initialize() {
    params_.enable_quic = true;
    quic_context_.params()->supported_versions =
        quic::test::SupportedVersions(version_);

    HttpNetworkSessionContext session_context;
    session_context.http_server_properties = &http_server_properties_;
    session_context.quic_context = &quic_context_;

    // Load a certificate that is valid for *.example.org
    scoped_refptr<X509Certificate> test_cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    EXPECT_TRUE(test_cert.get());
    verify_details_.cert_verify_result.verified_cert = test_cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details_);
    crypto_client_stream_factory_.set_handshake_mode(
        MockCryptoClientStream::CONFIRM_HANDSHAKE);
    session_context.cert_verifier = &cert_verifier_;
    session_context.quic_crypto_client_stream_factory =
        &crypto_client_stream_factory_;
    session_context.transport_security_state = &transport_security_state_;
    session_context.host_resolver = &host_resolver_;
    session_context.proxy_resolution_service = proxy_resolution_service_.get();
    session_context.ssl_config_service = ssl_config_service_.get();
    session_context.client_socket_factory = &socket_factory_;
    session_ = std::make_unique<HttpNetworkSession>(params_, session_context);
    session_->quic_session_pool()->set_has_quic_ever_worked_on_current_network(
        true);
  }

  void AddQuicAlternativeService(const url::SchemeHostPort& request_url,
                                 const std::string& alternative_destination) {
    const AlternativeService alternative_service(kProtoQUIC,
                                                 alternative_destination, 443);
    base::Time expiration = base::Time::Now() + base::Days(1);
    http_server_properties_.SetQuicAlternativeService(
        request_url, NetworkAnonymizationKey(), alternative_service, expiration,
        session_->context().quic_context->params()->supported_versions);
  }

  void AddQuicAlternativeService() {
    AddQuicAlternativeService(url::SchemeHostPort(default_url_),
                              "www.example.org");
  }

  test::QuicTestPacketMaker& client_packet_maker() {
    return client_packet_maker_;
  }
  test::QuicTestPacketMaker& server_packet_maker() {
    return server_packet_maker_;
  }

  MockTaggingClientSocketFactory& socket_factory() { return socket_factory_; }

  HttpNetworkSession* session() { return session_.get(); }

  const GURL default_url_;

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::ParsedQuicVersion version() const { return version_; }

  MockHostResolver* host_resolver() { return &host_resolver_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  quic::test::QuicFlagSaver saver_;
  const quic::ParsedQuicVersion version_;
  MockQuicContext quic_context_;
  test::QuicTestPacketMaker client_packet_maker_;
  test::QuicTestPacketMaker server_packet_maker_;
  MockTaggingClientSocketFactory socket_factory_;
  std::unique_ptr<HttpNetworkSession> session_;
  MockCertVerifier cert_verifier_;
  ProofVerifyDetailsChromium verify_details_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  HttpServerProperties http_server_properties_;
  TransportSecurityState transport_security_state_;
  MockHostResolver host_resolver_{
      /*default_result=*/
      MockHostResolverBase::RuleResolver::GetLocalhostResult()};
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<SSLConfigServiceDefaults> ssl_config_service_;
  HttpNetworkSessionParams params_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         HttpStreamFactoryBidirectionalQuicTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(HttpStreamFactoryBidirectionalQuicTest,
       RequestBidirectionalStreamImplQuicAlternative) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Explicitly disable HappyEyeballsV3 because it doesn't support bidirectional
  // streams yet.
  // TODO(crbug.com/346835898): Support bidirectional streams in
  // HappyEyeballsV3.
  scoped_feature_list.InitAndDisableFeature(features::kHappyEyeballsV3);

  MockQuicData mock_quic_data(version());
  // Set priority to default value so that
  // QuicTestPacketMaker::MakeRequestHeadersPacket() does not add mock
  // PRIORITY_UPDATE frame, which BidirectionalStreamQuicImpl currently does not
  // send.
  // TODO(crbug.com/40678380): Implement PRIORITY_UPDATE in
  // BidirectionalStreamQuicImpl.
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_length;
  int packet_num = 1;
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          /*fin=*/true, priority,
          client_packet_maker().GetRequestHeaders("GET", "https", "/"),
          &spdy_headers_frame_length));
  size_t spdy_response_headers_frame_length;
  mock_quic_data.AddRead(
      ASYNC, server_packet_maker().MakeResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0),
                 /*fin=*/true, server_packet_maker().GetResponseHeaders("200"),
                 &spdy_response_headers_frame_length));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory());

  // Add hanging data for http job.
  auto hanging_data = std::make_unique<StaticSocketDataProvider>();
  MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
  hanging_data->set_connect_data(hanging_connect);
  socket_factory().AddSocketDataProvider(hanging_data.get());
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl_data);

  // Set up QUIC as alternative_service.
  Initialize();
  AddQuicAlternativeService();

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = default_url_;
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session());
  requester.RequestBidirectionalStreamImpl(
      session()->http_stream_factory(), request_info, DEFAULT_PRIORITY,
      /*allowed_bad_certs=*/{},
      /*enable_ip_based_pooling=*/true,
      /*enable_alternative_services=*/true);

  requester.WaitForStream();
  EXPECT_TRUE(requester.stream_done());
  EXPECT_FALSE(requester.websocket_stream());
  ASSERT_FALSE(requester.stream());
  ASSERT_TRUE(requester.bidirectional_stream_impl());
  BidirectionalStreamImpl* stream_impl = requester.bidirectional_stream_impl();

  BidirectionalStreamRequestInfo bidi_request_info;
  bidi_request_info.method = "GET";
  bidi_request_info.url = default_url_;
  bidi_request_info.end_stream_on_headers = true;
  bidi_request_info.priority = LOWEST;

  TestBidirectionalDelegate delegate;
  stream_impl->Start(&bidi_request_info, NetLogWithSource(),
                     /*send_request_headers_automatically=*/true, &delegate,
                     nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  delegate.WaitUntilDone();

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1);
  EXPECT_THAT(stream_impl->ReadData(buffer.get(), 1), IsOk());
  EXPECT_EQ(kProtoQUIC, stream_impl->GetProtocol());
  EXPECT_EQ("200", delegate.response_headers().find(":status")->second);
  EXPECT_EQ(0,
            GetPoolGroupCount(session(), HttpNetworkSession::NORMAL_SOCKET_POOL,
                              ProxyChain::Direct()));
  EXPECT_TRUE(requester.used_proxy_info().is_direct());
}

// Tests that if Http job fails, but Quic job succeeds, we return
// BidirectionalStreamQuicImpl.
TEST_P(HttpStreamFactoryBidirectionalQuicTest,
       RequestBidirectionalStreamImplHttpJobFailsQuicJobSucceeds) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Explicitly disable HappyEyeballsV3 because it doesn't support bidirectional
  // streams yet.
  // TODO(crbug.com/346835898): Support bidirectional streams in
  // HappyEyeballsV3.
  scoped_feature_list.InitAndDisableFeature(features::kHappyEyeballsV3);

  // Set up Quic data.
  MockQuicData mock_quic_data(version());
  // Set priority to default value so that
  // QuicTestPacketMaker::MakeRequestHeadersPacket() does not add mock
  // PRIORITY_UPDATE frame, which BidirectionalStreamQuicImpl currently does not
  // send.
  // TODO(crbug.com/40678380): Implement PRIORITY_UPDATE in
  // BidirectionalStreamQuicImpl.
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_length;
  int packet_num = 1;
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          /*fin=*/true, priority,
          client_packet_maker().GetRequestHeaders("GET", "https", "/"),
          &spdy_headers_frame_length));
  size_t spdy_response_headers_frame_length;
  mock_quic_data.AddRead(
      ASYNC, server_packet_maker().MakeResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0),
                 /*fin=*/true, server_packet_maker().GetResponseHeaders("200"),
                 &spdy_response_headers_frame_length));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory());

  // Make the http job fail.
  auto http_job_data = std::make_unique<StaticSocketDataProvider>();
  MockConnect failed_connect(ASYNC, ERR_CONNECTION_REFUSED);
  http_job_data->set_connect_data(failed_connect);
  socket_factory().AddSocketDataProvider(http_job_data.get());
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl_data);

  // Set up QUIC as alternative_service.
  Initialize();
  AddQuicAlternativeService();

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = default_url_;
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session());
  requester.RequestBidirectionalStreamImpl(
      session()->http_stream_factory(), request_info, DEFAULT_PRIORITY,
      /*allowed_bad_certs=*/{},
      /*enable_ip_based_pooling=*/true,
      /*enable_alternative_services=*/true);

  requester.WaitForStream();
  EXPECT_TRUE(requester.stream_done());
  EXPECT_FALSE(requester.websocket_stream());
  ASSERT_FALSE(requester.stream());
  ASSERT_TRUE(requester.bidirectional_stream_impl());
  BidirectionalStreamImpl* stream_impl = requester.bidirectional_stream_impl();

  BidirectionalStreamRequestInfo bidi_request_info;
  bidi_request_info.method = "GET";
  bidi_request_info.url = default_url_;
  bidi_request_info.end_stream_on_headers = true;
  bidi_request_info.priority = LOWEST;

  TestBidirectionalDelegate delegate;
  stream_impl->Start(&bidi_request_info, NetLogWithSource(),
                     /*send_request_headers_automatically=*/true, &delegate,
                     nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);
  delegate.WaitUntilDone();

  // Make sure the BidirectionalStream negotiated goes through QUIC.
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(1);
  EXPECT_THAT(stream_impl->ReadData(buffer.get(), 1), IsOk());
  EXPECT_EQ(kProtoQUIC, stream_impl->GetProtocol());
  EXPECT_EQ("200", delegate.response_headers().find(":status")->second);
  // There is no Http2 socket pool.
  EXPECT_EQ(0,
            GetPoolGroupCount(session(), HttpNetworkSession::NORMAL_SOCKET_POOL,
                              ProxyChain::Direct()));
  EXPECT_TRUE(requester.used_proxy_info().is_direct());
}

TEST_P(HttpStreamFactoryTest, RequestBidirectionalStreamImplFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Explicitly disable HappyEyeballsV3 because it doesn't support bidirectional
  // streams yet.
  // TODO(crbug.com/346835898): Support bidirectional streams in
  // HappyEyeballsV3.
  scoped_feature_list.InitAndDisableFeature(features::kHappyEyeballsV3);

  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  SequencedSocketData socket_data(base::span_from_ref(mock_read),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);

  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);

  // If HTTP/1 is used, BidirectionalStreamImpl should not be obtained.
  ssl_socket_data.next_proto = kProtoHTTP11;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Now request a stream.
  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequester requester(session.get());
  requester.RequestBidirectionalStreamImpl(
      session->http_stream_factory(), request_info, DEFAULT_PRIORITY,
      /*allowed_bad_certs=*/{},
      /*enable_ip_based_pooling=*/true,
      /*enable_alternative_services=*/true);
  requester.WaitForStream();
  EXPECT_TRUE(requester.stream_done());
  ASSERT_THAT(requester.error_status(), IsError(ERR_FAILED));
  EXPECT_FALSE(requester.websocket_stream());
  ASSERT_FALSE(requester.stream());
  ASSERT_FALSE(requester.bidirectional_stream_impl());
  EXPECT_EQ(1, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
}

#if BUILDFLAG(IS_ANDROID)
// Verify HttpStreamFactory::Job passes socket tag along properly and that
// SpdySessions have unique socket tags (e.g. one sessions should not be shared
// amongst streams with different socket tags).
TEST_P(HttpStreamFactoryTest, Tag) {
  // SocketTag is not supported yet for HappyEyeballsV3.
  // TODO(crbug.com/346835898): Support SocketTag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kHappyEyeballsV3);

  SpdySessionDependencies session_deps;
  auto socket_factory = std::make_unique<MockTaggingClientSocketFactory>();
  auto* socket_factory_ptr = socket_factory.get();
  session_deps.socket_factory = std::move(socket_factory);

  // Prepare for two HTTPS connects.
  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::span_from_ref(mock_read),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);
  MockRead mock_read2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data2(base::span_from_ref(mock_read2),
                                   base::span<MockWrite>());
  socket_data2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data2);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  ssl_socket_data.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);
  SSLSocketDataProvider ssl_socket_data2(ASYNC, OK);
  ssl_socket_data2.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  ssl_socket_data2.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data2);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Prepare two different tags and corresponding HttpRequestInfos.
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://example.org");
  request_info1.load_flags = 0;
  request_info1.socket_tag = tag1;
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  SocketTag tag2(getuid(), 0x87654321);
  HttpRequestInfo request_info2 = request_info1;
  request_info2.socket_tag = tag2;
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  // Verify one stream with one tag results in one session, group and
  // socket.
  StreamRequester requester1(session.get());
  requester1.RequestStreamAndWait(session->http_stream_factory(), request_info1,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester1.stream_done());
  EXPECT_TRUE(nullptr == requester1.websocket_stream());
  ASSERT_TRUE(nullptr != requester1.stream());

  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  // Verify socket tagged appropriately.
  EXPECT_TRUE(tag1 == socket_factory_ptr->GetLastProducedTCPSocket()->tag());
  EXPECT_TRUE(socket_factory_ptr->GetLastProducedTCPSocket()
                  ->tagged_before_connected());

  // Verify one more stream with a different tag results in one more session and
  // socket.
  StreamRequester requester2(session.get());
  requester2.RequestStreamAndWait(session->http_stream_factory(), request_info2,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester2.stream_done());
  EXPECT_TRUE(nullptr == requester2.websocket_stream());
  ASSERT_TRUE(nullptr != requester2.stream());

  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(2,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  // Verify socket tagged appropriately.
  EXPECT_TRUE(tag2 == socket_factory_ptr->GetLastProducedTCPSocket()->tag());
  EXPECT_TRUE(socket_factory_ptr->GetLastProducedTCPSocket()
                  ->tagged_before_connected());

  // Verify one more stream reusing a tag does not create new sessions, groups
  // or sockets.
  StreamRequester requester3(session.get());
  requester3.RequestStreamAndWait(session->http_stream_factory(), request_info2,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester3.stream_done());
  EXPECT_TRUE(nullptr == requester3.websocket_stream());
  ASSERT_TRUE(nullptr != requester3.stream());

  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(2,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
}

// Verify HttpStreamFactory::Job passes socket tag along properly to QUIC
// sessions and that QuicSessions have unique socket tags (e.g. one sessions
// should not be shared amongst streams with different socket tags).
TEST_P(HttpStreamFactoryBidirectionalQuicTest, Tag) {
  // SocketTag is not supported yet for HappyEyeballsV3.
  // TODO(crbug.com/346835898): Support SocketTag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kHappyEyeballsV3);

  // Prepare mock QUIC data for a first session establishment.
  MockQuicData mock_quic_data(version());
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_length;
  int packet_num = 1;
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          /*fin=*/true, priority,
          client_packet_maker().GetRequestHeaders("GET", "https", "/"),
          &spdy_headers_frame_length));
  size_t spdy_response_headers_frame_length;
  mock_quic_data.AddRead(
      ASYNC, server_packet_maker().MakeResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0),
                 /*fin=*/true, server_packet_maker().GetResponseHeaders("200"),
                 &spdy_response_headers_frame_length));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory());

  // Prepare mock QUIC data for a second session establishment.
  client_packet_maker().Reset();
  MockQuicData mock_quic_data2(version());
  packet_num = 1;
  mock_quic_data2.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data2.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          /*fin=*/true, priority,
          client_packet_maker().GetRequestHeaders("GET", "https", "/"),
          &spdy_headers_frame_length));
  mock_quic_data2.AddRead(
      ASYNC, server_packet_maker().MakeResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0),
                 /*fin=*/true, server_packet_maker().GetResponseHeaders("200"),
                 &spdy_response_headers_frame_length));
  mock_quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data2.AddSocketDataToFactory(&socket_factory());

  // Add hanging data for http job.
  auto hanging_data = std::make_unique<StaticSocketDataProvider>();
  MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
  hanging_data->set_connect_data(hanging_connect);
  socket_factory().AddSocketDataProvider(hanging_data.get());
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl_data);

  // Set up QUIC as alternative_service.
  Initialize();
  AddQuicAlternativeService();

  // Prepare two different tags and corresponding HttpRequestInfos.
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = default_url_;
  request_info1.load_flags = 0;
  request_info1.socket_tag = tag1;
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  SocketTag tag2(getuid(), 0x87654321);
  HttpRequestInfo request_info2 = request_info1;
  request_info2.socket_tag = tag2;
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  // Verify one stream with one tag results in one QUIC session.
  StreamRequester requester1(session());
  requester1.RequestStreamAndWait(session()->http_stream_factory(),
                                  request_info1, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester1.stream_done());
  EXPECT_TRUE(nullptr == requester1.websocket_stream());
  ASSERT_TRUE(nullptr != requester1.stream());
  EXPECT_EQ(kProtoQUIC, requester1.request()->negotiated_protocol());
  EXPECT_EQ(1, GetQuicSessionCount(session()));

  // Verify socket tagged appropriately.
  EXPECT_TRUE(tag1 == socket_factory().GetLastProducedUDPSocket()->tag());
  EXPECT_TRUE(socket_factory()
                  .GetLastProducedUDPSocket()
                  ->tagged_before_data_transferred());

  // Verify one more stream with a different tag results in one more session and
  // socket.
  StreamRequester requester2(session());
  requester2.RequestStreamAndWait(session()->http_stream_factory(),
                                  request_info2, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester2.stream_done());
  EXPECT_TRUE(nullptr == requester2.websocket_stream());
  ASSERT_TRUE(nullptr != requester2.stream());
  EXPECT_EQ(kProtoQUIC, requester2.request()->negotiated_protocol());
  EXPECT_EQ(2, GetQuicSessionCount(session()));

  // Verify socket tagged appropriately.
  EXPECT_TRUE(tag2 == socket_factory().GetLastProducedUDPSocket()->tag());
  EXPECT_TRUE(socket_factory()
                  .GetLastProducedUDPSocket()
                  ->tagged_before_data_transferred());

  // Verify one more stream reusing a tag does not create new sessions.
  StreamRequester requester3(session());
  requester3.RequestStreamAndWait(session()->http_stream_factory(),
                                  request_info2, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester3.stream_done());
  EXPECT_TRUE(nullptr == requester3.websocket_stream());
  ASSERT_TRUE(nullptr != requester3.stream());
  EXPECT_EQ(kProtoQUIC, requester3.request()->negotiated_protocol());
  EXPECT_EQ(2, GetQuicSessionCount(session()));
}

TEST_P(HttpStreamFactoryTest, ChangeSocketTag) {
  // SocketTag is not supported yet for HappyEyeballsV3.
  // TODO(crbug.com/346835898): Support SocketTag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kHappyEyeballsV3);

  SpdySessionDependencies session_deps;
  auto socket_factory = std::make_unique<MockTaggingClientSocketFactory>();
  auto* socket_factory_ptr = socket_factory.get();
  session_deps.socket_factory = std::move(socket_factory);

  // Prepare for two HTTPS connects.
  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::span_from_ref(mock_read),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);
  MockRead mock_read2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data2(base::span_from_ref(mock_read2),
                                   base::span<MockWrite>());
  socket_data2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data2);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  // Use cert for *.example.org
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  ssl_socket_data.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);
  SSLSocketDataProvider ssl_socket_data2(ASYNC, OK);
  // Use cert for *.example.org
  ssl_socket_data2.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  ssl_socket_data2.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data2);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Prepare two different tags and corresponding HttpRequestInfos.
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org");
  request_info1.load_flags = 0;
  request_info1.socket_tag = tag1;
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  SocketTag tag2(getuid(), 0x87654321);
  HttpRequestInfo request_info2 = request_info1;
  request_info2.socket_tag = tag2;
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  // Prepare another HttpRequestInfo with tag1 and a different host name.
  HttpRequestInfo request_info3 = request_info1;
  request_info3.url = GURL("https://foo.example.org");
  request_info3.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  // Verify one stream with one tag results in one session, group and
  // socket.
  StreamRequester requester1(session.get());
  requester1.RequestStreamAndWait(session->http_stream_factory(), request_info1,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester1.stream_done());
  EXPECT_FALSE(requester1.websocket_stream());
  ASSERT_TRUE(requester1.stream());

  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  // Verify socket tagged appropriately.
  MockTaggingStreamSocket* socket =
      socket_factory_ptr->GetLastProducedTCPSocket();
  EXPECT_TRUE(tag1 == socket->tag());
  EXPECT_TRUE(socket->tagged_before_connected());

  // Verify the socket tag on the first session can be changed.
  StreamRequester requester2(session.get());
  requester2.RequestStreamAndWait(session->http_stream_factory(), request_info2,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester2.stream_done());
  EXPECT_FALSE(requester2.websocket_stream());
  ASSERT_TRUE(requester2.stream());
  // Verify still have just one session.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  // Verify no new sockets created.
  EXPECT_EQ(socket, socket_factory_ptr->GetLastProducedTCPSocket());
  // Verify socket tag changed.
  EXPECT_TRUE(tag2 == socket->tag());
  EXPECT_FALSE(socket->tagged_before_connected());

  // Verify attempting to use the first stream fails because the session's
  // socket tag has since changed.
  TestCompletionCallback callback1;
  requester1.stream()->RegisterRequest(&request_info1);
  EXPECT_EQ(ERR_FAILED, requester1.stream()->InitializeStream(
                            /* can_send_early = */ false, DEFAULT_PRIORITY,
                            NetLogWithSource(), callback1.callback()));

  // Verify the socket tag can be changed, this time using an IP alias
  // (different host, same IP).
  StreamRequester requester3(session.get());
  requester3.RequestStreamAndWait(session->http_stream_factory(), request_info3,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester3.stream_done());
  EXPECT_FALSE(requester3.websocket_stream());
  ASSERT_TRUE(requester3.stream());
  // Verify still have just one session.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  // Verify no new sockets created.
  EXPECT_EQ(socket, socket_factory_ptr->GetLastProducedTCPSocket());
  // Verify socket tag changed.
  EXPECT_TRUE(tag1 == socket->tag());
  EXPECT_FALSE(socket->tagged_before_connected());

  // Initialize the third stream, thus marking the session active, so it cannot
  // have its socket tag changed.
  TestCompletionCallback callback3;
  requester3.stream()->RegisterRequest(&request_info3);
  EXPECT_EQ(OK, requester3.stream()->InitializeStream(
                    /* can_send_early = */ false, DEFAULT_PRIORITY,
                    NetLogWithSource(), callback3.callback()));

  // Verify a new session is created when a request with a different tag is
  // started.
  StreamRequester requester4(session.get());
  requester4.RequestStreamAndWait(session->http_stream_factory(), request_info2,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester4.stream_done());
  EXPECT_FALSE(requester4.websocket_stream());
  ASSERT_TRUE(requester4.stream());
  // Verify we now have two sessions.
  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(2,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  // Verify a new socket was created.
  MockTaggingStreamSocket* socket2 =
      socket_factory_ptr->GetLastProducedTCPSocket();
  EXPECT_NE(socket, socket2);
  // Verify tag set appropriately.
  EXPECT_TRUE(tag2 == socket2->tag());
  EXPECT_TRUE(socket2->tagged_before_connected());
  // Verify tag on original socket is unchanged.
  EXPECT_TRUE(tag1 == socket->tag());

  requester3.stream()->Close(/* not_reusable = */ true);
}

// Regression test for https://crbug.com/954503.
TEST_P(HttpStreamFactoryTest, ChangeSocketTagAvoidOverwrite) {
  // SocketTag is not supported yet for HappyEyeballsV3.
  // TODO(crbug.com/346835898): Support SocketTag.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kHappyEyeballsV3);

  SpdySessionDependencies session_deps;
  auto socket_factory = std::make_unique<MockTaggingClientSocketFactory>();
  auto* socket_factory_ptr = socket_factory.get();
  session_deps.socket_factory = std::move(socket_factory);

  // Prepare for two HTTPS connects.
  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::span_from_ref(mock_read),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);
  MockRead mock_read2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data2(base::span_from_ref(mock_read2),
                                   base::span<MockWrite>());
  socket_data2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data2);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  // Use cert for *.example.org
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  ssl_socket_data.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);
  SSLSocketDataProvider ssl_socket_data2(ASYNC, OK);
  // Use cert for *.example.org
  ssl_socket_data2.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  ssl_socket_data2.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data2);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Prepare three different tags and corresponding HttpRequestInfos.
  SocketTag tag1(SocketTag::UNSET_UID, 2);
  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://www.example.org");
  request_info1.load_flags = 0;
  request_info1.socket_tag = tag1;
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  SocketTag tag2(SocketTag::UNSET_UID, 1);
  HttpRequestInfo request_info2 = request_info1;
  request_info2.socket_tag = tag2;

  HttpRequestInfo request_info3 = request_info1;
  SocketTag tag3(SocketTag::UNSET_UID, 3);
  request_info3.socket_tag = tag3;

  // Prepare another HttpRequestInfo with tag3 and a different host name.
  HttpRequestInfo request_info4 = request_info1;
  request_info4.socket_tag = tag3;
  request_info4.url = GURL("https://foo.example.org");

  // Verify one stream with one tag results in one session, group and
  // socket.
  StreamRequester requester1(session.get());
  requester1.RequestStreamAndWait(session->http_stream_factory(), request_info1,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester1.stream_done());
  EXPECT_FALSE(requester1.websocket_stream());
  ASSERT_TRUE(requester1.stream());

  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  // Verify socket tagged appropriately.
  MockTaggingStreamSocket* socket =
      socket_factory_ptr->GetLastProducedTCPSocket();
  EXPECT_TRUE(tag1 == socket->tag());
  EXPECT_TRUE(socket->tagged_before_connected());

  // Initialize the first stream, thus marking the session active, so it cannot
  // have its socket tag changed and be reused for the second session.
  TestCompletionCallback callback1;
  requester1.stream()->RegisterRequest(&request_info1);
  EXPECT_EQ(OK, requester1.stream()->InitializeStream(
                    /* can_send_early = */ false, DEFAULT_PRIORITY,
                    NetLogWithSource(), callback1.callback()));

  // Create a second stream with a new tag.
  StreamRequester requester2(session.get());
  requester2.RequestStreamAndWait(session->http_stream_factory(), request_info2,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester2.stream_done());
  EXPECT_FALSE(requester2.websocket_stream());
  ASSERT_TRUE(requester2.stream());
  // Verify we now have two sessions.
  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(2,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  // Verify a new socket was created.
  MockTaggingStreamSocket* socket2 =
      socket_factory_ptr->GetLastProducedTCPSocket();
  EXPECT_NE(socket, socket2);
  // Verify tag set appropriately.
  EXPECT_TRUE(tag2 == socket2->tag());
  EXPECT_TRUE(socket2->tagged_before_connected());
  // Verify tag on original socket is unchanged.
  EXPECT_TRUE(tag1 == socket->tag());

  // Initialize the second stream, thus marking the session active, so it cannot
  // have its socket tag changed and be reused for the third session.
  TestCompletionCallback callback2;
  requester2.stream()->RegisterRequest(&request_info2);
  EXPECT_EQ(OK, requester2.stream()->InitializeStream(
                    /* can_send_early = */ false, DEFAULT_PRIORITY,
                    NetLogWithSource(), callback2.callback()));

  // Release first stream so first session can be retagged for third request.
  requester1.stream()->Close(/* not_reusable = */ true);

  // Verify the first session can be retagged for a third request.
  StreamRequester requester3(session.get());
  requester3.RequestStreamAndWait(session->http_stream_factory(), request_info3,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester3.stream_done());
  EXPECT_FALSE(requester3.websocket_stream());
  ASSERT_TRUE(requester3.stream());
  // Verify still have two sessions.
  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(2,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  // Verify no new sockets created.
  EXPECT_EQ(socket2, socket_factory_ptr->GetLastProducedTCPSocket());
  // Verify socket tag changed.
  EXPECT_TRUE(tag3 == socket->tag());
  EXPECT_FALSE(socket->tagged_before_connected());

  // Release second stream so second session can be retagged for fourth request.
  requester2.stream()->Close(/* not_reusable = */ true);

  // Request a stream with a new tag and a different host that aliases existing
  // sessions.
  StreamRequester requester4(session.get());
  requester4.RequestStreamAndWait(session->http_stream_factory(), request_info4,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester4.stream_done());
  EXPECT_FALSE(requester4.websocket_stream());
  ASSERT_TRUE(requester4.stream());
  // Verify no new sockets created.
  EXPECT_EQ(socket2, socket_factory_ptr->GetLastProducedTCPSocket());
}

#endif  // BUILDFLAG(IS_ANDROID)

// Test that when creating a stream all sessions that alias an IP are tried,
// not just one.  This is important because there can be multiple sessions
// that could satisfy a stream request and they should all be tried.
TEST_P(HttpStreamFactoryTest, MultiIPAliases) {
  SpdySessionDependencies session_deps;

  // Prepare for two HTTPS connects.
  MockRead mock_read1(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data1(base::span_from_ref(mock_read1),
                                   base::span<MockWrite>());
  socket_data1.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data1);
  MockRead mock_read2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data2(base::span_from_ref(mock_read2),
                                   base::span<MockWrite>());
  socket_data2.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data2);
  SSLSocketDataProvider ssl_socket_data1(ASYNC, OK);
  // Load cert for *.example.org
  ssl_socket_data1.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  ssl_socket_data1.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data1);
  SSLSocketDataProvider ssl_socket_data2(ASYNC, OK);
  // Load cert for *.example.org
  ssl_socket_data2.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  ssl_socket_data2.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data2);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Create two HttpRequestInfos, differing only in host name.
  // Both will resolve to 127.0.0.1 and hence be IP aliases.
  HttpRequestInfo request_info1;
  request_info1.method = "GET";
  request_info1.url = GURL("https://a.example.org");
  request_info1.privacy_mode = PRIVACY_MODE_DISABLED;
  request_info1.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  HttpRequestInfo request_info1_alias = request_info1;
  request_info1.url = GURL("https://b.example.org");

  // Create two more HttpRequestInfos but with different privacy_mode.
  HttpRequestInfo request_info2;
  request_info2.method = "GET";
  request_info2.url = GURL("https://a.example.org");
  request_info2.privacy_mode = PRIVACY_MODE_ENABLED;
  request_info2.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  HttpRequestInfo request_info2_alias = request_info2;
  request_info2.url = GURL("https://b.example.org");

  // Open one session.
  StreamRequester requester1(session.get());
  requester1.RequestStreamAndWait(session->http_stream_factory(), request_info1,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester1.stream_done());
  EXPECT_FALSE(requester1.websocket_stream());
  ASSERT_TRUE(requester1.stream());

  // Verify just one session created.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_EQ(1, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));

  // Open another session to same IP but with different privacy mode.
  StreamRequester requester2(session.get());
  requester2.RequestStreamAndWait(session->http_stream_factory(), request_info2,
                                  DEFAULT_PRIORITY, /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester2.stream_done());
  EXPECT_FALSE(requester2.websocket_stream());
  ASSERT_TRUE(requester2.stream());

  // Verify two sessions are now open.
  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  EXPECT_EQ(2, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_EQ(2, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));

  // Open a third session that IP aliases first session.
  StreamRequester requester3(session.get());
  requester3.RequestStreamAndWait(session->http_stream_factory(),
                                  request_info1_alias, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester3.stream_done());
  EXPECT_FALSE(requester3.websocket_stream());
  ASSERT_TRUE(requester3.stream());

  // Verify the session pool reused the first session and no new session is
  // created.  This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  // When HappyEyeballsV3 is enabled, we create separate groups based on the
  // destination, even when the underlying connections share the same session.
  int expected_group_count =
      base::FeatureList::IsEnabled(features::kHappyEyeballsV3) ? 3 : 2;
  EXPECT_EQ(
      expected_group_count,
      GetPoolGroupCount(session.get(), HttpNetworkSession::NORMAL_SOCKET_POOL,
                        ProxyChain::Direct()));
  EXPECT_EQ(2, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));

  // Open a fourth session that IP aliases the second session.
  StreamRequester requester4(session.get());
  requester4.RequestStreamAndWait(session->http_stream_factory(),
                                  request_info2_alias, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester4.stream_done());
  EXPECT_FALSE(requester4.websocket_stream());
  ASSERT_TRUE(requester4.stream());

  // Verify the session pool reused the second session.  This will fail unless
  // the session pool supports multiple sessions aliasing a single IP.
  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  expected_group_count =
      base::FeatureList::IsEnabled(features::kHappyEyeballsV3) ? 4 : 2;
  EXPECT_EQ(
      expected_group_count,
      GetPoolGroupCount(session.get(), HttpNetworkSession::NORMAL_SOCKET_POOL,
                        ProxyChain::Direct()));
  EXPECT_EQ(2, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
}

TEST_P(HttpStreamFactoryTest, SpdyIPPoolingWithDnsAliases) {
  SpdySessionDependencies session_deps;

  const std::set<std::string> kDnsAliasesA({"alias1", "alias2"});
  const std::set<std::string> kDnsAliasesB({"b.com", "b.org", "b.net"});
  const std::string kHostnameC("c.example.org");

  session_deps.host_resolver->rules()->AddIPLiteralRuleWithDnsAliases(
      "a.example.org", "127.0.0.1", kDnsAliasesA);
  session_deps.host_resolver->rules()->AddIPLiteralRuleWithDnsAliases(
      "b.example.org", "127.0.0.1", kDnsAliasesB);
  session_deps.host_resolver->rules()->AddIPLiteralRuleWithDnsAliases(
      "c.example.org", "127.0.0.1", /*dns_aliases=*/std::set<std::string>());

  // Prepare for an HTTPS connect.
  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::span_from_ref(mock_read),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);
  SSLSocketDataProvider ssl_socket_data(ASYNC, OK);
  // Load cert for *.example.org
  ssl_socket_data.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
  ssl_socket_data.next_proto = kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_socket_data);

  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Create three HttpRequestInfos, differing only in host name.
  // All three will resolve to 127.0.0.1 and hence be IP aliases.
  HttpRequestInfo request_info_a;
  request_info_a.method = "GET";
  request_info_a.url = GURL("https://a.example.org");
  request_info_a.privacy_mode = PRIVACY_MODE_DISABLED;
  request_info_a.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  HttpRequestInfo request_info_b = request_info_a;
  HttpRequestInfo request_info_c = request_info_a;
  request_info_b.url = GURL("https://b.example.org");
  request_info_c.url = GURL("https://c.example.org");

  // Open one session.
  StreamRequester requester1(session.get());
  requester1.RequestStreamAndWait(session->http_stream_factory(),
                                  request_info_a, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester1.stream_done());
  EXPECT_FALSE(requester1.websocket_stream());
  ASSERT_TRUE(requester1.stream());
  EXPECT_EQ(kDnsAliasesA, requester1.stream()->GetDnsAliases());

  // Verify just one session created.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1, GetPoolGroupCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
  EXPECT_EQ(1, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));

  // Open a session that IP aliases first session.
  StreamRequester requester2(session.get());
  requester2.RequestStreamAndWait(session->http_stream_factory(),
                                  request_info_b, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester2.stream_done());
  EXPECT_FALSE(requester2.websocket_stream());
  ASSERT_TRUE(requester2.stream());
  EXPECT_EQ(kDnsAliasesB, requester2.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  // When HappyEyeballsV3 is enabled, we create separate groups based on the
  // destination, even when the underlying connections share the same session.
  int expected_group_count =
      base::FeatureList::IsEnabled(features::kHappyEyeballsV3) ? 2 : 1;
  EXPECT_EQ(
      expected_group_count,
      GetPoolGroupCount(session.get(), HttpNetworkSession::NORMAL_SOCKET_POOL,
                        ProxyChain::Direct()));
  EXPECT_EQ(1, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));

  // Open another session that IP aliases the first session.
  StreamRequester requester3(session.get());
  requester3.RequestStreamAndWait(session->http_stream_factory(),
                                  request_info_c, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester3.stream_done());
  EXPECT_FALSE(requester3.websocket_stream());
  ASSERT_TRUE(requester3.stream());
  EXPECT_THAT(requester3.stream()->GetDnsAliases(), ElementsAre(kHostnameC));

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  expected_group_count =
      base::FeatureList::IsEnabled(features::kHappyEyeballsV3) ? 3 : 1;
  EXPECT_EQ(
      expected_group_count,
      GetPoolGroupCount(session.get(), HttpNetworkSession::NORMAL_SOCKET_POOL,
                        ProxyChain::Direct()));
  EXPECT_EQ(1, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));

  // Clear host resolver rules to ensure that cached values for DNS aliases
  // are used.
  session_deps.host_resolver->rules()->ClearRules();

  // Re-request the original resource using `request_info_a`, which had
  // non-default DNS aliases.
  StreamRequester requester4(session.get());
  requester4.RequestStreamAndWait(session->http_stream_factory(),
                                  request_info_a, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester4.stream_done());
  EXPECT_FALSE(requester4.websocket_stream());
  ASSERT_TRUE(requester4.stream());
  EXPECT_EQ(kDnsAliasesA, requester4.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  expected_group_count =
      base::FeatureList::IsEnabled(features::kHappyEyeballsV3) ? 3 : 1;
  EXPECT_EQ(
      expected_group_count,
      GetPoolGroupCount(session.get(), HttpNetworkSession::NORMAL_SOCKET_POOL,
                        ProxyChain::Direct()));
  EXPECT_EQ(1, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));

  // Re-request a resource using `request_info_b`, which had non-default DNS
  // aliases.
  StreamRequester requester5(session.get());
  requester5.RequestStreamAndWait(session->http_stream_factory(),
                                  request_info_b, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester5.stream_done());
  EXPECT_FALSE(requester5.websocket_stream());
  ASSERT_TRUE(requester5.stream());
  EXPECT_EQ(kDnsAliasesB, requester5.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  expected_group_count =
      base::FeatureList::IsEnabled(features::kHappyEyeballsV3) ? 3 : 1;
  EXPECT_EQ(
      expected_group_count,
      GetPoolGroupCount(session.get(), HttpNetworkSession::NORMAL_SOCKET_POOL,
                        ProxyChain::Direct()));
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));

  // Re-request a resource using `request_info_c`, which had only the default
  // DNS alias (the host name).
  StreamRequester requester6(session.get());
  requester6.RequestStreamAndWait(session->http_stream_factory(),
                                  request_info_c, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester6.stream_done());
  EXPECT_FALSE(requester6.websocket_stream());
  ASSERT_TRUE(requester6.stream());
  EXPECT_THAT(requester6.stream()->GetDnsAliases(), ElementsAre(kHostnameC));

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  expected_group_count =
      base::FeatureList::IsEnabled(features::kHappyEyeballsV3) ? 3 : 1;
  EXPECT_EQ(
      expected_group_count,
      GetPoolGroupCount(session.get(), HttpNetworkSession::NORMAL_SOCKET_POOL,
                        ProxyChain::Direct()));
  EXPECT_EQ(1, GetHandedOutCount(session.get(),
                                 HttpNetworkSession::NORMAL_SOCKET_POOL,
                                 ProxyChain::Direct()));
}

TEST_P(HttpStreamFactoryBidirectionalQuicTest, QuicIPPoolingWithDnsAliases) {
  const GURL kUrlA("https://a.example.org");
  const GURL kUrlB("https://b.example.org");
  const GURL kUrlC("https://c.example.org");
  const std::set<std::string> kDnsAliasesA({"alias1", "alias2"});
  const std::set<std::string> kDnsAliasesB({"b.com", "b.org", "b.net"});

  host_resolver()->rules()->AddIPLiteralRuleWithDnsAliases(
      kUrlA.host(), "127.0.0.1", kDnsAliasesA);
  host_resolver()->rules()->AddIPLiteralRuleWithDnsAliases(
      kUrlB.host(), "127.0.0.1", kDnsAliasesB);
  host_resolver()->rules()->AddIPLiteralRuleWithDnsAliases(
      kUrlC.host(), "127.0.0.1",
      /*dns_aliases=*/std::set<std::string>());

  // Prepare mock QUIC data for a first session establishment.
  MockQuicData mock_quic_data(version());
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_length;
  int packet_num = 1;
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(
      SYNCHRONOUS,
      client_packet_maker().MakeRequestHeadersPacket(
          packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
          /*fin=*/true, priority,
          client_packet_maker().GetRequestHeaders("GET", "https", "/"),
          &spdy_headers_frame_length));
  size_t spdy_response_headers_frame_length;
  mock_quic_data.AddRead(
      ASYNC, server_packet_maker().MakeResponseHeadersPacket(
                 1, GetNthClientInitiatedBidirectionalStreamId(0),
                 /*fin=*/true, server_packet_maker().GetResponseHeaders("200"),
                 &spdy_response_headers_frame_length));
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // No more read data.
  mock_quic_data.AddSocketDataToFactory(&socket_factory());

  // Add hanging data for http job.
  auto hanging_data = std::make_unique<StaticSocketDataProvider>();
  MockConnect hanging_connect(SYNCHRONOUS, ERR_IO_PENDING);
  hanging_data->set_connect_data(hanging_connect);
  socket_factory().AddSocketDataProvider(hanging_data.get());
  SSLSocketDataProvider ssl_data(ASYNC, OK);
  socket_factory().AddSSLSocketDataProvider(&ssl_data);

  // Set up QUIC as alternative_service.
  Initialize();
  AddQuicAlternativeService(url::SchemeHostPort(kUrlA), kUrlA.host());
  AddQuicAlternativeService(url::SchemeHostPort(kUrlB), kUrlB.host());
  AddQuicAlternativeService(url::SchemeHostPort(kUrlC), kUrlC.host());

  // Create three HttpRequestInfos, differing only in host name.
  // All three will resolve to 127.0.0.1 and hence be IP aliases.
  HttpRequestInfo request_info_a;
  request_info_a.method = "GET";
  request_info_a.url = kUrlA;
  request_info_a.privacy_mode = PRIVACY_MODE_DISABLED;
  request_info_a.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  HttpRequestInfo request_info_b = request_info_a;
  HttpRequestInfo request_info_c = request_info_a;
  request_info_b.url = kUrlB;
  request_info_c.url = kUrlC;

  // Open one session.
  StreamRequester requester1(session());
  requester1.RequestStreamAndWait(session()->http_stream_factory(),
                                  request_info_a, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester1.stream_done());
  EXPECT_FALSE(requester1.websocket_stream());
  ASSERT_TRUE(requester1.stream());
  EXPECT_EQ(kDnsAliasesA, requester1.stream()->GetDnsAliases());

  // Verify just one session created.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, requester1.request()->negotiated_protocol());

  // Create a request that will alias and reuse the first session.
  StreamRequester requester2(session());
  requester2.RequestStreamAndWait(session()->http_stream_factory(),
                                  request_info_b, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester2.stream_done());
  EXPECT_FALSE(requester2.websocket_stream());
  ASSERT_TRUE(requester2.stream());
  EXPECT_EQ(kDnsAliasesB, requester2.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, requester2.request()->negotiated_protocol());

  // Create another request that will alias and reuse the first session.
  StreamRequester requester3(session());
  requester3.RequestStreamAndWait(session()->http_stream_factory(),
                                  request_info_c, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester3.stream_done());
  EXPECT_FALSE(requester3.websocket_stream());
  ASSERT_TRUE(requester3.stream());
  EXPECT_THAT(requester3.stream()->GetDnsAliases(), ElementsAre(kUrlC.host()));

  // Clear the host resolve rules to ensure that we are using cached info.
  host_resolver()->rules()->ClearRules();

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, requester3.request()->negotiated_protocol());

  // Create a request that will reuse the first session.
  StreamRequester requester4(session());
  requester4.RequestStreamAndWait(session()->http_stream_factory(),
                                  request_info_a, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester4.stream_done());
  EXPECT_FALSE(requester4.websocket_stream());
  ASSERT_TRUE(requester4.stream());
  EXPECT_EQ(kDnsAliasesA, requester4.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, requester4.request()->negotiated_protocol());

  // Create another request that will alias and reuse the first session.
  StreamRequester requester5(session());
  requester5.RequestStreamAndWait(session()->http_stream_factory(),
                                  request_info_b, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester5.stream_done());
  EXPECT_FALSE(requester5.websocket_stream());
  ASSERT_TRUE(requester5.stream());
  EXPECT_EQ(kDnsAliasesB, requester5.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, requester5.request()->negotiated_protocol());

  // Create another request that will alias and reuse the first session.
  StreamRequester requester6(session());
  requester6.RequestStreamAndWait(session()->http_stream_factory(),
                                  request_info_c, DEFAULT_PRIORITY,
                                  /*allowed_bad_certs=*/{},
                                  /*enable_ip_based_pooling=*/true,
                                  /*enable_alternative_services=*/true);
  EXPECT_TRUE(requester6.stream_done());
  EXPECT_FALSE(requester6.websocket_stream());
  ASSERT_TRUE(requester6.stream());
  EXPECT_THAT(requester6.stream()->GetDnsAliases(), ElementsAre(kUrlC.host()));

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, requester6.request()->negotiated_protocol());
}

class ProcessAlternativeServicesTest : public TestWithTaskEnvironment {
 public:
  ProcessAlternativeServicesTest() {
    session_params_.enable_quic = true;

    session_context_.proxy_resolution_service = proxy_resolution_service_.get();
    session_context_.host_resolver = &host_resolver_;
    session_context_.cert_verifier = &cert_verifier_;
    session_context_.transport_security_state = &transport_security_state_;
    session_context_.client_socket_factory = &socket_factory_;
    session_context_.ssl_config_service = &ssl_config_service_;
    session_context_.http_user_agent_settings = &http_user_agent_settings_;
    session_context_.http_server_properties = &http_server_properties_;
    session_context_.quic_context = &quic_context_;
  }

 private:
  // Parameters passed in the NetworkSessionContext must outlive the
  // HttpNetworkSession.
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateDirect();
  SSLConfigServiceDefaults ssl_config_service_;
  StaticHttpUserAgentSettings http_user_agent_settings_ = {"*", "test-ua"};
  MockClientSocketFactory socket_factory_;
  MockHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;

 protected:
  HttpServerProperties http_server_properties_;
  QuicContext quic_context_;
  HttpNetworkSessionParams session_params_;
  HttpNetworkSessionContext session_context_;
  std::unique_ptr<HttpNetworkSession> session_;
};

TEST_F(ProcessAlternativeServicesTest, ProcessEmptyAltSvc) {
  session_ =
      std::make_unique<HttpNetworkSession>(session_params_, session_context_);
  url::SchemeHostPort origin;
  NetworkAnonymizationKey network_anonymization_key;

  auto headers = base::MakeRefCounted<HttpResponseHeaders>("");

  session_->http_stream_factory()->ProcessAlternativeServices(
      session_.get(), network_anonymization_key, headers.get(), origin);

  AlternativeServiceInfoVector alternatives =
      http_server_properties_.GetAlternativeServiceInfos(
          origin, network_anonymization_key);
  EXPECT_TRUE(alternatives.empty());
}

TEST_F(ProcessAlternativeServicesTest, ProcessAltSvcClear) {
  session_ =
      std::make_unique<HttpNetworkSession>(session_params_, session_context_);
  url::SchemeHostPort origin(url::kHttpsScheme, "example.com", 443);

  auto network_anonymization_key = NetworkAnonymizationKey::CreateSameSite(
      SchemefulSite(GURL("https://example.com")));

  http_server_properties_.SetAlternativeServices(
      origin, network_anonymization_key,
      {AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          {kProtoQUIC, "", 443}, base::Time::Now() + base::Seconds(30),
          quic::AllSupportedVersions())});

  EXPECT_FALSE(
      http_server_properties_
          .GetAlternativeServiceInfos(origin, network_anonymization_key)
          .empty());

  auto headers = base::MakeRefCounted<HttpResponseHeaders>("");
  headers->AddHeader("alt-svc", "clear");

  session_->http_stream_factory()->ProcessAlternativeServices(
      session_.get(), network_anonymization_key, headers.get(), origin);

  AlternativeServiceInfoVector alternatives =
      http_server_properties_.GetAlternativeServiceInfos(
          origin, network_anonymization_key);
  EXPECT_TRUE(alternatives.empty());
}

TEST_F(ProcessAlternativeServicesTest, ProcessAltSvcQuicIetf) {
  quic_context_.params()->supported_versions = quic::AllSupportedVersions();
  session_ =
      std::make_unique<HttpNetworkSession>(session_params_, session_context_);
  url::SchemeHostPort origin(url::kHttpsScheme, "example.com", 443);

  auto network_anonymization_key = NetworkAnonymizationKey::CreateSameSite(
      SchemefulSite(GURL("https://example.com")));

  auto headers = base::MakeRefCounted<HttpResponseHeaders>("");
  headers->AddHeader("alt-svc",
                     "h3-29=\":443\","
                     "h3-Q050=\":443\","
                     "h3-Q043=\":443\"");

  session_->http_stream_factory()->ProcessAlternativeServices(
      session_.get(), network_anonymization_key, headers.get(), origin);

  quic::ParsedQuicVersionVector versions = {
      quic::ParsedQuicVersion::Draft29(),
  };
  AlternativeServiceInfoVector alternatives =
      http_server_properties_.GetAlternativeServiceInfos(
          origin, network_anonymization_key);
  ASSERT_EQ(versions.size(), alternatives.size());
  for (size_t i = 0; i < alternatives.size(); ++i) {
    EXPECT_EQ(kProtoQUIC, alternatives[i].protocol());
    EXPECT_EQ(HostPortPair("example.com", 443),
              alternatives[i].host_port_pair());
    EXPECT_EQ(1u, alternatives[i].advertised_versions().size());
    EXPECT_EQ(versions[i], alternatives[i].advertised_versions()[0]);
  }
}

TEST_F(ProcessAlternativeServicesTest, ProcessAltSvcHttp2) {
  quic_context_.params()->supported_versions = quic::AllSupportedVersions();
  session_ =
      std::make_unique<HttpNetworkSession>(session_params_, session_context_);
  url::SchemeHostPort origin(url::kHttpsScheme, "example.com", 443);

  auto network_anonymization_key = NetworkAnonymizationKey::CreateSameSite(
      SchemefulSite(GURL("https://example.com")));

  auto headers = base::MakeRefCounted<HttpResponseHeaders>("");
  headers->AddHeader("alt-svc", "h2=\"other.example.com:443\"");

  session_->http_stream_factory()->ProcessAlternativeServices(
      session_.get(), network_anonymization_key, headers.get(), origin);

  AlternativeServiceInfoVector alternatives =
      http_server_properties_.GetAlternativeServiceInfos(
          origin, network_anonymization_key);
  ASSERT_EQ(1u, alternatives.size());
  EXPECT_EQ(kProtoHTTP2, alternatives[0].protocol());
  EXPECT_EQ(HostPortPair("other.example.com", 443),
            alternatives[0].host_port_pair());
  EXPECT_EQ(0u, alternatives[0].advertised_versions().size());
}

}  // namespace

}  // namespace net::test
