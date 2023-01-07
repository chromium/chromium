// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_manager.h"

#include "content/browser/buckets/bucket_context.h"
#include "content/browser/buckets/bucket_manager_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content {

BucketManager::BucketManager(StoragePartitionImpl* storage_partition)
    : storage_partition_(storage_partition) {}

BucketManager::~BucketManager() = default;

void BucketManager::BindReceiver(
    base::WeakPtr<BucketContext> context,
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
    mojo::ReportBadMessageCallback bad_message_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(context);

  blink::StorageKey storage_key = context->GetBucketStorageKey();
  auto it = hosts_.find(storage_key);
  if (it != hosts_.end()) {
    it->second->BindReceiver(std::move(receiver), context);
    return;
  }

  if (!network::IsOriginPotentiallyTrustworthy(storage_key.origin())) {
    std::move(bad_message_callback)
        .Run("Called Buckets from an insecure context");
    return;
  }

  auto [insert_it, insert_succeeded] = hosts_.insert(
      {storage_key, std::make_unique<BucketManagerHost>(this, storage_key)});
  DCHECK(insert_succeeded);
  insert_it->second->BindReceiver(std::move(receiver), context);
}

void BucketManager::OnHostReceiverDisconnect(BucketManagerHost* host,
                                             base::PassKey<BucketManagerHost>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host != nullptr);
  DCHECK_GT(hosts_.count(host->storage_key()), 0u);
  DCHECK_EQ(hosts_[host->storage_key()].get(), host);

  if (host->has_connected_receivers())
    return;

  hosts_.erase(host->storage_key());
}

}  // namespace content
