// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/network_utils.h"

#include "media/media_buildflags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/buildflags.h"

namespace blink {
namespace network_utils {

namespace {

constexpr char kJsonAcceptHeader[] = "application/json,*/*;q=0.5";
constexpr char kStylesheetAcceptHeader[] = "text/css,*/*;q=0.1";
constexpr char kWebBundleAcceptHeader[] = "application/webbundle;v=b2";

}  // namespace

bool AlwaysAccessNetwork(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  if (!headers)
    return false;

  // RFC 2616, section 14.9.
  return headers->HasHeaderValue("cache-control", "no-cache") ||
         headers->HasHeaderValue("cache-control", "no-store") ||
         headers->HasHeaderValue("pragma", "no-cache") ||
         headers->HasHeaderValue("vary", "*");
}

const char* ImageAcceptHeader() {
#if BUILDFLAG(ENABLE_AV1_DECODER)
  return "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#else
  return "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#endif
}

void SetAcceptHeader(net::HttpRequestHeaders& headers,
                     network::mojom::RequestDestination request_destination) {
  if (request_destination == network::mojom::RequestDestination::kStyle ||
      request_destination == network::mojom::RequestDestination::kXslt ||
      request_destination == network::mojom::RequestDestination::kWebBundle ||
      request_destination == network::mojom::RequestDestination::kJson) {
    headers.SetHeader(net::HttpRequestHeaders::kAccept,
                      GetAcceptHeaderForDestination(request_destination));
    return;
  }
  // Calling SetHeaderIfMissing() instead of SetHeader() because JS can
  // manually set an accept header on an XHR.
  headers.SetHeaderIfMissing(
      net::HttpRequestHeaders::kAccept,
      GetAcceptHeaderForDestination(request_destination));
}

const char* GetAcceptHeaderForDestination(
    network::mojom::RequestDestination request_destination) {
  if (request_destination == network::mojom::RequestDestination::kStyle ||
      request_destination == network::mojom::RequestDestination::kXslt) {
    return kStylesheetAcceptHeader;
  } else if (request_destination ==
             network::mojom::RequestDestination::kImage) {
    return ImageAcceptHeader();
  } else if (request_destination ==
             network::mojom::RequestDestination::kWebBundle) {
    return kWebBundleAcceptHeader;
  } else if (request_destination == network::mojom::RequestDestination::kJson) {
    return kJsonAcceptHeader;
  } else {
    return network::kDefaultAcceptHeaderValue;
  }
}

}  // namespace network_utils
}  // namespace blink
