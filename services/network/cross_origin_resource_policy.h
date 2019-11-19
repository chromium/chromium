// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CROSS_ORIGIN_RESOURCE_POLICY_H_
#define SERVICES_NETWORK_CROSS_ORIGIN_RESOURCE_POLICY_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/origin.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {

struct ResourceResponseInfo;

// Implementation of Cross-Origin-Resource-Policy - see:
// - https://fetch.spec.whatwg.org/#cross-origin-resource-policy-header
// - https://github.com/whatwg/fetch/issues/687
class COMPONENT_EXPORT(NETWORK_SERVICE) CrossOriginResourcePolicy {
 public:
  // Only static methods.
  CrossOriginResourcePolicy() = delete;

  // For "no-cors" fetches, the Verify method checks whether the response has a
  // Cross-Origin-Resource-Policy header which says the response should not be
  // delivered to a cross-origin or cross-site context.
  enum VerificationResult {
    kBlock,
    kAllow,
  };
  static VerificationResult Verify(
      const GURL& request_url,
      const base::Optional<url::Origin>& request_initiator,
      const ResourceResponseInfo& response,
      mojom::RequestMode request_mode,
      base::Optional<url::Origin> request_initiator_site_lock,
      mojom::CrossOriginEmbedderPolicy embedder_policy);

  // Parsing of the Cross-Origin-Resource-Policy http response header.
  enum ParsedHeader {
    kNoHeader,
    kSameOrigin,
    kSameSite,
    kCrossOrigin,
    kParsingError,
  };
  static ParsedHeader ParseHeaderForTesting(
      const net::HttpResponseHeaders* headers);

  // Determines if the response with "CORP: same-site" header should be allowed
  // for the given |initiator| and |target_origin|.  Note that responses might
  // be allowed even if schemes of |initiator| and |target_origin| differ - this
  // logic is distinct from site-related code in content::SiteInstance and/or
  // GURL::DomainIs.
  static bool ShouldAllowSameSiteForTesting(const url::Origin& initiator,
                                            const url::Origin& target_origin);
};

}  // namespace network

#endif  // SERVICES_NETWORK_CROSS_ORIGIN_RESOURCE_POLICY_H_
