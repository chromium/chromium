// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_NETWORK_RESTRICTIONS_WORKER_THROTTLE_H_
#define CONTENT_BROWSER_WORKER_HOST_NETWORK_RESTRICTIONS_WORKER_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {

class StoragePartition;

// A URLLoaderThrottle that applies network restrictions for a worker's
// subresources based on the worker script's response headers (for network
// workers) or the creator's policies (for local workers).
class NetworkRestrictionsWorkerThrottle : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<NetworkRestrictionsWorkerThrottle> Create(
      StoragePartition* storage_partition,
      const base::UnguessableToken& network_restrictions_id,
      PolicyContainerPolicies creator_policies);

  NetworkRestrictionsWorkerThrottle(
      StoragePartition* storage_partition,
      const base::UnguessableToken& network_restrictions_id,
      PolicyContainerPolicies creator_policies);
  ~NetworkRestrictionsWorkerThrottle() override;

  // blink::URLLoaderThrottle:
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  const char* NameForLoggingWillProcessResponse() override;

 private:
  void OnRevokeComplete();

  raw_ptr<StoragePartition> storage_partition_;
  const base::UnguessableToken network_restrictions_id_;
  const PolicyContainerPolicies creator_policies_;

  base::WeakPtrFactory<NetworkRestrictionsWorkerThrottle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_NETWORK_RESTRICTIONS_WORKER_THROTTLE_H_
