// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/linux/linux_system_proxy_resolution_request.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/proxy_resolution/linux/linux_system_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

namespace {

constexpr NetworkTrafficAnnotationTag kLinuxResolverTrafficAnnotation =
    DefineNetworkTrafficAnnotation("proxy_config_linux_resolver", R"(
      semantics {
        sender: "Proxy Config for Linux System Resolver"
        description:
          "Establishing a connection through a proxy server using system proxy "
          "settings resolved by the XDG desktop portal "
          "(org.freedesktop.portal.ProxyResolver)."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, the Linux system proxy resolver is enabled, and the "
          "result indicates usage of a proxy server."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other:
          "The proxy server specified in the configuration."
        last_reviewed: "2026-09-29"
        internal {
          contacts {
            email: "thomasanderson@chromium.org"
          }
          contacts {
            owners: "//net/OWNERS"
          }
        }
        user_data {
          type: SENSITIVE_URL
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "User cannot override system proxy settings, but can change them "
          "through the desktop environment's network proxy settings."
        policy_exception_justification:
          "Using either of 'ProxyMode', 'ProxyServer', or 'ProxyPacUrl' "
          "policies can set Chrome to use a specific proxy settings and avoid "
          "system proxy."
      })");

}  // namespace

LinuxSystemProxyResolutionRequest::LinuxSystemProxyResolutionRequest(
    LinuxSystemProxyResolutionService* service,
    GURL url,
    std::string method,
    NetworkAnonymizationKey network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback user_callback,
    const NetLogWithSource& net_log,
    LinuxSystemProxyResolver& linux_system_proxy_resolver)
    : SystemProxyResolutionRequest(service,
                                   std::move(url),
                                   std::move(method),
                                   std::move(network_anonymization_key),
                                   results,
                                   std::move(user_callback),
                                   net_log),
      linux_service_(service) {
  proxy_resolution_request_ = linux_system_proxy_resolver.GetProxyForUrl(
      url_, base::BindOnce(
                &LinuxSystemProxyResolutionRequest::ProxyResolutionComplete,
                weak_factory_.GetWeakPtr()));
}

LinuxSystemProxyResolutionRequest::~LinuxSystemProxyResolutionRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel the platform-specific resolver request before the base destructor
  // runs (which handles removing from the base pending set and net log
  // events).
  proxy_resolution_request_.reset();
  if (linux_service_) {
    linux_service_->RemoveLinuxPendingRequest(this);
    linux_service_ = nullptr;
  }
}

void LinuxSystemProxyResolutionRequest::ProxyResolutionComplete(
    const ProxyList& proxy_list,
    LinuxProxyResolutionStatus linux_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!was_completed());

  // Skip histogram recording for aborted requests (e.g., service destruction
  // during shutdown). Only record metrics for genuine resolution outcomes.
  if (linux_status != LinuxProxyResolutionStatus::kAborted) {
    base::UmaHistogramEnumeration("Net.HttpProxy.LinuxSystemResolver.Status",
                                  linux_status);
  }

  proxy_resolution_request_.reset();
  results_->UseProxyList(proxy_list);

  // Note that DidFinishResolvingProxy might modify `results_`.
  int net_error = linux_service_->DidFinishResolvingProxy(
      url_, method_, network_anonymization_key_, results_, linux_status,
      net_log_);

  // Make a note in the results which configuration was in use at the
  // time of the resolve.
  results_->set_proxy_resolve_start_time(creation_time_);
  results_->set_proxy_resolve_end_time(base::TimeTicks::Now());
  results_->set_traffic_annotation(
      MutableNetworkTrafficAnnotationTag(kLinuxResolverTrafficAnnotation));

  // Move the callback out before MarkCompleted() because MarkCompleted()
  // clears service_, and the callback invocation may destroy `this`.
  CompletionOnceCallback callback = std::move(user_callback_);

  linux_service_->RemoveLinuxPendingRequest(this);
  linux_service_ = nullptr;
  MarkCompleted();
  std::move(callback).Run(net_error);
}

}  // namespace net
