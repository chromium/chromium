// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_ISOLATION_PARTITION_H_
#define NET_BASE_NETWORK_ISOLATION_PARTITION_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "net/base/net_export.h"

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

  // Protected Audience has been removed, so kProtectedAudienceSellerWorklet is
  // no longer used. However, it
  // may still appear in persisted data, and should not be reused.
  // kProtectedAudienceSellerWorklet = 1,

  // This use case isolates network state for FedCM-related requests.
  kFedCmUncredentialedRequests = 2,
  // This use case isolates network state for DNS over HTTPS requests.
  kDnsOverHttps = 3,
  // This use case isolates network state and V8 code cache for SharedWorkers
  // with SameSite=None cookie access restrictions (e.g. StorageAccessHandle).
  //
  // SharedWorkers that were created via the Storage Access API use a
  // first-party NetworkIsolationKey, but do not have access to
  // SameSite=Strict/Lax cookies.
  kSharedWorkerSameSiteCookiesNone = 4,

  kMaxValue = kSharedWorkerSameSiteCookiesNone,
};

std::string NetworkIsolationPartitionToDebugString(
    NetworkIsolationPartition network_isolation_partition);

// Converts `value` to a NetworkIsolationPartition, returning std::nullopt if
// it does not correspond to a current NetworkIsolationPartition value. Use this
// when deserializing persisted values, which may contain values that have since
// been removed.
NET_EXPORT_PRIVATE std::optional<NetworkIsolationPartition>
NetworkIsolationPartitionFromInt(int32_t value);

// Returns true if requests in this partition are allowed to have an empty
// IsolationInfo, even when
// `URLRequestContext::require_network_anonymization_key()` is true.
NET_EXPORT_PRIVATE bool NetworkIsolationPartitionAlwaysAllowEmptyPartition(
    NetworkIsolationPartition network_isolation_partition);

}  // namespace net

#endif  // NET_BASE_NETWORK_ISOLATION_PARTITION_H_
