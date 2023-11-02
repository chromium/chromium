// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job_factory.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/connect_job.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/ssl/ssl_config.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {
namespace {

// Mock HttpProxyConnectJob::Factory that records the `params` used and then
// passes on to a real factory.
class TestHttpProxyConnectJobFactory : public HttpProxyConnectJob::Factory {
 public:
  std::unique_ptr<HttpProxyConnectJob> Create(
      RequestPriority priority,
      const SocketTag& socket_tag,
      const CommonConnectJobParams* common_connect_job_params,
      scoped_refptr<HttpProxySocketParams> params,
      ConnectJob::Delegate* delegate,
      const NetLogWithSource* net_log) override {
    params_.push_back(params);
    return HttpProxyConnectJob::Factory::Create(priority, socket_tag,
                                                common_connect_job_params,
                                                params, delegate, net_log);
  }

  const std::vector<scoped_refptr<HttpProxySocketParams>>& params() const {
    return params_;
  }

 private:
  std::vector<scoped_refptr<HttpProxySocketParams>> params_;
};

// Mock SOCKSConnectJob::Factory that records the `params` used and then passes
// on to a real factory.
class TestSocksConnectJobFactory : public SOCKSConnectJob::Factory {
 public:
  std::unique_ptr<SOCKSConnectJob> Create(
      RequestPriority priority,
      const SocketTag& socket_tag,
      const CommonConnectJobParams* common_connect_job_params,
      scoped_refptr<SOCKSSocketParams> socks_params,
      ConnectJob::Delegate* delegate,
      const NetLogWithSource* net_log) override {
    params_.push_back(socks_params);
    return SOCKSConnectJob::Factory::Create(priority, socket_tag,
                                            common_connect_job_params,
                                            socks_params, delegate, net_log);
  }

  const std::vector<scoped_refptr<SOCKSSocketParams>>& params() const {
    return params_;
  }

 private:
  std::vector<scoped_refptr<SOCKSSocketParams>> params_;
};

// Mock SSLConnectJob::Factory that records the `params` used and then passes on
// to a real factory.
class TestSslConnectJobFactory : public SSLConnectJob::Factory {
 public:
  std::unique_ptr<SSLConnectJob> Create(
      RequestPriority priority,
      const SocketTag& socket_tag,
      const CommonConnectJobParams* common_connect_job_params,
      scoped_refptr<SSLSocketParams> params,
      ConnectJob::Delegate* delegate,
      const NetLogWithSource* net_log) override {
    params_.push_back(params);
    return SSLConnectJob::Factory::Create(priority, socket_tag,
                                          common_connect_job_params, params,
                                          delegate, net_log);
  }

  const std::vector<scoped_refptr<SSLSocketParams>>& params() const {
    return params_;
  }

 private:
  std::vector<scoped_refptr<SSLSocketParams>> params_;
};

// Mock TransportConnectJob::Factory that records the `params` used and then
// passes on to a real factory.
class TestTransportConnectJobFactory : public TransportConnectJob::Factory {
 public:
  std::unique_ptr<TransportConnectJob> Create(
      RequestPriority priority,
      const SocketTag& socket_tag,
      const CommonConnectJobParams* common_connect_job_params,
      const scoped_refptr<TransportSocketParams>& params,
      ConnectJob::Delegate* delegate,
      const NetLogWithSource* net_log) override {
    params_.push_back(params);
    return TransportConnectJob::Factory::Create(priority, socket_tag,
                                                common_connect_job_params,
                                                params, delegate, net_log);
  }

  const std::vector<scoped_refptr<TransportSocketParams>>& params() const {
    return params_;
  }

 private:
  std::vector<scoped_refptr<TransportSocketParams>> params_;
};

class ConnectJobFactoryTest : public TestWithTaskEnvironment {
 public:
  ConnectJobFactoryTest() {
    auto http_proxy_job_factory =
        std::make_unique<TestHttpProxyConnectJobFactory>();
    http_proxy_job_factory_ = http_proxy_job_factory.get();

    auto socks_job_factory = std::make_unique<TestSocksConnectJobFactory>();
    socks_job_factory_ = socks_job_factory.get();

    auto ssl_job_factory = std::make_unique<TestSslConnectJobFactory>();
    ssl_job_factory_ = ssl_job_factory.get();

    auto transport_job_factory =
        std::make_unique<TestTransportConnectJobFactory>();
    transport_job_factory_ = transport_job_factory.get();

    factory_ = std::make_unique<ConnectJobFactory>(
        std::move(http_proxy_job_factory), std::move(socks_job_factory),
        std::move(ssl_job_factory), std::move(transport_job_factory));
  }

 protected:
  // Gets the total number of ConnectJob creations across all types.
  size_t GetCreationCount() const {
    return http_proxy_job_factory_->params().size() +
           socks_job_factory_->params().size() +
           ssl_job_factory_->params().size() +
           transport_job_factory_->params().size();
  }

  const CommonConnectJobParams common_connect_job_params_{
      /*client_socket_factory=*/nullptr,
      /*host_resolver=*/nullptr,
      /*http_auth_cache=*/nullptr,
      /*http_auth_handler_factory=*/nullptr,
      /*spdy_session_pool=*/nullptr,
      /*quic_supported_versions=*/nullptr,
      /*quic_stream_factory=*/nullptr,
      /*proxy_delegate=*/nullptr,
      /*http_user_agent_settings=*/nullptr,
      /*ssl_client_context=*/nullptr,
      /*socket_performance_watcher_factory=*/nullptr,
      /*network_quality_estimator=*/nullptr,
      /*net_log=*/nullptr,
      /*websocket_endpoint_lock_manager=*/nullptr};
  TestConnectJobDelegate delegate_;

  raw_ptr<TestHttpProxyConnectJobFactory> http_proxy_job_factory_;
  raw_ptr<TestSocksConnectJobFactory> socks_job_factory_;
  raw_ptr<TestSslConnectJobFactory> ssl_job_factory_;
  raw_ptr<TestTransportConnectJobFactory> transport_job_factory_;

  std::unique_ptr<ConnectJobFactory> factory_;
};

TEST_F(ConnectJobFactoryTest, CreateConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 82);

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, ProxyServer::Direct(),
      /*proxy_annotation_tag=*/absl::nullopt,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(transport_job_factory_->params(), testing::SizeIs(1));
  const TransportSocketParams& params =
      *transport_job_factory_->params().front();
  EXPECT_THAT(params.destination(),
              testing::VariantWith<url::SchemeHostPort>(kEndpoint));
}

TEST_F(ConnectJobFactoryTest, CreateConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 82);

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/false, kEndpoint, ProxyServer::Direct(),
      /*proxy_annotation_tag=*/absl::nullopt,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(transport_job_factory_->params(), testing::SizeIs(1));
  const TransportSocketParams& params =
      *transport_job_factory_->params().front();
  EXPECT_THAT(params.destination(),
              testing::VariantWith<HostPortPair>(kEndpoint));
}

TEST_F(ConnectJobFactoryTest, CreateHttpsConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 84);
  SSLConfig ssl_config;
  ssl_config.alpn_protos = {kProtoHTTP2, kProtoHTTP11};

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, ProxyServer::Direct(),
      /*proxy_annotation_tag=*/absl::nullopt,
      /*ssl_config_for_origin=*/&ssl_config,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& params = *ssl_job_factory_->params().front();
  EXPECT_EQ(params.host_and_port(),
            HostPortPair::FromSchemeHostPort(kEndpoint));

  ASSERT_EQ(params.GetConnectionType(), SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *params.GetDirectConnectionParams();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<url::SchemeHostPort>(kEndpoint));
  EXPECT_THAT(transport_params.supported_alpns(),
              testing::UnorderedElementsAre("h2", "http/1.1"));
}

TEST_F(ConnectJobFactoryTest, CreateHttpsConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 84);
  SSLConfig ssl_config;

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/true, kEndpoint, ProxyServer::Direct(),
      /*proxy_annotation_tag=*/absl::nullopt,
      /*ssl_config_for_origin=*/&ssl_config,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& params = *ssl_job_factory_->params().front();
  EXPECT_EQ(params.host_and_port(), kEndpoint);

  ASSERT_EQ(params.GetConnectionType(), SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *params.GetDirectConnectionParams();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kEndpoint));
}

TEST_F(ConnectJobFactoryTest, CreateHttpProxyConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 85);
  const ProxyServer kProxy(ProxyServer::SCHEME_HTTP,
                           HostPortPair("proxy.test", 86));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  const HttpProxySocketParams& params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(params.is_quic());
  EXPECT_EQ(params.endpoint(), HostPortPair::FromSchemeHostPort(kEndpoint));

  ASSERT_TRUE(params.transport_params());
  const TransportSocketParams& transport_params = *params.transport_params();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kProxy.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpProxyConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 85);
  const ProxyServer kProxy(ProxyServer::SCHEME_HTTP,
                           HostPortPair("proxy.test", 86));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/false, kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  const HttpProxySocketParams& params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(params.is_quic());
  EXPECT_EQ(params.endpoint(), kEndpoint);

  ASSERT_TRUE(params.transport_params());
  const TransportSocketParams& transport_params = *params.transport_params();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kProxy.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpProxyConnectJobForHttps) {
  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 87);
  const ProxyServer kProxy(ProxyServer::SCHEME_HTTP,
                           HostPortPair("proxy.test", 88));
  SSLConfig ssl_config;

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*ssl_config_for_origin=*/&ssl_config,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& params = *ssl_job_factory_->params().front();
  EXPECT_EQ(params.host_and_port(),
            HostPortPair::FromSchemeHostPort(kEndpoint));

  ASSERT_EQ(params.GetConnectionType(), SSLSocketParams::HTTP_PROXY);
  const HttpProxySocketParams& proxy_params =
      *params.GetHttpProxyConnectionParams();
  EXPECT_FALSE(proxy_params.is_quic());
  EXPECT_EQ(proxy_params.endpoint(),
            HostPortPair::FromSchemeHostPort(kEndpoint));

  ASSERT_TRUE(proxy_params.transport_params());
  const TransportSocketParams& transport_params =
      *proxy_params.transport_params();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kProxy.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpProxyConnectJobForHttpsWithoutScheme) {
  const HostPortPair kEndpoint("test", 87);
  const ProxyServer kProxy(ProxyServer::SCHEME_HTTP,
                           HostPortPair("proxy.test", 88));
  SSLConfig ssl_config;

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/true, kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*ssl_config_for_origin=*/&ssl_config,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& params = *ssl_job_factory_->params().front();
  EXPECT_EQ(params.host_and_port(), kEndpoint);

  ASSERT_EQ(params.GetConnectionType(), SSLSocketParams::HTTP_PROXY);
  const HttpProxySocketParams& proxy_params =
      *params.GetHttpProxyConnectionParams();
  EXPECT_FALSE(proxy_params.is_quic());
  EXPECT_EQ(proxy_params.endpoint(), kEndpoint);

  ASSERT_TRUE(proxy_params.transport_params());
  const TransportSocketParams& transport_params =
      *proxy_params.transport_params();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kProxy.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpsProxyConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 89);
  const ProxyServer kProxy(ProxyServer::SCHEME_HTTPS,
                           HostPortPair("proxy.test", 90));
  SSLConfig ssl_config;

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/&ssl_config,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  const HttpProxySocketParams& params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(params.is_quic());
  EXPECT_EQ(params.endpoint(), HostPortPair::FromSchemeHostPort(kEndpoint));

  ASSERT_TRUE(params.ssl_params());
  const SSLSocketParams& ssl_params = *params.ssl_params();
  EXPECT_EQ(ssl_params.host_and_port(), kProxy.host_port_pair());

  ASSERT_EQ(ssl_params.GetConnectionType(), SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *ssl_params.GetDirectConnectionParams();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kProxy.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpsProxyConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 89);
  const ProxyServer kProxy(ProxyServer::SCHEME_HTTPS,
                           HostPortPair("proxy.test", 90));
  SSLConfig ssl_config;

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/false, kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/&ssl_config,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  const HttpProxySocketParams& params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(params.is_quic());
  EXPECT_EQ(params.endpoint(), kEndpoint);

  ASSERT_TRUE(params.ssl_params());
  const SSLSocketParams& ssl_params = *params.ssl_params();
  EXPECT_EQ(ssl_params.host_and_port(), kProxy.host_port_pair());

  ASSERT_EQ(ssl_params.GetConnectionType(), SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *ssl_params.GetDirectConnectionParams();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kProxy.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateSocksProxyConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 91);
  const ProxyServer kProxy(ProxyServer::SCHEME_SOCKS5,
                           HostPortPair("proxy.test", 92));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(socks_job_factory_->params(), testing::SizeIs(1));
  const SOCKSSocketParams& params = *socks_job_factory_->params().front();
  EXPECT_EQ(params.destination(), HostPortPair::FromSchemeHostPort(kEndpoint));
  EXPECT_TRUE(params.is_socks_v5());

  const TransportSocketParams& transport_params = *params.transport_params();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kProxy.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateSocksProxyConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 91);
  const ProxyServer kProxy(ProxyServer::SCHEME_SOCKS5,
                           HostPortPair("proxy.test", 92));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/false, kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(socks_job_factory_->params(), testing::SizeIs(1));
  const SOCKSSocketParams& params = *socks_job_factory_->params().front();
  EXPECT_EQ(params.destination(), kEndpoint);
  EXPECT_TRUE(params.is_socks_v5());

  const TransportSocketParams& transport_params = *params.transport_params();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kProxy.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateWebsocketConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 93);

  WebSocketEndpointLockManager websocket_endpoint_lock_manager;
  CommonConnectJobParams common_connect_job_params = common_connect_job_params_;
  common_connect_job_params.websocket_endpoint_lock_manager =
      &websocket_endpoint_lock_manager;

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, ProxyServer::Direct(),
      /*proxy_annotation_tag=*/absl::nullopt,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(transport_job_factory_->params(), testing::SizeIs(1));
  const TransportSocketParams& params =
      *transport_job_factory_->params().front();
  EXPECT_THAT(params.destination(),
              testing::VariantWith<url::SchemeHostPort>(kEndpoint));
}

TEST_F(ConnectJobFactoryTest, CreateWebsocketConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 93);

  WebSocketEndpointLockManager websocket_endpoint_lock_manager;
  CommonConnectJobParams common_connect_job_params = common_connect_job_params_;
  common_connect_job_params.websocket_endpoint_lock_manager =
      &websocket_endpoint_lock_manager;

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/false, kEndpoint, ProxyServer::Direct(),
      /*proxy_annotation_tag=*/absl::nullopt,
      /*ssl_config_for_origin=*/nullptr,
      /*ssl_config_for_proxy=*/nullptr,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(transport_job_factory_->params(), testing::SizeIs(1));
  const TransportSocketParams& params =
      *transport_job_factory_->params().front();
  EXPECT_THAT(params.destination(),
              testing::VariantWith<HostPortPair>(kEndpoint));
}

}  // namespace
}  // namespace net
