// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/windows_system_proxy_resolution_request.h"

#include <utility>

#include "net/base/net_errors.h"
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
    ProxyInfo* results,
    CompletionOnceCallback user_callback,
    const NetLogWithSource& net_log,
    WindowsSystemProxyResolver* windows_system_proxy_resolver)
    : service_(service),
      user_callback_(std::move(user_callback)),
      results_(results),
      url_(url),
      method_(method),
      net_log_(net_log),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK(!user_callback_.is_null());
  DCHECK(windows_system_proxy_resolver);
  proxy_resolution_request_ =
      windows_system_proxy_resolver->GetProxyForUrl(url, this);
}

WindowsSystemProxyResolutionRequest::~WindowsSystemProxyResolutionRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (service_) {
    service_->RemovePendingRequest(this);
    net_log_.AddEvent(NetLogEventType::CANCELLED);

    CancelResolveRequest();

    net_log_.EndEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
  }
}

LoadState WindowsSystemProxyResolutionRequest::GetLoadState() const {
  // TODO(crbug.com/40111093): Consider adding a LoadState for "We're
  // waiting on system APIs to do their thing".
  return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
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
  // TODO(crbug.com/40111093): Log Windows error |windows_error|.

  proxy_resolution_request_.reset();
  results_->UseProxyList(proxy_list);

  // Note that DidFinishResolvingProxy might modify |results_|.
  int net_error = service_->DidFinishResolvingProxy(url_, method_, results_,
                                                    winhttp_status, net_log_);

  // Make a note in the results which configuration was in use at the
  // time of the resolve.
  results_->set_proxy_resolve_start_time(creation_time_);
  results_->set_proxy_resolve_end_time(base::TimeTicks::Now());
  results_->set_traffic_annotation(
      MutableNetworkTrafficAnnotationTag(kWindowsResolverTrafficAnnotation));

  CompletionOnceCallback callback = std::move(user_callback_);

  service_->RemovePendingRequest(this);
  service_ = nullptr;
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
