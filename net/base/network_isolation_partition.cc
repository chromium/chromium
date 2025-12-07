// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_isolation_partition.h"

#include <string>

namespace net {

std::string NetworkIsolationPartitionToDebugString(
    NetworkIsolationPartition network_isolation_partition) {
  switch (network_isolation_partition) {
    case NetworkIsolationPartition::kGeneral:
      return "general partition";
    case NetworkIsolationPartition::kProtectedAudienceSellerWorklet:
      return "protected audience seller worklet partition";
    case NetworkIsolationPartition::kFedCmUncredentialedRequests:
      return "fedcm uncredentialed requests";
  }
}

}  // namespace net
