// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_MAC_MAC_SYSTEM_PROXY_RESOLVER_H_
#define NET_PROXY_RESOLUTION_MAC_MAC_SYSTEM_PROXY_RESOLVER_H_

#include <memory>

#include "net/base/net_export.h"

class GURL;

namespace net {

class MacSystemProxyResolutionRequest;

// This is used to communicate with a utility process that resolves a proxy
// using macOS SystemConfiguration/CFNetwork APIs. These APIs must be called in
// a separate process because they will not be allowed in the network service
// when the sandbox gets locked down. This interface is intended to be used via
// the MacSystemProxyResolutionRequest, which manages individual proxy
// resolutions.
class NET_EXPORT MacSystemProxyResolver {
 public:
  // A handle to a cross-process proxy resolution request. Deleting it will
  // cancel the request.
  class Request {
   public:
    virtual ~Request() = default;
  };

  MacSystemProxyResolver() = default;
  MacSystemProxyResolver(const MacSystemProxyResolver&) = delete;
  MacSystemProxyResolver& operator=(const MacSystemProxyResolver&) = delete;
  virtual ~MacSystemProxyResolver() = default;

  // Asynchronously finds a proxy for `url`. Deleting the returned Request
  // cancels the in-flight resolution.
  //
  // Lifetime requirements:
  //  - The returned Request must not outlive `this` (the resolver). Callers
  //    must destroy all outstanding Requests before destroying the resolver.
  //    MacSystemProxyResolutionService guarantees this by aborting pending
  //    requests in its destructor before the resolver is destroyed.
  //  - `callback_target` must remain valid until either (a) the returned
  //    Request is destroyed (cancelling resolution) or (b)
  //    ProxyResolutionComplete() is called. This is naturally satisfied
  //    because `callback_target` owns the returned Request.
  virtual std::unique_ptr<Request> GetProxyForUrl(
      const GURL& url,
      MacSystemProxyResolutionRequest* callback_target) = 0;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_MAC_MAC_SYSTEM_PROXY_RESOLVER_H_
