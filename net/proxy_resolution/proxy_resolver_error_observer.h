// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLVER_ERROR_OBSERVER_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLVER_ERROR_OBSERVER_H_

#include <string>

#include "net/base/net_export.h"

namespace net {

// Interface for observing JavaScript error messages from PAC scripts.
class NET_EXPORT_PRIVATE ProxyResolverErrorObserver {
 public:
  ProxyResolverErrorObserver() = default;

  ProxyResolverErrorObserver(const ProxyResolverErrorObserver&) = delete;
  ProxyResolverErrorObserver& operator=(const ProxyResolverErrorObserver&) =
      delete;

  virtual ~ProxyResolverErrorObserver() = default;

  // Handler for when an error is encountered. |line_number| may be -1
  // if a line number is not applicable to this error. |error| is a message
  // describing the error.
  //
  // Note on threading: This may get called from a worker thread. If the
  // backing proxy resolver is ProxyResolverV8Tracing, then it will not
  // be called concurrently, however it will be called from a different
  // thread than the proxy resolver's origin thread.
  virtual void OnPACScriptError(int line_number,
                                const std::u16string& error) = 0;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLVER_ERROR_OBSERVER_H_
