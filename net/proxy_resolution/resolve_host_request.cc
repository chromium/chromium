// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/resolve_host_request.h"

namespace net {

ResolveHostResult::ResolveHostResult(
    int result_code,
    const HostResolver::ResolveHostRequest& completed_request)
    : result_code(result_code),
      is_address_list_empty(completed_request.GetAddressResults().empty()) {}

void ResolveHostResult::AddToDict(base::Value::Dict& dict) const {
  dict.Set("net_error", result_code);
  dict.Set("is_address_list_empty", is_address_list_empty);
}

}  // namespace net
