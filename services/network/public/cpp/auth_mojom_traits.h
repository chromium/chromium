// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_AUTH_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_AUTH_MOJOM_TRAITS_H_

#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/auth.h"
#include "services/network/public/mojom/auth.mojom-shared.h"
#include "url/scheme_host_port.h"

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
    StructTraits<network::mojom::AuthCredentialsDataView,
                 net::AuthCredentials> {
 public:
  static const std::u16string& username(
      const net::AuthCredentials& auth_credentials) {
    return auth_credentials.username();
  }
  static const std::u16string& password(
      const net::AuthCredentials& auth_credentials) {
    return auth_credentials.password();
  }

  static bool Read(network::mojom::AuthCredentialsDataView data,
                   net::AuthCredentials* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_AUTH_MOJOM_TRAITS_H_
