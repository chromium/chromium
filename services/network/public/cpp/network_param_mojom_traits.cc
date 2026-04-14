// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_param_mojom_traits.h"

#include <string_view>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/string_view_util.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/log/net_log_source_type.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "services/network/public/cpp/ct_policy_status_mojom_traits.h"
#include "services/network/public/cpp/http_response_headers_mojom_traits.h"
#include "services/network/public/cpp/ocsp_verify_result_mojom_traits.h"
#include "services/network/public/cpp/signed_certificate_timestamp_and_status_mojom_traits.h"
#include "services/network/public/cpp/x509_certificate_mojom_traits.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace mojo {

// static
bool StructTraits<network::mojom::HttpVersionDataView, net::HttpVersion>::Read(
    network::mojom::HttpVersionDataView data,
    net::HttpVersion* out) {
  *out = net::HttpVersion(data.major_value(), data.minor_value());
  return true;
}

// static
bool StructTraits<
    network::mojom::ResolveErrorInfoDataView,
    net::ResolveErrorInfo>::Read(network::mojom::ResolveErrorInfoDataView data,
                                 net::ResolveErrorInfo* out) {
  // There should not be a secure network error if the error code indicates no
  // error.
  if (data.error() == net::OK && data.is_secure_network_error()) {
    return false;
  }
  *out = net::ResolveErrorInfo(data.error(), data.is_secure_network_error());
  return true;
}

// static
bool StructTraits<network::mojom::HostPortPairDataView, net::HostPortPair>::
    Read(network::mojom::HostPortPairDataView data, net::HostPortPair* out) {
  std::string host;
  if (!data.ReadHost(&host)) {
    return false;
  }
  *out = net::HostPortPair(std::move(host), data.port());
  return true;
}

network::mojom::ProxyScheme
EnumTraits<network::mojom::ProxyScheme, net::ProxyServer::Scheme>::ToMojom(
    net::ProxyServer::Scheme scheme) {
  using net::ProxyServer;
  switch (scheme) {
    case ProxyServer::SCHEME_INVALID:
      return network::mojom::ProxyScheme::kInvalid;
    case ProxyServer::SCHEME_HTTP:
      return network::mojom::ProxyScheme::kHttp;
    case ProxyServer::SCHEME_SOCKS4:
      return network::mojom::ProxyScheme::kSocks4;
    case ProxyServer::SCHEME_SOCKS5:
      return network::mojom::ProxyScheme::kSocks5;
    case ProxyServer::SCHEME_HTTPS:
      return network::mojom::ProxyScheme::kHttps;
    case ProxyServer::SCHEME_QUIC:
      return network::mojom::ProxyScheme::kQuic;
  }
  NOTREACHED();
}

net::ProxyServer::Scheme
EnumTraits<network::mojom::ProxyScheme, net::ProxyServer::Scheme>::FromMojom(
    network::mojom::ProxyScheme scheme) {
  using net::ProxyServer;
  switch (scheme) {
    case network::mojom::ProxyScheme::kInvalid:
      return ProxyServer::SCHEME_INVALID;
    case network::mojom::ProxyScheme::kHttp:
      return ProxyServer::SCHEME_HTTP;
    case network::mojom::ProxyScheme::kSocks4:
      return ProxyServer::SCHEME_SOCKS4;
    case network::mojom::ProxyScheme::kSocks5:
      return ProxyServer::SCHEME_SOCKS5;
    case network::mojom::ProxyScheme::kHttps:
      return ProxyServer::SCHEME_HTTPS;
    case network::mojom::ProxyScheme::kQuic:
      return ProxyServer::SCHEME_QUIC;
  }
  NOTREACHED();
}

std::optional<net::HostPortPair>
StructTraits<network::mojom::ProxyServerDataView,
             net::ProxyServer>::host_and_port(const net::ProxyServer& s) {
  if (s.scheme() == net::ProxyServer::SCHEME_INVALID) {
    return std::nullopt;
  }
  return s.host_port_pair();
}

bool StructTraits<network::mojom::ProxyServerDataView, net::ProxyServer>::Read(
    network::mojom::ProxyServerDataView data,
    net::ProxyServer* out) {
  net::ProxyServer::Scheme scheme;
  if (!data.ReadScheme(&scheme)) {
    return false;
  }

  std::optional<net::HostPortPair> host_and_port;
  if (!data.ReadHostAndPort(&host_and_port)) {
    return false;
  }

  if (scheme == net::ProxyServer::SCHEME_INVALID) {
    if (host_and_port) {
      return false;
    }
    *out = net::ProxyServer(scheme, net::HostPortPair());
    return true;
  } else {
    if (!host_and_port) {
      return false;
    }
    *out = net::ProxyServer(scheme, std::move(*host_and_port));
    return true;
  }
}

bool StructTraits<network::mojom::ProxyChainDataView, net::ProxyChain>::Read(
    network::mojom::ProxyChainDataView data,
    net::ProxyChain* out) {
  std::optional<std::vector<net::ProxyServer>> proxy_servers;
  if (!data.ReadProxyServers(&proxy_servers)) {
    return false;
  }

  if (proxy_servers.has_value()) {
    int chain_id = data.ip_protection_chain_id();
    if (chain_id != net::ProxyChain::kNotIpProtectionChainId) {
      *out =
          net::ProxyChain::ForIpProtection(std::move(*proxy_servers), chain_id);
    } else {
      *out = net::ProxyChain(std::move(*proxy_servers));
    }
    if (!out->IsValid()) {
      return false;
    }
  } else {
    *out = net::ProxyChain();
  }

  return true;
}

// static
bool StructTraits<network::mojom::SSLCertRequestInfoDataView,
                  scoped_refptr<net::SSLCertRequestInfo>>::
    Read(network::mojom::SSLCertRequestInfoDataView data,
         scoped_refptr<net::SSLCertRequestInfo>* out) {
  net::HostPortPair host_and_port;
  if (!data.ReadHostAndPort(&host_and_port)) {
    return false;
  }
  std::vector<std::string> cert_authorities;
  if (!data.ReadCertAuthorities(&cert_authorities)) {
    return false;
  }
  std::vector<uint16_t> signature_algorithms;
  if (!data.ReadSignatureAlgorithms(&signature_algorithms)) {
    return false;
  }

  auto ssl_cert_request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();
  ssl_cert_request_info->host_and_port = std::move(host_and_port);
  ssl_cert_request_info->is_proxy = data.is_proxy();
  ssl_cert_request_info->cert_authorities = std::move(cert_authorities);
  ssl_cert_request_info->signature_algorithms = std::move(signature_algorithms);

  *out = ssl_cert_request_info;

  return true;
}

// static
bool StructTraits<network::mojom::NetLogSourceDataView, net::NetLogSource>::
    Read(network::mojom::NetLogSourceDataView data, net::NetLogSource* out) {
  if (data.source_type() >=
      static_cast<uint32_t>(net::NetLogSourceType::COUNT)) {
    return false;
  }
  base::TimeTicks start_time;
  if (!data.ReadStartTime(&start_time)) {
    return false;
  }
  *out =
      net::NetLogSource(static_cast<net::NetLogSourceType>(data.source_type()),
                        data.source_id(), start_time);
  return true;
}

}  // namespace mojo
