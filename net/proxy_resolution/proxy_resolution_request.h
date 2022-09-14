// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLUTION_REQUEST_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLUTION_REQUEST_H_

#include "net/base/load_states.h"
#include "net/base/net_export.h"

namespace net {

// Used to track proxy resolution requests that complete asynchronously.
class NET_EXPORT ProxyResolutionRequest {
 public:
  ProxyResolutionRequest(const ProxyResolutionRequest&) = delete;
  ProxyResolutionRequest& operator=(const ProxyResolutionRequest&) = delete;

  virtual ~ProxyResolutionRequest() = default;
  virtual LoadState GetLoadState() const = 0;

 protected:
  ProxyResolutionRequest() = default;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLUTION_REQUEST_H_
