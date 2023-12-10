// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_PROXY_FALLBACK_H_
#define NET_HTTP_PROXY_FALLBACK_H_

// ------------------------------------------------------------
// Proxy Fallback Overview
// ------------------------------------------------------------
//
// Proxy fallback is a feature that is split between the proxy resolution layer
// and the HTTP layers.
//
// The proxy resolution layer is responsible for:
//   * Obtaining a list of proxies to use
//     (ProxyResolutionService::ResolveProxy). Proxy lists are (usually) the
//     result of having evaluated a PAC script, such as:
//         return "PROXY foobar1:8080; HTTPS foobar2:8080; DIRECT";
//
//   * Re-ordering the proxy list such that proxy chains that have recently
//     failed are given lower priority (ProxyInfo::DeprioritizeBadProxyChains)
//
//   * Maintaining the expiring cache of proxy chains that have recently failed.
//
//
// The HTTP layer is responsible for:
//   * Attempting to issue the URLRequest through each of the
//     proxy chains, in the order specified by the list.
//
//   * Deciding whether this attempt was successful, whether it was a failure
//     but should keep trying other proxy chains, or whether it was a failure
//     and should stop trying other proxy chains.
//
//   * Upon successful completion of an attempt though a proxy chain, calling
//     ProxyResolutionService::ReportSuccess to inform it of all the failed
//     attempts that were made. (A proxy chain is only considered to be "bad"
//     if the request was able to be completed through some other proxy chain).
//
//
// Exactly how to interpret the proxy lists returned by PAC is not specified by
// a standard. The justifications for what errors are considered for fallback
// are given beside the implementation.

#include "net/base/net_export.h"

namespace net {

class ProxyChain;

// Returns true if a failed request issued through a proxy chain should be
// re-tried using the next proxy chain in the fallback list.
//
// The proxy fallback logic is a compromise between compatibility and
// increasing odds of success, and may choose not to retry a request on the
// next proxy option, even though that could work.
//
//  - `proxy_chain` is the proxy chain that failed the request.
//  - `error` is the error for the request when it was sent through
//    `proxy_chain`.
//  - `final_error` is an out parameter that is set with the "final" error to
//    report to the caller. The error is only re-written in cases where
//    CanFalloverToNextProxy() returns false.
//  - `is_for_ip_protection` is true if this request is to an IP Protection
//    proxy.
NET_EXPORT bool CanFalloverToNextProxy(const ProxyChain& proxy_chain,
                                       int error,
                                       int* final_error,
                                       bool is_for_ip_protection = false);

}  // namespace net

#endif  // NET_HTTP_PROXY_FALLBACK_H_
