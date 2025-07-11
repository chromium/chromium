// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "third_party/blink/renderer/platform/network/http_request_headers_mojom_traits.h"

namespace mojo {

// static
blink::Vector<network::mojom::blink::HttpRequestHeaderKeyValuePairPtr>
StructTraits<network::mojom::HttpRequestHeadersDataView,
             blink::HTTPHeaderMap>::headers(const blink::HTTPHeaderMap& map) {
  std::unique_ptr<blink::CrossThreadHTTPHeaderMapData> headers = map.CopyData();
  blink::Vector<network::mojom::blink::HttpRequestHeaderKeyValuePairPtr>
      headers_out;
  for (const auto& header : *headers) {
    auto header_ptr =
        network::mojom::blink::HttpRequestHeaderKeyValuePair::New();
    header_ptr->key = header.first;
    header_ptr->value = header.second.Utf8();
    headers_out.push_back(std::move(header_ptr));
  }
  return headers_out;
}

// static
bool StructTraits<
    network::mojom::HttpRequestHeadersDataView,
    blink::HTTPHeaderMap>::Read(network::mojom::HttpRequestHeadersDataView data,
                                blink::HTTPHeaderMap* out) {
  blink::Vector<network::mojom::blink::HttpRequestHeaderKeyValuePairPtr>
      headers;
  if (!data.ReadHeaders(&headers)) {
    return false;
  }
  out->Clear();
  for (const auto& header : headers) {
    out->Set(blink::AtomicString(header->key),
             blink::AtomicString(blink::String(header->value)));
  }
  return true;
}

}  // namespace mojo
