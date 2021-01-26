// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_proxy.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

QuotaManagerProxy::QuotaManagerProxy(
    QuotaManager* quota_manager,
    scoped_refptr<base::SequencedTaskRunner> quota_manager_task_runner)
    : quota_manager_(quota_manager),
      quota_manager_task_runner_(std::move(quota_manager_task_runner)) {
  DCHECK(quota_manager_task_runner_.get());

  DETACH_FROM_SEQUENCE(quota_manager_sequence_checker_);
}

void QuotaManagerProxy::RegisterLegacyClient(
    scoped_refptr<QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::RegisterLegacyClient, this,
                       std::move(client), client_type, storage_types));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (!quota_manager_) {
    client->OnQuotaManagerDestroyed();
    return;
  }

  quota_manager_->RegisterLegacyClient(std::move(client), client_type,
                                       storage_types);
}

void QuotaManagerProxy::RegisterClient(
    mojo::PendingRemote<mojom::QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::RegisterClient, this,
                       std::move(client), client_type, storage_types));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (quota_manager_) {
    quota_manager_->RegisterClient(std::move(client), client_type,
                                   storage_types);
  }
}

void QuotaManagerProxy::NotifyStorageAccessed(const url::Origin& origin,
                                              blink::mojom::StorageType type,
                                              base::Time access_time) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyStorageAccessed,
                                  this, origin, type, access_time));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (quota_manager_)
    quota_manager_->NotifyStorageAccessed(origin, type, access_time);
}

void QuotaManagerProxy::NotifyStorageModified(QuotaClientType client_id,
                                              const url::Origin& origin,
                                              blink::mojom::StorageType type,
                                              int64_t delta,
                                              base::Time modification_time) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyStorageModified, this,
                       client_id, origin, type, delta, modification_time));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (quota_manager_) {
    quota_manager_->NotifyStorageModified(client_id, origin, type, delta,
                                          modification_time);
  }
}

void QuotaManagerProxy::NotifyOriginInUse(const url::Origin& origin) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyOriginInUse, this, origin));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (quota_manager_)
    quota_manager_->NotifyOriginInUse(origin);
}

void QuotaManagerProxy::NotifyOriginNoLongerInUse(const url::Origin& origin) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyOriginNoLongerInUse,
                                  this, origin));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (quota_manager_)
    quota_manager_->NotifyOriginNoLongerInUse(origin);
}

void QuotaManagerProxy::NotifyWriteFailed(const url::Origin& origin) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyWriteFailed, this, origin));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (quota_manager_)
    quota_manager_->NotifyWriteFailed(origin);
}

void QuotaManagerProxy::SetUsageCacheEnabled(QuotaClientType client_id,
                                             const url::Origin& origin,
                                             blink::mojom::StorageType type,
                                             bool enabled) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::SetUsageCacheEnabled,
                                  this, client_id, origin, type, enabled));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (quota_manager_)
    quota_manager_->SetUsageCacheEnabled(client_id, origin, type, enabled);
}

namespace {

void DidGetUsageAndQuota(base::SequencedTaskRunner* original_task_runner,
                         QuotaManagerProxy::UsageAndQuotaCallback callback,
                         blink::mojom::QuotaStatusCode status,
                         int64_t usage,
                         int64_t quota) {
  if (original_task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run(status, usage, quota);
    return;
  }
  original_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DidGetUsageAndQuota,
                                base::RetainedRef(original_task_runner),
                                std::move(callback), status, usage, quota));
}

}  // namespace

void QuotaManagerProxy::GetUsageAndQuota(
    base::SequencedTaskRunner* original_task_runner,
    const url::Origin& origin,
    blink::mojom::StorageType type,
    UsageAndQuotaCallback callback) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::GetUsageAndQuota, this,
                                  base::RetainedRef(original_task_runner),
                                  origin, type, std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (!quota_manager_) {
    DidGetUsageAndQuota(original_task_runner, std::move(callback),
                        blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0);
    return;
  }

  quota_manager_->GetUsageAndQuota(
      origin, type,
      base::BindOnce(&DidGetUsageAndQuota,
                     base::RetainedRef(original_task_runner),
                     std::move(callback)));
}

void QuotaManagerProxy::IsStorageUnlimited(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(bool)> callback) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::IsStorageUnlimited, this,
                                  origin, type,
                                  std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  bool is_storage_unlimited =
      quota_manager_ ? quota_manager_->IsStorageUnlimited(origin, type) : false;

  if (callback_task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run(is_storage_unlimited);
    return;
  }
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), is_storage_unlimited));
}

std::unique_ptr<QuotaOverrideHandle>
QuotaManagerProxy::GetQuotaOverrideHandle() {
  return std::make_unique<QuotaOverrideHandle>(this);
}

void QuotaManagerProxy::OverrideQuotaForOrigin(
    int handle_id,
    url::Origin origin,
    base::Optional<int64_t> quota_size,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::OverrideQuotaForOrigin, this,
                       handle_id, origin, quota_size,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (quota_manager_)
    quota_manager_->OverrideQuotaForOrigin(handle_id, origin, quota_size);

  if (callback_task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run();
    return;
  }
  callback_task_runner->PostTask(FROM_HERE, std::move(callback));
}

void QuotaManagerProxy::WithdrawOverridesForHandle(int handle_id) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::WithdrawOverridesForHandle, this,
                       handle_id));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  if (quota_manager_)
    quota_manager_->WithdrawOverridesForHandle(handle_id);
}

QuotaManager* QuotaManagerProxy::quota_manager() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  DCHECK(quota_manager_task_runner_->RunsTasksInCurrentSequence());

  return quota_manager_;
}

void QuotaManagerProxy::GetOverrideHandleId(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(int)> callback) {
  if (!quota_manager_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetOverrideHandleId, this,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);
  int handle_id = quota_manager_ ? quota_manager_->GetOverrideHandleId() : 0;

  if (callback_task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run(handle_id);
    return;
  }
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), handle_id));
}

QuotaManagerProxy::~QuotaManagerProxy() = default;

void QuotaManagerProxy::InvalidateQuotaManager(base::PassKey<QuotaManager>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_sequence_checker_);

  DCHECK(quota_manager_) << __func__ << " called multiple times";
  quota_manager_ = nullptr;
}

}  // namespace storage
