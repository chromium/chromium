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
    GetQuotaManagerProxy()->UpdateBucketPersistence(
        bucket_info_.id, /*persistent=*/true,
        base::SequencedTaskRunnerHandle::Get(),
        base::BindOnce(
            &BucketHost::DidUpdateBucket, weak_factory_.GetWeakPtr(),
            base::BindOnce(std::move(callback), /*persisted=*/true)));
  } else {
    std::move(callback).Run(false, false);
  }
}

void BucketHost::Persisted(PersistedCallback callback) {
  std::move(callback).Run(bucket_info_.persistent, true);
}

void BucketHost::Estimate(EstimateCallback callback) {
  GetQuotaManagerProxy()->GetBucketUsageAndQuota(
      bucket_info_, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&BucketHost::DidGetUsageAndQuota,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BucketHost::Durability(DurabilityCallback callback) {
  std::move(callback).Run(bucket_info_.durability, true);
}

void BucketHost::SetExpires(base::Time expires, SetExpiresCallback callback) {
  GetQuotaManagerProxy()->UpdateBucketExpiration(
      bucket_info_.id, expires, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&BucketHost::DidUpdateBucket, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
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

storage::QuotaManagerProxy* BucketHost::GetQuotaManagerProxy() {
  return bucket_manager_host_->GetQuotaManagerProxy();
}

void BucketHost::DidUpdateBucket(
    base::OnceCallback<void(bool)> callback,
    storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!bucket_info.ok()) {
    std::move(callback).Run(false);
    return;
  }

  bucket_info_ = bucket_info.value();
  std::move(callback).Run(true);
}

void BucketHost::DidGetUsageAndQuota(EstimateCallback callback,
                                     blink::mojom::QuotaStatusCode code,
                                     int64_t usage,
                                     int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(usage, quota,
                          code == blink::mojom::QuotaStatusCode::kOk);
}

}  // namespace content
