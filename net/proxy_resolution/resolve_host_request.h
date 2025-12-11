// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_RESOLVE_HOST_REQUEST_H_
#define NET_PROXY_RESOLUTION_RESOLVE_HOST_REQUEST_H_

#include "base/values.h"
#include "net/dns/host_resolver.h"

namespace net {

// Struct capturing the result code of a completed DNS resolution request.
struct ResolveHostResult {
  // Constructs the struct with `result_code` being the net error code of a
  // completed DNS resolution request `completed_request`.
  ResolveHostResult(int result_code,
                    const HostResolver::ResolveHostRequest& completed_request);

  void AddToDict(base::Value::Dict& dict) const;

  const int result_code;
  const bool is_address_list_empty;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_RESOLVE_HOST_REQUEST_H_
