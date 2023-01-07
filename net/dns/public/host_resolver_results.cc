// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/host_resolver_results.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

HostResolverEndpointResult::HostResolverEndpointResult() = default;
HostResolverEndpointResult::~HostResolverEndpointResult() = default;
HostResolverEndpointResult::HostResolverEndpointResult(
    const HostResolverEndpointResult&) = default;
HostResolverEndpointResult::HostResolverEndpointResult(
    HostResolverEndpointResult&&) = default;

}  // namespace net
