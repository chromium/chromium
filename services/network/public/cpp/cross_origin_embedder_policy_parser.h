// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_PARSER_H_

#include "base/component_export.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {

struct CrossOriginEmbedderPolicy;

// Specification:
// https://wicg.github.io/cross-origin-embedder-policy/
//
// TODO(arthursonzogni): Add a fuzzer.
COMPONENT_EXPORT(NETWORK_CPP)
CrossOriginEmbedderPolicy ParseCrossOriginEmbedderPolicy(
    const net::HttpResponseHeaders& headers);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_ORIGIN_EMBEDDER_POLICY_PARSER_H_
