// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/web_transport_http_request_headers_mojom_traits.h"

#include "base/strings/string_util.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/crash_keys.h"
#include "services/network/public/cpp/http_request_headers_mojom_traits.h"
#include "services/network/public/mojom/http_request_headers.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<network::mojom::WebTransportHttpRequestHeadersDataView,
                  std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>>::
    Read(network::mojom::WebTransportHttpRequestHeadersDataView data,
         std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>* out) {
  if (!data.ReadHeaders(out)) {
    return false;
  }
  for (const auto& pair : *out) {
    // https://w3c.github.io/webtransport/
    if (base::EqualsCaseInsensitiveASCII(pair.key, "wt-available-protocols")) {
      network::debug::SetDeserializationCrashKeyString(
          "webtransport_reserved_header");
      return false;
    }
    // https://fetch.spec.whatwg.org/#forbidden-request-header
    if (!net::HttpUtil::IsSafeHeader(pair.key, pair.value)) {
      network::debug::SetDeserializationCrashKeyString(
          "webtransport_forbidden_header");
      return false;
    }
  }
  return true;
}

}  // namespace mojo
