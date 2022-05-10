// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_host.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/buckets/bucket_manager_host.h"

namespace content {

BucketHost::BucketHost(BucketManagerHost* bucket_manager_host,
                       const storage::BucketInfo& bucket_info,
                       blink::mojom::BucketPoliciesPtr policies)
    : bucket_manager_host_(bucket_manager_host),
      bucket_info_(bucket_info),
      policies_(std::move(policies)) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BucketHost::OnReceiverDisconnected, base::Unretained(this)));
}

BucketHost::~BucketHost() = default;

mojo::PendingRemote<blink::mojom::BucketHost>
BucketHost::CreateStorageBucketBinding() {
  mojo::PendingRemote<blink::mojom::BucketHost> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void BucketHost::Persist(PersistCallback callback) {
  // TODO(ayui): Add implementation for requesting Storage Bucket persistence.
  policies_->has_persisted = true;
  policies_->persisted = true;
  std::move(callback).Run(true, true);
}

void BucketHost::Persisted(PersistedCallback callback) {
  // TODO(ayui): Retrieve from DB once Storage Buckets table is implemented.
  std::move(callback).Run(policies_->has_persisted && policies_->persisted,
                          true);
}

void BucketHost::Estimate(EstimateCallback callback) {
  // TODO(ayui): Add implementation once connected to QuotaManager.
  std::move(callback).Run(0, 0, true);
}

void BucketHost::Durability(DurabilityCallback callback) {
  auto durability = policies_->has_durability
                        ? policies_->durability
                        : blink::mojom::BucketDurability::kRelaxed;
  // TODO(ayui): Retrieve from DB once Storage Buckets table is implemented.
  std::move(callback).Run(durability, true);
}

void BucketHost::SetExpires(base::Time expires, SetExpiresCallback callback) {
  // TODO(ayui): Update DB once Storage Buckets table is implemented.
  policies_->expires = expires;
  std::move(callback).Run(true);
}

void BucketHost::Expires(ExpiresCallback callback) {
  // TODO(ayui): Retrieve from DB once Storage Buckets table is implemented.
  std::move(callback).Run(policies_->expires, true);
}

void BucketHost::OnReceiverDisconnected() {
  if (!receivers_.empty())
    return;
  // Destroys `this`.
  bucket_manager_host_->RemoveBucketHost(bucket_info_.name);
}

}  // namespace content
