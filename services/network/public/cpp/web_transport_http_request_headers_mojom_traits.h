// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_HTTP_REQUEST_HEADERS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_HTTP_REQUEST_HEADERS_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/web_transport.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::WebTransportHttpRequestHeadersDataView,
                 std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>> {
  static const std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>&
  headers(
      const std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>& input) {
    return input;
  }
  static bool Read(
      network::mojom::WebTransportHttpRequestHeadersDataView data,
      std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_HTTP_REQUEST_HEADERS_MOJOM_TRAITS_H_
