// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SHARED_HTTP_CACHE_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SHARED_HTTP_CACHE_UTIL_H_

#include "base/component_export.h"

namespace network {

struct ResourceRequest;

// Returns whether `request` is eligible for writing to / storing in the
// Renderer Accessible HTTP Cache (SharedHttpCache).
//
// Only static subresources (images, scripts, styles, and fonts) fetched via
// GET over HTTPS without Range headers, and without LOAD_DISABLE_CACHE,
// are eligible to be stored.
//
// Note: Requests with LOAD_BYPASS_CACHE (shift-reload) bypass reading from the
// cache, but the fresh network response is still written to the cache.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsRequestEligibleForSharedHttpCacheWrite(const ResourceRequest& request);

// Returns whether `request` is eligible for cache lookup in the
// Renderer Accessible HTTP Cache (SharedHttpCache).
//
// In addition to write eligibility, the request must not bypass or force
// validation of the cache (LOAD_BYPASS_CACHE or LOAD_VALIDATE_CACHE) and must
// not be a revalidation request.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsRequestEligibleForSharedHttpCacheLookup(const ResourceRequest& request);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SHARED_HTTP_CACHE_UTIL_H_
