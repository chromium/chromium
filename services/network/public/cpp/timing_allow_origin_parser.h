// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TIMING_ALLOW_ORIGIN_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TIMING_ALLOW_ORIGIN_PARSER_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "services/network/public/mojom/timing_allow_origin.mojom-forward.h"
#include "services/network/public/mojom/timing_allow_origin.mojom.h"
#include "url/origin.h"

namespace network {

// Parses the Timing-Allow-Origin as defined in the Resource Timing Level 2
// standard (https://w3c.github.io/resource-timing/#sec-timing-allow-origin).
// Typically, callers will use this to perform a TAO check as defined in
// https://fetch.spec.whatwg.org/#concept-tao-check. Never returns a null
// object.
//
// Note: This function intentionally takes a `std::string` argument rather than
// a `URLResponseHead` to allow easier reuse between Blink and network service.
// In an ideal world though, Blink would be able to use `URLResponseHead` as
// well...
//
// TODO(crbug.com/40177882): Use std::string_view here, though this
// requires fixing a lot of other plumbing.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::TimingAllowOriginPtr ParseTimingAllowOrigin(const std::string& value);

COMPONENT_EXPORT(NETWORK_CPP)
bool TimingAllowOriginCheck(const mojom::TimingAllowOriginPtr& tao,
                            const url::Origin& origin);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TIMING_ALLOW_ORIGIN_PARSER_H_
