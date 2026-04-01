// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_MAC_MAC_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
#define NET_PROXY_RESOLUTION_MAC_MAC_SYSTEM_PROXY_RESOLUTION_SERVICE_H_

#include <memory>
#include <string>

#include "net/base/net_export.h"
#include "net/proxy_resolution/mac/mac_proxy_resolution_status.h"
#include "net/proxy_resolution/system_proxy_resolution_service.h"

namespace net {

class MacSystemProxyResolutionRequest;
class MacSystemProxyResolver;

// This class decides which proxy server(s) to use for a particular URL request.
// It does NOT support passing in fetched proxy configurations. Instead, it
// relies entirely on macOS SystemConfiguration/CFNetwork APIs to determine the
// proxy that should be used for each network request.
class NET_EXPORT MacSystemProxyResolutionService
    : public SystemProxyResolutionService {
 public:
  // Creates a MacSystemProxyResolutionService or returns nullptr if the
  // resolver is null.
  static std::unique_ptr<MacSystemProxyResolutionService> Create(
      std::unique_ptr<MacSystemProxyResolver> mac_system_proxy_resolver);

  MacSystemProxyResolutionService(const MacSystemProxyResolutionService&) =
      delete;
  MacSystemProxyResolutionService& operator=(
      const MacSystemProxyResolutionService&) = delete;

  ~MacSystemProxyResolutionService() override;

  // ProxyResolutionService implementation
  int ResolveProxy(const GURL& url,
                   const std::string& method,
                   const NetworkAnonymizationKey& network_anonymization_key,
                   ProxyInfo* results,
                   CompletionOnceCallback callback,
                   std::unique_ptr<ProxyResolutionRequest>* request,
                   const NetLogWithSource& net_log,
                   RequestPriority priority) override;

 private:
  friend class MacSystemProxyResolutionRequest;

  explicit MacSystemProxyResolutionService(
      std::unique_ptr<MacSystemProxyResolver> mac_system_proxy_resolver);

  // SystemProxyResolutionService:
  base::DictValue GetProxySettingsForNetLog() override;

  // Called when proxy resolution has completed (either synchronously or
  // asynchronously). Handles logging the result, and cleaning out
  // bad entries from the results list.
  int DidFinishResolvingProxy(
      const GURL& url,
      const std::string& method,
      const NetworkAnonymizationKey& network_anonymization_key,
      ProxyInfo* result,
      MacProxyResolutionStatus mac_status,
      int os_error,
      const NetLogWithSource& net_log);

  // Used to launch proxy resolution requests. Individual
  // MacSystemProxyResolutionRequest instances use this to initiate proxy
  // resolution.
  std::unique_ptr<MacSystemProxyResolver> mac_system_proxy_resolver_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_MAC_MAC_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
