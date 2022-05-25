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
                       const storage::BucketInfo& bucket_info)
    : bucket_manager_host_(bucket_manager_host), bucket_info_(bucket_info) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BucketHost::OnReceiverDisconnected, base::Unretained(this)));
}

BucketHost::~BucketHost() = default;

mojo::PendingRemote<blink::mojom::BucketHost>
BucketHost::CreateStorageBucketBinding(
    const PermissionDecisionCallback& permission_decision) {
  mojo::PendingRemote<blink::mojom::BucketHost> remote;
  permission_decider_map_.emplace(
      receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver()),
      permission_decision);
  return remote;
}

void BucketHost::OnUpdate(const storage::BucketInfo& bucket_info) {
  bucket_info_ = bucket_info;
}

void BucketHost::Persist(PersistCallback callback) {
  if (bucket_info_.persistent) {
    std::move(callback).Run(true, true);
    return;
  }

  auto it = permission_decider_map_.find(receivers_.current_receiver());
  if (it == permission_decider_map_.end()) {
    NOTREACHED();
    std::move(callback).Run(false, false);
    return;
  }
  if (it->second.Run(blink::PermissionType::DURABLE_STORAGE) ==
      blink::mojom::PermissionStatus::GRANTED) {
    bucket_manager_host_->UpdateBucketPersistence(
        bucket_info_.id, true, base::BindOnce(std::move(callback), true));
  } else {
    std::move(callback).Run(false, false);
  }
}

void BucketHost::Persisted(PersistedCallback callback) {
  std::move(callback).Run(bucket_info_.persistent, true);
}

void BucketHost::Estimate(EstimateCallback callback) {
  // TODO(ayui): Add implementation once connected to QuotaManager.
  std::move(callback).Run(0, 0, true);
}

void BucketHost::Durability(DurabilityCallback callback) {
  std::move(callback).Run(bucket_info_.durability, true);
}

void BucketHost::SetExpires(base::Time expires, SetExpiresCallback callback) {
  bucket_manager_host_->UpdateBucketExpiration(bucket_info_.id, expires,
                                               std::move(callback));
}

void BucketHost::Expires(ExpiresCallback callback) {
  absl::optional<base::Time> expires;
  if (!bucket_info_.expiration.is_null())
    expires = bucket_info_.expiration;
  std::move(callback).Run(expires, true);
}

void BucketHost::OnReceiverDisconnected() {
  permission_decider_map_.erase(receivers_.current_receiver());
  if (!receivers_.empty())
    return;
  // Destroys `this`.
  bucket_manager_host_->RemoveBucketHost(bucket_info_.name);
}

}  // namespace content
