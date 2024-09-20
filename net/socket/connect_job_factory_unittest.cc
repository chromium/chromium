// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job_factory.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
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
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

// TODO(crbug.com/365771838): Add tests for non-ip protection nested proxy
// chains if support is enabled for all builds.
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

  const NextProtoVector alpn_protos_{kProtoHTTP2, kProtoHTTP11};
  const SSLConfig::ApplicationSettings application_settings_{{kProtoHTTP2, {}}};
  bool early_data_enabled_ = true;
  const StaticHttpUserAgentSettings http_user_agent_settings_ = {"*",
                                                                 "test-ua"};
  const CommonConnectJobParams common_connect_job_params_{
      /*client_socket_factory=*/nullptr,
      /*host_resolver=*/nullptr,
      /*http_auth_cache=*/nullptr,
      /*http_auth_handler_factory=*/nullptr,
      /*spdy_session_pool=*/nullptr,
      /*quic_supported_versions=*/nullptr,
      /*quic_session_pool=*/nullptr,
      /*proxy_delegate=*/nullptr,
      &http_user_agent_settings_,
      /*ssl_client_context=*/nullptr,
      /*socket_performance_watcher_factory=*/nullptr,
      /*network_quality_estimator=*/nullptr,
      /*net_log=*/nullptr,
      /*websocket_endpoint_lock_manager=*/nullptr,
      /*http_server_properties=*/nullptr,
      &alpn_protos_,
      &application_settings_,
      /*ignore_certificate_errors=*/nullptr,
      &early_data_enabled_};
  TestConnectJobDelegate delegate_;

  std::unique_ptr<ConnectJobFactory> factory_;
  raw_ptr<TestHttpProxyConnectJobFactory> http_proxy_job_factory_;
  raw_ptr<TestSocksConnectJobFactory> socks_job_factory_;
  raw_ptr<TestSslConnectJobFactory> ssl_job_factory_;
  raw_ptr<TestTransportConnectJobFactory> transport_job_factory_;
};

TEST_F(ConnectJobFactoryTest, CreateConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 82);

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, ProxyChain::Direct(), /*proxy_annotation_tag=*/std::nullopt,
      /*allowed_bad_certs=*/{}, ConnectJobFactory::AlpnMode::kHttpAll,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false, &common_connect_job_params_,
      &delegate_);
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
      /*using_ssl=*/false, kEndpoint, ProxyChain::Direct(),
      /*proxy_annotation_tag=*/std::nullopt,
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

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, ProxyChain::Direct(), /*proxy_annotation_tag=*/std::nullopt,
      /*allowed_bad_certs=*/{}, ConnectJobFactory::AlpnMode::kHttpAll,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false, &common_connect_job_params_,
      &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& params = *ssl_job_factory_->params().front();
  EXPECT_EQ(params.host_and_port(),
            HostPortPair::FromSchemeHostPort(kEndpoint));
  EXPECT_FALSE(params.ssl_config().disable_cert_verification_network_fetches);
  EXPECT_EQ(0, params.ssl_config().GetCertVerifyFlags());
  EXPECT_THAT(params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(params.ssl_config().application_settings, application_settings_);
  EXPECT_EQ(params.ssl_config().renego_allowed_default, true);
  EXPECT_THAT(params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre(kProtoHTTP11));
  EXPECT_TRUE(params.ssl_config().early_data_enabled);

  ASSERT_EQ(params.GetConnectionType(), SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *params.GetDirectConnectionParams();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<url::SchemeHostPort>(kEndpoint));
  EXPECT_THAT(transport_params.supported_alpns(),
              testing::UnorderedElementsAre("h2", "http/1.1"));
}

TEST_F(ConnectJobFactoryTest, CreateHttpsConnectJobForHttp11) {
  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 84);

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, ProxyChain::Direct(), /*proxy_annotation_tag=*/std::nullopt,
      /*allowed_bad_certs=*/{}, ConnectJobFactory::AlpnMode::kHttp11Only,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false, &common_connect_job_params_,
      &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& params = *ssl_job_factory_->params().front();
  EXPECT_EQ(params.host_and_port(),
            HostPortPair::FromSchemeHostPort(kEndpoint));
  EXPECT_FALSE(params.ssl_config().disable_cert_verification_network_fetches);
  EXPECT_EQ(0, params.ssl_config().GetCertVerifyFlags());
  EXPECT_THAT(params.ssl_config().alpn_protos,
              testing::ElementsAre(kProtoHTTP11));
  EXPECT_EQ(params.ssl_config().application_settings, application_settings_);
  EXPECT_EQ(params.ssl_config().renego_allowed_default, true);
  EXPECT_THAT(params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre(kProtoHTTP11));
  EXPECT_TRUE(params.ssl_config().early_data_enabled);

  ASSERT_EQ(params.GetConnectionType(), SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *params.GetDirectConnectionParams();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<url::SchemeHostPort>(kEndpoint));
  EXPECT_THAT(transport_params.supported_alpns(),
              testing::UnorderedElementsAre("http/1.1"));
}

TEST_F(ConnectJobFactoryTest, CreateHttpsConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 84);

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/true, kEndpoint, ProxyChain::Direct(),
      /*proxy_annotation_tag=*/std::nullopt, /*force_tunnel=*/false,
      PrivacyMode::PRIVACY_MODE_DISABLED, OnHostResolutionCallback(),
      DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& params = *ssl_job_factory_->params().front();
  EXPECT_EQ(params.host_and_port(), kEndpoint);
  EXPECT_FALSE(params.ssl_config().disable_cert_verification_network_fetches);
  EXPECT_EQ(0, params.ssl_config().GetCertVerifyFlags());
  EXPECT_THAT(params.ssl_config().alpn_protos, testing::ElementsAre());
  EXPECT_TRUE(params.ssl_config().application_settings.empty());
  EXPECT_EQ(params.ssl_config().renego_allowed_default, false);
  EXPECT_THAT(params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_TRUE(params.ssl_config().early_data_enabled);

  ASSERT_EQ(params.GetConnectionType(), SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *params.GetDirectConnectionParams();
  EXPECT_THAT(transport_params.destination(),
              testing::VariantWith<HostPortPair>(kEndpoint));
}

TEST_F(ConnectJobFactoryTest, CreateHttpProxyConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 85);
  const ProxyChain kProxy(ProxyServer::SCHEME_HTTP,
                          HostPortPair("proxy.test", 86));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS, /*allowed_bad_certs=*/{},
      ConnectJobFactory::AlpnMode::kHttpAll, /*force_tunnel=*/false,
      PrivacyMode::PRIVACY_MODE_DISABLED, OnHostResolutionCallback(),
      DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  const HttpProxySocketParams& params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(params.proxy_server().is_quic());
  EXPECT_EQ(params.endpoint(), HostPortPair::FromSchemeHostPort(kEndpoint));

  ASSERT_TRUE(params.transport_params());
  const TransportSocketParams& transport_params = *params.transport_params();
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxy.First().host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpProxyConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 85);
  const ProxyChain kProxy(ProxyServer::SCHEME_HTTP,
                          HostPortPair("proxy.test", 86));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/false, kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);
  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  const HttpProxySocketParams& params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(params.proxy_server().is_quic());
  EXPECT_EQ(params.endpoint(), kEndpoint);

  ASSERT_TRUE(params.transport_params());
  const TransportSocketParams& transport_params = *params.transport_params();
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxy.First().host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpProxyConnectJobForHttps) {
  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 87);
  const ProxyChain kProxy(ProxyServer::SCHEME_HTTP,
                          HostPortPair("proxy.test", 88));
  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS, /*allowed_bad_certs=*/{},
      ConnectJobFactory::AlpnMode::kHttpAll, /*force_tunnel=*/false,
      PrivacyMode::PRIVACY_MODE_DISABLED, OnHostResolutionCallback(),
      DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& params = *ssl_job_factory_->params().front();
  EXPECT_EQ(params.host_and_port(),
            HostPortPair::FromSchemeHostPort(kEndpoint));
  EXPECT_FALSE(params.ssl_config().disable_cert_verification_network_fetches);
  EXPECT_EQ(0, params.ssl_config().GetCertVerifyFlags());
  EXPECT_THAT(params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(params.ssl_config().application_settings, application_settings_);
  EXPECT_EQ(params.ssl_config().renego_allowed_default, true);
  EXPECT_THAT(params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre(kProtoHTTP11));
  EXPECT_TRUE(params.ssl_config().early_data_enabled);

  ASSERT_EQ(params.GetConnectionType(), SSLSocketParams::HTTP_PROXY);
  const HttpProxySocketParams& proxy_params =
      *params.GetHttpProxyConnectionParams();
  EXPECT_FALSE(proxy_params.proxy_server().is_quic());
  EXPECT_EQ(proxy_params.endpoint(),
            HostPortPair::FromSchemeHostPort(kEndpoint));

  ASSERT_TRUE(proxy_params.transport_params());
  const TransportSocketParams& transport_params =
      *proxy_params.transport_params();
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxy.First().host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpProxyConnectJobForHttpsWithoutScheme) {
  const HostPortPair kEndpoint("test", 87);
  const ProxyChain kProxy(ProxyServer::SCHEME_HTTP,
                          HostPortPair("proxy.test", 88));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/true, kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
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
  EXPECT_FALSE(proxy_params.proxy_server().is_quic());
  EXPECT_EQ(proxy_params.endpoint(), kEndpoint);
  EXPECT_THAT(params.ssl_config().alpn_protos, testing::ElementsAre());
  EXPECT_TRUE(params.ssl_config().application_settings.empty());
  EXPECT_EQ(params.ssl_config().renego_allowed_default, false);
  EXPECT_THAT(params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  // While the only production caller of this method disables SSL early data, it
  // does so by configuring the HttpNetworkSession, rather than by relying on
  // the ConnectJobFactory to disable early data when there's no scheme.
  EXPECT_TRUE(params.ssl_config().early_data_enabled);

  ASSERT_TRUE(proxy_params.transport_params());
  const TransportSocketParams& transport_params =
      *proxy_params.transport_params();
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxy.First().host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpsProxyConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 89);
  const ProxyChain kProxy(ProxyServer::SCHEME_HTTPS,
                          HostPortPair("proxy.test", 90));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS, /*allowed_bad_certs=*/{},
      ConnectJobFactory::AlpnMode::kHttpAll, /*force_tunnel=*/false,
      PrivacyMode::PRIVACY_MODE_DISABLED, OnHostResolutionCallback(),
      DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  const HttpProxySocketParams& params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(params.proxy_server().is_quic());
  EXPECT_EQ(params.endpoint(), HostPortPair::FromSchemeHostPort(kEndpoint));

  ASSERT_TRUE(params.ssl_params());
  const SSLSocketParams& ssl_params = *params.ssl_params();
  EXPECT_EQ(ssl_params.host_and_port(), kProxy.First().host_port_pair());
  EXPECT_TRUE(
      ssl_params.ssl_config().disable_cert_verification_network_fetches);
  EXPECT_EQ(CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES,
            ssl_params.ssl_config().GetCertVerifyFlags());
  EXPECT_THAT(ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(ssl_params.ssl_config().application_settings,
            application_settings_);
  // Renegotiation is never allowed for proxies.
  EXPECT_EQ(ssl_params.ssl_config().renego_allowed_default, false);
  EXPECT_THAT(ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(ssl_params.ssl_config().early_data_enabled);

  ASSERT_EQ(ssl_params.GetConnectionType(), SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *ssl_params.GetDirectConnectionParams();
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxy.First().host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateHttpsProxyConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 89);
  const ProxyChain kProxy(ProxyServer::SCHEME_HTTPS,
                          HostPortPair("proxy.test", 90));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/false, kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  const HttpProxySocketParams& params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(params.proxy_server().is_quic());
  EXPECT_EQ(params.endpoint(), kEndpoint);

  ASSERT_TRUE(params.ssl_params());
  const SSLSocketParams& ssl_params = *params.ssl_params();
  EXPECT_EQ(ssl_params.host_and_port(), kProxy.First().host_port_pair());
  EXPECT_TRUE(
      ssl_params.ssl_config().disable_cert_verification_network_fetches);
  EXPECT_EQ(CertVerifier::VERIFY_DISABLE_NETWORK_FETCHES,
            ssl_params.ssl_config().GetCertVerifyFlags());
  // Alpn should always be used for HTTPS proxies.
  EXPECT_THAT(ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(ssl_params.ssl_config().application_settings,
            application_settings_);
  // Renegotiation is never allowed for proxies.
  EXPECT_EQ(ssl_params.ssl_config().renego_allowed_default, false);
  EXPECT_THAT(ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(ssl_params.ssl_config().early_data_enabled);

  ASSERT_EQ(ssl_params.GetConnectionType(), SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *ssl_params.GetDirectConnectionParams();
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxy.First().host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateNestedHttpsProxyConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 89);
  const ProxyServer kProxyServer1{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy1.test", 443)};
  const ProxyServer kProxyServer2{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy2.test", 443)};
  const ProxyChain kNestedProxyChain =
      ProxyChain::ForIpProtection({{kProxyServer1, kProxyServer2}});

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kNestedProxyChain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, ConnectJobFactory::AlpnMode::kHttpAll,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false, &common_connect_job_params_,
      &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  // The corresponding HttpProxySocketParams and SSLSocketParams for each hop
  // should be present in reverse order.
  const HttpProxySocketParams& proxy_server2_http_params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(proxy_server2_http_params.proxy_server().is_quic());
  // We should to send a CONNECT to `kProxyServer2` for `kEndpoint`.
  EXPECT_EQ(proxy_server2_http_params.endpoint(),
            HostPortPair::FromSchemeHostPort(kEndpoint));
  EXPECT_TRUE(proxy_server2_http_params.tunnel());

  const SSLSocketParams& proxy_server2_ssl_params =
      *proxy_server2_http_params.ssl_params();
  EXPECT_EQ(proxy_server2_ssl_params.host_and_port(),
            kProxyServer2.host_port_pair());
  EXPECT_THAT(proxy_server2_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(proxy_server2_ssl_params.ssl_config().application_settings,
            application_settings_);
  EXPECT_EQ(proxy_server2_ssl_params.ssl_config().renego_allowed_default,
            false);
  EXPECT_THAT(proxy_server2_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(proxy_server2_ssl_params.ssl_config().early_data_enabled);

  const HttpProxySocketParams& proxy_server1_http_params =
      *proxy_server2_ssl_params.GetHttpProxyConnectionParams();
  EXPECT_FALSE(proxy_server1_http_params.proxy_server().is_quic());
  // We should to send a CONNECT to `kProxyServer1` for `kProxyServer2`.
  EXPECT_EQ(proxy_server1_http_params.endpoint(),
            kProxyServer2.host_port_pair());

  ASSERT_TRUE(proxy_server1_http_params.ssl_params());
  const SSLSocketParams& proxy_server1_ssl_params =
      *proxy_server1_http_params.ssl_params();
  EXPECT_EQ(proxy_server1_ssl_params.host_and_port(),
            kProxyServer1.host_port_pair());
  EXPECT_THAT(proxy_server1_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(proxy_server1_ssl_params.ssl_config().application_settings,
            application_settings_);
  EXPECT_EQ(proxy_server1_ssl_params.ssl_config().renego_allowed_default,
            false);
  EXPECT_THAT(proxy_server1_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(proxy_server1_ssl_params.ssl_config().early_data_enabled);

  ASSERT_EQ(proxy_server1_ssl_params.GetConnectionType(),
            SSLSocketParams::DIRECT);
  ASSERT_EQ(proxy_server2_ssl_params.GetConnectionType(),
            SSLSocketParams::HTTP_PROXY);

  const TransportSocketParams& transport_params =
      *proxy_server1_ssl_params.GetDirectConnectionParams();
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxyServer1.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateNestedHttpsProxyConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 89);
  const ProxyServer kProxyServer1{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy1.test", 443)};
  const ProxyServer kProxyServer2{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy2.test", 443)};
  const ProxyChain kNestedProxyChain =
      ProxyChain::ForIpProtection({{kProxyServer1, kProxyServer2}});

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/false, kEndpoint, kNestedProxyChain,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*force_tunnel=*/false,
      PrivacyMode::PRIVACY_MODE_DISABLED, OnHostResolutionCallback(),
      DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(http_proxy_job_factory_->params(), testing::SizeIs(1));
  // The corresponding HttpProxySocketParams and SSLSocketParams for each hop
  // should be present in reverse order.
  const HttpProxySocketParams& proxy_server2_http_params =
      *http_proxy_job_factory_->params().front();
  EXPECT_FALSE(proxy_server2_http_params.proxy_server().is_quic());
  // We should to send a CONNECT to `kProxyServer2` for `kEndpoint`.
  EXPECT_EQ(proxy_server2_http_params.endpoint(), kEndpoint);

  const SSLSocketParams& proxy_server2_ssl_params =
      *proxy_server2_http_params.ssl_params();
  EXPECT_EQ(proxy_server2_ssl_params.host_and_port(),
            kProxyServer2.host_port_pair());

  const HttpProxySocketParams& proxy_server1_http_params =
      *proxy_server2_ssl_params.GetHttpProxyConnectionParams();
  EXPECT_FALSE(proxy_server1_http_params.proxy_server().is_quic());
  // We should to send a CONNECT to `kProxyServer1` for `kProxyServer2`.
  EXPECT_EQ(proxy_server1_http_params.endpoint(),
            kProxyServer2.host_port_pair());
  EXPECT_THAT(proxy_server2_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(proxy_server2_ssl_params.ssl_config().application_settings,
            application_settings_);
  EXPECT_EQ(proxy_server2_ssl_params.ssl_config().renego_allowed_default,
            false);
  EXPECT_THAT(proxy_server2_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(proxy_server2_ssl_params.ssl_config().early_data_enabled);

  ASSERT_TRUE(proxy_server1_http_params.ssl_params());
  const SSLSocketParams& proxy_server1_ssl_params =
      *proxy_server1_http_params.ssl_params();
  EXPECT_EQ(proxy_server1_ssl_params.host_and_port(),
            kProxyServer1.host_port_pair());
  EXPECT_THAT(proxy_server1_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(proxy_server1_ssl_params.ssl_config().application_settings,
            application_settings_);
  EXPECT_EQ(proxy_server1_ssl_params.ssl_config().renego_allowed_default,
            false);
  EXPECT_THAT(proxy_server1_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(proxy_server1_ssl_params.ssl_config().early_data_enabled);

  ASSERT_EQ(proxy_server1_ssl_params.GetConnectionType(),
            SSLSocketParams::DIRECT);
  ASSERT_EQ(proxy_server2_ssl_params.GetConnectionType(),
            SSLSocketParams::HTTP_PROXY);

  ASSERT_EQ(proxy_server1_ssl_params.GetConnectionType(),
            SSLSocketParams::DIRECT);
  const TransportSocketParams& transport_params =
      *proxy_server1_ssl_params.GetDirectConnectionParams();
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxyServer1.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateNestedHttpsProxyConnectJobForHttps) {
  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 443);

  const ProxyServer kProxyServer1{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy1.test", 443)};
  const ProxyServer kProxyServer2{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy2.test", 443)};

  const ProxyChain kNestedProxyChain =
      ProxyChain::ForIpProtection({{kProxyServer1, kProxyServer2}});

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kNestedProxyChain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, ConnectJobFactory::AlpnMode::kHttpAll,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false, &common_connect_job_params_,
      &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& endpoint_ssl_params =
      *ssl_job_factory_->params().at(0);
  EXPECT_THAT(endpoint_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(endpoint_ssl_params.ssl_config().application_settings,
            application_settings_);
  EXPECT_EQ(endpoint_ssl_params.ssl_config().renego_allowed_default, true);
  EXPECT_THAT(endpoint_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre(kProtoHTTP11));
  EXPECT_TRUE(endpoint_ssl_params.ssl_config().early_data_enabled);

  // The SSLSocketParams for the destination should be configured to go through
  // the chain of proxies, with the corresponding HttpProxySocketParams and
  // SSLSocketParams for each hop present in reverse order.
  const HttpProxySocketParams& proxy_server2_http_params =
      *endpoint_ssl_params.GetHttpProxyConnectionParams();
  EXPECT_FALSE(proxy_server2_http_params.proxy_server().is_quic());
  // We should to send a CONNECT to `kProxyServer2` for `kEndpoint`.
  EXPECT_EQ(proxy_server2_http_params.endpoint(),
            HostPortPair::FromSchemeHostPort(kEndpoint));

  const SSLSocketParams& proxy_server2_ssl_params =
      *proxy_server2_http_params.ssl_params();
  EXPECT_EQ(proxy_server2_ssl_params.host_and_port(),
            kProxyServer2.host_port_pair());
  EXPECT_THAT(proxy_server2_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(proxy_server2_ssl_params.ssl_config().application_settings,
            application_settings_);
  EXPECT_EQ(proxy_server2_ssl_params.ssl_config().renego_allowed_default,
            false);
  EXPECT_THAT(proxy_server2_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(proxy_server2_ssl_params.ssl_config().early_data_enabled);

  const HttpProxySocketParams& proxy_server1_http_params =
      *proxy_server2_ssl_params.GetHttpProxyConnectionParams();
  EXPECT_FALSE(proxy_server1_http_params.proxy_server().is_quic());
  // We should to send a CONNECT to `kProxyServer1` for `kProxyServer2`.
  EXPECT_EQ(proxy_server1_http_params.endpoint(),
            kProxyServer2.host_port_pair());

  ASSERT_TRUE(proxy_server1_http_params.ssl_params());
  const SSLSocketParams& proxy_server1_ssl_params =
      *proxy_server1_http_params.ssl_params();
  EXPECT_EQ(proxy_server1_ssl_params.host_and_port(),
            kProxyServer1.host_port_pair());
  EXPECT_THAT(proxy_server1_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(proxy_server1_ssl_params.ssl_config().application_settings,
            application_settings_);
  EXPECT_EQ(proxy_server1_ssl_params.ssl_config().renego_allowed_default,
            false);
  EXPECT_THAT(proxy_server1_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(proxy_server1_ssl_params.ssl_config().early_data_enabled);

  ASSERT_EQ(proxy_server1_ssl_params.GetConnectionType(),
            SSLSocketParams::DIRECT);
  ASSERT_EQ(proxy_server2_ssl_params.GetConnectionType(),
            SSLSocketParams::HTTP_PROXY);
  ASSERT_EQ(endpoint_ssl_params.GetConnectionType(),
            SSLSocketParams::HTTP_PROXY);

  const TransportSocketParams& transport_params =
      *proxy_server1_ssl_params.GetDirectConnectionParams();
  // We should establish a physical socket / direct connection to
  // `kProxyServer1` (and will tunnel all subsequent traffic through
  // that).
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxyServer1.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest,
       CreateNestedHttpsProxyConnectJobForHttpsWithoutScheme) {
  const HostPortPair kEndpoint("test", 443);

  const ProxyServer kProxyServer1{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy1.test", 443)};
  const ProxyServer kProxyServer2{ProxyServer::SCHEME_HTTPS,
                                  HostPortPair("proxy2.test", 443)};

  const ProxyChain kNestedProxyChain =
      ProxyChain::ForIpProtection({{kProxyServer1, kProxyServer2}});

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/true, kEndpoint, kNestedProxyChain,
      TRAFFIC_ANNOTATION_FOR_TESTS, /*force_tunnel=*/false,
      PrivacyMode::PRIVACY_MODE_DISABLED, OnHostResolutionCallback(),
      DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(ssl_job_factory_->params(), testing::SizeIs(1));
  const SSLSocketParams& endpoint_ssl_params =
      *ssl_job_factory_->params().at(0);
  EXPECT_THAT(endpoint_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAre());
  EXPECT_TRUE(endpoint_ssl_params.ssl_config().application_settings.empty());
  EXPECT_EQ(endpoint_ssl_params.ssl_config().renego_allowed_default, false);
  EXPECT_THAT(endpoint_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_TRUE(endpoint_ssl_params.ssl_config().early_data_enabled);

  // The SSLSocketParams for the destination should be configured to go through
  // the chain of proxies, with the corresponding HttpProxySocketParams and
  // SSLSocketParams for each hop present in reverse order.
  const HttpProxySocketParams& proxy_server2_http_params =
      *endpoint_ssl_params.GetHttpProxyConnectionParams();
  EXPECT_FALSE(proxy_server2_http_params.proxy_server().is_quic());
  // We should to send a CONNECT to `kProxyServer2` for `kEndpoint`.
  EXPECT_EQ(proxy_server2_http_params.endpoint(), kEndpoint);

  const SSLSocketParams& proxy_server2_ssl_params =
      *proxy_server2_http_params.ssl_params();
  EXPECT_EQ(proxy_server2_ssl_params.host_and_port(),
            kProxyServer2.host_port_pair());

  const HttpProxySocketParams& proxy_server1_http_params =
      *proxy_server2_ssl_params.GetHttpProxyConnectionParams();
  EXPECT_FALSE(proxy_server1_http_params.proxy_server().is_quic());
  // We should to send a CONNECT to `kProxyServer1` for `kProxyServer2`.
  EXPECT_EQ(proxy_server1_http_params.endpoint(),
            kProxyServer2.host_port_pair());
  EXPECT_THAT(proxy_server2_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(proxy_server2_ssl_params.ssl_config().application_settings,
            application_settings_);
  EXPECT_EQ(proxy_server2_ssl_params.ssl_config().renego_allowed_default,
            false);
  EXPECT_THAT(proxy_server2_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(proxy_server2_ssl_params.ssl_config().early_data_enabled);

  ASSERT_TRUE(proxy_server1_http_params.ssl_params());
  const SSLSocketParams& proxy_server1_ssl_params =
      *proxy_server1_http_params.ssl_params();
  EXPECT_EQ(proxy_server1_ssl_params.host_and_port(),
            kProxyServer1.host_port_pair());
  EXPECT_THAT(proxy_server1_ssl_params.ssl_config().alpn_protos,
              testing::ElementsAreArray(alpn_protos_));
  EXPECT_EQ(proxy_server1_ssl_params.ssl_config().application_settings,
            application_settings_);
  EXPECT_EQ(proxy_server1_ssl_params.ssl_config().renego_allowed_default,
            false);
  EXPECT_THAT(proxy_server1_ssl_params.ssl_config().renego_allowed_for_protos,
              testing::ElementsAre());
  EXPECT_FALSE(proxy_server1_ssl_params.ssl_config().early_data_enabled);

  ASSERT_EQ(proxy_server1_ssl_params.GetConnectionType(),
            SSLSocketParams::DIRECT);
  ASSERT_EQ(proxy_server2_ssl_params.GetConnectionType(),
            SSLSocketParams::HTTP_PROXY);
  ASSERT_EQ(endpoint_ssl_params.GetConnectionType(),
            SSLSocketParams::HTTP_PROXY);

  const TransportSocketParams& transport_params =
      *proxy_server1_ssl_params.GetDirectConnectionParams();
  // We should establish a physical socket / direct connection to
  // `kProxyServer1` (and will tunnel all subsequent traffic through
  // that).
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxyServer1.host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateSocksProxyConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 91);
  const ProxyChain kProxy(ProxyServer::SCHEME_SOCKS5,
                          HostPortPair("proxy.test", 92));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS, /*allowed_bad_certs=*/{},
      ConnectJobFactory::AlpnMode::kHttpAll, /*force_tunnel=*/false,
      PrivacyMode::PRIVACY_MODE_DISABLED, OnHostResolutionCallback(),
      DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, /*disable_cert_network_fetches=*/false,
      &common_connect_job_params_, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(socks_job_factory_->params(), testing::SizeIs(1));
  const SOCKSSocketParams& params = *socks_job_factory_->params().front();
  EXPECT_EQ(params.destination(), HostPortPair::FromSchemeHostPort(kEndpoint));
  EXPECT_TRUE(params.is_socks_v5());

  const TransportSocketParams& transport_params = *params.transport_params();
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxy.First().host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateSocksProxyConnectJobWithoutScheme) {
  const HostPortPair kEndpoint("test", 91);
  const ProxyChain kProxy(ProxyServer::SCHEME_SOCKS5,
                          HostPortPair("proxy.test", 92));

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      /*using_ssl=*/false, kEndpoint, kProxy, TRAFFIC_ANNOTATION_FOR_TESTS,
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
  EXPECT_THAT(
      transport_params.destination(),
      testing::VariantWith<HostPortPair>(kProxy.First().host_port_pair()));
}

TEST_F(ConnectJobFactoryTest, CreateWebsocketConnectJob) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 93);

  WebSocketEndpointLockManager websocket_endpoint_lock_manager;
  CommonConnectJobParams common_connect_job_params = common_connect_job_params_;
  common_connect_job_params.websocket_endpoint_lock_manager =
      &websocket_endpoint_lock_manager;

  std::unique_ptr<ConnectJob> job = factory_->CreateConnectJob(
      kEndpoint, ProxyChain::Direct(), /*proxy_annotation_tag=*/std::nullopt,
      /*allowed_bad_certs=*/{}, ConnectJobFactory::AlpnMode::kHttpAll,
      /*force_tunnel=*/false, PrivacyMode::PRIVACY_MODE_DISABLED,
      OnHostResolutionCallback(), DEFAULT_PRIORITY, SocketTag(),
      NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      /*disable_cert_network_fetches=*/false, &common_connect_job_params,
      &delegate_);
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
      /*using_ssl=*/false, kEndpoint, ProxyChain::Direct(),
      /*proxy_annotation_tag=*/std::nullopt, /*force_tunnel=*/false,
      PrivacyMode::PRIVACY_MODE_DISABLED, OnHostResolutionCallback(),
      DEFAULT_PRIORITY, SocketTag(), NetworkAnonymizationKey(),
      SecureDnsPolicy::kAllow, &common_connect_job_params, &delegate_);
  EXPECT_EQ(GetCreationCount(), 1u);

  ASSERT_THAT(transport_job_factory_->params(), testing::SizeIs(1));
  const TransportSocketParams& params =
      *transport_job_factory_->params().front();
  EXPECT_THAT(params.destination(),
              testing::VariantWith<HostPortPair>(kEndpoint));
}

}  // namespace
}  // namespace net
