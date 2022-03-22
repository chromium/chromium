// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager_proxy.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
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
      mock_quota_manager_(quota_manager),
      storage_accessed_count_(0),
      storage_modified_count_(0),
      last_notified_type_(blink::mojom::StorageType::kUnknown),
      last_notified_delta_(0) {}

void MockQuotaManagerProxy::RegisterClient(
    mojo::PendingRemote<storage::mojom::QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  DCHECK(!registered_client_);
  registered_client_.Bind(std::move(client));
}

void MockQuotaManagerProxy::GetOrCreateBucket(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (mock_quota_manager_) {
    mock_quota_manager_->GetOrCreateBucket(storage_key, bucket_name,
                                           std::move(callback));
  }
}

void MockQuotaManagerProxy::GetBucket(
    const blink::StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (mock_quota_manager_) {
    mock_quota_manager_->GetBucket(storage_key, bucket_name, type,
                                   std::move(callback));
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

void MockQuotaManagerProxy::NotifyStorageAccessed(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    base::Time access_time) {
  ++storage_accessed_count_;
  last_notified_storage_key_ = storage_key;
  last_notified_type_ = type;
}

void MockQuotaManagerProxy::NotifyStorageModified(
    storage::QuotaClientType client_id,
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    int64_t delta,
    base::Time modification_time,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  ++storage_modified_count_;
  last_notified_storage_key_ = storage_key;
  last_notified_type_ = type;
  last_notified_delta_ = delta;
  if (mock_quota_manager_) {
    mock_quota_manager_->UpdateUsage(storage_key, type, delta);
  }
  if (callback)
    callback_task_runner->PostTask(FROM_HERE, std::move(callback));
}

MockQuotaManagerProxy::~MockQuotaManagerProxy() = default;

}  // namespace storage
