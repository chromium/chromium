// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_RESOLV_READER_H_
#define NET_DNS_PUBLIC_RESOLV_READER_H_

#include <resolv.h>

#include <memory>
#include <optional>
#include <vector>

#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/public/scoped_res_state.h"

namespace net {

// Test-overridable class to handle the interactions with OS APIs for reading
// resolv.conf.
class NET_EXPORT ResolvReader {
 public:
  virtual ~ResolvReader() = default;

  // Null on failure.
  virtual std::unique_ptr<ScopedResState> GetResState();
};

// Returns configured DNS servers or nullopt on failure.
NET_EXPORT std::optional<std::vector<IPEndPoint>> GetNameservers(
    const struct __res_state& res);

}  // namespace net

#endif  // NET_DNS_PUBLIC_RESOLV_READER_H_
