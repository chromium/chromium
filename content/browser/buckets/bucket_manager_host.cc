// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_manager_host.h"

#include "base/containers/contains.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/buckets/bucket_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

BucketManagerHost::BucketManagerHost(BucketManager* manager,
                                     const blink::StorageKey& storage_key)
    : manager_(manager), storage_key_(storage_key) {
  DCHECK(manager != nullptr);

  // base::Unretained is safe here because this BucketManagerHost owns
  // `receivers_`. So, the unretained BucketManagerHost is guaranteed to
  // outlive |receivers_| and the closure that it uses.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BucketManagerHost::OnReceiverDisconnect, base::Unretained(this)));
}

BucketManagerHost::~BucketManagerHost() = default;

void BucketManagerHost::BindReceiver(
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver,
    base::WeakPtr<BucketContext> context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver), std::move(context));
}

void BucketManagerHost::OpenBucket(const std::string& name,
                                   blink::mojom::BucketPoliciesPtr policies,
                                   OpenBucketCallback callback) {
  if (!IsValidBucketName(name)) {
    receivers_.ReportBadMessage("Invalid bucket name");
    return;
  }

  storage::BucketInitParams params(storage_key_, name);
  if (policies) {
    if (policies->expires)
      params.expiration = *policies->expires;

    if (policies->has_quota)
      params.quota = policies->quota;

    if (policies->has_durability)
      params.durability = policies->durability;

    if (policies->has_persisted) {
      // Only grant persistence if permitted.
      if (receivers_.current_context() &&
          receivers_.current_context()->GetPermissionStatus(
              blink::PermissionType::DURABLE_STORAGE) ==
              blink::mojom::PermissionStatus::GRANTED) {
        params.persistent = policies->persisted;
      }
    }
  }

  GetQuotaManagerProxy()->UpdateOrCreateBucket(
      params, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BucketManagerHost::DidGetBucket,
                     weak_factory_.GetWeakPtr(), receivers_.current_context(),
                     std::move(callback)));
}

void BucketManagerHost::Keys(KeysCallback callback) {
  GetQuotaManagerProxy()->GetBucketsForStorageKey(
      storage_key_, blink::mojom::StorageType::kTemporary,
      /*delete_expired=*/true, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BucketManagerHost::DidGetBuckets,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BucketManagerHost::DeleteBucket(const std::string& name,
                                     DeleteBucketCallback callback) {
  if (!IsValidBucketName(name)) {
    receivers_.ReportBadMessage("Invalid bucket name");
    return;
  }

  GetQuotaManagerProxy()->DeleteBucket(
      storage_key_, name, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BucketManagerHost::DidDeleteBucket,
                     weak_factory_.GetWeakPtr(), name, std::move(callback)));
}

void BucketManagerHost::RemoveBucketHost(storage::BucketId id) {
  DCHECK(base::Contains(bucket_map_, id));
  bucket_map_.erase(id);
}

StoragePartitionImpl* BucketManagerHost::GetStoragePartition() {
  return manager_->storage_partition();
}

storage::QuotaManagerProxy* BucketManagerHost::GetQuotaManagerProxy() {
  return manager_->storage_partition()->GetQuotaManagerProxy();
}

void BucketManagerHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  manager_->OnHostReceiverDisconnect(this, base::PassKey<BucketManagerHost>());
}

void BucketManagerHost::DidGetBucket(
    base::WeakPtr<BucketContext> bucket_context,
    OpenBucketCallback callback,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.ok() || !bucket_context) {
    // Getting a bucket can fail if there is a database error.
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  const auto& bucket = result.value();
  auto it = bucket_map_.find(bucket.id);
  if (it == bucket_map_.end()) {
    it = bucket_map_
             .emplace(bucket.id, std::make_unique<BucketHost>(this, bucket))
             .first;
  }

  auto pending_remote = it->second->CreateStorageBucketBinding(bucket_context);
  std::move(callback).Run(std::move(pending_remote));
}

void BucketManagerHost::DidGetBuckets(
    KeysCallback callback,
    storage::QuotaErrorOr<std::set<storage::BucketInfo>> buckets) {
  if (!buckets.ok()) {
    std::move(callback).Run({}, false);
    return;
  }

  std::vector<std::string> keys;
  for (auto& bucket : buckets.value()) {
    if (!bucket.is_default())
      keys.push_back(bucket.name);
  }
  std::sort(keys.begin(), keys.end());

  std::move(callback).Run(keys, true);
}

void BucketManagerHost::DidDeleteBucket(const std::string& bucket_name,
                                        DeleteBucketCallback callback,
                                        blink::mojom::QuotaStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(status == blink::mojom::QuotaStatusCode::kOk);
}

}  // namespace content
