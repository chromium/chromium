// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager_proxy.h"

#include "base/single_thread_task_runner.h"

namespace content {

MockQuotaManagerProxy::MockQuotaManagerProxy(
    MockQuotaManager* quota_manager,
    base::SingleThreadTaskRunner* task_runner)
    : QuotaManagerProxy(quota_manager, task_runner),
      storage_accessed_count_(0),
      storage_modified_count_(0),
      last_notified_type_(blink::mojom::StorageType::kUnknown),
      last_notified_delta_(0),
      registered_client_(nullptr) {}

void MockQuotaManagerProxy::RegisterClient(QuotaClient* client) {
  DCHECK(!registered_client_);
  registered_client_ = client;
}

void MockQuotaManagerProxy::SimulateQuotaManagerDestroyed() {
  if (registered_client_) {
    // We cannot call this in the destructor as the client (indirectly)
    // holds a refptr of the proxy.
    registered_client_->OnQuotaManagerDestroyed();
    registered_client_ = nullptr;
  }
}

void MockQuotaManagerProxy::GetUsageAndQuota(
    base::SequencedTaskRunner* original_task_runner,
    const url::Origin& origin,
    blink::mojom::StorageType type,
    QuotaManager::UsageAndQuotaCallback callback) {
  if (mock_manager()) {
    mock_manager()->GetUsageAndQuota(origin, type, std::move(callback));
  }
}

void MockQuotaManagerProxy::NotifyStorageAccessed(
    QuotaClient::ID client_id,
    const url::Origin& origin,
    blink::mojom::StorageType type) {
  ++storage_accessed_count_;
  last_notified_origin_ = origin;
  last_notified_type_ = type;
}

void MockQuotaManagerProxy::NotifyStorageModified(
    QuotaClient::ID client_id,
    const url::Origin& origin,
    blink::mojom::StorageType type,
    int64_t delta) {
  ++storage_modified_count_;
  last_notified_origin_ = origin;
  last_notified_type_ = type;
  last_notified_delta_ = delta;
  if (mock_manager())
    mock_manager()->UpdateUsage(origin, type, delta);
}

MockQuotaManagerProxy::~MockQuotaManagerProxy() {
  DCHECK(!registered_client_);
}

}  // namespace content
