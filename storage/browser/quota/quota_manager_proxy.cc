// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_proxy.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

namespace {

void DidGetUsageAndQuota(base::SequencedTaskRunner* original_task_runner,
                         QuotaManagerProxy::UsageAndQuotaCallback callback,
                         blink::mojom::QuotaStatusCode status,
                         int64_t usage,
                         int64_t quota) {
  if (!original_task_runner->RunsTasksInCurrentSequence()) {
    original_task_runner->PostTask(
        FROM_HERE, base::BindOnce(&DidGetUsageAndQuota,
                                  base::RetainedRef(original_task_runner),
                                  std::move(callback), status, usage, quota));
    return;
  }
  std::move(callback).Run(status, usage, quota);
}

}  // namespace

void QuotaManagerProxy::RegisterClient(
    scoped_refptr<QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::RegisterClient, this,
                       std::move(client), client_type, storage_types));
    return;
  }

  if (manager_) {
    manager_->RegisterClient(std::move(client), client_type, storage_types);
  } else {
    client->OnQuotaManagerDestroyed();
  }
}

void QuotaManagerProxy::NotifyStorageAccessed(const url::Origin& origin,
                                              blink::mojom::StorageType type) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyStorageAccessed,
                                  this, origin, type));
    return;
  }

  if (manager_)
    manager_->NotifyStorageAccessed(origin, type);
}

void QuotaManagerProxy::NotifyStorageModified(QuotaClientType client_id,
                                              const url::Origin& origin,
                                              blink::mojom::StorageType type,
                                              int64_t delta) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyStorageModified,
                                  this, client_id, origin, type, delta));
    return;
  }

  if (manager_)
    manager_->NotifyStorageModified(client_id, origin, type, delta);
}

void QuotaManagerProxy::NotifyOriginInUse(const url::Origin& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyOriginInUse, this, origin));
    return;
  }

  if (manager_)
    manager_->NotifyOriginInUse(origin);
}

void QuotaManagerProxy::NotifyOriginNoLongerInUse(const url::Origin& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyOriginNoLongerInUse,
                                  this, origin));
    return;
  }
  if (manager_)
    manager_->NotifyOriginNoLongerInUse(origin);
}

void QuotaManagerProxy::NotifyWriteFailed(const url::Origin& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyWriteFailed, this, origin));
    return;
  }
  if (manager_)
    manager_->NotifyWriteFailed(origin);
}

void QuotaManagerProxy::SetUsageCacheEnabled(QuotaClientType client_id,
                                             const url::Origin& origin,
                                             blink::mojom::StorageType type,
                                             bool enabled) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::SetUsageCacheEnabled,
                                  this, client_id, origin, type, enabled));
    return;
  }
  if (manager_)
    manager_->SetUsageCacheEnabled(client_id, origin, type, enabled);
}

void QuotaManagerProxy::GetUsageAndQuota(
    base::SequencedTaskRunner* original_task_runner,
    const url::Origin& origin,
    blink::mojom::StorageType type,
    UsageAndQuotaCallback callback) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::GetUsageAndQuota, this,
                                  base::RetainedRef(original_task_runner),
                                  origin, type, std::move(callback)));
    return;
  }
  if (!manager_) {
    DidGetUsageAndQuota(original_task_runner, std::move(callback),
                        blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0);
    return;
  }

  manager_->GetUsageAndQuota(
      origin, type,
      base::BindOnce(&DidGetUsageAndQuota,
                     base::RetainedRef(original_task_runner),
                     std::move(callback)));
}

QuotaManager* QuotaManagerProxy::quota_manager() const {
  DCHECK(!io_thread_.get() || io_thread_->BelongsToCurrentThread());
  return manager_;
}

QuotaManagerProxy::QuotaManagerProxy(
    QuotaManager* manager,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread)
    : manager_(manager), io_thread_(std::move(io_thread)) {}

QuotaManagerProxy::~QuotaManagerProxy() = default;

}  // namespace storage
