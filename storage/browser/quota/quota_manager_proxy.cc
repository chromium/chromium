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
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

QuotaManagerProxy::QuotaManagerProxy(
    QuotaManagerImpl* quota_manager_impl,
    scoped_refptr<base::SequencedTaskRunner> quota_manager_impl_task_runner)
    : quota_manager_impl_(quota_manager_impl),
      quota_manager_impl_task_runner_(
          std::move(quota_manager_impl_task_runner)) {
  DCHECK(quota_manager_impl_task_runner_.get());

  DETACH_FROM_SEQUENCE(quota_manager_impl_sequence_checker_);
}

void QuotaManagerProxy::RegisterLegacyClient(
    scoped_refptr<QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::RegisterLegacyClient, this,
                       std::move(client), client_type, storage_types));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (!quota_manager_impl_) {
    client->OnQuotaManagerDestroyed();
    return;
  }

  quota_manager_impl_->RegisterLegacyClient(std::move(client), client_type,
                                            storage_types);
}

void QuotaManagerProxy::RegisterClient(
    mojo::PendingRemote<mojom::QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::RegisterClient, this,
                       std::move(client), client_type, storage_types));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_) {
    quota_manager_impl_->RegisterClient(std::move(client), client_type,
                                        storage_types);
  }
}

void QuotaManagerProxy::NotifyStorageAccessed(const url::Origin& origin,
                                              blink::mojom::StorageType type,
                                              base::Time access_time) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyStorageAccessed,
                                  this, origin, type, access_time));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->NotifyStorageAccessed(origin, type, access_time);
}

void QuotaManagerProxy::NotifyStorageModified(
    QuotaClientType client_id,
    const url::Origin& origin,
    blink::mojom::StorageType type,
    int64_t delta,
    base::Time modification_time,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  DCHECK(!callback || callback_task_runner);
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyStorageModified, this,
                       client_id, origin, type, delta, modification_time,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_) {
    base::OnceClosure manager_callback;
    if (callback) {
      manager_callback = base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
             base::OnceClosure callback) {
            if (callback_task_runner->RunsTasksInCurrentSequence()) {
              std::move(callback).Run();
              return;
            }
            callback_task_runner->PostTask(FROM_HERE, std::move(callback));
          },
          std::move(callback_task_runner), std::move(callback));
    }
    quota_manager_impl_->NotifyStorageModified(client_id, origin, type, delta,
                                               modification_time,
                                               std::move(manager_callback));
  }
}

void QuotaManagerProxy::NotifyOriginInUse(const url::Origin& origin) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyOriginInUse, this, origin));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->NotifyOriginInUse(origin);
}

void QuotaManagerProxy::NotifyOriginNoLongerInUse(const url::Origin& origin) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyOriginNoLongerInUse,
                                  this, origin));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->NotifyOriginNoLongerInUse(origin);
}

void QuotaManagerProxy::NotifyWriteFailed(const url::Origin& origin) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyWriteFailed, this, origin));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->NotifyWriteFailed(origin);
}

void QuotaManagerProxy::SetUsageCacheEnabled(QuotaClientType client_id,
                                             const url::Origin& origin,
                                             blink::mojom::StorageType type,
                                             bool enabled) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::SetUsageCacheEnabled,
                                  this, client_id, origin, type, enabled));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->SetUsageCacheEnabled(client_id, origin, type, enabled);
}

namespace {

void DidGetUsageAndQuota(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    QuotaManagerProxy::UsageAndQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  if (callback_task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run(status, usage, quota);
    return;
  }
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status, usage, quota));
}

}  // namespace

void QuotaManagerProxy::GetUsageAndQuota(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    UsageAndQuotaCallback callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetUsageAndQuota, this, origin, type,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (!quota_manager_impl_) {
    DidGetUsageAndQuota(std::move(callback_task_runner), std::move(callback),
                        blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0);
    return;
  }

  quota_manager_impl_->GetUsageAndQuota(
      origin, type,
      base::BindOnce(&DidGetUsageAndQuota, std::move(callback_task_runner),
                     std::move(callback)));
}

void QuotaManagerProxy::IsStorageUnlimited(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(bool)> callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::IsStorageUnlimited, this,
                                  origin, type, std::move(callback_task_runner),
                                  std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  bool is_storage_unlimited =
      quota_manager_impl_
          ? quota_manager_impl_->IsStorageUnlimited(origin, type)
          : false;

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
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::OverrideQuotaForOrigin, this,
                       handle_id, origin, quota_size,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->OverrideQuotaForOrigin(handle_id, origin, quota_size);

  if (callback_task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run();
    return;
  }
  callback_task_runner->PostTask(FROM_HERE, std::move(callback));
}

void QuotaManagerProxy::WithdrawOverridesForHandle(int handle_id) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::WithdrawOverridesForHandle, this,
                       handle_id));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->WithdrawOverridesForHandle(handle_id);
}

void QuotaManagerProxy::GetOverrideHandleId(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(int)> callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetOverrideHandleId, this,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  int handle_id =
      quota_manager_impl_ ? quota_manager_impl_->GetOverrideHandleId() : 0;

  if (callback_task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run(handle_id);
    return;
  }
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), handle_id));
}

QuotaManagerProxy::~QuotaManagerProxy() = default;

void QuotaManagerProxy::InvalidateQuotaManagerImpl(
    base::PassKey<QuotaManagerImpl>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  DCHECK(quota_manager_impl_) << __func__ << " called multiple times";
  quota_manager_impl_ = nullptr;
}

}  // namespace storage
