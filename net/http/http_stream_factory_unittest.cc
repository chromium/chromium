// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
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
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
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
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
// This file can be included from net/http even though
// it is in net/websockets because it doesn't
// introduce any link dependency to net/websockets.
#include <optional>

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
  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override {}
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

class StreamRequestWaiter : public HttpStreamRequest::Delegate {
 public:
  StreamRequestWaiter() = default;

  StreamRequestWaiter(const StreamRequestWaiter&) = delete;
  StreamRequestWaiter& operator=(const StreamRequestWaiter&) = delete;

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

  void WaitForStream() {
    stream_done_ = false;
    loop_ = std::make_unique<base::RunLoop>();
    while (!stream_done_) {
      loop_->Run();
    }
    loop_.reset();
  }

  const ProxyInfo& used_proxy_info() const { return used_proxy_info_; }

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
  bool stream_done_ = false;
  std::unique_ptr<base::RunLoop> loop_;
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
    NOTREACHED();
    return nullptr;
  }
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp3Stream(
      std::unique_ptr<QuicChromiumClientSession::Handle> session,
      std::set<std::string> dns_aliases) override {
    NOTREACHED();
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

using HttpStreamFactoryTest = TestWithTaskEnvironment;

TEST_F(HttpStreamFactoryTest, PreconnectDirect) {
  for (const auto& test : kTests) {
    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateDirect());
    std::unique_ptr<HttpNetworkSession> session(
        SpdySessionDependencies::SpdyCreateSession(&session_deps));
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

TEST_F(HttpStreamFactoryTest, PreconnectHttpProxy) {
  for (const auto& test : kTests) {
    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateFixedForTest(
            "http_proxy", TRAFFIC_ANNOTATION_FOR_TESTS));
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

TEST_F(HttpStreamFactoryTest, PreconnectSocksProxy) {
  for (const auto& test : kTests) {
    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateFixedForTest(
            "socks4://socks_proxy:1080", TRAFFIC_ANNOTATION_FOR_TESTS));
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

TEST_F(HttpStreamFactoryTest, PreconnectDirectWithExistingSpdySession) {
  for (const auto& test : kTests) {
    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateDirect());
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

// Verify that preconnects to unsafe ports are cancelled before they reach
// the SocketPool.
TEST_F(HttpStreamFactoryTest, PreconnectUnsafePort) {
  ASSERT_FALSE(IsPortAllowedForScheme(7, "http"));

  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
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

  PreconnectHelperForURL(1, GURL("http://www.google.com:7"),
                         NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                         session.get());
  EXPECT_EQ(-1, transport_conn_pool->last_num_streams());
}

// Verify that preconnects use the specified NetworkAnonymizationKey.
TEST_F(HttpStreamFactoryTest, PreconnectNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
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

  const GURL kURL("http://foo.test/");
  SchemefulSite kSiteFoo(GURL("http://foo.test"));
  SchemefulSite kSiteBar(GURL("http://bar.test"));
  const auto kKey1 = NetworkAnonymizationKey::CreateSameSite(kSiteFoo);
  const auto kKey2 = NetworkAnonymizationKey::CreateSameSite(kSiteBar);
  PreconnectHelperForURL(1, kURL, kKey1, SecureDnsPolicy::kAllow,
                         session.get());
  EXPECT_EQ(1, transport_conn_pool->last_num_streams());
  EXPECT_EQ(kKey1,
            transport_conn_pool->last_group_id().network_anonymization_key());

  PreconnectHelperForURL(2, kURL, kKey2, SecureDnsPolicy::kAllow,
                         session.get());
  EXPECT_EQ(2, transport_conn_pool->last_num_streams());
  EXPECT_EQ(kKey2,
            transport_conn_pool->last_group_id().network_anonymization_key());
}

// Verify that preconnects use the specified Secure DNS Tag.
TEST_F(HttpStreamFactoryTest, PreconnectDisableSecureDns) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  std::unique_ptr<HttpNetworkSession> session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));
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

  const GURL kURL("http://foo.test/");
  SchemefulSite kSiteFoo(GURL("http://foo.test"));
  SchemefulSite kSiteBar(GURL("http://bar.test"));
  PreconnectHelperForURL(1, kURL, NetworkAnonymizationKey(),
                         SecureDnsPolicy::kAllow, session.get());
  EXPECT_EQ(1, transport_conn_pool->last_num_streams());
  EXPECT_EQ(SecureDnsPolicy::kAllow,
            transport_conn_pool->last_group_id().secure_dns_policy());

  PreconnectHelperForURL(2, kURL, NetworkAnonymizationKey(),
                         SecureDnsPolicy::kDisable, session.get());
  EXPECT_EQ(2, transport_conn_pool->last_num_streams());
  EXPECT_EQ(SecureDnsPolicy::kDisable,
            transport_conn_pool->last_group_id().secure_dns_policy());
}

TEST_F(HttpStreamFactoryTest, JobNotifiesProxy) {
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();

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
TEST_F(HttpStreamFactoryTest, NoProxyFallbackOnTunnelFail) {
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();

  // The stream should have failed, since the proxy server failed to
  // establish a tunnel.
  ASSERT_THAT(waiter.error_status(), IsError(ERR_TUNNEL_CONNECTION_FAILED));

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
TEST_F(HttpStreamFactoryTest, QuicProxyMarkedAsBad) {
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
    session_context.proxy_resolution_service = proxy_resolution_service.get();
    session_context.ssl_config_service = &ssl_config_service;
    session_context.http_server_properties = &http_server_properties;
    session_context.quic_context = &quic_context;

    host_resolver.rules()->AddRule("www.google.com", "2.3.4.5");
    host_resolver.rules()->AddRule("bad", "1.2.3.4");

    auto session =
        std::make_unique<HttpNetworkSession>(session_params, session_context);
    session->quic_session_pool()->set_is_quic_known_to_work_on_current_network(
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

    StreamRequestWaiter waiter;
    std::unique_ptr<HttpStreamRequest> request(
        session->http_stream_factory()->RequestStream(
            request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
            &waiter, /* enable_ip_based_pooling = */ true,
            /* enable_alternative_services = */ true, NetLogWithSource()));
    waiter.WaitForStream();

    // The proxy that failed should now be known to the
    // proxy_resolution_service as bad.
    const ProxyRetryInfoMap& retry_info =
        session->proxy_resolution_service()->proxy_retry_info();
    EXPECT_EQ(1u, retry_info.size()) << quic_proxy_test_mock_error;
    EXPECT_TRUE(waiter.used_proxy_info().is_direct());

    auto iter = retry_info.find(quic_proxy_chain);
    EXPECT_TRUE(iter != retry_info.end()) << quic_proxy_test_mock_error;
  }
}

// BidirectionalStreamImpl::Delegate to wait until response headers are
// received.
class TestBidirectionalDelegate : public BidirectionalStreamImpl::Delegate {
 public:
  void WaitUntilDone() { loop_.Run(); }

  const spdy::Http2HeaderBlock& response_headers() const {
    return response_headers_;
  }

 private:
  void OnStreamReady(bool request_headers_sent) override {}
  void OnHeadersReceived(
      const spdy::Http2HeaderBlock& response_headers) override {
    response_headers_ = response_headers.Clone();
    loop_.Quit();
  }
  void OnDataRead(int bytes_read) override { NOTREACHED(); }
  void OnDataSent() override { NOTREACHED(); }
  void OnTrailersReceived(const spdy::Http2HeaderBlock& trailers) override {
    NOTREACHED();
  }
  void OnFailed(int error) override { NOTREACHED(); }
  base::RunLoop loop_;
  spdy::Http2HeaderBlock response_headers_;
};

// Helper class to encapsulate MockReads and MockWrites for QUIC.
// Simplify ownership issues and the interaction with the MockSocketFactory.
class MockQuicData {
 public:
  explicit MockQuicData(quic::ParsedQuicVersion version) : printer_(version) {}

  ~MockQuicData() = default;

  void AddRead(std::unique_ptr<quic::QuicEncryptedPacket> packet) {
    reads_.emplace_back(ASYNC, packet->data(), packet->length(),
                        packet_number_++);
    packets_.push_back(std::move(packet));
  }

  void AddRead(IoMode mode, int rv) {
    reads_.emplace_back(mode, rv, packet_number_++);
  }

  void AddWrite(std::unique_ptr<quic::QuicEncryptedPacket> packet) {
    writes_.emplace_back(SYNCHRONOUS, packet->data(), packet->length(),
                         packet_number_++);
    packets_.push_back(std::move(packet));
  }

  void AddSocketDataToFactory(MockClientSocketFactory* factory) {
    socket_data_ = std::make_unique<SequencedSocketData>(reads_, writes_);
    socket_data_->set_printer(&printer_);
    factory->AddSocketDataProvider(socket_data_.get());
  }

 private:
  std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> packets_;
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  size_t packet_number_ = 0;
  QuicPacketPrinter printer_;
  std::unique_ptr<SequencedSocketData> socket_data_;
};

}  // namespace

TEST_F(HttpStreamFactoryTest, UsePreConnectIfNoZeroRTT) {
  for (int num_streams = 1; num_streams < 3; ++num_streams) {
    GURL url = GURL("https://www.google.com");

    SpdySessionDependencies session_deps(
        ConfiguredProxyResolutionService::CreateFixedForTest(
            "http_proxy", TRAFFIC_ANNOTATION_FOR_TESTS));

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

// Return count of distinct QUIC sessions.
int GetQuicSessionCount(HttpNetworkSession* session) {
  base::Value dict(session->QuicInfoToValue());
  base::Value::List* session_list = dict.GetDict().FindList("sessions");
  if (!session_list) {
    return -1;
  }
  return session_list->size();
}

TEST_F(HttpStreamFactoryTest, PrivacyModeUsesDifferentSocketPoolGroup) {
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

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 0);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.privacy_mode = PRIVACY_MODE_DISABLED;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);

  StreamRequestWaiter waiter;

  std::unique_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 1);

  std::unique_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 1);

  request_info.privacy_mode = PRIVACY_MODE_ENABLED;
  std::unique_ptr<HttpStreamRequest> request3(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 2);
}

TEST_F(HttpStreamFactoryTest, DisableSecureDnsUsesDifferentSocketPoolGroup) {
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

  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 0);

  HttpRequestInfo request_info;
  request_info.method = "GET";
  request_info.url = GURL("https://www.google.com");
  request_info.load_flags = 0;
  request_info.privacy_mode = PRIVACY_MODE_DISABLED;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  request_info.secure_dns_policy = SecureDnsPolicy::kAllow;

  StreamRequestWaiter waiter;

  std::unique_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();

  EXPECT_EQ(SecureDnsPolicy::kAllow,
            session_deps.host_resolver->last_secure_dns_policy());
  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 1);

  std::unique_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();

  EXPECT_EQ(SecureDnsPolicy::kAllow,
            session_deps.host_resolver->last_secure_dns_policy());
  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 1);

  request_info.secure_dns_policy = SecureDnsPolicy::kDisable;
  std::unique_ptr<HttpStreamRequest> request3(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();

  EXPECT_EQ(SecureDnsPolicy::kDisable,
            session_deps.host_resolver->last_secure_dns_policy());
  EXPECT_EQ(GetSocketPoolGroupCount(ssl_pool), 2);
}

TEST_F(HttpStreamFactoryTest, GetLoadState) {
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));

  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, request->GetLoadState());

  waiter.WaitForStream();
}

TEST_F(HttpStreamFactoryTest, RequestHttpStream) {
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(nullptr != waiter.stream());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());

  EXPECT_EQ(0, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

// Test the race of SetPriority versus stream completion where SetPriority may
// be called on an HttpStreamFactory::Job after the stream has been created by
// the job.
TEST_F(HttpStreamFactoryTest, ReprioritizeAfterStreamReceived) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());
  session_deps.host_resolver->set_synchronous_mode(true);

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider socket_data(base::make_span(&mock_read, 1u),
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

  StreamRequestWaiter waiter;
  EXPECT_EQ(0, GetSpdySessionCount(session.get()));
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, LOWEST, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  EXPECT_FALSE(waiter.stream_done());

  // Confirm a stream has been created by asserting that a new session
  // has been created.  (The stream is only created at the SPDY level on
  // first write, which happens after the request has returned a stream).
  ASSERT_EQ(1, GetSpdySessionCount(session.get()));

  // Test to confirm that a SetPriority received after the stream is created
  // but before the request returns it does not crash.
  request->SetPriority(HIGHEST);

  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(waiter.stream());
  EXPECT_FALSE(waiter.websocket_stream());
}

TEST_F(HttpStreamFactoryTest, RequestHttpStreamOverSSL) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(base::make_span(&mock_read, 1u),
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(nullptr != waiter.stream());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());

  EXPECT_EQ(0, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST_F(HttpStreamFactoryTest, RequestHttpStreamOverProxy) {
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_TRUE(nullptr != waiter.stream());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());

  EXPECT_EQ(0, GetSpdySessionCount(session.get()));
  EXPECT_EQ(0,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1, GetSocketPoolGroupCount(session->GetSocketPool(
                   HttpNetworkSession::NORMAL_SOCKET_POOL,
                   ProxyChain(ProxyServer::SCHEME_HTTP,
                              HostPortPair("myproxy", 8888)))));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPool(
                   HttpNetworkSession::NORMAL_SOCKET_POOL,
                   ProxyChain(ProxyServer::SCHEME_HTTPS,
                              HostPortPair("myproxy", 8888)))));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPool(
                   HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
                   ProxyChain(ProxyServer::SCHEME_HTTP,
                              HostPortPair("myproxy", 8888)))));
  EXPECT_FALSE(waiter.used_proxy_info().is_direct());
}

TEST_F(HttpStreamFactoryTest, RequestWebSocketBasicHandshakeStream) {
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

  StreamRequestWaiter waiter;
  WebSocketStreamCreateHelper create_helper;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestWebSocketHandshakeStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          &create_helper, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.stream());
  ASSERT_TRUE(nullptr != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            waiter.websocket_stream()->type());
  EXPECT_EQ(0,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST_F(HttpStreamFactoryTest, RequestWebSocketBasicHandshakeStreamOverSSL) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  StaticSocketDataProvider socket_data(base::make_span(&mock_read, 1u),
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

  StreamRequestWaiter waiter;
  WebSocketStreamCreateHelper create_helper;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestWebSocketHandshakeStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          &create_helper, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.stream());
  ASSERT_TRUE(nullptr != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            waiter.websocket_stream()->type());
  EXPECT_EQ(0,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST_F(HttpStreamFactoryTest, RequestWebSocketBasicHandshakeStreamOverProxy) {
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

  StreamRequestWaiter waiter;
  WebSocketStreamCreateHelper create_helper;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestWebSocketHandshakeStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          &create_helper, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.stream());
  ASSERT_TRUE(nullptr != waiter.websocket_stream());
  EXPECT_EQ(MockWebSocketHandshakeStream::kStreamTypeBasic,
            waiter.websocket_stream()->type());
  EXPECT_EQ(
      0, GetSocketPoolGroupCount(session->GetSocketPool(
             HttpNetworkSession::WEBSOCKET_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(0, GetSocketPoolGroupCount(session->GetSocketPool(
                   HttpNetworkSession::NORMAL_SOCKET_POOL,
                   ProxyChain(ProxyServer::SCHEME_HTTP,
                              HostPortPair("myproxy", 8888)))));
  EXPECT_EQ(1, GetSocketPoolGroupCount(session->GetSocketPool(
                   HttpNetworkSession::WEBSOCKET_SOCKET_POOL,
                   ProxyChain(ProxyServer::SCHEME_HTTP,
                              HostPortPair("myproxy", 8888)))));
  EXPECT_FALSE(waiter.used_proxy_info().is_direct());
}

TEST_F(HttpStreamFactoryTest, RequestSpdyHttpStreamHttpsURL) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::make_span(&mock_read, 1u),
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());
  ASSERT_TRUE(nullptr != waiter.stream());

  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST_F(HttpStreamFactoryTest, RequestSpdyHttpStreamHttpURL) {
  url::SchemeHostPort scheme_host_port("http", "myproxy.org", 443);
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::make_span(&mock_read, 1u),
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());
  ASSERT_TRUE(nullptr != waiter.stream());

  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(0,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_FALSE(waiter.used_proxy_info().is_direct());
  EXPECT_TRUE(http_server_properties->GetSupportsSpdy(
      scheme_host_port, NetworkAnonymizationKey()));
}

// Same as above, but checks HttpServerProperties is updated using the correct
// NetworkAnonymizationKey. When/if NetworkAnonymizationKey is enabled by
// default, this should probably be merged into the above test.
TEST_F(HttpStreamFactoryTest,
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
      features::kPartitionHttpServerPropertiesByNetworkIsolationKey);

  url::SchemeHostPort scheme_host_port("http", "myproxy.org", 443);
  auto session_deps = std::make_unique<SpdySessionDependencies>(
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
          "HTTPS myproxy.org:443", TRAFFIC_ANNOTATION_FOR_TESTS);

  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::make_span(&mock_read, 1u),
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_TRUE(nullptr == waiter.websocket_stream());
  ASSERT_TRUE(nullptr != waiter.stream());

  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(0,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_FALSE(waiter.used_proxy_info().is_direct());
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
TEST_F(HttpStreamFactoryTest, NewSpdySessionCloseIdleH2Sockets) {
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

  StreamRequestWaiter waiter1;
  StreamRequestWaiter waiter2;
  std::unique_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  std::unique_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_TRUE(waiter2.stream_done());
  ASSERT_NE(nullptr, waiter1.stream());
  ASSERT_NE(nullptr, waiter2.stream());
  ASSERT_NE(waiter1.stream(), waiter2.stream());

  // Establishing the SpdySession will close idle H2 sockets.
  EXPECT_EQ(0, session
                   ->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                   ProxyChain::Direct())
                   ->IdleSocketCount());
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
}

// Regression test for https://crbug.com/706974.
TEST_F(HttpStreamFactoryTest, TwoSpdyConnects) {
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
  StreamRequestWaiter waiter1;
  std::unique_ptr<HttpStreamRequest> request1 =
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource());

  StreamRequestWaiter waiter2;
  std::unique_ptr<HttpStreamRequest> request2 =
      session->http_stream_factory()->RequestStream(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource());

  waiter1.WaitForStream();
  waiter2.WaitForStream();

  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_TRUE(waiter2.stream_done());
  ASSERT_NE(nullptr, waiter1.stream());
  ASSERT_NE(nullptr, waiter2.stream());
  ASSERT_NE(waiter1.stream(), waiter2.stream());

  // Establishing the SpdySession will close the extra H2 socket.
  EXPECT_EQ(0, session
                   ->GetSocketPool(HttpNetworkSession::NORMAL_SOCKET_POOL,
                                   ProxyChain::Direct())
                   ->IdleSocketCount());
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_TRUE(data0.AllReadDataConsumed());
  EXPECT_TRUE(data1.AllReadDataConsumed());
}

TEST_F(HttpStreamFactoryTest, RequestBidirectionalStreamImpl) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  SequencedSocketData socket_data(base::make_span(&mock_read, 1u),
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestBidirectionalStreamImpl(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_FALSE(waiter.websocket_stream());
  ASSERT_FALSE(waiter.stream());
  ASSERT_TRUE(waiter.bidirectional_stream_impl());
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
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
    session_->quic_session_pool()->set_is_quic_known_to_work_on_current_network(
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
  MockQuicData mock_quic_data(version());
  // Set priority to default value so that
  // QuicTestPacketMaker::MakeRequestHeadersPacket() does not add mock
  // PRIORITY_UPDATE frame, which BidirectionalStreamQuicImpl currently does not
  // send.
  // TODO(https://crbug.com/1059250): Implement PRIORITY_UPDATE in
  // BidirectionalStreamQuicImpl.
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_length;
  int packet_num = 1;
  mock_quic_data.AddWrite(
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(client_packet_maker().MakeRequestHeadersPacket(
      packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
      /*fin=*/true, priority,
      client_packet_maker().GetRequestHeaders("GET", "https", "/"),
      &spdy_headers_frame_length));
  size_t spdy_response_headers_frame_length;
  mock_quic_data.AddRead(server_packet_maker().MakeResponseHeadersPacket(
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session()->http_stream_factory()->RequestBidirectionalStreamImpl(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));

  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_FALSE(waiter.websocket_stream());
  ASSERT_FALSE(waiter.stream());
  ASSERT_TRUE(waiter.bidirectional_stream_impl());
  BidirectionalStreamImpl* stream_impl = waiter.bidirectional_stream_impl();

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

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  EXPECT_THAT(stream_impl->ReadData(buffer.get(), 1), IsOk());
  EXPECT_EQ(kProtoQUIC, stream_impl->GetProtocol());
  EXPECT_EQ("200", delegate.response_headers().find(":status")->second);
  EXPECT_EQ(0,
            GetSocketPoolGroupCount(session()->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

// Tests that if Http job fails, but Quic job succeeds, we return
// BidirectionalStreamQuicImpl.
TEST_P(HttpStreamFactoryBidirectionalQuicTest,
       RequestBidirectionalStreamImplHttpJobFailsQuicJobSucceeds) {
  // Set up Quic data.
  MockQuicData mock_quic_data(version());
  // Set priority to default value so that
  // QuicTestPacketMaker::MakeRequestHeadersPacket() does not add mock
  // PRIORITY_UPDATE frame, which BidirectionalStreamQuicImpl currently does not
  // send.
  // TODO(https://crbug.com/1059250): Implement PRIORITY_UPDATE in
  // BidirectionalStreamQuicImpl.
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_length;
  int packet_num = 1;
  mock_quic_data.AddWrite(
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(client_packet_maker().MakeRequestHeadersPacket(
      packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
      /*fin=*/true, priority,
      client_packet_maker().GetRequestHeaders("GET", "https", "/"),
      &spdy_headers_frame_length));
  size_t spdy_response_headers_frame_length;
  mock_quic_data.AddRead(server_packet_maker().MakeResponseHeadersPacket(
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session()->http_stream_factory()->RequestBidirectionalStreamImpl(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));

  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  EXPECT_FALSE(waiter.websocket_stream());
  ASSERT_FALSE(waiter.stream());
  ASSERT_TRUE(waiter.bidirectional_stream_impl());
  BidirectionalStreamImpl* stream_impl = waiter.bidirectional_stream_impl();

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
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(1);
  EXPECT_THAT(stream_impl->ReadData(buffer.get(), 1), IsOk());
  EXPECT_EQ(kProtoQUIC, stream_impl->GetProtocol());
  EXPECT_EQ("200", delegate.response_headers().find(":status")->second);
  // There is no Http2 socket pool.
  EXPECT_EQ(0,
            GetSocketPoolGroupCount(session()->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_TRUE(waiter.used_proxy_info().is_direct());
}

TEST_F(HttpStreamFactoryTest, RequestBidirectionalStreamImplFailure) {
  SpdySessionDependencies session_deps(
      ConfiguredProxyResolutionService::CreateDirect());

  MockRead mock_read(ASYNC, OK);
  SequencedSocketData socket_data(base::make_span(&mock_read, 1u),
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

  StreamRequestWaiter waiter;
  std::unique_ptr<HttpStreamRequest> request(
      session->http_stream_factory()->RequestBidirectionalStreamImpl(
          request_info, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {}, &waiter,
          /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter.WaitForStream();
  EXPECT_TRUE(waiter.stream_done());
  ASSERT_THAT(waiter.error_status(), IsError(ERR_FAILED));
  EXPECT_FALSE(waiter.websocket_stream());
  ASSERT_FALSE(waiter.stream());
  ASSERT_FALSE(waiter.bidirectional_stream_impl());
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
}

#if BUILDFLAG(IS_ANDROID)
// Verify HttpStreamFactory::Job passes socket tag along properly and that
// SpdySessions have unique socket tags (e.g. one sessions should not be shared
// amongst streams with different socket tags).
TEST_F(HttpStreamFactoryTest, Tag) {
  SpdySessionDependencies session_deps;
  auto socket_factory = std::make_unique<MockTaggingClientSocketFactory>();
  auto* socket_factory_ptr = socket_factory.get();
  session_deps.socket_factory = std::move(socket_factory);

  // Prepare for two HTTPS connects.
  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::make_span(&mock_read, 1u),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);
  MockRead mock_read2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data2(base::make_span(&mock_read2, 1u),
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
  StreamRequestWaiter waiter1;
  std::unique_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info1, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_TRUE(nullptr == waiter1.websocket_stream());
  ASSERT_TRUE(nullptr != waiter1.stream());

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
  StreamRequestWaiter waiter2;
  std::unique_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info2, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  EXPECT_TRUE(nullptr == waiter2.websocket_stream());
  ASSERT_TRUE(nullptr != waiter2.stream());

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
  StreamRequestWaiter waiter3;
  std::unique_ptr<HttpStreamRequest> request3(
      session->http_stream_factory()->RequestStream(
          request_info2, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter3, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter3.WaitForStream();
  EXPECT_TRUE(waiter3.stream_done());
  EXPECT_TRUE(nullptr == waiter3.websocket_stream());
  ASSERT_TRUE(nullptr != waiter3.stream());

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
  // Prepare mock QUIC data for a first session establishment.
  MockQuicData mock_quic_data(version());
  spdy::SpdyPriority priority =
      ConvertRequestPriorityToQuicPriority(DEFAULT_PRIORITY);
  size_t spdy_headers_frame_length;
  int packet_num = 1;
  mock_quic_data.AddWrite(
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(client_packet_maker().MakeRequestHeadersPacket(
      packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
      /*fin=*/true, priority,
      client_packet_maker().GetRequestHeaders("GET", "https", "/"),
      &spdy_headers_frame_length));
  size_t spdy_response_headers_frame_length;
  mock_quic_data.AddRead(server_packet_maker().MakeResponseHeadersPacket(
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
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data2.AddWrite(client_packet_maker().MakeRequestHeadersPacket(
      packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
      /*fin=*/true, priority,
      client_packet_maker().GetRequestHeaders("GET", "https", "/"),
      &spdy_headers_frame_length));
  mock_quic_data2.AddRead(server_packet_maker().MakeResponseHeadersPacket(
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
  StreamRequestWaiter waiter1;
  std::unique_ptr<HttpStreamRequest> request1(
      session()->http_stream_factory()->RequestStream(
          request_info1, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_TRUE(nullptr == waiter1.websocket_stream());
  ASSERT_TRUE(nullptr != waiter1.stream());
  EXPECT_EQ(kProtoQUIC, request1->negotiated_protocol());
  EXPECT_EQ(1, GetQuicSessionCount(session()));

  // Verify socket tagged appropriately.
  EXPECT_TRUE(tag1 == socket_factory().GetLastProducedUDPSocket()->tag());
  EXPECT_TRUE(socket_factory()
                  .GetLastProducedUDPSocket()
                  ->tagged_before_data_transferred());

  // Verify one more stream with a different tag results in one more session and
  // socket.
  StreamRequestWaiter waiter2;
  std::unique_ptr<HttpStreamRequest> request2(
      session()->http_stream_factory()->RequestStream(
          request_info2, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  EXPECT_TRUE(nullptr == waiter2.websocket_stream());
  ASSERT_TRUE(nullptr != waiter2.stream());
  EXPECT_EQ(kProtoQUIC, request2->negotiated_protocol());
  EXPECT_EQ(2, GetQuicSessionCount(session()));

  // Verify socket tagged appropriately.
  EXPECT_TRUE(tag2 == socket_factory().GetLastProducedUDPSocket()->tag());
  EXPECT_TRUE(socket_factory()
                  .GetLastProducedUDPSocket()
                  ->tagged_before_data_transferred());

  // Verify one more stream reusing a tag does not create new sessions.
  StreamRequestWaiter waiter3;
  std::unique_ptr<HttpStreamRequest> request3(
      session()->http_stream_factory()->RequestStream(
          request_info2, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter3, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter3.WaitForStream();
  EXPECT_TRUE(waiter3.stream_done());
  EXPECT_TRUE(nullptr == waiter3.websocket_stream());
  ASSERT_TRUE(nullptr != waiter3.stream());
  EXPECT_EQ(kProtoQUIC, request3->negotiated_protocol());
  EXPECT_EQ(2, GetQuicSessionCount(session()));
}

TEST_F(HttpStreamFactoryTest, ChangeSocketTag) {
  SpdySessionDependencies session_deps;
  auto socket_factory = std::make_unique<MockTaggingClientSocketFactory>();
  auto* socket_factory_ptr = socket_factory.get();
  session_deps.socket_factory = std::move(socket_factory);

  // Prepare for two HTTPS connects.
  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::make_span(&mock_read, 1u),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);
  MockRead mock_read2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data2(base::make_span(&mock_read2, 1u),
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
  StreamRequestWaiter waiter1;
  std::unique_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info1, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_FALSE(waiter1.websocket_stream());
  ASSERT_TRUE(waiter1.stream());

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
  StreamRequestWaiter waiter2;
  std::unique_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info2, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  EXPECT_FALSE(waiter2.websocket_stream());
  ASSERT_TRUE(waiter2.stream());
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
  waiter1.stream()->RegisterRequest(&request_info1);
  EXPECT_EQ(ERR_FAILED, waiter1.stream()->InitializeStream(
                            /* can_send_early = */ false, DEFAULT_PRIORITY,
                            NetLogWithSource(), callback1.callback()));

  // Verify the socket tag can be changed, this time using an IP alias
  // (different host, same IP).
  StreamRequestWaiter waiter3;
  std::unique_ptr<HttpStreamRequest> request3(
      session->http_stream_factory()->RequestStream(
          request_info3, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter3, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter3.WaitForStream();
  EXPECT_TRUE(waiter3.stream_done());
  EXPECT_FALSE(waiter3.websocket_stream());
  ASSERT_TRUE(waiter3.stream());
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
  waiter3.stream()->RegisterRequest(&request_info3);
  EXPECT_EQ(OK, waiter3.stream()->InitializeStream(
                    /* can_send_early = */ false, DEFAULT_PRIORITY,
                    NetLogWithSource(), callback3.callback()));

  // Verify a new session is created when a request with a different tag is
  // started.
  StreamRequestWaiter waiter4;
  std::unique_ptr<HttpStreamRequest> request4(
      session->http_stream_factory()->RequestStream(
          request_info2, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter4, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter4.WaitForStream();
  EXPECT_TRUE(waiter4.stream_done());
  EXPECT_FALSE(waiter4.websocket_stream());
  ASSERT_TRUE(waiter4.stream());
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

  waiter3.stream()->Close(/* not_reusable = */ true);
}

// Regression test for https://crbug.com/954503.
TEST_F(HttpStreamFactoryTest, ChangeSocketTagAvoidOverwrite) {
  SpdySessionDependencies session_deps;
  auto socket_factory = std::make_unique<MockTaggingClientSocketFactory>();
  auto* socket_factory_ptr = socket_factory.get();
  session_deps.socket_factory = std::move(socket_factory);

  // Prepare for two HTTPS connects.
  MockRead mock_read(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data(base::make_span(&mock_read, 1u),
                                  base::span<MockWrite>());
  socket_data.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);
  MockRead mock_read2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data2(base::make_span(&mock_read2, 1u),
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
  StreamRequestWaiter waiter1;
  std::unique_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info1, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_FALSE(waiter1.websocket_stream());
  ASSERT_TRUE(waiter1.stream());

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
  waiter1.stream()->RegisterRequest(&request_info1);
  EXPECT_EQ(OK, waiter1.stream()->InitializeStream(
                    /* can_send_early = */ false, DEFAULT_PRIORITY,
                    NetLogWithSource(), callback1.callback()));

  // Create a second stream with a new tag.
  StreamRequestWaiter waiter2;
  std::unique_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info2, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  EXPECT_FALSE(waiter2.websocket_stream());
  ASSERT_TRUE(waiter2.stream());
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
  waiter2.stream()->RegisterRequest(&request_info2);
  EXPECT_EQ(OK, waiter2.stream()->InitializeStream(
                    /* can_send_early = */ false, DEFAULT_PRIORITY,
                    NetLogWithSource(), callback2.callback()));

  // Release first stream so first session can be retagged for third request.
  waiter1.stream()->Close(/* not_reusable = */ true);

  // Verify the first session can be retagged for a third request.
  StreamRequestWaiter waiter3;
  std::unique_ptr<HttpStreamRequest> request3(
      session->http_stream_factory()->RequestStream(
          request_info3, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter3, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter3.WaitForStream();
  EXPECT_TRUE(waiter3.stream_done());
  EXPECT_FALSE(waiter3.websocket_stream());
  ASSERT_TRUE(waiter3.stream());
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
  waiter2.stream()->Close(/* not_reusable = */ true);

  // Request a stream with a new tag and a different host that aliases existing
  // sessions.
  StreamRequestWaiter waiter4;
  std::unique_ptr<HttpStreamRequest> request4(
      session->http_stream_factory()->RequestStream(
          request_info4, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter4, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter4.WaitForStream();
  EXPECT_TRUE(waiter4.stream_done());
  EXPECT_FALSE(waiter4.websocket_stream());
  ASSERT_TRUE(waiter4.stream());
  // Verify no new sockets created.
  EXPECT_EQ(socket2, socket_factory_ptr->GetLastProducedTCPSocket());
}
#endif

// Test that when creating a stream all sessions that alias an IP are tried,
// not just one.  This is important because there can be multiple sessions
// that could satisfy a stream request and they should all be tried.
TEST_F(HttpStreamFactoryTest, MultiIPAliases) {
  SpdySessionDependencies session_deps;

  // Prepare for two HTTPS connects.
  MockRead mock_read1(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data1(base::make_span(&mock_read1, 1u),
                                   base::span<MockWrite>());
  socket_data1.set_connect_data(MockConnect(ASYNC, OK));
  session_deps.socket_factory->AddSocketDataProvider(&socket_data1);
  MockRead mock_read2(SYNCHRONOUS, ERR_IO_PENDING);
  SequencedSocketData socket_data2(base::make_span(&mock_read2, 1u),
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
  StreamRequestWaiter waiter1;
  std::unique_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info1, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_FALSE(waiter1.websocket_stream());
  ASSERT_TRUE(waiter1.stream());

  // Verify just one session created.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));

  // Open another session to same IP but with different privacy mode.
  StreamRequestWaiter waiter2;
  std::unique_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info2, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  EXPECT_FALSE(waiter2.websocket_stream());
  ASSERT_TRUE(waiter2.stream());

  // Verify two sessions are now open.
  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  EXPECT_EQ(2,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(2,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));

  // Open a third session that IP aliases first session.
  StreamRequestWaiter waiter3;
  std::unique_ptr<HttpStreamRequest> request3(
      session->http_stream_factory()->RequestStream(
          request_info1_alias, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter3, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter3.WaitForStream();
  EXPECT_TRUE(waiter3.stream_done());
  EXPECT_FALSE(waiter3.websocket_stream());
  ASSERT_TRUE(waiter3.stream());

  // Verify the session pool reused the first session and no new session is
  // created.  This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  EXPECT_EQ(2,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(2,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));

  // Open a fourth session that IP aliases the second session.
  StreamRequestWaiter waiter4;
  std::unique_ptr<HttpStreamRequest> request4(
      session->http_stream_factory()->RequestStream(
          request_info2_alias, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter4, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter4.WaitForStream();
  EXPECT_TRUE(waiter4.stream_done());
  EXPECT_FALSE(waiter4.websocket_stream());
  ASSERT_TRUE(waiter4.stream());

  // Verify the session pool reused the second session.  This will fail unless
  // the session pool supports multiple sessions aliasing a single IP.
  EXPECT_EQ(2, GetSpdySessionCount(session.get()));
  EXPECT_EQ(2,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(2,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
}

TEST_F(HttpStreamFactoryTest, SpdyIPPoolingWithDnsAliases) {
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
  SequencedSocketData socket_data(base::make_span(&mock_read, 1u),
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
  StreamRequestWaiter waiter1;
  std::unique_ptr<HttpStreamRequest> request1(
      session->http_stream_factory()->RequestStream(
          request_info_a, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_FALSE(waiter1.websocket_stream());
  ASSERT_TRUE(waiter1.stream());
  EXPECT_EQ(kDnsAliasesA, waiter1.stream()->GetDnsAliases());

  // Verify just one session created.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));

  // Open a session that IP aliases first session.
  StreamRequestWaiter waiter2;
  std::unique_ptr<HttpStreamRequest> request2(
      session->http_stream_factory()->RequestStream(
          request_info_b, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  EXPECT_FALSE(waiter2.websocket_stream());
  ASSERT_TRUE(waiter2.stream());
  EXPECT_EQ(kDnsAliasesB, waiter2.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));

  // Open another session that IP aliases the first session.
  StreamRequestWaiter waiter3;
  std::unique_ptr<HttpStreamRequest> request3(
      session->http_stream_factory()->RequestStream(
          request_info_c, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter3, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter3.WaitForStream();
  EXPECT_TRUE(waiter3.stream_done());
  EXPECT_FALSE(waiter3.websocket_stream());
  ASSERT_TRUE(waiter3.stream());
  EXPECT_THAT(waiter3.stream()->GetDnsAliases(), ElementsAre(kHostnameC));

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));

  // Clear host resolver rules to ensure that cached values for DNS aliases
  // are used.
  session_deps.host_resolver->rules()->ClearRules();

  // Re-request the original resource using `request_info_a`, which had
  // non-default DNS aliases.
  std::unique_ptr<HttpStreamRequest> request4(
      session->http_stream_factory()->RequestStream(
          request_info_a, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_FALSE(waiter1.websocket_stream());
  ASSERT_TRUE(waiter1.stream());
  EXPECT_EQ(kDnsAliasesA, waiter1.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));

  // Re-request a resource using `request_info_b`, which had non-default DNS
  // aliases.
  std::unique_ptr<HttpStreamRequest> request5(
      session->http_stream_factory()->RequestStream(
          request_info_b, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  EXPECT_FALSE(waiter2.websocket_stream());
  ASSERT_TRUE(waiter2.stream());
  EXPECT_EQ(kDnsAliasesB, waiter2.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));

  // Re-request a resource using `request_info_c`, which had only the default
  // DNS alias (the host name).
  std::unique_ptr<HttpStreamRequest> request6(
      session->http_stream_factory()->RequestStream(
          request_info_c, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter3, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter3.WaitForStream();
  EXPECT_TRUE(waiter3.stream_done());
  EXPECT_FALSE(waiter3.websocket_stream());
  ASSERT_TRUE(waiter3.stream());
  EXPECT_THAT(waiter3.stream()->GetDnsAliases(), ElementsAre(kHostnameC));

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetSpdySessionCount(session.get()));
  EXPECT_EQ(1,
            GetSocketPoolGroupCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
  EXPECT_EQ(1,
            GetHandedOutSocketCount(session->GetSocketPool(
                HttpNetworkSession::NORMAL_SOCKET_POOL, ProxyChain::Direct())));
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
      client_packet_maker().MakeInitialSettingsPacket(packet_num++));
  mock_quic_data.AddWrite(client_packet_maker().MakeRequestHeadersPacket(
      packet_num++, GetNthClientInitiatedBidirectionalStreamId(0),
      /*fin=*/true, priority,
      client_packet_maker().GetRequestHeaders("GET", "https", "/"),
      &spdy_headers_frame_length));
  size_t spdy_response_headers_frame_length;
  mock_quic_data.AddRead(server_packet_maker().MakeResponseHeadersPacket(
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
  StreamRequestWaiter waiter1;
  std::unique_ptr<HttpStreamRequest> request1(
      session()->http_stream_factory()->RequestStream(
          request_info_a, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_FALSE(waiter1.websocket_stream());
  ASSERT_TRUE(waiter1.stream());
  EXPECT_EQ(kDnsAliasesA, waiter1.stream()->GetDnsAliases());

  // Verify just one session created.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, request1->negotiated_protocol());

  // Create a request that will alias and reuse the first session.
  StreamRequestWaiter waiter2;
  std::unique_ptr<HttpStreamRequest> request2(
      session()->http_stream_factory()->RequestStream(
          request_info_b, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  EXPECT_FALSE(waiter2.websocket_stream());
  ASSERT_TRUE(waiter2.stream());
  EXPECT_EQ(kDnsAliasesB, waiter2.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, request2->negotiated_protocol());

  // Create another request that will alias and reuse the first session.
  StreamRequestWaiter waiter3;
  std::unique_ptr<HttpStreamRequest> request3(
      session()->http_stream_factory()->RequestStream(
          request_info_c, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter3, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter3.WaitForStream();
  EXPECT_TRUE(waiter3.stream_done());
  EXPECT_FALSE(waiter3.websocket_stream());
  ASSERT_TRUE(waiter3.stream());
  EXPECT_THAT(waiter3.stream()->GetDnsAliases(), ElementsAre(kUrlC.host()));

  // Clear the host resolve rules to ensure that we are using cached info.
  host_resolver()->rules()->ClearRules();

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, request3->negotiated_protocol());

  // Create a request that will reuse the first session.
  std::unique_ptr<HttpStreamRequest> request4(
      session()->http_stream_factory()->RequestStream(
          request_info_a, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter1, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter1.WaitForStream();
  EXPECT_TRUE(waiter1.stream_done());
  EXPECT_FALSE(waiter1.websocket_stream());
  ASSERT_TRUE(waiter1.stream());
  EXPECT_EQ(kDnsAliasesA, waiter1.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, request4->negotiated_protocol());

  // Create another request that will alias and reuse the first session.
  std::unique_ptr<HttpStreamRequest> request5(
      session()->http_stream_factory()->RequestStream(
          request_info_b, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter2, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter2.WaitForStream();
  EXPECT_TRUE(waiter2.stream_done());
  EXPECT_FALSE(waiter2.websocket_stream());
  ASSERT_TRUE(waiter2.stream());
  EXPECT_EQ(kDnsAliasesB, waiter2.stream()->GetDnsAliases());

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, request5->negotiated_protocol());

  // Create another request that will alias and reuse the first session.
  std::unique_ptr<HttpStreamRequest> request6(
      session()->http_stream_factory()->RequestStream(
          request_info_c, DEFAULT_PRIORITY, /* allowed_bad_certs = */ {},
          &waiter3, /* enable_ip_based_pooling = */ true,
          /* enable_alternative_services = */ true, NetLogWithSource()));
  waiter3.WaitForStream();
  EXPECT_TRUE(waiter3.stream_done());
  EXPECT_FALSE(waiter3.websocket_stream());
  ASSERT_TRUE(waiter3.stream());
  EXPECT_THAT(waiter3.stream()->GetDnsAliases(), ElementsAre(kUrlC.host()));

  // Verify the session pool reused the first session and no new session is
  // created. This will fail unless the session pool supports multiple
  // sessions aliasing a single IP.
  EXPECT_EQ(1, GetQuicSessionCount(session()));
  EXPECT_EQ(kProtoQUIC, request6->negotiated_protocol());
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
    session_context_.http_server_properties = &http_server_properties_;
    session_context_.quic_context = &quic_context_;
  }

 private:
  // Parameters passed in the NetworkSessionContext must outlive the
  // HttpNetworkSession.
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_ =
      ConfiguredProxyResolutionService::CreateDirect();
  SSLConfigServiceDefaults ssl_config_service_;
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

}  // namespace net
