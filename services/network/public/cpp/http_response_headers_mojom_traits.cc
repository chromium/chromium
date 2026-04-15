// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/http_response_headers_mojom_traits.h"

#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "mojo/public/cpp/bindings/array_data_view.h"

namespace mojo {

// static
std::vector<uint8_t> StructTraits<network::mojom::HttpResponseHeadersDataView,
                                  scoped_refptr<net::HttpResponseHeaders>>::
    headers(const scoped_refptr<net::HttpResponseHeaders>& headers) {
  return headers->SerializeForMojoIpc();
}

// static
bool StructTraits<network::mojom::HttpResponseHeadersDataView,
                  scoped_refptr<net::HttpResponseHeaders>>::
    Read(network::mojom::HttpResponseHeadersDataView data,
         scoped_refptr<net::HttpResponseHeaders>* out) {
  mojo::ArrayDataView<uint8_t> headers_data;
  data.GetHeadersDataView(&headers_data);
  *out = base::MakeRefCounted<net::HttpResponseHeaders>(
      base::as_string_view(headers_data));
  return true;
}

}  // namespace mojo
