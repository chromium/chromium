// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/resource_broker/eligibility_evaluator.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

namespace {

// Allows standard `Vary` headers commonly sent by CDNs on static assets for
// content negotiation (`Accept-Encoding`, `Accept`) and Fetch Metadata or CORS
// (`Origin`, `Sec-Fetch-*`). Note that `BodyPump` observes decoded bytes after
// `Accept-Encoding` is stripped. Other headers like `Cookie`, `Authorization`,
// `User-Agent`, or `*` indicate request-specific responses.
bool HasDisallowedVary(const net::HttpResponseHeaders& headers) {
  size_t iter = 0;
  while (std::optional<std::string_view> field =
             headers.EnumerateHeader(&iter, "Vary")) {
    if (field->empty()) {
      continue;
    }
    if (!base::EqualsCaseInsensitiveASCII(*field, "Accept-Encoding") &&
        !base::EqualsCaseInsensitiveASCII(*field, "Accept") &&
        !base::EqualsCaseInsensitiveASCII(*field, "Origin") &&
        !base::EqualsCaseInsensitiveASCII(*field, "Sec-Fetch-Dest") &&
        !base::EqualsCaseInsensitiveASCII(*field, "Sec-Fetch-Mode") &&
        !base::EqualsCaseInsensitiveASCII(*field, "Sec-Fetch-Site")) {
      return true;
    }
  }
  return false;
}

bool IsCacheableStable(const network::mojom::URLResponseHead& head,
                       bool request_has_authorization_header) {
  if (request_has_authorization_header || head.did_use_server_http_auth) {
    return false;
  }
  if (!head.headers || head.headers->response_code() != net::HTTP_OK) {
    return false;
  }
  // `Cache-Control: private` is allowed because it only restricts shared
  // caches, and signed-in web apps commonly serve static bundles with
  // `Cache-Control: private, max-age=...`. `GetFreshnessLifetimes()` handles
  // `no-store`, `no-cache`, `Pragma: no-cache`, `max-age`, `Expires`, and
  // `Last-Modified` heuristic freshness.
  const base::Time response_time =
      head.response_time.is_null() ? base::Time::Now() : head.response_time;
  if (head.headers->GetFreshnessLifetimes(response_time).freshness.is_zero()) {
    return false;
  }
  if (HasDisallowedVary(*head.headers)) {
    return false;
  }
  if (head.headers->HasHeader("WWW-Authenticate") ||
      head.headers->HasHeader("Proxy-Authenticate")) {
    return false;
  }
  return true;
}

}  // namespace

uint32_t EvaluateHeadEligibility(const network::mojom::URLResponseHead& head,
                                 bool request_has_authorization_header) {
  uint32_t failures = kEligible;

  if (head.response_type ==
          network::mojom::FetchResponseType::kOpaqueRedirect ||
      head.response_type == network::mojom::FetchResponseType::kError) {
    failures |= kInvalidResponseType;
  }

  if (head.has_range_requested ||
      (head.headers &&
       head.headers->response_code() == net::HTTP_PARTIAL_CONTENT)) {
    failures |= kPartialContent;
  }

  if (!IsCacheableStable(head, request_has_authorization_header)) {
    failures |= kNotCacheableStable;
  }

  return failures;
}

uint32_t EvaluateBodyEligibility(uint64_t body_size) {
  return body_size < kMinEligibleBodySize ? kBelowMinSize : kEligible;
}

uint32_t EvaluateEligibility(const network::mojom::URLResponseHead& head,
                             bool request_has_authorization_header,
                             uint64_t body_size) {
  return EvaluateHeadEligibility(head, request_has_authorization_header) |
         EvaluateBodyEligibility(body_size);
}

}  // namespace content
