// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_HTTP_RESPONSE_HEADERS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_HTTP_RESPONSE_HEADERS_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/http_response_headers.mojom-shared.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    StructTraits<network::mojom::HttpResponseHeadersDataView,
                 scoped_refptr<net::HttpResponseHeaders>> {
 public:
  static bool IsNull(const scoped_refptr<net::HttpResponseHeaders>& headers) {
    return !headers;
  }
  static void SetToNull(scoped_refptr<net::HttpResponseHeaders>* output) {
    *output = nullptr;
  }

  static std::vector<uint8_t> headers(
      const scoped_refptr<net::HttpResponseHeaders>& headers);

  static bool Read(network::mojom::HttpResponseHeadersDataView data,
                   scoped_refptr<net::HttpResponseHeaders>* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_HTTP_RESPONSE_HEADERS_MOJOM_TRAITS_H_
