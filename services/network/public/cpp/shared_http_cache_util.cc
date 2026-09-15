// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_http_cache_util.h"

#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace network {

bool IsRequestEligibleForSharedHttpCacheWrite(const ResourceRequest& request) {
  // Only HTTPS requests can be cached in the Renderer Accessible HTTP Cache.
  // Plain HTTP requests are excluded for security.
  if (!request.url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  // Only GET requests without Range headers can be cached in the Renderer
  // Accessible HTTP Cache. Non-GET or range requests (which could be sent by a
  // compromised renderer, or arise from future changes to renderer behavior)
  // must not be stored.
  if (request.method != net::HttpRequestHeaders::kGetMethod ||
      request.headers.HasHeader(net::HttpRequestHeaders::kRange)) {
    return false;
  }

  // Only static subresources (images, scripts, styles, and fonts) are supported
  // for the initial launch of the Renderer Accessible HTTP Cache. These static
  // assets represent the majority of cacheable subresources and have simple
  // lifecycles. Other destinations are currently out of scope:
  // - Documents: Handled by navigation loader with specific lifecycle and
  //   security checks.
  // - Media (audio/video): Frequently use range requests and streaming, which
  //   are unsupported.
  // - Workers (Dedicated/Shared/Service Workers): Have separate execution
  //   lifecycles and update check mechanisms.
  // - Fetches / XHR: Often contain dynamic, user-specific, or
  //   authorization-dependent data.
  if (request.destination != mojom::RequestDestination::kImage &&
      request.destination != mojom::RequestDestination::kScript &&
      request.destination != mojom::RequestDestination::kStyle &&
      request.destination != mojom::RequestDestination::kFont) {
    return false;
  }

  // Requests that disable the HTTP cache must not be written to cache.
  // Note: Requests with LOAD_BYPASS_CACHE (shift-reload) bypass reading from
  // the cache, but the fetched response is written to the cache.
  if (request.load_flags & net::LOAD_DISABLE_CACHE) {
    return false;
  }

  return true;
}

bool IsRequestEligibleForSharedHttpCacheLookup(const ResourceRequest& request) {
  if (!IsRequestEligibleForSharedHttpCacheWrite(request)) {
    return false;
  }

  // Requests that bypass the HTTP cache (e.g. shift-reload) or force cache
  // validation (e.g. reload) must not be read directly from the cache without
  // network validation.
  if (request.load_flags &
      (net::LOAD_BYPASS_CACHE | net::LOAD_VALIDATE_CACHE)) {
    return false;
  }

  // Revalidation requests cannot be served directly from the shared cache
  // without network validation.
  if (request.is_revalidating || request.revalidation_etag.has_value() ||
      request.revalidation_last_modified.has_value()) {
    return false;
  }

  return true;
}

}  // namespace network
