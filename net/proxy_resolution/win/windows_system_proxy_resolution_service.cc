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
#include "net/base/net_info_source_list.h"
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
    const NetLogWithSource& net_log,
    RequestPriority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  DCHECK(request);

  net_log.BeginEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);

  // Once it's created, the WindowsSystemProxyResolutionRequest immediately
  // kicks off proxy resolution in a separate process.
  auto req = std::make_unique<WindowsSystemProxyResolutionRequest>(
      this, url, method, network_anonymization_key, results,
      std::move(callback), net_log, windows_system_proxy_resolver_.get());

  DCHECK(!ContainsPendingRequest(req.get()));
  pending_requests_.insert(req.get());

  // Completion will be notified through |callback|, unless the caller cancels
  // the request using |request|.
  *request = std::move(req);
  return ERR_IO_PENDING;
}

void WindowsSystemProxyResolutionService::ReportSuccess(
    const ProxyInfo& proxy_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const ProxyRetryInfoMap& new_retry_info = proxy_info.proxy_retry_info();
  ProxyResolutionService::ProcessProxyRetryInfo(
      new_retry_info, proxy_retry_info_, proxy_delegate_);
}

void WindowsSystemProxyResolutionService::SetProxyDelegate(
    ProxyDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!proxy_delegate_ || !delegate);
  proxy_delegate_ = delegate;
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
  base::Value::Dict net_info_dict;

  // Log proxy settings - Windows system proxy uses the system configuration
  {
    base::Value::Dict dict;
    dict.Set("source", "system");
    dict.Set("description", "Windows system proxy configuration");
    net_info_dict.Set(kNetInfoProxySettings, std::move(dict));
  }

  // Log Bad Proxies.
  net_info_dict.Set(kNetInfoBadProxies, BuildBadProxiesList(proxy_retry_info_));

  return net_info_dict;
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
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* result,
    WinHttpStatus winhttp_status,
    int windows_error,
    const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Log the diagnostic information about the WinHTTP resolution
  net_log.AddEvent(
      NetLogEventType::PROXY_RESOLUTION_SERVICE_RESOLVED_PROXY_LIST, [&] {
        base::Value::Dict resolution_dict;
        resolution_dict.Set("winhttp_status", static_cast<int>(winhttp_status));
        resolution_dict.Set("windows_error", windows_error);
        if (winhttp_status == WinHttpStatus::kOk) {
          resolution_dict.Set("proxy_info", result->ToDebugString());
        }
        return resolution_dict;
      });

  if (winhttp_status == WinHttpStatus::kOk) {
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

}  // namespace net
