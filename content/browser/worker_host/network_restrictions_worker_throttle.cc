// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/network_restrictions_worker_throttle.h"

#include "base/functional/bind.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/features.h"

namespace content {

// static
std::unique_ptr<NetworkRestrictionsWorkerThrottle>
NetworkRestrictionsWorkerThrottle::Create(
    StoragePartition* storage_partition,
    const base::UnguessableToken& network_restrictions_id,
    PolicyContainerPolicies creator_policies) {
  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return nullptr;
  }
  return std::make_unique<NetworkRestrictionsWorkerThrottle>(
      storage_partition, network_restrictions_id, std::move(creator_policies));
}

NetworkRestrictionsWorkerThrottle::NetworkRestrictionsWorkerThrottle(
    StoragePartition* storage_partition,
    const base::UnguessableToken& network_restrictions_id,
    PolicyContainerPolicies creator_policies)
    : storage_partition_(storage_partition),
      network_restrictions_id_(network_restrictions_id),
      creator_policies_(std::move(creator_policies)) {}

NetworkRestrictionsWorkerThrottle::~NetworkRestrictionsWorkerThrottle() =
    default;

void NetworkRestrictionsWorkerThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  PolicyContainerPolicies policies;
  if (response_url.SchemeIsLocal()) {
    policies.connection_allowlists = creator_policies_.connection_allowlists;
  } else {
    policies.connection_allowlists =
        response_head->parsed_headers->connection_allowlists;
  }

  if (!policies.connection_allowlists.enforced) {
    return;
  }

  std::set<std::string> allowlisted_patterns;
  for (const auto& pattern_string :
       policies.connection_allowlists.enforced->allowlist) {
    allowlisted_patterns.insert(pattern_string);
  }

  *defer = true;
  static_cast<StoragePartitionImpl*>(storage_partition_)
      ->RevokeNetworkForNoncesInNetworkContext(
          {{network_restrictions_id_, std::move(allowlisted_patterns)}},
          base::BindOnce(&NetworkRestrictionsWorkerThrottle::OnRevokeComplete,
                         weak_factory_.GetWeakPtr()));
}

const char*
NetworkRestrictionsWorkerThrottle::NameForLoggingWillProcessResponse() {
  return "NetworkRestrictionsWorkerThrottle";
}

void NetworkRestrictionsWorkerThrottle::OnRevokeComplete() {
  delegate_->Resume();
}

}  // namespace content
