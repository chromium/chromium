// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_LINUX_LINUX_SYSTEM_PROXY_RESOLVER_H_
#define NET_PROXY_RESOLUTION_LINUX_LINUX_SYSTEM_PROXY_RESOLVER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/linux/linux_proxy_resolution_status.h"

class GURL;

namespace net {

class ProxyList;

// This is used to communicate with the browser process, which resolves proxies
// using the org.freedesktop.portal.ProxyResolver XDG desktop portal over
// D-Bus. D-Bus is not accessed from the network service so that it may be
// sandboxed. This interface is intended to be used via the
// LinuxSystemProxyResolutionRequest, which manages individual proxy
// resolutions.
class NET_EXPORT LinuxSystemProxyResolver {
 public:
  // A handle to a cross-process proxy resolution request. Deleting it will
  // cancel the request.
  class Request {
   public:
    virtual ~Request() = default;
  };

  // Receives the result of a proxy resolution. `proxy_list` is the list of
  // proxies returned by the portal translated into Chromium-friendly terms.
  // `status` describes the outcome of the request.
  using ResultCallback =
      base::OnceCallback<void(const ProxyList& proxy_list,
                              LinuxProxyResolutionStatus status)>;

  LinuxSystemProxyResolver() = default;
  LinuxSystemProxyResolver(const LinuxSystemProxyResolver&) = delete;
  LinuxSystemProxyResolver& operator=(const LinuxSystemProxyResolver&) = delete;
  virtual ~LinuxSystemProxyResolver() = default;

  // Asynchronously finds a proxy for `url` and runs `callback` with the
  // result. Deleting the returned Request cancels the in-flight resolution;
  // `callback` must not be run after the returned Request is destroyed.
  //
  // The returned Request must not outlive `this` (the resolver).
  // LinuxSystemProxyResolutionService guarantees this by aborting pending
  // requests in its destructor before the resolver is destroyed.
  virtual std::unique_ptr<Request> GetProxyForUrl(const GURL& url,
                                                  ResultCallback callback) = 0;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_LINUX_LINUX_SYSTEM_PROXY_RESOLVER_H_
