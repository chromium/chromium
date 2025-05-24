// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PREFETCH_MATCHES_H_
#define SERVICES_NETWORK_PREFETCH_MATCHES_H_

#include "base/component_export.h"

namespace network {

struct ResourceRequest;

// Returns true if `prefetch_request` is sufficiently similar to `real_request`
// that a cached response for `prefetch_request` can be used to serve
// `real_request`. It is the caller's responsibility to ensure that the render
// process is permitted to use the `request_initiator` field they supplied, and
// that the `trusted_params` field is not set on `real_request`.
bool COMPONENT_EXPORT(NETWORK_SERVICE)
    PrefetchMatches(const ResourceRequest& prefetch_request,
                    const ResourceRequest& real_request);

}  // namespace network

#endif  // SERVICES_NETWORK_PREFETCH_MATCHES_H_
