// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/connect_job_params_factory.h"

#include <optional>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/socket/connect_job_params.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_connect_job.h"
#include "net/ssl/ssl_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

// Populates `ssl_config's` ALPN-related fields. Namely, `alpn_protos`,
// `application_settings`, `renego_allowed_default`, and
// `renego_allowed_for_protos`.
//
// In the case of `AlpnMode::kDisabled`, clears all of the fields.
//
// In the case of `AlpnMode::kHttp11Only`, sets `alpn_protos` to only allow
// HTTP/1.1 negotiation.
//
// In the case of `AlpnMode::kHttpAll`, copies `alpn_protos` from
// `common_connect_job_params`, and gives `HttpServerProperties` a chance to
// force use of HTTP/1.1 only.
//
// If `alpn_mode` is not `AlpnMode::kDisabled`, then `server` must be a
// `SchemeHostPort`, as it makes no sense to negotiate ALPN when the scheme
// isn't known.
void ConfigureAlpn(const ConnectJobFactory::Endpoint& endpoint,
                   ConnectJobFactory::AlpnMode alpn_mode,
                   const NetworkAnonymizationKey& network_anonymization_key,
                   const CommonConnectJobParams& common_connect_job_params,
                   SSLConfig& ssl_config,
                   bool renego_allowed) {
  if (alpn_mode == ConnectJobFactory::AlpnMode::kDisabled) {
    ssl_config.alpn_protos = {};
    ssl_config.application_settings = {};
    ssl_config.renego_allowed_default = false;
    return;
  }

  DCHECK(absl::holds_alternative<url::SchemeHostPort>(endpoint));

  if (alpn_mode == ConnectJobFactory::AlpnMode::kHttp11Only) {
    ssl_config.alpn_protos = {kProtoHTTP11};
    ssl_config.application_settings =
        *common_connect_job_params.application_settings;
  } else {
    DCHECK_EQ(alpn_mode, ConnectJobFactory::AlpnMode::kHttpAll);
    DCHECK(absl::holds_alternative<url::SchemeHostPort>(endpoint));
    ssl_config.alpn_protos = *common_connect_job_params.alpn_protos;
    ssl_config.application_settings =
        *common_connect_job_params.application_settings;
    if (common_connect_job_params.http_server_properties) {
      common_connect_job_params.http_server_properties->MaybeForceHTTP11(
          absl::get<url::SchemeHostPort>(endpoint), network_anonymization_key,
          &ssl_config);
    }
  }

  // Prior to HTTP/2 and SPDY, some servers used TLS renegotiation to request
  // TLS client authentication after the HTTP request was sent. Allow
  // renegotiation for only those connections.
  //
  // Note that this does NOT implement the provision in
  // https://http2.github.io/http2-spec/#rfc.section.9.2.1 which allows the
  // server to request a renegotiation immediately before sending the
  // connection preface as waiting for the preface would cost the round trip
  // that False Start otherwise saves.
  ssl_config.renego_allowed_default = renego_allowed;
  if (renego_allowed) {
    ssl_config.renego_allowed_for_protos = {kProtoHTTP11};
  }
}

base::flat_set<std::string> SupportedProtocolsFromSSLConfig(
    const SSLConfig& config) {
  // We convert because `SSLConfig` uses `NextProto` for ALPN protocols while
  // `TransportConnectJob` and DNS logic needs `std::string`. See
  // https://crbug.com/1286835.
  return base::MakeFlatSet<std::string>(config.alpn_protos, /*comp=*/{},
                                        NextProtoToString);
}

HostPortPair ToHostPortPair(const ConnectJobFactory::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return HostPortPair::FromSchemeHostPort(
        absl::get<url::SchemeHostPort>(endpoint));
  }

  DCHECK(
      absl::holds_alternative<ConnectJobFactory::SchemelessEndpoint>(endpoint));
  return absl::get<ConnectJobFactory::SchemelessEndpoint>(endpoint)
      .host_port_pair;
}

TransportSocketParams::Endpoint ToTransportEndpoint(
    const ConnectJobFactory::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return absl::get<url::SchemeHostPort>(endpoint);
  }

  DCHECK(
      absl::holds_alternative<ConnectJobFactory::SchemelessEndpoint>(endpoint));
  return absl::get<ConnectJobFactory::SchemelessEndpoint>(endpoint)
      .host_port_pair;
}

bool UsingSsl(const ConnectJobFactory::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return GURL::SchemeIsCryptographic(
        base::ToLowerASCII(absl::get<url::SchemeHostPort>(endpoint).scheme()));
  }

  DCHECK(
      absl::holds_alternative<ConnectJobFactory::SchemelessEndpoint>(endpoint));
  return absl::get<ConnectJobFactory::SchemelessEndpoint>(endpoint).using_ssl;
}

ConnectJobParams MakeSSLSocketParams(
    ConnectJobParams params,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return ConnectJobParams(base::MakeRefCounted<SSLSocketParams>(
      std::move(params), host_and_port, ssl_config, network_anonymization_key));
}

// Recursively generate the params for a proxy at `host_port_pair` and the given
// index in the proxy chain. This proceeds from the end of the proxy chain back
// to the first proxy server.
ConnectJobParams CreateProxyParams(
    HostPortPair host_port_pair,
    bool should_tunnel,
    const ConnectJobFactory::Endpoint& endpoint,
    const ProxyChain& proxy_chain,
    size_t proxy_chain_index,
    const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const OnHostResolutionCallback& resolution_callback,
    const NetworkAnonymizationKey& endpoint_network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    const CommonConnectJobParams* common_connect_job_params,
    const NetworkAnonymizationKey& proxy_dns_network_anonymization_key) {
  const ProxyServer& proxy_server =
      proxy_chain.GetProxyServer(proxy_chain_index);

  // If the requested session will be used to speak to a downstream proxy, then
  // it need not be partitioned based on the ultimate destination's NAK. If the
  // session is to the destination, then partition using that destination's NAK.
  // This allows sharing of connections to proxies in multi-server proxy chains.
  bool use_empty_nak =
      !base::FeatureList::IsEnabled(net::features::kPartitionProxyChains) &&
      proxy_chain_index < proxy_chain.length() - 1;
  // Note that C++ extends the lifetime of this value such that the reference
  // remains valid as long as the reference.
  const NetworkAnonymizationKey& network_anonymization_key =
      use_empty_nak ? NetworkAnonymizationKey()
                    : endpoint_network_anonymization_key;

  // Set up the SSLConfig if using SSL to the proxy.
  SSLConfig proxy_server_ssl_config;

  if (proxy_server.is_secure_http_like()) {
    // Disable cert verification network fetches for secure proxies, since
    // those network requests are probably going to need to go through the
    // proxy chain too.
    //
    // Any proxy-specific SSL behavior here should also be configured for
    // QUIC proxies.
    proxy_server_ssl_config.disable_cert_verification_network_fetches = true;
    ConfigureAlpn(url::SchemeHostPort(url::kHttpsScheme,
                                      proxy_server.host_port_pair().host(),
                                      proxy_server.host_port_pair().port()),
                  // Always enable ALPN for proxies.
                  ConnectJobFactory::AlpnMode::kHttpAll,
                  network_anonymization_key, *common_connect_job_params,
                  proxy_server_ssl_config,
                  /*renego_allowed=*/false);
  }

  // Create the nested parameters over which the connection to the proxy
  // will be made.
  ConnectJobParams params;

  if (proxy_server.is_quic()) {
    // If this and all proxies earlier in the chain are QUIC, then we can hand
    // off the remainder of the proxy connecting work to the QuicSocketPool, so
    // no further recursion is required. If any proxies earlier in the chain are
    // not QUIC, then the chain is unsupported. Such ProxyChains cannot be
    // constructed, so this is just a double-check.
    for (size_t i = 0; i < proxy_chain_index; i++) {
      CHECK(proxy_chain.GetProxyServer(i).is_quic());
    }
    return ConnectJobParams(base::MakeRefCounted<HttpProxySocketParams>(
        std::move(proxy_server_ssl_config), host_port_pair, proxy_chain,
        proxy_chain_index, should_tunnel, *proxy_annotation_tag,
        network_anonymization_key, secure_dns_policy));
  } else if (proxy_chain_index == 0) {
    // At the beginning of the chain, create the only TransportSocketParams
    // object, corresponding to the transport socket we want to create to the
    // first proxy.
    // TODO(crbug.com/40181080): For an http-like proxy, should this pass a
    // `SchemeHostPort`, so proxies can participate in ECH? Note doing so
    // with `SCHEME_HTTP` requires handling the HTTPS record upgrade.
    params = ConnectJobParams(base::MakeRefCounted<TransportSocketParams>(
        proxy_server.host_port_pair(), proxy_dns_network_anonymization_key,
        secure_dns_policy, resolution_callback,
        SupportedProtocolsFromSSLConfig(proxy_server_ssl_config)));
  } else {
    params = CreateProxyParams(
        proxy_server.host_port_pair(), true, endpoint, proxy_chain,
        proxy_chain_index - 1, proxy_annotation_tag, resolution_callback,
        endpoint_network_anonymization_key, secure_dns_policy,
        common_connect_job_params, proxy_dns_network_anonymization_key);
  }

  // For secure connections, wrap the underlying connection params in SSL
  // params.
  if (proxy_server.is_secure_http_like()) {
    params =
        MakeSSLSocketParams(std::move(params), proxy_server.host_port_pair(),
                            proxy_server_ssl_config, network_anonymization_key);
  }

  // Further wrap the underlying connection params, or the SSL params wrapping
  // them, with the proxy params.
  if (proxy_server.is_http_like()) {
    CHECK(!proxy_server.is_quic());
    params = ConnectJobParams(base::MakeRefCounted<HttpProxySocketParams>(
        std::move(params), host_port_pair, proxy_chain, proxy_chain_index,
        should_tunnel, *proxy_annotation_tag, network_anonymization_key,
        secure_dns_policy));
  } else {
    DCHECK(proxy_server.is_socks());
    DCHECK_EQ(1u, proxy_chain.length());
    // TODO(crbug.com/40181080): Pass `endpoint` directly (preserving scheme
    // when available)?
    params = ConnectJobParams(base::MakeRefCounted<SOCKSSocketParams>(
        std::move(params), proxy_server.scheme() == ProxyServer::SCHEME_SOCKS5,
        ToHostPortPair(endpoint), network_anonymization_key,
        *proxy_annotation_tag));
  }

  return params;
}

}  // namespace

ConnectJobParams ConstructConnectJobParams(
    const ConnectJobFactory::Endpoint& endpoint,
    const ProxyChain& proxy_chain,
    const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    ConnectJobFactory::AlpnMode alpn_mode,
    bool force_tunnel,
    PrivacyMode privacy_mode,
    const OnHostResolutionCallback& resolution_callback,
    const NetworkAnonymizationKey& endpoint_network_anonymization_key,
    SecureDnsPolicy secure_dns_policy,
    bool disable_cert_network_fetches,
    const CommonConnectJobParams* common_connect_job_params,
    const NetworkAnonymizationKey& proxy_dns_network_anonymization_key) {
  DCHECK(proxy_chain.IsValid());

  // Set up `ssl_config` if using SSL to the endpoint.
  SSLConfig ssl_config;
  if (UsingSsl(endpoint)) {
    ssl_config.allowed_bad_certs = allowed_bad_certs;
    ssl_config.privacy_mode = privacy_mode;

    ConfigureAlpn(endpoint, alpn_mode, endpoint_network_anonymization_key,
                  *common_connect_job_params, ssl_config,
                  /*renego_allowed=*/true);

    ssl_config.disable_cert_verification_network_fetches =
        disable_cert_network_fetches;

    // TODO(crbug.com/41459647): Also enable 0-RTT for TLS proxies.
    ssl_config.early_data_enabled =
        *common_connect_job_params->enable_early_data;
  }

  // Create the nested parameters over which the connection to the endpoint
  // will be made.
  ConnectJobParams params;
  if (proxy_chain.is_direct()) {
    params = ConnectJobParams(base::MakeRefCounted<TransportSocketParams>(
        ToTransportEndpoint(endpoint), endpoint_network_anonymization_key,
        secure_dns_policy, resolution_callback,
        SupportedProtocolsFromSSLConfig(ssl_config)));
  } else {
    bool should_tunnel = force_tunnel || UsingSsl(endpoint) ||
                         !proxy_chain.is_get_to_proxy_allowed();
    // Begin creating params for the last proxy in the chain. This will
    // recursively create params "backward" through the chain to the first.
    params = CreateProxyParams(
        ToHostPortPair(endpoint), should_tunnel, endpoint, proxy_chain,
        /*proxy_chain_index=*/proxy_chain.length() - 1, proxy_annotation_tag,
        resolution_callback, endpoint_network_anonymization_key,
        secure_dns_policy, common_connect_job_params,
        proxy_dns_network_anonymization_key);
  }

  if (UsingSsl(endpoint)) {
    // Wrap the final params (which includes connections through zero or more
    // proxies) in SSLSocketParams to handle SSL to to the endpoint.
    // TODO(crbug.com/40181080): Pass `endpoint` directly (preserving scheme
    // when available)?
    params =
        MakeSSLSocketParams(std::move(params), ToHostPortPair(endpoint),
                            ssl_config, endpoint_network_anonymization_key);
  }

  return params;
}

}  // namespace net
