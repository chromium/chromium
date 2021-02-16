// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager_proxy.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace storage {

MockQuotaManagerProxy::MockQuotaManagerProxy(
    MockQuotaManager* quota_manager,
    scoped_refptr<base::SequencedTaskRunner> quota_manager_task_runner)
    : QuotaManagerProxy(quota_manager, std::move(quota_manager_task_runner)),
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

void MockQuotaManagerProxy::RegisterLegacyClient(
    scoped_refptr<QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  DCHECK(!registered_client_);
  registered_legacy_client_ = std::move(client);
}

void MockQuotaManagerProxy::SimulateQuotaManagerDestroyed() {
  if (registered_legacy_client_) {
    // We cannot call this in the destructor as the client (indirectly)
    // holds a refptr of the proxy.
    registered_legacy_client_->OnQuotaManagerDestroyed();
    registered_legacy_client_ = nullptr;
  }

  if (registered_client_) {
    registered_client_.reset();
  }
}

void MockQuotaManagerProxy::GetUsageAndQuota(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    QuotaManager::UsageAndQuotaCallback callback) {
  if (mock_quota_manager_) {
    mock_quota_manager_->GetUsageAndQuota(origin, type, std::move(callback));
  }
}

void MockQuotaManagerProxy::NotifyStorageAccessed(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    base::Time access_time) {
  ++storage_accessed_count_;
  last_notified_origin_ = origin;
  last_notified_type_ = type;
}

void MockQuotaManagerProxy::NotifyStorageModified(
    storage::QuotaClientType client_id,
    const url::Origin& origin,
    blink::mojom::StorageType type,
    int64_t delta,
    base::Time modification_time,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  ++storage_modified_count_;
  last_notified_origin_ = origin;
  last_notified_type_ = type;
  last_notified_delta_ = delta;
  if (mock_quota_manager_) {
    mock_quota_manager_->UpdateUsage(origin, type, delta);
  }
  if (callback)
    callback_task_runner->PostTask(FROM_HERE, std::move(callback));
}

MockQuotaManagerProxy::~MockQuotaManagerProxy() {
  DCHECK(!registered_client_);
  DCHECK(!registered_legacy_client_);
}

}  // namespace storage
