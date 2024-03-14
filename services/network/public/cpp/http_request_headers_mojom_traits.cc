// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/http_request_headers_mojom_traits.h"

#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/crash_keys.h"

namespace mojo {

// static
bool StructTraits<network::mojom::HttpRequestHeaderKeyValuePairDataView,
                  net::HttpRequestHeaders::HeaderKeyValuePair>::
    Read(network::mojom::HttpRequestHeaderKeyValuePairDataView data,
         net::HttpRequestHeaders::HeaderKeyValuePair* item) {
  if (!data.ReadKey(&item->key))
    return false;
  if (!net::HttpUtil::IsValidHeaderName(item->key)) {
    network::debug::SetDeserializationCrashKeyString("header_key");
    return false;
  }
  if (!data.ReadValue(&item->value))
    return false;
  item->value = std::string(net::HttpUtil::TrimLWS(item->value));
  if (!net::HttpUtil::IsValidHeaderValue(item->value)) {
    network::debug::SetDeserializationCrashKeyString("header_value");
    return false;
  }
  return true;
}

// static
bool StructTraits<network::mojom::HttpRequestHeadersDataView,
                  net::HttpRequestHeaders>::
    Read(network::mojom::HttpRequestHeadersDataView data,
         net::HttpRequestHeaders* headers) {
  ArrayDataView<network::mojom::HttpRequestHeaderKeyValuePairDataView>
      data_view;
  data.GetHeadersDataView(&data_view);
  for (size_t i = 0; i < data_view.size(); ++i) {
    net::HttpRequestHeaders::HeaderKeyValuePair pair;
    if (!data_view.Read(i, &pair))
      return false;
    headers->SetHeader(pair.key, pair.value);
  }
  return true;
}

}  // namespace mojo
