// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_LINUX_LINUX_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
#define NET_PROXY_RESOLUTION_LINUX_LINUX_SYSTEM_PROXY_RESOLUTION_REQUEST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/linux/linux_proxy_resolution_status.h"
#include "net/proxy_resolution/linux/linux_system_proxy_resolver.h"
#include "net/proxy_resolution/system_proxy_resolution_request.h"

namespace net {

class ProxyInfo;
class ProxyList;
class LinuxSystemProxyResolutionService;

// This is the concrete implementation of ProxyResolutionRequest used by
// LinuxSystemProxyResolutionService. Manages a single asynchronous proxy
// resolution request via the org.freedesktop.portal.ProxyResolver portal.
class NET_EXPORT LinuxSystemProxyResolutionRequest
    : public SystemProxyResolutionRequest {
 public:
  // The `linux_system_proxy_resolver` is not saved by this object. Rather, it
  // is simply used to kick off proxy resolution from within the constructor.
  // Every other parameter is saved by this object. `service` may be null in
  // tests.
  LinuxSystemProxyResolutionRequest(
      LinuxSystemProxyResolutionService* service,
      GURL url,
      std::string method,
      NetworkAnonymizationKey network_anonymization_key,
      ProxyInfo* results,
      CompletionOnceCallback user_callback,
      const NetLogWithSource& net_log,
      LinuxSystemProxyResolver& linux_system_proxy_resolver);

  LinuxSystemProxyResolutionRequest(const LinuxSystemProxyResolutionRequest&) =
      delete;
  LinuxSystemProxyResolutionRequest& operator=(
      const LinuxSystemProxyResolutionRequest&) = delete;

  ~LinuxSystemProxyResolutionRequest() override;

  // Callback for when the cross-process proxy resolution has completed. The
  // `proxy_list` is the list of proxies returned by the portal translated into
  // Chromium-friendly terms. `linux_status` describes the status of the proxy
  // resolution request.
  void ProxyResolutionComplete(const ProxyList& proxy_list,
                               LinuxProxyResolutionStatus linux_status);

 private:
  // Typed pointer to the Linux-specific service. Cleared alongside the base
  // `service_` pointer in ProxyResolutionComplete() to avoid dangling when the
  // service is destroyed with in-flight requests.
  raw_ptr<LinuxSystemProxyResolutionService> linux_service_;

  // Manages the cross-process proxy resolution. Deleting this will cancel a
  // pending proxy resolution. After a callback has been received via
  // ProxyResolutionComplete(), this object will no longer do anything.
  std::unique_ptr<LinuxSystemProxyResolver::Request> proxy_resolution_request_;

  base::WeakPtrFactory<LinuxSystemProxyResolutionRequest> weak_factory_{this};
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_LINUX_LINUX_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
