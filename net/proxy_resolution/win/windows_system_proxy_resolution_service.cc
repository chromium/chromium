// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "net/proxy_resolution/win/winhttp_proxy_resolver_functions.h"

namespace net {

// static
bool WindowsSystemProxyResolutionService::IsSupported() {
  if (base::win::GetVersion() < base::win::Version::WIN8) {
    LOG(WARNING) << "WindowsSystemProxyResolutionService is only supported for "
                    "Windows 8 and later.";
    return false;
  }

  if (!WinHttpProxyResolverFunctions::GetInstance()
           .are_all_functions_loaded()) {
    LOG(ERROR) << "Failed to load functions necessary for "
                  "WindowsSystemProxyResolutionService!";
    return false;
  }

  return true;
}

// static
std::unique_ptr<WindowsSystemProxyResolutionService>
WindowsSystemProxyResolutionService::Create(NetLog* net_log) {
  if (!IsSupported())
    return nullptr;

  return base::WrapUnique(new WindowsSystemProxyResolutionService(net_log));
}

WindowsSystemProxyResolutionService::WindowsSystemProxyResolutionService(
    NetLog* net_log)
    : create_proxy_resolver_function_for_testing_(nullptr), net_log_(net_log) {}

WindowsSystemProxyResolutionService::~WindowsSystemProxyResolutionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel any in-progress requests.
  // This cancels the internal requests, but leaves the responsibility of
  // canceling the high-level Request (by deleting it) to the client.
  // Since |pending_requests_| might be modified in one of the requests'
  // callbacks (if it deletes another request), iterating through the set in a
  // for-loop will not work.
  while (!pending_requests_.empty()) {
    WindowsSystemProxyResolutionRequest* req = *pending_requests_.begin();
    ProxyList empty_list;
    req->AsynchronousProxyResolutionComplete(empty_list, ERR_ABORTED, 0);
    pending_requests_.erase(req);
  }
}

int WindowsSystemProxyResolutionService::ResolveProxy(
    const GURL& url,
    const std::string& method,
    const NetworkIsolationKey& network_isolation_key,
    ProxyInfo* results,
    CompletionOnceCallback callback,
    std::unique_ptr<ProxyResolutionRequest>* request,
    const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK(request);

  net_log.BeginEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);

  // TODO(https://crbug.com/1032820): Use a more detailed error.
  if (!CreateWindowsSystemProxyResolverIfNeeded())
    return DidFinishResolvingProxy(url, method, results, ERR_FAILED, net_log);

  auto req = std::make_unique<WindowsSystemProxyResolutionRequest>(
      this, url, method, results, std::move(callback), net_log,
      windows_system_proxy_resolver_);

  const int net_error = req->Start();
  if (net_error != ERR_IO_PENDING)
    return req->SynchronousProxyResolutionComplete(net_error);

  DCHECK(!ContainsPendingRequest(req.get()));
  pending_requests_.insert(req.get());

  // Completion will be notified through |callback|, unless the caller cancels
  // the request using |request|.
  *request = std::move(req);
  return net_error;
}

void WindowsSystemProxyResolutionService::ReportSuccess(
    const ProxyInfo& proxy_info) {
  // TODO(https://crbug.com/1032820): Update proxy retry info with new proxy
  // resolution data.
}

void WindowsSystemProxyResolutionService::SetProxyDelegate(
    ProxyDelegate* delegate) {
  // TODO(https://crbug.com/1032820): Implement proxy delegates.
}

void WindowsSystemProxyResolutionService::OnShutdown() {
  // TODO(https://crbug.com/1032820): Add cleanup here as necessary. If cleanup
  // is unnecessary, update the interface to not require an implementation for
  // this so OnShutdown() can be removed.
}

bool WindowsSystemProxyResolutionService::MarkProxiesAsBadUntil(
    const ProxyInfo& results,
    base::TimeDelta retry_delay,
    const std::vector<ProxyServer>& additional_bad_proxies,
    const NetLogWithSource& net_log) {
  // TODO(https://crbug.com/1032820): Implement bad proxy cache. We should be
  // able to share logic with the ConfiguredProxyResolutionService to accomplish
  // this.
  return false;
}

void WindowsSystemProxyResolutionService::ClearBadProxiesCache() {
  proxy_retry_info_.clear();
}

const ProxyRetryInfoMap& WindowsSystemProxyResolutionService::proxy_retry_info()
    const {
  return proxy_retry_info_;
}

base::Value WindowsSystemProxyResolutionService::GetProxyNetLogValues() {
  // TODO (https://crbug.com/1032820): Implement net logs.
  base::Value net_info_dict(base::Value::Type::DICTIONARY);
  return net_info_dict;
}

bool WindowsSystemProxyResolutionService::
    CastToConfiguredProxyResolutionService(
        ConfiguredProxyResolutionService**
            configured_proxy_resolution_service) {
  *configured_proxy_resolution_service = nullptr;
  return false;
}

void WindowsSystemProxyResolutionService::
    SetCreateWindowsSystemProxyResolverFunctionForTesting(
        CreateWindowsSystemProxyResolverFunctionForTesting function) {
  create_proxy_resolver_function_for_testing_ = function;
}

void WindowsSystemProxyResolutionService::
    SetWindowsSystemProxyResolverForTesting(
        scoped_refptr<WindowsSystemProxyResolver>
            windows_system_proxy_resolver) {
  windows_system_proxy_resolver_ = windows_system_proxy_resolver;
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

bool WindowsSystemProxyResolutionService::
    CreateWindowsSystemProxyResolverIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (windows_system_proxy_resolver_)
    return true;

  if (create_proxy_resolver_function_for_testing_) {
    windows_system_proxy_resolver_ =
        create_proxy_resolver_function_for_testing_();
  } else {
    windows_system_proxy_resolver_ =
        WindowsSystemProxyResolver::CreateWindowsSystemProxyResolver();
  }

  return !!windows_system_proxy_resolver_;
}

int WindowsSystemProxyResolutionService::DidFinishResolvingProxy(
    const GURL& url,
    const std::string& method,
    ProxyInfo* result,
    int result_code,
    const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/1032820): Implement net logs.
  // TODO(https://crbug.com/1032820): Implement proxy delegate.
  // TODO(https://crbug.com/1032820): Implement proxy retry info.

  if (result_code != OK)
    result->UseDirect();

  net_log.EndEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
  return OK;
}

}  // namespace net
