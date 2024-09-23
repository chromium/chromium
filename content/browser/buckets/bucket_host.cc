// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_host.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "content/browser/buckets/bucket_context.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/buckets/bucket_manager_host.h"
#include "content/browser/locks/lock_manager.h"
#include "content/browser/renderer_host/indexed_db_client_state_checker_factory.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

// `BucketContext` assumes these two mojom methods have the same signature. This
// assert is here instead of in `bucket_context.h` to avoid pulling in too many
// includes in a header.
static_assert(
    std::is_same_v<
        blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback,
        blink::mojom::BucketHost::GetDirectoryCallback>);

BucketHost::BucketHost(BucketManagerHost* bucket_manager_host,
                       const storage::BucketInfo& bucket_info)
    : bucket_manager_host_(bucket_manager_host),
      bucket_info_(bucket_info),
      bucket_id_(bucket_info.id) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BucketHost::OnReceiverDisconnected, base::Unretained(this)));
}

BucketHost::~BucketHost() = default;

mojo::PendingRemote<blink::mojom::BucketHost>
BucketHost::CreateStorageBucketBinding(
    base::WeakPtr<BucketContext> bucket_context) {
  DCHECK(bucket_context);
  mojo::PendingRemote<blink::mojom::BucketHost> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver(), bucket_context);
  return remote;
}

void BucketHost::PassStorageBucketBinding(
    base::WeakPtr<BucketContext> bucket_context,
    mojo::PendingReceiver<blink::mojom::BucketHost> receiver) {
  DCHECK(bucket_context);
  receivers_.Add(this, std::move(receiver), bucket_context);
}

void BucketHost::Persist(PersistCallback callback) {
  if (bucket_info_.is_null() || !receivers_.current_context()) {
    std::move(callback).Run(false, /*success=*/false);
    return;
  }

  // Persistence is a one-way operation. If the bucket is already persistent, it
  // can't be made non-persistent, so skip the permission status check (which
  // hypothetically could have reverted from GRANTED to DENIED).
  if (bucket_info_.persistent) {
    std::move(callback).Run(true, /*success=*/true);
    return;
  }

  if (receivers_.current_context()->GetPermissionStatus(
          blink::PermissionType::DURABLE_STORAGE) ==
      blink::mojom::PermissionStatus::GRANTED) {
    GetQuotaManagerProxy()->UpdateBucketPersistence(
        bucket_id_, /*persistent=*/true,
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::BindOnce(
            &BucketHost::DidGetBucket, weak_factory_.GetWeakPtr(),
            base::BindOnce(&BucketHost::DidValidateForPersist,
                           base::Unretained(this), std::move(callback))));
  } else {
    std::move(callback).Run(false, /*success=*/true);
  }
}

void BucketHost::Persisted(PersistedCallback callback) {
  if (bucket_info_.is_null()) {
    std::move(callback).Run(false, false);
    return;
  }

  GetQuotaManagerProxy()->GetBucketById(
      bucket_id_, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          &BucketHost::DidGetBucket, weak_factory_.GetWeakPtr(),
          base::BindOnce(&BucketHost::DidValidateForPersist,
                         base::Unretained(this), std::move(callback))));
}

void BucketHost::DidValidateForPersist(PersistedCallback callback,
                                       bool bucket_exists) {
  std::move(callback).Run(bucket_info_.persistent, bucket_exists);
}

void BucketHost::Estimate(EstimateCallback callback) {
  if (bucket_info_.is_null()) {
    std::move(callback).Run({}, {}, /*success=*/false);
    return;
  }

  GetQuotaManagerProxy()->GetBucketUsageAndQuota(
      bucket_id_, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BucketHost::DidGetUsageAndQuota,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BucketHost::Durability(DurabilityCallback callback) {
  if (bucket_info_.is_null()) {
    std::move(callback).Run({}, false);
    return;
  }

  GetQuotaManagerProxy()->GetBucketById(
      bucket_id_, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          &BucketHost::DidGetBucket, weak_factory_.GetWeakPtr(),
          base::BindOnce(&BucketHost::DidValidateForDurability,
                         base::Unretained(this), std::move(callback))));
}

void BucketHost::DidValidateForDurability(DurabilityCallback callback,
                                          bool bucket_exists) {
  std::move(callback).Run(bucket_info_.durability, bucket_exists);
}

void BucketHost::SetExpires(base::Time expires, SetExpiresCallback callback) {
  GetQuotaManagerProxy()->UpdateBucketExpiration(
      bucket_id_, expires, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&BucketHost::DidGetBucket, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void BucketHost::Expires(ExpiresCallback callback) {
  if (bucket_info_.is_null()) {
    std::move(callback).Run(std::nullopt, /*success=*/false);
    return;
  }

  GetQuotaManagerProxy()->GetBucketById(
      bucket_id_, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          &BucketHost::DidGetBucket, weak_factory_.GetWeakPtr(),
          base::BindOnce(&BucketHost::DidValidateForExpires,
                         base::Unretained(this), std::move(callback))));
}

void BucketHost::DidValidateForExpires(ExpiresCallback callback,
                                       bool bucket_exists) {
  std::optional<base::Time> expires;
  if (bucket_exists && !bucket_info_.expiration.is_null()) {
    expires = bucket_info_.expiration;
  }

  std::move(callback).Run(expires, bucket_exists);
}

void BucketHost::GetIdbFactory(
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  auto bucket_context = receivers_.current_context();
  if (!bucket_context) {
    return;
  }

  storage::BucketClientInfo client_info = bucket_context->GetBucketClientInfo();
  auto state_checker =
      IndexedDBClientStateCheckerFactory::InitializePendingRemote(client_info);
  if (!state_checker) {
    // The client is not in a valid state to use IndexedDB.
    return;
  }

  bucket_manager_host_->GetStoragePartition()->BindIndexedDB(
      bucket_info_.ToBucketLocator(), client_info, std::move(state_checker),
      std::move(receiver));
}

void BucketHost::GetCaches(
    mojo::PendingReceiver<blink::mojom::CacheStorage> caches) {
  auto bucket_context = receivers_.current_context();
  if (!bucket_context) {
    return;
  }

  bucket_context->BindCacheStorageForBucket(bucket_info_, std::move(caches));
}

void BucketHost::GetDirectory(GetDirectoryCallback callback) {
  auto bucket_context = receivers_.current_context();
  if (!bucket_context) {
    return;
  }

  bucket_context->GetSandboxedFileSystemForBucket(
      bucket_info_, /*directory_path_components=*/{}, std::move(callback));
}

void BucketHost::GetDirectoryForDevtools(
    const std::vector<std::string>& directory_path_components,
    GetDirectoryCallback callback) {
  auto bucket_context = receivers_.current_context();
  if (!bucket_context) {
    return;
  }

  bucket_context->GetSandboxedFileSystemForBucket(
      bucket_info_, directory_path_components, std::move(callback));
}

void BucketHost::GetLockManager(
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  bucket_manager_host_->GetStoragePartition()->GetLockManager()->BindReceiver(
      bucket_id_, std::move(receiver));
}

void BucketHost::OnReceiverDisconnected() {
  if (!receivers_.empty()) {
    return;
  }
  // Destroys `this`.
  bucket_manager_host_->RemoveBucketHost(bucket_id_);
}

storage::QuotaManagerProxy* BucketHost::GetQuotaManagerProxy() {
  return bucket_manager_host_->GetQuotaManagerProxy();
}

void BucketHost::DidGetBucket(
    base::OnceCallback<void(bool)> callback,
    storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bucket_info_ = bucket_info.value_or(storage::BucketInfo());
  std::move(callback).Run(bucket_info.has_value());
}

void BucketHost::DidGetUsageAndQuota(EstimateCallback callback,
                                     blink::mojom::QuotaStatusCode code,
                                     int64_t usage,
                                     int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (code != blink::mojom::QuotaStatusCode::kOk) {
    bucket_info_ = {};
  }

  std::move(callback).Run(usage, quota,
                          code == blink::mojom::QuotaStatusCode::kOk);
}

}  // namespace content
