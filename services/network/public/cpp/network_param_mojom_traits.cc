// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_param_mojom_traits.h"

#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::AuthChallengeInfoDataView,
                  net::AuthChallengeInfo>::
    Read(network::mojom::AuthChallengeInfoDataView data,
         net::AuthChallengeInfo* out) {
  out->is_proxy = data.is_proxy();
  if (!data.ReadChallenger(&out->challenger) ||
      !data.ReadScheme(&out->scheme) || !data.ReadRealm(&out->realm) ||
      !data.ReadChallenge(&out->challenge) || !data.ReadPath(&out->path)) {
    return false;
  }
  return true;
}

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
  if (data.error() == net::OK && data.is_secure_network_error())
    return false;
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

}  // namespace mojo
