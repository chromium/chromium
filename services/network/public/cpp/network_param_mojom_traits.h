// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/auth.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/network_param.mojom-shared.h"
#include "url/mojom/origin_mojom_traits.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::AuthChallengeInfoDataView,
                 net::AuthChallengeInfo> {
 public:
  static bool is_proxy(const net::AuthChallengeInfo& auth_challenge_info) {
    return auth_challenge_info.is_proxy;
  }
  static const url::Origin& challenger(
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
class COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::HttpVersionDataView, net::HttpVersion> {
 public:
  static int16_t major_value(net::HttpVersion version) {
    return version.major_value();
  }
  static int16_t minor_value(net::HttpVersion version) {
    return version.minor_value();
  }

  static bool Read(network::mojom::HttpVersionDataView data,
                   net::HttpVersion* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_PARAM_MOJOM_TRAITS_H_
