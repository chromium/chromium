// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_RESOURCE_BROKER_ELIGIBILITY_EVALUATOR_H_
#define CONTENT_SERVICES_RESOURCE_BROKER_ELIGIBILITY_EVALUATOR_H_

#include <cstdint>

#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace content {

// Bitmask of reasons why a response is ineligible for cross-renderer
// deduplication. Multiple reasons may be set on the same response.
enum EligibilityFailureReason : uint32_t {
  kEligible = 0,
  // Error or opaque-redirect response with no resource body. Plain `kOpaque`
  // (`no-cors`) responses still deliver their full body to the renderer and are
  // eligible.
  kInvalidResponseType = 1 << 0,
  // Non-200 status, missing headers, zero freshness (`no-store`, `no-cache`,
  // `max-age=0`, expired `Expires`, `Pragma: no-cache`, or no freshness
  // headers), disallowed `Vary`, or HTTP authentication. Responses requiring
  // revalidation or general request-header `Vary` matching are intentionally
  // excluded to measure what a broker can share without them. Browser-managed
  // subresource `Vary` headers (`Accept-Encoding`, `Accept`, `Origin`,
  // `Sec-Fetch-*`) and `Cache-Control: private` remain eligible.
  kNotCacheableStable = 1 << 1,
  // Body size is below `kMinEligibleBodySize`.
  kBelowMinSize = 1 << 2,
  // Range request or 206 Partial Content response.
  kPartialContent = 1 << 3,
};

// Minimum body size considered worth deduplicating (16 KiB).
inline constexpr uint64_t kMinEligibleBodySize = 16 * 1024;

// Evaluates eligibility checks that depend only on the request and response
// head, before the body is read. `request_has_authorization_header` indicates
// whether the request included an `Authorization` header.
uint32_t EvaluateHeadEligibility(const network::mojom::URLResponseHead& head,
                                 bool request_has_authorization_header);

// Evaluates eligibility checks that depend on the completed body size.
uint32_t EvaluateBodyEligibility(uint64_t body_size);

// Combines `EvaluateHeadEligibility()` and `EvaluateBodyEligibility()`.
uint32_t EvaluateEligibility(const network::mojom::URLResponseHead& head,
                             bool request_has_authorization_header,
                             uint64_t body_size);

}  // namespace content

#endif  // CONTENT_SERVICES_RESOURCE_BROKER_ELIGIBILITY_EVALUATOR_H_
