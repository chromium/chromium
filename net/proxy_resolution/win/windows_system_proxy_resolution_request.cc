// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/windows_system_proxy_resolution_request.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

namespace {

constexpr net::NetworkTrafficAnnotationTag kWindowsResolverTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("proxy_config_windows_resolver", R"(
      semantics {
        sender: "Proxy Config for Windows System Resolver"
        description:
          "Establishing a connection through a proxy server using system proxy "
          "settings and Windows system proxy resolution code."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, the Windows system proxy resolver is enabled, and the "
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
          "through 'Advanced/System/Open proxy settings'."
        policy_exception_justification:
          "Using either of 'ProxyMode', 'ProxyServer', or 'ProxyPacUrl' "
          "policies can set Chrome to use a specific proxy settings and avoid "
          "system proxy."
      })");

}  // namespace

WindowsSystemProxyResolutionRequest::WindowsSystemProxyResolutionRequest(
    WindowsSystemProxyResolutionService* service,
    const GURL& url,
    const std::string& method,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback user_callback,
    const NetLogWithSource& net_log,
    WindowsSystemProxyResolver* windows_system_proxy_resolver)
    : SystemProxyResolutionRequest(service,
                                   url,
                                   method,
                                   network_anonymization_key,
                                   results,
                                   std::move(user_callback),
                                   net_log) {
  DCHECK(windows_system_proxy_resolver);
  proxy_resolution_request_ =
      windows_system_proxy_resolver->GetProxyForUrl(url, this);
}

WindowsSystemProxyResolutionRequest::~WindowsSystemProxyResolutionRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel the platform-specific resolver request before the base destructor
  // runs (which handles removing from pending requests and net log events).
  // C++ destructor ordering guarantees this runs before
  // ~SystemProxyResolutionRequest.
  // Safe to call even after completion — proxy_resolution_request_.reset() is
  // a no-op when already null.
  CancelResolveRequest();
}

WindowsSystemProxyResolutionService*
WindowsSystemProxyResolutionRequest::windows_service() const {
  DCHECK(service_);
  // The constructor guarantees service_ is a
  // WindowsSystemProxyResolutionService, so this downcast is safe.
  return static_cast<WindowsSystemProxyResolutionService*>(service_.get());
}

void WindowsSystemProxyResolutionRequest::CancelResolveRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_resolution_request_.reset();
}

void WindowsSystemProxyResolutionRequest::ProxyResolutionComplete(
    const ProxyList& proxy_list,
    WinHttpStatus winhttp_status,
    int windows_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!was_completed());

  if (windows_error != 0) {
    base::UmaHistogramSparse("Net.HttpProxy.WindowsSystemResolver.WinError",
                             windows_error);
  }

  proxy_resolution_request_.reset();
  results_->UseProxyList(proxy_list);

  // Note that DidFinishResolvingProxy might modify |results_|.
  int net_error = windows_service()->DidFinishResolvingProxy(
      url_, method_, network_anonymization_key_, results_, winhttp_status,
      windows_error, net_log_);

  // Make a note in the results which configuration was in use at the
  // time of the resolve.
  results_->set_proxy_resolve_start_time(creation_time_);
  results_->set_proxy_resolve_end_time(base::TimeTicks::Now());
  results_->set_traffic_annotation(
      MutableNetworkTrafficAnnotationTag(kWindowsResolverTrafficAnnotation));

  CompletionOnceCallback callback = std::move(user_callback_);

  MarkCompleted();
  user_callback_.Reset();
  std::move(callback).Run(net_error);
}

WindowsSystemProxyResolver::Request*
WindowsSystemProxyResolutionRequest::GetProxyResolutionRequestForTesting() {
  return proxy_resolution_request_.get();
}

void WindowsSystemProxyResolutionRequest::
    ResetProxyResolutionRequestForTesting() {
  proxy_resolution_request_.reset();
}

}  // namespace net
