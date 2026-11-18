// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_POLICY_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_POLICY_PARSER_H_

#include <string_view>

#include "base/component_export.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {
struct IntegrityPolicy;

enum class IntegrityPolicyHeaderType { kEnforce, kReportOnly };

// Parses the Integrity-Policy and the Integrity-Policy-Report-Only headers.
// These headers enable developers to enforce that Subresource Integrity is used
// in their documents when loading resources of certain destinations. Currently
// the only supported destination is "script".
// https://w3c.github.io/webappsec-subresource-integrity/#integrity-policy-section
COMPONENT_EXPORT(NETWORK_CPP_INTEGRITY_POLICY)
IntegrityPolicy ParseIntegrityPolicyFromHeaders(
    const net::HttpResponseHeaders& headers,
    IntegrityPolicyHeaderType header_type);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_POLICY_PARSER_H_
