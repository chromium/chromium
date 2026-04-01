// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/mac/mac_system_proxy_resolution_request.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/proxy_resolution/mac/mac_system_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

namespace {

constexpr net::NetworkTrafficAnnotationTag kMacResolverTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("proxy_config_mac_resolver", R"(
      semantics {
        sender: "Proxy Config for macOS System Resolver"
        description:
          "Establishing a connection through a proxy server using system proxy "
          "settings and macOS system proxy resolution code."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, the macOS system proxy resolver is enabled, and the "
          "result indicates usage of a proxy server."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other:
          "The proxy server specified in the configuration."
      }
      policy {
        cookies_allowed: NO
        setting:
          "User cannot override system proxy settings, but can change them "
          "through 'System Settings > Network > Proxies'."
        policy_exception_justification:
          "Using either of 'ProxyMode', 'ProxyServer', or 'ProxyPacUrl' "
          "policies can set Chrome to use a specific proxy settings and avoid "
          "system proxy."
      })");

}  // namespace

MacSystemProxyResolutionRequest::MacSystemProxyResolutionRequest(
    MacSystemProxyResolutionService* service,
    GURL url,
    std::string method,
    NetworkAnonymizationKey network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback user_callback,
    const NetLogWithSource& net_log,
    MacSystemProxyResolver& mac_system_proxy_resolver)
    : SystemProxyResolutionRequest(service,
                                   std::move(url),
                                   std::move(method),
                                   std::move(network_anonymization_key),
                                   results,
                                   std::move(user_callback),
                                   net_log),
      mac_service_(static_cast<MacSystemProxyResolutionService*>(service)) {
  proxy_resolution_request_ =
      mac_system_proxy_resolver.GetProxyForUrl(url_, this);
}

MacSystemProxyResolutionRequest::~MacSystemProxyResolutionRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel the platform-specific resolver request before the base destructor
  // runs (which handles removing from pending requests and net log events).
  // C++ destructor ordering guarantees this runs before
  // ~SystemProxyResolutionRequest.
  // Safe to call even after completion — proxy_resolution_request_.reset() is
  // a no-op when already null.
  CancelResolveRequest();
}

void MacSystemProxyResolutionRequest::CancelResolveRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_resolution_request_.reset();
}

void MacSystemProxyResolutionRequest::ProxyResolutionComplete(
    const ProxyList& proxy_list,
    MacProxyResolutionStatus mac_status,
    int os_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!was_completed());

  // Skip histogram recording for aborted requests (e.g., service destruction
  // during shutdown). Only record metrics for genuine resolution outcomes.
  if (mac_status != MacProxyResolutionStatus::kAborted) {
    // Record status for every resolution request. Volume is bounded by the
    // number of URL loads when the system proxy resolver is active.
    base::UmaHistogramEnumeration("Net.HttpProxy.MacSystemResolver.Status",
                                  mac_status);
  }

  if (os_error != 0) {
    base::UmaHistogramSparse("Net.HttpProxy.MacSystemResolver.OsError",
                             os_error);
  }

  proxy_resolution_request_.reset();
  results_->UseProxyList(proxy_list);

  // Note that DidFinishResolvingProxy might modify `results_`.
  int net_error = mac_service_->DidFinishResolvingProxy(
      url_, method_, network_anonymization_key_, results_, mac_status, os_error,
      net_log_);

  // Make a note in the results which configuration was in use at the
  // time of the resolve.
  results_->set_proxy_resolve_start_time(creation_time_);
  results_->set_proxy_resolve_end_time(base::TimeTicks::Now());
  results_->set_traffic_annotation(
      MutableNetworkTrafficAnnotationTag(kMacResolverTrafficAnnotation));

  // Move the callback out before MarkCompleted() because MarkCompleted()
  // clears service_, and the callback invocation may destroy `this`.
  CompletionOnceCallback callback = std::move(user_callback_);

  MarkCompleted();
  std::move(callback).Run(net_error);
}

MacSystemProxyResolver::Request*
MacSystemProxyResolutionRequest::GetProxyResolutionRequestForTesting() {
  CHECK_IS_TEST();
  return proxy_resolution_request_.get();
}

void MacSystemProxyResolutionRequest::ResetProxyResolutionRequestForTesting() {
  CHECK_IS_TEST();
  proxy_resolution_request_.reset();
}

}  // namespace net
