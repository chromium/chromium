// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/linux/linux_system_proxy_resolution_service.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/linux/linux_system_proxy_resolution_request.h"
#include "net/proxy_resolution/linux/linux_system_proxy_resolver.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/proxy_resolution_url_sanitizer.h"
#include "url/gurl.h"

namespace net {

// static
std::unique_ptr<LinuxSystemProxyResolutionService>
LinuxSystemProxyResolutionService::Create(
    std::unique_ptr<LinuxSystemProxyResolver> linux_system_proxy_resolver) {
  if (!linux_system_proxy_resolver) {
    return nullptr;
  }

  return base::WrapUnique(new LinuxSystemProxyResolutionService(
      std::move(linux_system_proxy_resolver)));
}

LinuxSystemProxyResolutionService::LinuxSystemProxyResolutionService(
    std::unique_ptr<LinuxSystemProxyResolver> linux_system_proxy_resolver)
    : linux_system_proxy_resolver_(std::move(linux_system_proxy_resolver)) {}

LinuxSystemProxyResolutionService::~LinuxSystemProxyResolutionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel any in-progress requests. This cancels the internal requests, but
  // leaves the responsibility of canceling the high-level
  // ProxyResolutionRequest (by deleting it) to the client. Since the pending
  // sets might be modified in one of the requests' callbacks (if it deletes
  // another request), iterating through the set in a for-loop will not work.
  while (!linux_pending_requests_.empty()) {
    const size_t size_before = linux_pending_requests_.size();
    LinuxSystemProxyResolutionRequest* req =
        linux_pending_requests_.begin()->get();
    // ProxyResolutionComplete() removes `req` from both pending sets and
    // clears the request's service pointers, so the request's destructor will
    // not attempt to touch this service again.
    req->ProxyResolutionComplete(ProxyList(),
                                 LinuxProxyResolutionStatus::kAborted);
    CHECK_LT(linux_pending_requests_.size(), size_before);
  }
  DCHECK(pending_requests_.empty());
}

int LinuxSystemProxyResolutionService::ResolveProxy(
    const GURL& url,
    const std::string& method,
    const NetworkAnonymizationKey& network_anonymization_key,
    handles::NetworkHandle target_network,
    ProxyInfo* results,
    CompletionOnceCallback callback,
    std::unique_ptr<ProxyResolutionRequest>* request,
    const NetLogWithSource& net_log,
    RequestPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK(request);

  net_log.BeginEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);

  // Once it's created, the LinuxSystemProxyResolutionRequest immediately kicks
  // off proxy resolution in the browser process.
  auto req = std::make_unique<LinuxSystemProxyResolutionRequest>(
      this, SanitizeUrlForProxyResolution(url), method,
      network_anonymization_key, results, std::move(callback), net_log,
      *linux_system_proxy_resolver_);

  DCHECK(!ContainsPendingRequest(req.get()));
  pending_requests_.insert(req.get());
  linux_pending_requests_.insert(req.get());

  // Completion will be notified through `callback`, unless the caller cancels
  // the request using `request`.
  *request = std::move(req);
  return ERR_IO_PENDING;
}

base::DictValue LinuxSystemProxyResolutionService::GetProxySettingsForNetLog() {
  base::DictValue dict;
  dict.Set("source", "system");
  dict.Set("description",
           "Linux system proxy configuration via "
           "org.freedesktop.portal.ProxyResolver");
  return dict;
}

int LinuxSystemProxyResolutionService::DidFinishResolvingProxy(
    const GURL& url,
    const std::string& method,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* result,
    LinuxProxyResolutionStatus linux_status,
    const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net_log.AddEvent(
      NetLogEventType::PROXY_RESOLUTION_SERVICE_RESOLVED_PROXY_LIST, [&] {
        base::DictValue resolution_dict;
        resolution_dict.Set("linux_status", static_cast<int>(linux_status));
        if (linux_status == LinuxProxyResolutionStatus::kOk) {
          resolution_dict.Set("proxy_info", result->ToDebugString());
        }
        return resolution_dict;
      });

  if (linux_status == LinuxProxyResolutionStatus::kOk) {
    if (proxy_delegate_) {
      proxy_delegate_->OnResolveProxy(url, network_anonymization_key, method,
                                      proxy_retry_info_, result);
    }

    DeprioritizeBadProxyChains(proxy_retry_info_, result, net_log);
  } else {
    result->UseDirect();
  }

  net_log.EndEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
  return OK;
}

void LinuxSystemProxyResolutionService::RemoveLinuxPendingRequest(
    LinuxSystemProxyResolutionRequest* req) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(linux_pending_requests_.contains(req));
  linux_pending_requests_.erase(req);
}

}  // namespace net
