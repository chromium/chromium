// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_PARSER_H_

#include "base/component_export.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {

struct CrossOriginOpenerPolicy;

// https://html.spec.whatwg.org/multipage/browsers.html#obtain-coop
COMPONENT_EXPORT(NETWORK_CPP)
CrossOriginOpenerPolicy ParseCrossOriginOpenerPolicy(
    const net::HttpResponseHeaders& headers);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_PARSER_H_
