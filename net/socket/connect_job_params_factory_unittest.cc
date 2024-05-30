// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job_params_factory.h"

#include <ostream>
#include <tuple>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/socket/connect_job_factory.h"
#include "net/socket/next_proto.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/ssl/ssl_config.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

struct TestParams {
  using ParamTuple = std::tuple<bool,
                                PrivacyMode,
                                SecureDnsPolicy,
                                ConnectJobFactory::AlpnMode,
                                bool,
                                bool>;

  explicit TestParams(ParamTuple tup)
      : disable_cert_network_fetches(std::get<0>(tup)),
        privacy_mode(std::get<1>(tup)),
        secure_dns_policy(std::get<2>(tup)),
        alpn_mode(std::get<3>(tup)),
        enable_early_data(std::get<4>(tup)),
        partition_proxy_chains(std::get<5>(tup)) {}

  bool disable_cert_network_fetches;
  PrivacyMode privacy_mode;
  SecureDnsPolicy secure_dns_policy;
  ConnectJobFactory::AlpnMode alpn_mode;
  bool enable_early_data;
  bool partition_proxy_chains;
};

std::ostream& operator<<(std::ostream& os, const TestParams& test_params) {
  os << "TestParams {.disable_cert_network_fetches="
     << test_params.disable_cert_network_fetches;
  os << ", .privacy_mode=" << test_params.privacy_mode;
  os << ", .secure_dns_policy="
     << (test_params.secure_dns_policy == SecureDnsPolicy::kAllow ? "kAllow"
                                                                  : "kDisable");
  os << ", .alpn_mode="
     << (test_params.alpn_mode == ConnectJobFactory::AlpnMode::kDisabled
             ? "kDisabled"
         : test_params.alpn_mode == ConnectJobFactory::AlpnMode::kHttp11Only
             ? "kHttp11Only"
             : "kHttpAll");
  os << ", .enable_early_data=" << test_params.enable_early_data;
  os << ", .partition_proxy_chains=" << test_params.partition_proxy_chains;
  os << "}";
  return os;
}

// Get a string describing the params variant.
const char* ParamsName(ConnectJobParams& params) {
  if (params.is_http_proxy()) {
    return "HttpProxySocketParams";
  }
  if (params.is_socks()) {
    return "SOCKSSocketParams";
  }
  if (params.is_ssl()) {
    return "SSLSocketParams";
  }
  if (params.is_transport()) {
    return "TransportSocketParams";
  }
  return "Unknown";
}

scoped_refptr<HttpProxySocketParams> ExpectHttpProxySocketParams(
    ConnectJobParams params) {
  EXPECT_TRUE(params.is_http_proxy())
      << "Expected HttpProxySocketParams, got " << ParamsName(params);
  return params.take_http_proxy();
}

void VerifyHttpProxySocketParams(
    scoped_refptr<HttpProxySocketParams> params,
    const char* description,
    // Only QUIC proxies have a quic_ssl_config.
    std::optional<SSLConfig> quic_ssl_config,
    const HostPortPair& endpoint,
    const ProxyChain& proxy_chain,
    size_t proxy_chain_index,
    bool tunnel,
    const NetworkAnonymizationKey& network_anonymization_key,
    const SecureDnsPolicy secure_dns_policy) {
  SCOPED_TRACE(testing::Message() << "Verifying " << description);
  if (quic_ssl_config) {
    // Only examine the values used for QUIC connections.
    ASSERT_TRUE(params->quic_ssl_config().has_value());
    EXPECT_EQ(params->quic_ssl_config()->privacy_mode,
              quic_ssl_config->privacy_mode);
    EXPECT_EQ(params->quic_ssl_config()->GetCertVerifyFlags(),
              quic_ssl_config->GetCertVerifyFlags());
  } else {
    EXPECT_FALSE(params->quic_ssl_config().has_value());
  }
  EXPECT_EQ(params->endpoint(), endpoint);
  EXPECT_EQ(params->proxy_chain(), proxy_chain);
  EXPECT_EQ(params->proxy_chain_index(), proxy_chain_index);
  EXPECT_EQ(params->tunnel(), tunnel);
  EXPECT_EQ(params->network_anonymization_key(), network_anonymization_key);
  EXPECT_EQ(params->secure_dns_policy(), secure_dns_policy);
}

scoped_refptr<SOCKSSocketParams> ExpectSOCKSSocketParams(
    ConnectJobParams params) {
  EXPECT_TRUE(params.is_socks())
      << "Expected SOCKSSocketParams, got " << ParamsName(params);
  return params.take_socks();
}

// Verify the properties of SOCKSSocketParams.
void VerifySOCKSSocketParams(
    scoped_refptr<SOCKSSocketParams>& params,
    const char* description,
    bool is_socks_v5,
    const HostPortPair& destination,
    const NetworkAnonymizationKey& network_anonymization_key) {
  SCOPED_TRACE(testing::Message() << "Verifying " << description);
  EXPECT_EQ(params->is_socks_v5(), is_socks_v5);
  EXPECT_EQ(params->destination(), destination);
  EXPECT_EQ(params->network_anonymization_key(), network_anonymization_key);
}

// Assert that the params are TransportSocketParams and return them.
scoped_refptr<TransportSocketParams> ExpectTransportSocketParams(
    ConnectJobParams params) {
  EXPECT_TRUE(params.is_transport())
      << "Expected TransportSocketParams, got " << ParamsName(params);
  return params.take_transport();
}

// Verify the properties of TransportSocketParams.
void VerifyTransportSocketParams(
    scoped_refptr<TransportSocketParams>& params,
    const char* description,
    const TransportSocketParams::Endpoint destination,
    const SecureDnsPolicy secure_dns_policy,
    const NetworkAnonymizationKey& network_anonymization_key,
    const base::flat_set<std::string>& supported_alpns) {
  SCOPED_TRACE(testing::Message() << "Verifying " << description);
  EXPECT_EQ(params->destination(), destination);
  EXPECT_EQ(params->secure_dns_policy(), secure_dns_policy);
  EXPECT_EQ(params->network_anonymization_key(), network_anonymization_key);
  EXPECT_EQ(params->supported_alpns(), supported_alpns);
}

// Assert that the params are SSLSocketParams and return them.
scoped_refptr<SSLSocketParams> ExpectSSLSocketParams(ConnectJobParams params) {
  EXPECT_TRUE(params.is_ssl())
      << "Expected SSLSocketParams, got " << ParamsName(params);
  return params.take_ssl();
}

// Verify the properties of SSLSocketParams.
void VerifySSLSocketParams(
    scoped_refptr<SSLSocketParams>& params,
    const char* description,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    PrivacyMode privacy_mode,
    const NetworkAnonymizationKey& network_anonymization_key) {
  SCOPED_TRACE(testing::Message() << "Verifying " << description);
  EXPECT_EQ(params->host_and_port(), host_and_port);
  // SSLConfig doesn't implement operator==, so just check the properties the
  // factory uses.
  EXPECT_EQ(params->ssl_config().disable_cert_verification_network_fetches,
            ssl_config.disable_cert_verification_network_fetches);
  EXPECT_EQ(params->ssl_config().alpn_protos, ssl_config.alpn_protos);
  EXPECT_EQ(params->ssl_config().application_settings,
            ssl_config.application_settings);
  EXPECT_EQ(params->ssl_config().renego_allowed_default,
            ssl_config.renego_allowed_default);
  EXPECT_EQ(params->ssl_config().renego_allowed_for_protos,
            ssl_config.renego_allowed_for_protos);
  EXPECT_EQ(params->ssl_config().privacy_mode, privacy_mode);
  EXPECT_EQ(params->network_anonymization_key(), network_anonymization_key);
}

// Calculate the ALPN protocols for the given ALPN mode.
base::flat_set<std::string> AlpnProtoStringsForMode(
    ConnectJobFactory::AlpnMode alpn_mode) {
  switch (alpn_mode) {
    case ConnectJobFactory::AlpnMode::kDisabled:
      return {};
    case ConnectJobFactory::AlpnMode::kHttp11Only:
      return {"http/1.1"};
    case ConnectJobFactory::AlpnMode::kHttpAll:
      return {"h2", "http/1.1"};
  }
}

class ConnectJobParamsFactoryTest : public testing::TestWithParam<TestParams> {
 public:
  ConnectJobParamsFactoryTest() {
    if (partition_proxy_chains()) {
      scoped_feature_list_.InitAndEnableFeature(
          net::features::kPartitionProxyChains);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          net::features::kPartitionProxyChains);
    }

    early_data_enabled_ = enable_early_data();
    switch (alpn_mode()) {
      case ConnectJobFactory::AlpnMode::kDisabled:
        alpn_protos_ = {};
        application_settings_ = {};
        break;
      case ConnectJobFactory::AlpnMode::kHttp11Only:
        alpn_protos_ = {kProtoHTTP11};
        application_settings_ = {};
        break;
      case ConnectJobFactory::AlpnMode::kHttpAll:
        alpn_protos_ = {kProtoHTTP2, kProtoHTTP11};
        application_settings_ = {{kProtoHTTP2, {}}};
        break;
    }
  }

 protected:
  // Parameter accessors.
  bool disable_cert_network_fetches() const {
    return GetParam().disable_cert_network_fetches;
  }
  PrivacyMode privacy_mode() const { return GetParam().privacy_mode; }
  SecureDnsPolicy secure_dns_policy() const {
    return GetParam().secure_dns_policy;
  }
  ConnectJobFactory::AlpnMode alpn_mode() const { return GetParam().alpn_mode; }
  bool enable_early_data() const { return GetParam().enable_early_data; }
  bool partition_proxy_chains() const {
    return GetParam().partition_proxy_chains;
  }

  // Create an SSL config for connection to the endpoint, based on the test
  // parameters.
  SSLConfig SSLConfigForEndpoint() const {
    SSLConfig endpoint_ssl_config;
    endpoint_ssl_config.disable_cert_verification_network_fetches =
        disable_cert_network_fetches();
    endpoint_ssl_config.early_data_enabled = enable_early_data();
    switch (alpn_mode()) {
      case ConnectJobFactory::AlpnMode::kDisabled:
        endpoint_ssl_config.alpn_protos = {};
        endpoint_ssl_config.application_settings = {};
        endpoint_ssl_config.renego_allowed_default = false;
        endpoint_ssl_config.renego_allowed_for_protos = {};
        break;
      case ConnectJobFactory::AlpnMode::kHttp11Only:
        endpoint_ssl_config.alpn_protos = {kProtoHTTP11};
        endpoint_ssl_config.application_settings = {};
        endpoint_ssl_config.renego_allowed_default = true;
        endpoint_ssl_config.renego_allowed_for_protos = {kProtoHTTP11};
        break;
      case ConnectJobFactory::AlpnMode::kHttpAll:
        endpoint_ssl_config.alpn_protos = {kProtoHTTP2, kProtoHTTP11};
        endpoint_ssl_config.application_settings = {{kProtoHTTP2, {}}};
        endpoint_ssl_config.renego_allowed_default = true;
        endpoint_ssl_config.renego_allowed_for_protos = {kProtoHTTP11};
        break;
    }
    return endpoint_ssl_config;
  }

  // Create an SSL config for connection to an HTTPS proxy, based on the test
  // parameters.
  SSLConfig SSLConfigForProxy() const {
    SSLConfig proxy_ssl_config;
    proxy_ssl_config.disable_cert_verification_network_fetches = true;
    proxy_ssl_config.early_data_enabled = true;
    proxy_ssl_config.renego_allowed_default = false;
    proxy_ssl_config.renego_allowed_for_protos = {};
    switch (alpn_mode()) {
      case ConnectJobFactory::AlpnMode::kDisabled:
        proxy_ssl_config.alpn_protos = {};
        proxy_ssl_config.application_settings = {};
        break;
      case ConnectJobFactory::AlpnMode::kHttp11Only:
        proxy_ssl_config.alpn_protos = {kProtoHTTP11};
        proxy_ssl_config.application_settings = {};
        break;
      case ConnectJobFactory::AlpnMode::kHttpAll:
        proxy_ssl_config.alpn_protos = {kProtoHTTP2, kProtoHTTP11};
        proxy_ssl_config.application_settings = {{kProtoHTTP2, {}}};
        break;
    }
    return proxy_ssl_config;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  NextProtoVector alpn_protos_;
  SSLConfig::ApplicationSettings application_settings_;
  bool early_data_enabled_;
  const CommonConnectJobParams common_connect_job_params_{
      /*client_socket_factory=*/nullptr,
      /*host_resolver=*/nullptr,
      /*http_auth_cache=*/nullptr,
      /*http_auth_handler_factory=*/nullptr,
      /*spdy_session_pool=*/nullptr,
      /*quic_supported_versions=*/nullptr,
      /*quic_session_pool=*/nullptr,
      /*proxy_delegate=*/nullptr,
      /*http_user_agent_settings=*/nullptr,
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

  const NetworkAnonymizationKey kEndpointNak =
      NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(GURL("http://test")));
  const NetworkAnonymizationKey kProxyDnsNak =
      NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(GURL("http://example-dns.test")));
};

// A connect to a simple HTTP endpoint produces just transport params.
TEST_P(ConnectJobParamsFactoryTest, HttpEndpoint) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 82);
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, ProxyChain::Direct(),
      /*proxy_annotation_tag=*/std::nullopt,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<TransportSocketParams> transport_socket_params =
      ExpectTransportSocketParams(params);
  VerifyTransportSocketParams(
      transport_socket_params, "transport_socket_params", kEndpoint,
      secure_dns_policy(), kEndpointNak, base::flat_set<std::string>());
}

// A connect to a endpoint without SSL, specified as a `SchemelessEndpoint`,
// produces just transport params.
TEST_P(ConnectJobParamsFactoryTest, UnencryptedEndpointWithoutScheme) {
  const ConnectJobFactory::SchemelessEndpoint kEndpoint{
      /*using_ssl=*/false, HostPortPair("test", 82)};
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, ProxyChain::Direct(),
      /*proxy_annotation_tag=*/std::nullopt,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<TransportSocketParams> transport_socket_params =
      ExpectTransportSocketParams(params);
  VerifyTransportSocketParams(transport_socket_params,
                              "transport_socket_params",
                              HostPortPair("test", 82), secure_dns_policy(),
                              kEndpointNak, base::flat_set<std::string>());
}

// A connect to a simple HTTPS endpoint produces SSL params wrapping transport
// params.
TEST_P(ConnectJobParamsFactoryTest, HttpsEndpoint) {
  // HTTPS endpoints are not supported without ALPN.
  if (alpn_mode() == ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 82);
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, ProxyChain::Direct(), TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<SSLSocketParams> ssl_socket_params =
      ExpectSSLSocketParams(params);
  SSLConfig ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(ssl_socket_params, "ssl_socket_params",
                        HostPortPair::FromSchemeHostPort(kEndpoint), ssl_config,
                        privacy_mode(), kEndpointNak);
  scoped_refptr<TransportSocketParams> transport_socket_params =
      ssl_socket_params->GetDirectConnectionParams();
  VerifyTransportSocketParams(
      transport_socket_params, "transport_socket_params", kEndpoint,
      secure_dns_policy(), kEndpointNak, AlpnProtoStringsForMode(alpn_mode()));
}

// A connect to a endpoint SSL, specified as a `SchemelessEndpoint`,
// produces just transport params.
TEST_P(ConnectJobParamsFactoryTest, EncryptedEndpointWithoutScheme) {
  // Encrypted endpoints without scheme are only supported without ALPN.
  if (alpn_mode() != ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const ConnectJobFactory::SchemelessEndpoint kEndpoint{
      /*using_ssl=*/true, HostPortPair("test", 4433)};
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, ProxyChain::Direct(), TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<SSLSocketParams> ssl_socket_params =
      ExpectSSLSocketParams(params);
  SSLConfig ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(ssl_socket_params, "ssl_socket_params",
                        HostPortPair("test", 4433), ssl_config, privacy_mode(),
                        kEndpointNak);
  scoped_refptr<TransportSocketParams> transport_socket_params =
      ssl_socket_params->GetDirectConnectionParams();
  VerifyTransportSocketParams(
      transport_socket_params, "transport_socket_params",
      HostPortPair("test", 4433), secure_dns_policy(), kEndpointNak,
      AlpnProtoStringsForMode(alpn_mode()));
}

// A connection to an HTTP endpoint via an HTTPS proxy, without forcing a
// tunnel, sets up an HttpProxySocketParams, wrapping SSLSocketParams wrapping
// TransportSocketParams, intending to use GET to the proxy. This is not
// tunneled.
TEST_P(ConnectJobParamsFactoryTest, HttpEndpointViaHttpsProxy) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTPS, "proxy", 443);
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params =
      ExpectHttpProxySocketParams(params);
  VerifyHttpProxySocketParams(
      http_proxy_socket_params, "http_proxy_socket_params",
      /*quic_ssl_config=*/std::nullopt,
      HostPortPair::FromSchemeHostPort(kEndpoint), proxy_chain,
      /*proxy_chain_index=*/0,
      /*tunnel=*/false, kEndpointNak, secure_dns_policy());

  scoped_refptr<SSLSocketParams> ssl_socket_params =
      http_proxy_socket_params->ssl_params();
  ASSERT_TRUE(ssl_socket_params);
  SSLConfig ssl_config = SSLConfigForProxy();
  VerifySSLSocketParams(ssl_socket_params, "ssl_socket_params",
                        HostPortPair::FromString("proxy:443"), ssl_config,
                        PrivacyMode::PRIVACY_MODE_DISABLED, kEndpointNak);

  scoped_refptr<TransportSocketParams> transport_socket_params =
      ssl_socket_params->GetDirectConnectionParams();
  VerifyTransportSocketParams(
      transport_socket_params, "transport_socket_params",
      HostPortPair("proxy", 443), secure_dns_policy(), kProxyDnsNak,
      AlpnProtoStringsForMode(alpn_mode()));
}

// A connection to an HTTP endpoint via an QUIC proxy sets up an
// HttpProxySocketParams, wrapping almost-unused SSLSocketParams, intending to
// use GET to the proxy. This is not tunneled.
TEST_P(ConnectJobParamsFactoryTest, HttpEndpointViaQuicProxy) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC, "proxy",
                                         443),
  });
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  auto http_proxy_socket_params = ExpectHttpProxySocketParams(params);
  SSLConfig quic_ssl_config = SSLConfigForProxy();
  // Traffic always tunnels over QUIC proxies.
  const bool tunnel = true;
  VerifyHttpProxySocketParams(
      http_proxy_socket_params, "http_proxy_socket_params", quic_ssl_config,
      HostPortPair::FromSchemeHostPort(kEndpoint), proxy_chain,
      /*proxy_chain_index=*/0, tunnel, kEndpointNak, secure_dns_policy());
}

// A connection to an HTTPS endpoint via an HTTPS proxy,
// sets up an SSLSocketParams, wrapping HttpProxySocketParams, wrapping
// SSLSocketParams, wrapping TransportSocketParams. This is always tunneled.
TEST_P(ConnectJobParamsFactoryTest, HttpsEndpointViaHttpsProxy) {
  // HTTPS endpoints are not supported without ALPN.
  if (alpn_mode() == ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::FromSchemeHostAndPort(
      ProxyServer::SCHEME_HTTPS, "proxy", 443);
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<SSLSocketParams> endpoint_ssl_socket_params =
      ExpectSSLSocketParams(params);
  SSLConfig endpoint_ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(endpoint_ssl_socket_params,
                        "endpoint_ssl_socket_params",
                        HostPortPair::FromSchemeHostPort(kEndpoint),
                        endpoint_ssl_config, privacy_mode(), kEndpointNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params =
      endpoint_ssl_socket_params->GetHttpProxyConnectionParams();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params, "http_proxy_socket_params",
      /*quic_ssl_config=*/std::nullopt,
      HostPortPair::FromSchemeHostPort(kEndpoint), proxy_chain,
      /*proxy_chain_index=*/0,
      /*tunnel=*/true, kEndpointNak, secure_dns_policy());

  scoped_refptr<SSLSocketParams> proxy_ssl_socket_params =
      http_proxy_socket_params->ssl_params();
  ASSERT_TRUE(proxy_ssl_socket_params);
  SSLConfig proxy_ssl_config = SSLConfigForProxy();
  VerifySSLSocketParams(proxy_ssl_socket_params, "proxy_ssl_socket_params",
                        HostPortPair::FromString("proxy:443"), proxy_ssl_config,
                        PrivacyMode::PRIVACY_MODE_DISABLED, kEndpointNak);

  scoped_refptr<TransportSocketParams> transport_socket_params =
      proxy_ssl_socket_params->GetDirectConnectionParams();
  VerifyTransportSocketParams(
      transport_socket_params, "transport_socket_params",
      HostPortPair("proxy", 443), secure_dns_policy(), kProxyDnsNak,
      AlpnProtoStringsForMode(alpn_mode()));
}

// A connection to an HTTPS endpoint via a QUIC proxy,
// sets up an SSLSocketParams, wrapping HttpProxySocketParams, wrapping
// SSLSocketParams. This is always tunneled.
TEST_P(ConnectJobParamsFactoryTest, HttpsEndpointViaQuicProxy) {
  // HTTPS endpoints are not supported without ALPN.
  if (alpn_mode() == ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC, "proxy",
                                         443),
  });
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  auto endpoint_ssl_socket_params = ExpectSSLSocketParams(params);
  SSLConfig endpoint_ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(endpoint_ssl_socket_params,
                        "endpoint_ssl_socket_params",
                        HostPortPair::FromSchemeHostPort(kEndpoint),
                        endpoint_ssl_config, privacy_mode(), kEndpointNak);

  auto http_proxy_socket_params =
      endpoint_ssl_socket_params->GetHttpProxyConnectionParams();
  SSLConfig quic_ssl_config = SSLConfigForProxy();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params, "http_proxy_socket_params", quic_ssl_config,
      HostPortPair::FromSchemeHostPort(kEndpoint), proxy_chain,
      /*proxy_chain_index=*/0,
      /*tunnel=*/true, kEndpointNak, secure_dns_policy());
}

// A connection to an HTTPS endpoint via an HTTP proxy
// sets up an SSLSocketParams, wrapping HttpProxySocketParams, wrapping
// TransportSocketParams. This is always tunneled.
TEST_P(ConnectJobParamsFactoryTest, HttpsEndpointViaHttpProxy) {
  if (alpn_mode() == ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 82);
  ProxyChain proxy_chain =
      ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTP, "proxy", 80);
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<SSLSocketParams> endpoint_ssl_socket_params =
      ExpectSSLSocketParams(params);
  SSLConfig endpoint_ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(endpoint_ssl_socket_params,
                        "endpoint_ssl_socket_params",
                        HostPortPair::FromSchemeHostPort(kEndpoint),
                        endpoint_ssl_config, privacy_mode(), kEndpointNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params =
      endpoint_ssl_socket_params->GetHttpProxyConnectionParams();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params, "http_proxy_socket_params",
      /*quic_ssl_config=*/std::nullopt,
      HostPortPair::FromSchemeHostPort(kEndpoint), proxy_chain,
      /*proxy_chain_index=*/0,
      /*tunnel=*/true, kEndpointNak, secure_dns_policy());

  scoped_refptr<TransportSocketParams> transport_socket_params =
      http_proxy_socket_params->transport_params();
  ASSERT_TRUE(transport_socket_params);
  VerifyTransportSocketParams(transport_socket_params,
                              "transport_socket_params",
                              HostPortPair("proxy", 80), secure_dns_policy(),
                              kProxyDnsNak, base::flat_set<std::string>({}));
}

// A connection to an HTTP endpoint via a SOCKS proxy,
// sets up an SOCKSSocketParams wrapping TransportSocketParams.
TEST_P(ConnectJobParamsFactoryTest, HttpEndpointViaSOCKSProxy) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::FromSchemeHostAndPort(
      ProxyServer::SCHEME_SOCKS4, "proxy", 999);
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<SOCKSSocketParams> socks_socket_params =
      ExpectSOCKSSocketParams(params);
  VerifySOCKSSocketParams(socks_socket_params, "socks_socket_params",
                          /*is_socks_v5=*/false,
                          HostPortPair::FromSchemeHostPort(kEndpoint),
                          kEndpointNak);

  scoped_refptr<TransportSocketParams> transport_socket_params =
      socks_socket_params->transport_params();
  VerifyTransportSocketParams(
      transport_socket_params, "transport_socket_params",
      HostPortPair("proxy", 999), secure_dns_policy(), kProxyDnsNak, {});
}

// A connection to an HTTPS endpoint via a SOCKS proxy,
// sets up an SSLSocketParams wrapping SOCKSSocketParams wrapping
// TransportSocketParams.
TEST_P(ConnectJobParamsFactoryTest, HttpsEndpointViaSOCKSProxy) {
  // HTTPS endpoints are not supported without ALPN.
  if (alpn_mode() == ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::FromSchemeHostAndPort(
      ProxyServer::SCHEME_SOCKS5, "proxy", 999);
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<SSLSocketParams> endpoint_ssl_socket_params =
      ExpectSSLSocketParams(params);
  SSLConfig endpoint_ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(endpoint_ssl_socket_params,
                        "endpoint_ssl_socket_params",
                        HostPortPair::FromSchemeHostPort(kEndpoint),
                        endpoint_ssl_config, privacy_mode(), kEndpointNak);

  scoped_refptr<SOCKSSocketParams> socks_socket_params =
      endpoint_ssl_socket_params->GetSocksProxyConnectionParams();
  VerifySOCKSSocketParams(socks_socket_params, "socks_socket_params",
                          /*is_socks_v5=*/true,
                          HostPortPair::FromSchemeHostPort(kEndpoint),
                          kEndpointNak);

  scoped_refptr<TransportSocketParams> transport_socket_params =
      socks_socket_params->transport_params();
  VerifyTransportSocketParams(
      transport_socket_params, "transport_socket_params",
      HostPortPair("proxy", 999), secure_dns_policy(), kProxyDnsNak, {});
}

// A connection to an HTTP endpoint via a two-proxy HTTPS chain
// sets up the required parameters.
TEST_P(ConnectJobParamsFactoryTest, HttpEndpointViaHttpsProxyViaHttpsProxy) {
  const url::SchemeHostPort kEndpoint(url::kHttpScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "proxya",
                                         443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "proxyb",
                                         443),
  });
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_b =
      ExpectHttpProxySocketParams(params);
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_b, "http_proxy_socket_params_b",
      /*quic_ssl_config=*/std::nullopt,
      HostPortPair::FromSchemeHostPort(kEndpoint), proxy_chain,
      /*proxy_chain_index=*/1,
      /*tunnel=*/true, kEndpointNak, secure_dns_policy());

  scoped_refptr<SSLSocketParams> proxy_ssl_socket_params_b =
      http_proxy_socket_params_b->ssl_params();
  ASSERT_TRUE(proxy_ssl_socket_params_b);
  SSLConfig proxy_ssl_config = SSLConfigForProxy();
  VerifySSLSocketParams(proxy_ssl_socket_params_b, "proxy_ssl_socket_params_b",
                        HostPortPair::FromString("proxyb:443"),
                        proxy_ssl_config, PrivacyMode::PRIVACY_MODE_DISABLED,
                        kEndpointNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_a =
      proxy_ssl_socket_params_b->GetHttpProxyConnectionParams();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_a, "http_proxy_socket_params_a",
      /*quic_ssl_config=*/std::nullopt, HostPortPair("proxyb", 443),
      proxy_chain,
      /*proxy_chain_index=*/0,
      /*tunnel=*/true,
      partition_proxy_chains() ? kEndpointNak : NetworkAnonymizationKey(),
      secure_dns_policy());

  scoped_refptr<SSLSocketParams> proxy_ssl_socket_params_a =
      http_proxy_socket_params_a->ssl_params();
  ASSERT_TRUE(proxy_ssl_socket_params_a);
  VerifySSLSocketParams(
      proxy_ssl_socket_params_a, "proxy_ssl_socket_params_a",
      HostPortPair::FromString("proxya:443"), proxy_ssl_config,
      PrivacyMode::PRIVACY_MODE_DISABLED,
      partition_proxy_chains() ? kEndpointNak : NetworkAnonymizationKey());

  scoped_refptr<TransportSocketParams> transport_socket_params =
      proxy_ssl_socket_params_a->GetDirectConnectionParams();
  VerifyTransportSocketParams(
      transport_socket_params, "transport_socket_params",
      HostPortPair("proxya", 443), secure_dns_policy(), kProxyDnsNak,
      AlpnProtoStringsForMode(alpn_mode()));
}

// A connection to an HTTPS endpoint via a two-proxy HTTPS chain
// sets up the required parameters.
TEST_P(ConnectJobParamsFactoryTest, HttpsEndpointViaHttpsProxyViaHttpsProxy) {
  // HTTPS endpoints are not supported without ALPN.
  if (alpn_mode() == ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "proxya",
                                         443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "proxyb",
                                         443),
  });
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<SSLSocketParams> endpoint_ssl_socket_params =
      ExpectSSLSocketParams(params);
  SSLConfig endpoint_ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(endpoint_ssl_socket_params,
                        "endpoint_ssl_socket_params",
                        HostPortPair::FromSchemeHostPort(kEndpoint),
                        endpoint_ssl_config, privacy_mode(), kEndpointNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_b =
      endpoint_ssl_socket_params->GetHttpProxyConnectionParams();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_b, "http_proxy_socket_params_b",
      /*quic_ssl_config=*/std::nullopt,
      HostPortPair::FromSchemeHostPort(kEndpoint), proxy_chain,
      /*proxy_chain_index=*/1,
      /*tunnel=*/true, kEndpointNak, secure_dns_policy());

  scoped_refptr<SSLSocketParams> proxy_ssl_socket_params_b =
      http_proxy_socket_params_b->ssl_params();
  ASSERT_TRUE(proxy_ssl_socket_params_b);
  SSLConfig proxy_ssl_config = SSLConfigForProxy();
  VerifySSLSocketParams(proxy_ssl_socket_params_b, "proxy_ssl_socket_params_b",
                        HostPortPair::FromString("proxyb:443"),
                        proxy_ssl_config, PrivacyMode::PRIVACY_MODE_DISABLED,
                        kEndpointNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_a =
      proxy_ssl_socket_params_b->GetHttpProxyConnectionParams();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_a, "http_proxy_socket_params_a",
      /*quic_ssl_config=*/std::nullopt, HostPortPair("proxyb", 443),
      proxy_chain,
      /*proxy_chain_index=*/0,
      /*tunnel=*/true,
      partition_proxy_chains() ? kEndpointNak : NetworkAnonymizationKey(),
      secure_dns_policy());

  scoped_refptr<SSLSocketParams> proxy_ssl_socket_params_a =
      http_proxy_socket_params_a->ssl_params();
  ASSERT_TRUE(proxy_ssl_socket_params_a);
  VerifySSLSocketParams(
      proxy_ssl_socket_params_a, "proxy_ssl_socket_params_a",
      HostPortPair::FromString("proxya:443"), proxy_ssl_config,
      PrivacyMode::PRIVACY_MODE_DISABLED,
      partition_proxy_chains() ? kEndpointNak : NetworkAnonymizationKey());

  scoped_refptr<TransportSocketParams> transport_socket_params =
      proxy_ssl_socket_params_a->GetDirectConnectionParams();
  VerifyTransportSocketParams(
      transport_socket_params, "transport_socket_params",
      HostPortPair("proxya", 443), secure_dns_policy(), kProxyDnsNak,
      AlpnProtoStringsForMode(alpn_mode()));
}

// A connection to an HTTPS endpoint via a two-proxy chain mixing QUIC and HTTPS
// sets up the required parameters.
TEST_P(ConnectJobParamsFactoryTest, HttpsEndpointViaHttpsProxyViaQuicProxy) {
  // HTTPS endpoints are not supported without ALPN.
  if (alpn_mode() == ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC, "proxya",
                                         443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "proxyb",
                                         443),
  });
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  scoped_refptr<SSLSocketParams> endpoint_ssl_socket_params =
      ExpectSSLSocketParams(params);
  SSLConfig endpoint_ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(endpoint_ssl_socket_params,
                        "endpoint_ssl_socket_params",
                        HostPortPair::FromSchemeHostPort(kEndpoint),
                        endpoint_ssl_config, privacy_mode(), kEndpointNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_b =
      endpoint_ssl_socket_params->GetHttpProxyConnectionParams();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_b, "http_proxy_socket_params_b",
      /*quic_ssl_config=*/std::nullopt,
      HostPortPair::FromSchemeHostPort(kEndpoint), proxy_chain,
      /*proxy_chain_index=*/1,
      /*tunnel=*/true, kEndpointNak, secure_dns_policy());

  scoped_refptr<SSLSocketParams> proxy_ssl_socket_params_b =
      http_proxy_socket_params_b->ssl_params();
  ASSERT_TRUE(proxy_ssl_socket_params_b);
  SSLConfig proxy_ssl_config = SSLConfigForProxy();
  VerifySSLSocketParams(proxy_ssl_socket_params_b, "proxy_ssl_socket_params_b",
                        HostPortPair::FromString("proxyb:443"),
                        proxy_ssl_config, PrivacyMode::PRIVACY_MODE_DISABLED,
                        kEndpointNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_a =
      proxy_ssl_socket_params_b->GetHttpProxyConnectionParams();
  SSLConfig quic_ssl_config = SSLConfigForProxy();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_a, "http_proxy_socket_params_a", quic_ssl_config,
      HostPortPair("proxyb", 443), proxy_chain,
      /*proxy_chain_index=*/0,
      /*tunnel=*/true,
      partition_proxy_chains() ? kEndpointNak : NetworkAnonymizationKey(),
      secure_dns_policy());
}

// A connection to an HTTPS endpoint via a two-proxy QUIC chain
// sets up the required parameters.
TEST_P(ConnectJobParamsFactoryTest, HttpsEndpointViaQuicProxyViaQuicProxy) {
  // HTTPS endpoints are not supported without ALPN.
  if (alpn_mode() == ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC, "proxya",
                                         443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC, "proxyb",
                                         443),
  });
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  auto endpoint_ssl_socket_params = ExpectSSLSocketParams(params);
  SSLConfig endpoint_ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(endpoint_ssl_socket_params,
                        "endpoint_ssl_socket_params",
                        HostPortPair::FromSchemeHostPort(kEndpoint),
                        endpoint_ssl_config, privacy_mode(), kEndpointNak);

  auto http_proxy_socket_params_b =
      endpoint_ssl_socket_params->GetHttpProxyConnectionParams();
  SSLConfig quic_ssl_config_b = SSLConfigForProxy();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_b, "http_proxy_socket_params_b",
      quic_ssl_config_b, HostPortPair::FromSchemeHostPort(kEndpoint),
      proxy_chain,
      /*proxy_chain_index=*/1,
      /*tunnel=*/true, kEndpointNak, secure_dns_policy());
}

// A connection to an HTTPS endpoint via a proxy chain with two HTTPS proxies
// and two QUIC proxies.
TEST_P(ConnectJobParamsFactoryTest, HttpsEndpointViaMixedProxyChain) {
  // HTTPS endpoints are not supported without ALPN.
  if (alpn_mode() == ConnectJobFactory::AlpnMode::kDisabled) {
    return;
  }

  const url::SchemeHostPort kEndpoint(url::kHttpsScheme, "test", 82);
  ProxyChain proxy_chain = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC, "proxya",
                                         443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC, "proxyb",
                                         443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "proxyc",
                                         443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::SCHEME_HTTPS, "proxyd",
                                         443),
  });
  ConnectJobParams params = ConstructConnectJobParams(
      kEndpoint, proxy_chain, TRAFFIC_ANNOTATION_FOR_TESTS,
      /*allowed_bad_certs=*/{}, alpn_mode(),
      /*force_tunnel=*/false, privacy_mode(), OnHostResolutionCallback(),
      kEndpointNak, secure_dns_policy(), disable_cert_network_fetches(),
      &common_connect_job_params_, kProxyDnsNak);

  auto endpoint_ssl_socket_params = ExpectSSLSocketParams(params);
  SSLConfig endpoint_ssl_config = SSLConfigForEndpoint();
  VerifySSLSocketParams(endpoint_ssl_socket_params,
                        "endpoint_ssl_socket_params",
                        HostPortPair::FromSchemeHostPort(kEndpoint),
                        endpoint_ssl_config, privacy_mode(), kEndpointNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_d =
      endpoint_ssl_socket_params->GetHttpProxyConnectionParams();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_d, "http_proxy_socket_params_d",
      /*quic_ssl_config=*/std::nullopt,
      HostPortPair::FromSchemeHostPort(kEndpoint), proxy_chain,
      /*proxy_chain_index=*/3,
      /*tunnel=*/true, kEndpointNak, secure_dns_policy());

  scoped_refptr<SSLSocketParams> proxy_ssl_socket_params_d =
      http_proxy_socket_params_d->ssl_params();
  ASSERT_TRUE(proxy_ssl_socket_params_d);
  SSLConfig proxy_ssl_config = SSLConfigForProxy();
  VerifySSLSocketParams(proxy_ssl_socket_params_d, "proxy_ssl_socket_params_d",
                        HostPortPair::FromString("proxyd:443"),
                        proxy_ssl_config, PrivacyMode::PRIVACY_MODE_DISABLED,
                        kEndpointNak);

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_c =
      proxy_ssl_socket_params_d->GetHttpProxyConnectionParams();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_c, "http_proxy_socket_params_c",
      /*quic_ssl_config=*/std::nullopt, HostPortPair("proxyd", 443),
      proxy_chain,
      /*proxy_chain_index=*/2,
      /*tunnel=*/true,
      partition_proxy_chains() ? kEndpointNak : NetworkAnonymizationKey(),
      secure_dns_policy());

  scoped_refptr<SSLSocketParams> proxy_ssl_socket_params_c =
      http_proxy_socket_params_c->ssl_params();
  ASSERT_TRUE(proxy_ssl_socket_params_c);
  VerifySSLSocketParams(
      proxy_ssl_socket_params_c, "proxy_ssl_socket_params_c",
      HostPortPair::FromString("proxyc:443"), proxy_ssl_config,
      PrivacyMode::PRIVACY_MODE_DISABLED,
      partition_proxy_chains() ? kEndpointNak : NetworkAnonymizationKey());

  auto http_proxy_socket_params_b =
      proxy_ssl_socket_params_c->GetHttpProxyConnectionParams();
  SSLConfig quic_ssl_config_b = SSLConfigForProxy();
  VerifyHttpProxySocketParams(
      http_proxy_socket_params_b, "http_proxy_socket_params_b",
      quic_ssl_config_b, HostPortPair("proxyc", 443), proxy_chain,
      /*proxy_chain_index=*/1,
      /*tunnel=*/true,
      partition_proxy_chains() ? kEndpointNak : NetworkAnonymizationKey(),
      secure_dns_policy());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ConnectJobParamsFactoryTest,
    testing::ConvertGenerator<TestParams::ParamTuple>(testing::Combine(
        testing::Values(false, true),
        testing::Values(PrivacyMode::PRIVACY_MODE_ENABLED,
                        PrivacyMode::PRIVACY_MODE_DISABLED),
        testing::Values(SecureDnsPolicy::kAllow, SecureDnsPolicy::kDisable),
        testing::Values(ConnectJobFactory::AlpnMode::kDisabled,
                        ConnectJobFactory::AlpnMode::kHttp11Only,
                        ConnectJobFactory::AlpnMode::kHttpAll),
        testing::Values(false, true),
        testing::Values(false, true))));

}  // namespace

}  // namespace net
