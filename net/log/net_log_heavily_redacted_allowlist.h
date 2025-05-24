// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_HEAVILY_REDACTED_ALLOWLIST_H_
#define NET_LOG_NET_LOG_HEAVILY_REDACTED_ALLOWLIST_H_

#include <array>

namespace net {

// The list of netlog params that will be kept even in the "heavily redacted"
// netlog capture mode. See NetLogCaptureMode::kHeavilyRedacted.
//
// Params should only be added to this list if they do not reveal any
// information that could be considered potentially privacy-sensitive. In
// particular the params in this list should not make it possible to deduce
// any specific information about the user making the request, where they are,
// the app they are using, the sites they are visiting, the application-level
// meaning of HTTP requests/responses, or which credentials are being used.
//
// Examples of information that is fine to include:
//  - Timing information;
//  - Protocol information, such as HTTP version, TLS version, IP version, etc.
//  - Information about which code branches are taken;
//  - Predefined error codes;
//  - Retry information;
//  - Request/response size information;
//  - HTTP method name;
//  - HTTP status codes;
//  - etc.
//
// Examples of information that is NOT fine to include:
//  - Any IP address (including source addresses, destination addresses,
//    DNS server addresses, etc.);
//  - Hostnames;
//  - Any part of any URL;
//  - Arbitrary HTTP header names;
//  - Arbitrary HTTP header values;
//  - Identifiable details of TLS certificates;
//  - Keys (public AND private);
//  - Raw unencrypted plaintext;
//  - Information that can make it possible to identify which app is being used
//    (in the case of embedders e.g. WebView, Cronet)
//  - User personal information e.g. user name, email;
//  - User location;
//  - Free-form unstructured fields that could potentially contain any of the
//    above;
//  - etc.
//
// TODO(https://crbug.com/410018349): this list is probably incomplete. Add
// more entries.
inline constexpr std::array kNetLogHeavilyRedactedParamAllowlist = {
    "allow_cached_response",
    "allow_dns_over_https_upgrade",
    "append_to_multi_label_name",
    "attempts",
    "backup_job",
    "byte_count",
    "cached",
    "can_use_insecure_dns_transactions",
    "can_use_secure_dns_transactions",
    "dns_over_tls_active",
    "dns_query_type",
    "doh_attempts",
    "elapsed",
    "error_ttl_sec",
    "expect_spdy",
    "extraction_error",
    "get_address_net_error",
    "get_sts_state_result",
    "host_found_in_hsts_bypass_list",
    "ipv6_available",
    "is_preconnect",
    "is_speculative",
    "load_flags",
    "method",
    "ndots",
    "net_error",
    "num_hosts",
    "os_error",
    "priority",
    "privacy_mode",
    "queued_transactions",
    "request_type",
    "rotate",
    "secure_dns_mode",
    "secure_dns_policy",
    "secure",
    "should_upgrade_to_ssl",
    "should_wait",
    "source_dependency",
    "started_transactions",
    "thread_number",
    "timedout",
    "timeout",
    "transactions_needed",
    "type",
    "unhandled_options",
    "use_local_ipv6",
    "using_quic",
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_HEAVILY_REDACTED_ALLOWLIST_H_
