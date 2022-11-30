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

// Parsing is done following the COOP spec draft:
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
// TODO(ahemery): add a fuzzer for the parser, see
// services/network/content_security_policy_fuzzer.cc for an example.
COMPONENT_EXPORT(NETWORK_CPP)
CrossOriginOpenerPolicy ParseCrossOriginOpenerPolicy(
    const net::HttpResponseHeaders& headers);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_OPENER_POLICY_PARSER_H_
