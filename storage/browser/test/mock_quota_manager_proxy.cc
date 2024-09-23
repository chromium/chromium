// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager_proxy.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

MockQuotaManagerProxy::MockQuotaManagerProxy(
    MockQuotaManager* quota_manager,
    scoped_refptr<base::SequencedTaskRunner> quota_manager_task_runner)
    : QuotaManagerProxy(
          quota_manager,
          std::move(quota_manager_task_runner),
          quota_manager ? quota_manager->profile_path() : base::FilePath()),
      mock_quota_manager_(quota_manager) {}

void MockQuotaManagerProxy::UpdateOrCreateBucket(
    const BucketInitParams& params,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (mock_quota_manager_) {
    mock_quota_manager_->UpdateOrCreateBucket(params, std::move(callback));
  }
}

QuotaErrorOr<BucketInfo> MockQuotaManagerProxy::GetOrCreateBucketSync(
    const BucketInitParams& params) {
  return (mock_quota_manager_)
             ? mock_quota_manager_->GetOrCreateBucketSync(params)
             : base::unexpected(QuotaError::kUnknownError);
}

void MockQuotaManagerProxy::CreateBucketForTesting(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (mock_quota_manager_) {
    mock_quota_manager_->CreateBucketForTesting(
        storage_key, bucket_name, storage_type, std::move(callback));
  }
}

void MockQuotaManagerProxy::GetBucketByNameUnsafe(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (mock_quota_manager_) {
    mock_quota_manager_->GetBucketByNameUnsafe(storage_key, bucket_name, type,
                                               std::move(callback));
  }
}

void MockQuotaManagerProxy::GetBucketById(
    const BucketId& bucket_id,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (mock_quota_manager_) {
    mock_quota_manager_->GetBucketById(bucket_id, std::move(callback));
  }
}

void MockQuotaManagerProxy::GetBucketsForStorageKey(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    bool delete_expired,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback) {
  if (mock_quota_manager_) {
    mock_quota_manager_->GetBucketsForStorageKey(
        storage_key, type, std::move(callback), delete_expired);
  } else {
    std::move(callback).Run(std::set<BucketInfo>());
  }
}

void MockQuotaManagerProxy::GetUsageAndQuota(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    QuotaManager::UsageAndQuotaCallback callback) {
  if (mock_quota_manager_) {
    mock_quota_manager_->GetUsageAndQuota(storage_key, type,
                                          std::move(callback));
  }
}

void MockQuotaManagerProxy::NotifyBucketAccessed(const BucketLocator& bucket,
                                                 base::Time access_time) {
  base::AutoLock locked(lock_);
  ++bucket_accessed_count_;
  last_notified_bucket_id_ = bucket.id;
  last_notified_storage_key_ = bucket.storage_key;
  last_notified_type_ = bucket.type;
}

void MockQuotaManagerProxy::NotifyBucketModified(
    QuotaClientType client_id,
    const BucketLocator& bucket,
    std::optional<int64_t> delta,
    base::Time modification_time,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  base::AutoLock locked(lock_);
  ++bucket_modified_count_;
  last_notified_bucket_id_ = bucket.id;
  last_notified_bucket_delta_ = delta;
  if (mock_quota_manager_) {
    mock_quota_manager_->UpdateUsage(bucket, delta);
  }
  if (callback) {
    callback_task_runner->PostTask(FROM_HERE, std::move(callback));
  }
}

MockQuotaManagerProxy::~MockQuotaManagerProxy() = default;

}  // namespace storage
