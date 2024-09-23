// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/header_util.h"

#include <string>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace network {

namespace {

// Headers that consumers are not trusted to set. All "Proxy-" prefixed messages
// are blocked inline. The"Authorization" auth header is deliberately not
// included, since OAuth requires websites be able to set it directly. These are
// a subset of headers forbidden by the fetch spec.
//
// This list has some values in common with
// https://fetch.spec.whatwg.org/#forbidden-header-name, but excludes some
// values that are still set by the caller in Chrome.
const char* kUnsafeHeaders[] = {
    // This is determined by the upload body and set by net/. A consumer
    // overriding that could allow for Bad Things.
    net::HttpRequestHeaders::kContentLength,

    // Disallow setting the Host header because it can conflict with specified
    // URL and logic related to isolating sites.
    net::HttpRequestHeaders::kHost,

    // Trailers are not supported.
    "Trailer", "Te",

    // Websockets use a different API.
    "Upgrade",

    // Obsolete header, and network stack manages headers itself.
    "Cookie2",

    // Not supported by net/.
    "Keep-Alive",

    // Forbidden by the fetch spec.
    net::HttpRequestHeaders::kTransferEncoding,

    // Semantically a response header, so not useful on requests.
    "Set-Cookie",

    // TODO(mmenke): Figure out what to do about the remaining headers:
    // Connection, Cookie, Date, Expect, Referer, Via.
};

// Headers that consumers are currently allowed to set, with the exception of
// certain values could cause problems.
// TODO(mmenke): Gather stats on these, and see if these headers can be banned
// outright instead.
const struct {
  const char* name;
  const char* value;
} kUnsafeHeaderValues[] = {
    // Websockets use a different API.
    {net::HttpRequestHeaders::kConnection, "Upgrade"},
};

}  // namespace

bool IsRequestHeaderSafe(std::string_view key, std::string_view value) {
  for (const auto* header : kUnsafeHeaders) {
    if (base::EqualsCaseInsensitiveASCII(header, key))
      return false;
  }

  for (const auto& header : kUnsafeHeaderValues) {
    if (base::EqualsCaseInsensitiveASCII(header.name, key) &&
        base::EqualsCaseInsensitiveASCII(header.value, value)) {
      return false;
    }
  }

  // Proxy headers are destined for the proxy, so shouldn't be set by callers.
  if (base::StartsWith(key, "Proxy-", base::CompareCase::INSENSITIVE_ASCII))
    return false;

  return true;
}

bool AreRequestHeadersSafe(const net::HttpRequestHeaders& request_headers) {
  net::HttpRequestHeaders::Iterator it(request_headers);

  while (it.GetNext()) {
    if (!IsRequestHeaderSafe(it.name(), it.value()))
      return false;
  }

  return true;
}

mojom::ReferrerPolicy ParseReferrerPolicy(
    const net::HttpResponseHeaders& response_headers) {
  using enum net::ReferrerPolicy;
  using enum mojom::ReferrerPolicy;
  std::string referrer_policy_header;
  if (!response_headers.GetNormalizedHeader("Referrer-Policy",
                                            &referrer_policy_header)) {
    return kDefault;
  }

  std::optional<net::ReferrerPolicy> net_policy =
      net::ReferrerPolicyFromHeader(referrer_policy_header);

  if (!net_policy) {
    return kDefault;
  }

  switch (net_policy.value()) {
    case NO_REFERRER:
      return kNever;
    case CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return kNoReferrerWhenDowngrade;
    case ORIGIN:
      return kOrigin;
    case ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return kOriginWhenCrossOrigin;
    case CLEAR_ON_TRANSITION_CROSS_ORIGIN:
      return kSameOrigin;
    case ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return kStrictOrigin;
    case REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return kStrictOriginWhenCrossOrigin;
    case NEVER_CLEAR:
      return kAlways;
  }

  NOTREACHED();
}

bool ShouldSniffContent(const GURL& url,
                        const mojom::URLResponseHead& response) {
  std::string content_type_options;
  if (response.headers) {
    response.headers->GetNormalizedHeader("x-content-type-options",
                                          &content_type_options);
  }
  bool sniffing_blocked =
      base::EqualsCaseInsensitiveASCII(content_type_options, "nosniff");
  bool we_would_like_to_sniff =
      net::ShouldSniffMimeType(url, response.mime_type);

  return !sniffing_blocked && we_would_like_to_sniff;
}

bool IsSuccessfulStatus(int status) {
  // This contains successful 2xx status code.
  return status >= net::HTTP_OK && status < net::HTTP_MULTIPLE_CHOICES;
}

}  // namespace network
