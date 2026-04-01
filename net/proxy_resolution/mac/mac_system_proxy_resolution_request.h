// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_MAC_MAC_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
#define NET_PROXY_RESOLUTION_MAC_MAC_SYSTEM_PROXY_RESOLUTION_REQUEST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/mac/mac_proxy_resolution_status.h"
#include "net/proxy_resolution/mac/mac_system_proxy_resolver.h"
#include "net/proxy_resolution/system_proxy_resolution_request.h"

namespace net {

class ProxyInfo;
class ProxyList;
class MacSystemProxyResolutionService;

// This is the concrete implementation of ProxyResolutionRequest used by
// MacSystemProxyResolutionService. Manages a single asynchronous proxy
// resolution request via macOS SystemConfiguration/CFNetwork APIs.
class NET_EXPORT MacSystemProxyResolutionRequest
    : public SystemProxyResolutionRequest {
 public:
  // The `mac_system_proxy_resolver` is not saved by this object. Rather, it
  // is simply used to kick off proxy resolution in a utility process from
  // within the constructor. The `mac_system_proxy_resolver` is not needed
  // after construction. Every other parameter is saved by this object. Details
  // for each one of these saved parameters can be found below.
  MacSystemProxyResolutionRequest(
      MacSystemProxyResolutionService* service,
      GURL url,
      std::string method,
      NetworkAnonymizationKey network_anonymization_key,
      ProxyInfo* results,
      CompletionOnceCallback user_callback,
      const NetLogWithSource& net_log,
      MacSystemProxyResolver& mac_system_proxy_resolver);

  MacSystemProxyResolutionRequest(const MacSystemProxyResolutionRequest&) =
      delete;
  MacSystemProxyResolutionRequest& operator=(
      const MacSystemProxyResolutionRequest&) = delete;

  ~MacSystemProxyResolutionRequest() override;

  // Callback for when the cross-process proxy resolution has completed. The
  // `proxy_list` is the list of proxies returned by macOS translated into
  // Chromium-friendly terms. The `mac_status` describes the status of the
  // proxy resolution request. If macOS proxy resolution fails for some reason,
  // `os_error` contains the specific error returned by the OS.
  virtual void ProxyResolutionComplete(const ProxyList& proxy_list,
                                       MacProxyResolutionStatus mac_status,
                                       int os_error);

  MacSystemProxyResolver::Request* GetProxyResolutionRequestForTesting();
  void ResetProxyResolutionRequestForTesting();

 private:
  // Cancels the callback from the resolver for a previously started proxy
  // resolution.
  void CancelResolveRequest();

  // Returns a typed pointer to the Mac-specific service. Stored as a member
  // with the static_cast performed once in the constructor where the type is
  // guaranteed. Used to call DidFinishResolvingProxy().
  const raw_ptr<MacSystemProxyResolutionService> mac_service_;

  // Manages the cross-process proxy resolution. Deleting this will cancel a
  // pending proxy resolution. After a callback has been received via
  // ProxyResolutionComplete(), this object will no longer do anything.
  std::unique_ptr<MacSystemProxyResolver::Request> proxy_resolution_request_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_MAC_MAC_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
