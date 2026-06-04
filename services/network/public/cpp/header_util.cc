// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/header_util.h"

#include <map>
#include <string>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
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

  if (base::EqualsCaseInsensitiveASCII(key, "X-HTTP-Method") ||
      base::EqualsCaseInsensitiveASCII(key, "X-HTTP-Method-Override") ||
      base::EqualsCaseInsensitiveASCII(key, "X-Method-Override")) {
    net::HttpUtil::ValuesIterator method_iterator(value, ',');
    while (method_iterator.GetNext()) {
      if (cors::IsForbiddenMethod(method_iterator.value())) {
        return false;
      }
    }
  }

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

bool ContainsForbiddenSecurityHeader(net::HttpRequestHeaders& headers) {
  static const bool enabled =
      base::FeatureList::IsEnabled(features::kRestrictForbiddenSecurityHeaders);
  if (!enabled) {
    return false;
  }

  std::map<std::string, std::string> headers_to_truncate;

  auto sanitize_and_check_security_header = [&](std::string_view name,
                                                std::string_view value) {
    // Client Hints are harmless and set by the renderer.
    if (base::StartsWith(name, "Sec-CH-",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      size_t size = value.size();
      base::UmaHistogramCounts10000("NetworkService.SecCHHeaderSize", size);
      if (size > 1024) {
        headers_to_truncate[std::string(name)] =
            std::string(value.substr(0, 1024));
      }
      return true;
    }
    // Sec-Purpose is used for prefetch hints and contains short strings.
    if (base::EqualsCaseInsensitiveASCII(name, "Sec-Purpose")) {
      return value.size() < 256;
    }
    // Sec-GPC is allowed with value "1".
    // https://w3c.github.io/gpc/#the-sec-gpc-header-field-for-http-requests
    if (base::EqualsCaseInsensitiveASCII(name, "Sec-GPC")) {
      return value == "1";
    }
    // Browsing Topics API headers contain structured interest tokens.
    if (base::EqualsCaseInsensitiveASCII(name, "Sec-Browsing-Topics")) {
      return value.size() < 1024;
    }
    // Shared Storage and FLEDGE fetch headers use structured boolean "?1".
    if (base::EqualsCaseInsensitiveASCII(name, "Sec-Shared-Storage-Writable") ||
        base::EqualsCaseInsensitiveASCII(name, "Sec-Ad-Auction-Fetch")) {
      return value == "?1";
    }
    // Shared Storage data origin headers contain origin URLs.
    if (base::EqualsCaseInsensitiveASCII(name,
                                         "Sec-Shared-Storage-Data-Origin")) {
      return value.size() <= 267;
    }
    // Speculation Rules headers contain comma-separated tokens.
    if (base::EqualsCaseInsensitiveASCII(name, "Sec-Speculation-Tags")) {
      size_t size = value.size();
      base::UmaHistogramCounts10000(
          "NetworkService.SecSpeculationTagsHeaderSize", size);
      if (size > 2048) {
        std::string_view truncated_value = value.substr(0, 2048);
        size_t last_comma = truncated_value.rfind(',');
        if (last_comma != std::string_view::npos) {
          truncated_value = truncated_value.substr(0, last_comma);
        }
        headers_to_truncate[std::string(name)] = std::string(truncated_value);
      }
      return true;
    }
    // FLEDGE/Protected Audience auction headers contain encoded auction
    // signals.
    if (base::StartsWith(name, "Sec-Ad-Auction-",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      size_t size = value.size();
      base::UmaHistogramCounts10000("NetworkService.SecAdAuctionHeaderSize",
                                    size);
      if (size > 2048) {
        headers_to_truncate[std::string(name)] =
            std::string(value.substr(0, 2048));
      }
      return true;
    }
    return false;
  };

  net::HttpRequestHeaders::Iterator it(headers);
  while (it.GetNext()) {
    if (base::StartsWith(it.name(), "Sec-",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      if (!sanitize_and_check_security_header(it.name(), it.value())) {
        return true;
      }
    }
  }

  for (const auto& [name, value] : headers_to_truncate) {
    headers.SetHeader(name, value);
  }

  return false;
}

mojom::ReferrerPolicy ParseReferrerPolicy(
    const net::HttpResponseHeaders& response_headers) {
  using enum net::ReferrerPolicy;
  using enum mojom::ReferrerPolicy;
  std::optional<std::string> referrer_policy_header =
      response_headers.GetNormalizedHeader("Referrer-Policy");
  if (!referrer_policy_header) {
    return kDefault;
  }

  std::optional<net::ReferrerPolicy> net_policy =
      net::ReferrerPolicyFromHeader(*referrer_policy_header);

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
  return net::ShouldSniffMimeType(url, response.headers.get(),
                                  response.mime_type);
}

bool IsSuccessfulStatus(int status) {
  // This contains successful 2xx status code.
  return status >= net::HTTP_OK && status < net::HTTP_MULTIPLE_CHOICES;
}

}  // namespace network
