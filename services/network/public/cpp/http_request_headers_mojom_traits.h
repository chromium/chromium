// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HTTP_REQUEST_HEADERS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HTTP_REQUEST_HEADERS_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/http_request_headers.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::HttpRequestHeaderKeyValuePairDataView,
                 net::HttpRequestHeaders::HeaderKeyValuePair> {
  static const std::string& key(
      const net::HttpRequestHeaders::HeaderKeyValuePair& item) {
    return item.key;
  }
  static const std::string& value(
      const net::HttpRequestHeaders::HeaderKeyValuePair& item) {
    return item.value;
  }
  static bool Read(network::mojom::HttpRequestHeaderKeyValuePairDataView data,
                   net::HttpRequestHeaders::HeaderKeyValuePair* item);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::HttpRequestHeadersDataView,
                 net::HttpRequestHeaders> {
  static net::HttpRequestHeaders::HeaderVector headers(
      const net::HttpRequestHeaders& data) {
    return data.GetHeaderVector();
  }
  static bool Read(network::mojom::HttpRequestHeadersDataView data,
                   net::HttpRequestHeaders* headers);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HTTP_REQUEST_HEADERS_MOJOM_TRAITS_H_
