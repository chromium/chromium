// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/net/ip_address_space_util.h"

#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace blink {
namespace {

using network::mojom::IPAddressSpace;
using network::mojom::URLResponseHead;

// Helper for CalculateClientAddressSpace() with the same arguments.
//
// If the response was fetched via service workers, returns the last URL in the
// list. Otherwise returns |request_url|.
//
// See: https://fetch.spec.whatwg.org/#concept-response-url-list
const GURL& ResponseUrl(const GURL& request_url,
                        const URLResponseHead* response_head) {
  if (response_head && !response_head->url_list_via_service_worker.empty()) {
    return response_head->url_list_via_service_worker.back();
  }

  return request_url;
}

}  // namespace

IPAddressSpace CalculateClientAddressSpace(
    const GURL& url,
    const URLResponseHead* response_head) {
  if (ResponseUrl(url, response_head).SchemeIsFile()) {
    // See: https://wicg.github.io/cors-rfc1918/#file-url.
    return IPAddressSpace::kLocal;
  }

  if (!response_head) {
    return IPAddressSpace::kUnknown;
  }

  // First, check whether the response forces itself into a public address space
  // as per https://wicg.github.io/cors-rfc1918/#csp.
  DCHECK(response_head->parsed_headers)
      << "CalculateIPAddressSpace() called for URL " << url
      << " with null parsed_headers.";
  if (response_head->parsed_headers &&
      network::ShouldTreatAsPublicAddress(
          response_head->parsed_headers->content_security_policy)) {
    return IPAddressSpace::kPublic;
  }

  // Otherwise, calculate the address space via the provided IP address.
  return network::IPEndPointToIPAddressSpace(response_head->remote_endpoint);
}

IPAddressSpace CalculateResourceAddressSpace(const GURL& url,
                                             const net::IPEndPoint& endpoint) {
  if (url.SchemeIsFile()) {
    // See: https://wicg.github.io/cors-rfc1918/#file-url.
    return IPAddressSpace::kLocal;
  }

  return network::IPEndPointToIPAddressSpace(endpoint);
}

}  // namespace blink
