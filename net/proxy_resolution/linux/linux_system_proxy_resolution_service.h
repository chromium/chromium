// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_LINUX_LINUX_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
#define NET_PROXY_RESOLUTION_LINUX_LINUX_SYSTEM_PROXY_RESOLUTION_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/linux/linux_proxy_resolution_status.h"
#include "net/proxy_resolution/system_proxy_resolution_service.h"

namespace net {

class LinuxSystemProxyResolutionRequest;
class LinuxSystemProxyResolver;

// This class decides which proxy server(s) to use for a particular URL request
// on Linux by querying the org.freedesktop.portal.ProxyResolver XDG desktop
// portal (via the browser process), which applies the host's proxy settings,
// including PAC scripts. This is primarily useful when running inside a
// sandbox such as Flatpak, where the desktop's proxy settings are not directly
// readable.
class NET_EXPORT LinuxSystemProxyResolutionService
    : public SystemProxyResolutionService {
 public:
  // Returns nullptr if `linux_system_proxy_resolver` is null.
  static std::unique_ptr<LinuxSystemProxyResolutionService> Create(
      std::unique_ptr<LinuxSystemProxyResolver> linux_system_proxy_resolver);

  LinuxSystemProxyResolutionService(const LinuxSystemProxyResolutionService&) =
      delete;
  LinuxSystemProxyResolutionService& operator=(
      const LinuxSystemProxyResolutionService&) = delete;

  ~LinuxSystemProxyResolutionService() override;

  // ProxyResolutionService implementation:
  int ResolveProxy(const GURL& url,
                   const std::string& method,
                   const NetworkAnonymizationKey& network_anonymization_key,
                   handles::NetworkHandle target_network,
                   ProxyInfo* results,
                   CompletionOnceCallback callback,
                   std::unique_ptr<ProxyResolutionRequest>* request,
                   const NetLogWithSource& net_log,
                   RequestPriority priority) override;

 private:
  friend class LinuxSystemProxyResolutionRequest;

  explicit LinuxSystemProxyResolutionService(
      std::unique_ptr<LinuxSystemProxyResolver> linux_system_proxy_resolver);

  // SystemProxyResolutionService:
  base::DictValue GetProxySettingsForNetLog() override;

  // Called when proxy resolution has completed. Handles logging the result,
  // and cleaning out bad entries from the results list.
  int DidFinishResolvingProxy(
      const GURL& url,
      const std::string& method,
      const NetworkAnonymizationKey& network_anonymization_key,
      ProxyInfo* result,
      LinuxProxyResolutionStatus linux_status,
      const NetLogWithSource& net_log);

  // Called by a request when it completes or is destroyed while pending.
  void RemoveLinuxPendingRequest(LinuxSystemProxyResolutionRequest* req);

  // Typed view of the requests in the base class's `pending_requests_`. This
  // allows the destructor to abort in-flight requests without downcasting.
  std::set<raw_ptr<LinuxSystemProxyResolutionRequest, SetExperimental>>
      linux_pending_requests_;

  // Used to launch proxy resolution requests. Individual
  // LinuxSystemProxyResolutionRequest instances use this to initiate proxy
  // resolution.
  std::unique_ptr<LinuxSystemProxyResolver> linux_system_proxy_resolver_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_LINUX_LINUX_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
