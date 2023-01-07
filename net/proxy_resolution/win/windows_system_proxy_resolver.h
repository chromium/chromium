// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLVER_H_
#define NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLVER_H_

#include <memory>
#include <string>

#include "net/base/net_export.h"

class GURL;

namespace net {

class WindowsSystemProxyResolutionRequest;

// This is used to communicate with a utility process that resolves a proxy
// using WinHttp APIs. These APIs must be called in a separate process because
// they will not be allowed in the network service when the sandbox gets locked
// down. This interface is intended to be used via the
// WindowsSystemProxyResolutionRequest, which manages individual proxy
// resolutions.
class NET_EXPORT WindowsSystemProxyResolver {
 public:
  // A handle to a cross-process proxy resolution request. Deleting it will
  // cancel the request.
  class Request {
   public:
    virtual ~Request() = default;
  };

  WindowsSystemProxyResolver() = default;
  WindowsSystemProxyResolver(const WindowsSystemProxyResolver&) = delete;
  WindowsSystemProxyResolver& operator=(const WindowsSystemProxyResolver&) =
      delete;
  virtual ~WindowsSystemProxyResolver() = default;

  // Asynchronously finds a proxy for `url`. The `callback_target` must outlive
  // `this`.
  virtual std::unique_ptr<Request> GetProxyForUrl(
      const GURL& url,
      WindowsSystemProxyResolutionRequest* callback_target) = 0;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLVER_H_
