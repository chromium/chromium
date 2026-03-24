// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/system_proxy_resolution_service.h"

#include "base/check.h"
#include "base/values.h"
#include "net/base/net_info_source_list.h"
#include "net/proxy_resolution/proxy_info.h"

namespace net {

SystemProxyResolutionService::SystemProxyResolutionService() = default;

SystemProxyResolutionService::~SystemProxyResolutionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Subclass destructors must drain pending_requests_ (by completing or
  // cancelling all in-flight requests) before this base destructor runs.
  DCHECK(pending_requests_.empty());
}

void SystemProxyResolutionService::ReportSuccess(const ProxyInfo& proxy_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const ProxyRetryInfoMap& new_retry_info = proxy_info.proxy_retry_info();
  ProxyResolutionService::ProcessProxyRetryInfo(
      new_retry_info, proxy_retry_info_, proxy_delegate_);
}

void SystemProxyResolutionService::SetProxyDelegate(ProxyDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!proxy_delegate_ || !delegate);
  proxy_delegate_ = delegate;
}

void SystemProxyResolutionService::OnShutdown() {
  // TODO(crbug.com/40111093): Add cleanup here as necessary. If cleanup
  // is unnecessary, update the interface to not require an implementation for
  // this so OnShutdown() can be removed.
}

void SystemProxyResolutionService::ClearBadProxiesCache() {
  proxy_retry_info_.clear();
}

const ProxyRetryInfoMap& SystemProxyResolutionService::proxy_retry_info()
    const {
  return proxy_retry_info_;
}

base::DictValue SystemProxyResolutionService::GetProxyNetLogValues() {
  base::DictValue net_info_dict;

  // Log proxy settings - platform subclass provides the description.
  net_info_dict.Set(kNetInfoProxySettings, GetProxySettingsForNetLog());

  // Log Bad Proxies.
  net_info_dict.Set(kNetInfoBadProxies, BuildBadProxiesList(proxy_retry_info_));

  return net_info_dict;
}

bool SystemProxyResolutionService::CastToConfiguredProxyResolutionService(
    ConfiguredProxyResolutionService** configured_proxy_resolution_service) {
  *configured_proxy_resolution_service = nullptr;
  return false;
}

bool SystemProxyResolutionService::ContainsPendingRequest(
    SystemProxyResolutionRequest* req) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_requests_.contains(req);
}

void SystemProxyResolutionService::RemovePendingRequest(
    SystemProxyResolutionRequest* req) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ContainsPendingRequest(req));
  pending_requests_.erase(req);
}

}  // namespace net
