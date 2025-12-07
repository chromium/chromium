// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ISOLATION_PARTITION_H_
#define NET_BASE_NETWORK_ISOLATION_PARTITION_H_

#include <stdint.h>

#include <string>

namespace net {

// Specifies the use case for IsolationInfo, NetworkIsolationKey, and
// NetworkAnonymizationKey. This allows further partitioning of network state
// (e.g., HTTP cache) beyond what's provided by the top-level site and frame
// site. This is useful for isolating network state for specific features.
//
// This enum gets serialized to disk, so values of
// existing entries must not change when adding/removing values, and obsolete
// values must not be reused.
enum class NetworkIsolationPartition : int32_t {
  // General use case. This is the default and should be used for most
  // requests.
  kGeneral = 0,
  // This use case isolates network state for Protected Audience seller
  // worklets.
  kProtectedAudienceSellerWorklet = 1,
  // This use case isolates network state for FedCM-related requests.
  kFedCmUncredentialedRequests = 2,

  kMaxValue = kFedCmUncredentialedRequests
};

std::string NetworkIsolationPartitionToDebugString(
    NetworkIsolationPartition network_isolation_partition);

}  // namespace net

#endif  // NET_BASE_NETWORK_ISOLATION_PARTITION_H_
