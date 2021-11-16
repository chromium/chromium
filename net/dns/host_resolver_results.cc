// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_results.h"

namespace net {

HostResolverEndpointResult::HostResolverEndpointResult() = default;
HostResolverEndpointResult::~HostResolverEndpointResult() = default;
HostResolverEndpointResult::HostResolverEndpointResult(
    const HostResolverEndpointResult&) = default;
HostResolverEndpointResult::HostResolverEndpointResult(
    HostResolverEndpointResult&&) = default;

}  // namespace net
