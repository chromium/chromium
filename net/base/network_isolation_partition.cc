// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_isolation_partition.h"

#include <stdint.h>

#include <optional>
#include <string>

#include "base/notreached.h"

namespace net {

std::string NetworkIsolationPartitionToDebugString(
    NetworkIsolationPartition network_isolation_partition) {
  switch (network_isolation_partition) {
    case NetworkIsolationPartition::kGeneral:
      return "general partition";
    case NetworkIsolationPartition::kFedCmUncredentialedRequests:
      return "fedcm uncredentialed requests";
    case NetworkIsolationPartition::kDnsOverHttps:
      return "dns over https";
    case NetworkIsolationPartition::kSharedWorkerSameSiteCookiesNone:
      return "shared worker same site cookies none";
  }
  NOTREACHED();
}

std::optional<NetworkIsolationPartition> NetworkIsolationPartitionFromInt(
    int32_t value) {
  if (value < 0 ||
      value > static_cast<int32_t>(NetworkIsolationPartition::kMaxValue)) {
    return std::nullopt;
  }
  auto network_isolation_partition =
      static_cast<NetworkIsolationPartition>(value);
  switch (network_isolation_partition) {
    case NetworkIsolationPartition::kGeneral:
    case NetworkIsolationPartition::kFedCmUncredentialedRequests:
    case NetworkIsolationPartition::kDnsOverHttps:
    case NetworkIsolationPartition::kSharedWorkerSameSiteCookiesNone:
      return network_isolation_partition;
  }
  // Obsolete values are still in the range [0, kMaxValue], but should return
  // std::nullopt.
  return std::nullopt;
}

bool NetworkIsolationPartitionAlwaysAllowEmptyPartition(
    NetworkIsolationPartition network_isolation_partition) {
  return network_isolation_partition ==
         NetworkIsolationPartition::kDnsOverHttps;
}

}  // namespace net
