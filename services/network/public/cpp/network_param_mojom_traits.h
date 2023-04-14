// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/network_param.mojom-shared.h"
#include "url/mojom/scheme_host_port_mojom_traits.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::AuthChallengeInfoDataView,
                 net::AuthChallengeInfo> {
 public:
  static bool is_proxy(const net::AuthChallengeInfo& auth_challenge_info) {
    return auth_challenge_info.is_proxy;
  }
  static const url::SchemeHostPort& challenger(
      const net::AuthChallengeInfo& auth_challenge_info) {
    return auth_challenge_info.challenger;
  }
  static const std::string& scheme(
      const net::AuthChallengeInfo& auth_challenge_info) {
    return auth_challenge_info.scheme;
  }
  static const std::string& realm(
      const net::AuthChallengeInfo& auth_challenge_info) {
    return auth_challenge_info.realm;
  }
  static const std::string& challenge(
      const net::AuthChallengeInfo& auth_challenge_info) {
    return auth_challenge_info.challenge;
  }
  static const std::string& path(
      const net::AuthChallengeInfo& auth_challenge_info) {
    return auth_challenge_info.path;
  }

  static bool Read(network::mojom::AuthChallengeInfoDataView data,
                   net::AuthChallengeInfo* out);
};

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::HttpVersionDataView, net::HttpVersion> {
 public:
  static uint16_t major_value(net::HttpVersion version) {
    return version.major_value();
  }
  static uint16_t minor_value(net::HttpVersion version) {
    return version.minor_value();
  }

  static bool Read(network::mojom::HttpVersionDataView data,
                   net::HttpVersion* out);
};

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::ResolveErrorInfoDataView,
                 net::ResolveErrorInfo> {
 public:
  static int error(const net::ResolveErrorInfo& resolve_error_info) {
    return resolve_error_info.error;
  }

  static bool is_secure_network_error(
      const net::ResolveErrorInfo& resolve_error_info) {
    return resolve_error_info.is_secure_network_error;
  }

  static bool Read(network::mojom::ResolveErrorInfoDataView data,
                   net::ResolveErrorInfo* out);
};

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::HostPortPairDataView, net::HostPortPair> {
 public:
  static const std::string& host(const net::HostPortPair& host_port_pair) {
    return host_port_pair.host();
  }

  static uint16_t port(const net::HostPortPair& host_port_pair) {
    return host_port_pair.port();
  }

  static bool Read(network::mojom::HostPortPairDataView data,
                   net::HostPortPair* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_MOJOM_TRAITS_H_
