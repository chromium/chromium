// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/windows_system_proxy_resolution_service.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "base/win/windows_version.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_request.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolver.h"

namespace net {

// static
bool WindowsSystemProxyResolutionService::IsSupported() {
  // The sandbox required to run the WinHttp functions  used in the resolver is
  // only supported in RS1 and later.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1) {
    LOG(WARNING) << "WindowsSystemProxyResolutionService is only supported for "
                    "Windows 10 Version 1607 (RS1) and later.";
    return false;
  }

  return true;
}

// static
std::unique_ptr<WindowsSystemProxyResolutionService>
WindowsSystemProxyResolutionService::Create(
    std::unique_ptr<WindowsSystemProxyResolver> windows_system_proxy_resolver,
    NetLog* net_log) {
  if (!IsSupported() || !windows_system_proxy_resolver)
    return nullptr;

  return base::WrapUnique(new WindowsSystemProxyResolutionService(
      std::move(windows_system_proxy_resolver), net_log));
}

WindowsSystemProxyResolutionService::WindowsSystemProxyResolutionService(
    std::unique_ptr<WindowsSystemProxyResolver> windows_system_proxy_resolver,
    NetLog* net_log)
    : windows_system_proxy_resolver_(std::move(windows_system_proxy_resolver)),
      net_log_(net_log) {}

WindowsSystemProxyResolutionService::~WindowsSystemProxyResolutionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel any in-progress requests.
  // This cancels the internal requests, but leaves the responsibility of
  // canceling the high-level ProxyResolutionRequest (by deleting it) to the
  // client. Since |pending_requests_| might be modified in one of the requests'
  // callbacks (if it deletes another request), iterating through the set in a
  // for-loop will not work.
  while (!pending_requests_.empty()) {
    WindowsSystemProxyResolutionRequest* req = *pending_requests_.begin();
    req->ProxyResolutionComplete(ProxyList(), WinHttpStatus::kAborted, 0);
    pending_requests_.erase(req);
  }
}

int WindowsSystemProxyResolutionService::ResolveProxy(
    const GURL& url,
    const std::string& method,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback callback,
    std::unique_ptr<ProxyResolutionRequest>* request,
    const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK(request);

  net_log.BeginEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);

  // Once it's created, the WindowsSystemProxyResolutionRequest immediately
  // kicks off proxy resolution in a separate process.
  auto req = std::make_unique<WindowsSystemProxyResolutionRequest>(
      this, url, method, results, std::move(callback), net_log,
      windows_system_proxy_resolver_.get());

  DCHECK(!ContainsPendingRequest(req.get()));
  pending_requests_.insert(req.get());

  // Completion will be notified through |callback|, unless the caller cancels
  // the request using |request|.
  *request = std::move(req);
  return ERR_IO_PENDING;
}

void WindowsSystemProxyResolutionService::ReportSuccess(
    const ProxyInfo& proxy_info) {
  // TODO(crbug.com/40111093): Update proxy retry info with new proxy
  // resolution data.
}

void WindowsSystemProxyResolutionService::SetProxyDelegate(
    ProxyDelegate* delegate) {
  // TODO(crbug.com/40111093): Implement proxy delegates.
}

void WindowsSystemProxyResolutionService::OnShutdown() {
  // TODO(crbug.com/40111093): Add cleanup here as necessary. If cleanup
  // is unnecessary, update the interface to not require an implementation for
  // this so OnShutdown() can be removed.
}

void WindowsSystemProxyResolutionService::ClearBadProxiesCache() {
  proxy_retry_info_.clear();
}

const ProxyRetryInfoMap& WindowsSystemProxyResolutionService::proxy_retry_info()
    const {
  return proxy_retry_info_;
}

base::Value::Dict WindowsSystemProxyResolutionService::GetProxyNetLogValues() {
  // TODO (https://crbug.com/1032820): Implement net logs.
  return base::Value::Dict();
}

bool WindowsSystemProxyResolutionService::
    CastToConfiguredProxyResolutionService(
        ConfiguredProxyResolutionService**
            configured_proxy_resolution_service) {
  *configured_proxy_resolution_service = nullptr;
  return false;
}

bool WindowsSystemProxyResolutionService::ContainsPendingRequest(
    WindowsSystemProxyResolutionRequest* req) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_requests_.count(req) == 1;
}

void WindowsSystemProxyResolutionService::RemovePendingRequest(
    WindowsSystemProxyResolutionRequest* req) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsPendingRequest(req));
  pending_requests_.erase(req);
}

int WindowsSystemProxyResolutionService::DidFinishResolvingProxy(
    const GURL& url,
    const std::string& method,
    ProxyInfo* result,
    WinHttpStatus winhttp_status,
    const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40111093): Implement net logs.
  // TODO(crbug.com/40111093): Implement proxy delegate.
  // TODO(crbug.com/40111093): Implement proxy retry info.

  if (winhttp_status != WinHttpStatus::kOk)
    result->UseDirect();

  net_log.EndEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
  return OK;
}

}  // namespace net
