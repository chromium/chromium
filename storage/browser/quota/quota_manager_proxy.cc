// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_proxy.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using ::blink::StorageKey;

namespace storage {

namespace {

void DidGetBucket(scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
                  base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
                  QuotaErrorOr<BucketInfo> result) {
  if (callback_task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run(std::move(result));
    return;
  }
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void DidGetStatus(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(blink::mojom::QuotaStatusCode)> callback,
    blink::mojom::QuotaStatusCode status) {
  if (callback_task_runner->RunsTasksInCurrentSequence()) {
    std::move(callback).Run(std::move(status));
    return;
  }
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(status)));
}

}  // namespace

QuotaManagerProxy::QuotaManagerProxy(
    QuotaManagerImpl* quota_manager_impl,
    scoped_refptr<base::SequencedTaskRunner> quota_manager_impl_task_runner)
    : quota_manager_impl_(quota_manager_impl),
      quota_manager_impl_task_runner_(
          std::move(quota_manager_impl_task_runner)) {
  DCHECK(quota_manager_impl_task_runner_.get());

  DETACH_FROM_SEQUENCE(quota_manager_impl_sequence_checker_);
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

void QuotaManagerProxy::GetOrCreateBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetOrCreateBucket, this, storage_key,
                       bucket_name, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (!quota_manager_impl_) {
    DidGetBucket(std::move(callback_task_runner), std::move(callback),
                 QuotaErrorOr<BucketInfo>(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->GetOrCreateBucket(
      storage_key, bucket_name,
      base::BindOnce(&DidGetBucket, std::move(callback_task_runner),
                     std::move(callback)));
}

void QuotaManagerProxy::CreateBucketForTesting(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::CreateBucketForTesting, this,
                       storage_key, bucket_name, storage_type,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (!quota_manager_impl_) {
    DidGetBucket(std::move(callback_task_runner), std::move(callback),
                 QuotaErrorOr<BucketInfo>(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->CreateBucketForTesting(  // IN-TEST
      storage_key, bucket_name, storage_type,
      base::BindOnce(&DidGetBucket, std::move(callback_task_runner),
                     std::move(callback)));
}

void QuotaManagerProxy::GetBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetBucket, this, storage_key,
                       bucket_name, type, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (!quota_manager_impl_) {
    DidGetBucket(std::move(callback_task_runner), std::move(callback),
                 QuotaErrorOr<BucketInfo>(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->GetBucket(
      storage_key, bucket_name, type,
      base::BindOnce(&DidGetBucket, std::move(callback_task_runner),
                     std::move(callback)));
}

void QuotaManagerProxy::DeleteBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(blink::mojom::QuotaStatusCode)> callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::DeleteBucket, this, storage_key,
                       bucket_name, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (!quota_manager_impl_) {
    DidGetStatus(std::move(callback_task_runner), std::move(callback),
                 blink::mojom::QuotaStatusCode::kUnknown);
    return;
  }

  quota_manager_impl_->FindAndDeleteBucketData(
      storage_key, bucket_name,
      base::BindOnce(&DidGetStatus, std::move(callback_task_runner),
                     std::move(callback)));
}

void QuotaManagerProxy::NotifyStorageAccessed(const StorageKey& storage_key,
                                              blink::mojom::StorageType type,
                                              base::Time access_time) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyStorageAccessed,
                                  this, storage_key, type, access_time));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->NotifyStorageAccessed(storage_key, type, access_time);
}

void QuotaManagerProxy::NotifyBucketAccessed(BucketId bucket_id,
                                             base::Time access_time) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyBucketAccessed,
                                  this, bucket_id, access_time));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->NotifyBucketAccessed(bucket_id, access_time);
}

void QuotaManagerProxy::NotifyStorageModified(
    QuotaClientType client_id,
    const StorageKey& storage_key,
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
                       client_id, storage_key, type, delta, modification_time,
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
    quota_manager_impl_->NotifyStorageModified(client_id, storage_key, type,
                                               delta, modification_time,
                                               std::move(manager_callback));
  }
}

void QuotaManagerProxy::NotifyBucketModified(
    QuotaClientType client_id,
    BucketId bucket_id,
    int64_t delta,
    base::Time modification_time,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  DCHECK(!callback || callback_task_runner);
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyBucketModified, this,
                       client_id, bucket_id, delta, modification_time,
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
    quota_manager_impl_->NotifyBucketModified(client_id, bucket_id, delta,
                                              modification_time,
                                              std::move(manager_callback));
  }
}

void QuotaManagerProxy::NotifyWriteFailed(const StorageKey& storage_key) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyWriteFailed, this,
                                  storage_key));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->NotifyWriteFailed(storage_key);
}

void QuotaManagerProxy::SetUsageCacheEnabled(QuotaClientType client_id,
                                             const StorageKey& storage_key,
                                             blink::mojom::StorageType type,
                                             bool enabled) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::SetUsageCacheEnabled,
                                  this, client_id, storage_key, type, enabled));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->SetUsageCacheEnabled(client_id, storage_key, type,
                                              enabled);
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
    const StorageKey& storage_key,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    UsageAndQuotaCallback callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetUsageAndQuota, this, storage_key,
                       type, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (!quota_manager_impl_) {
    DidGetUsageAndQuota(std::move(callback_task_runner), std::move(callback),
                        blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0);
    return;
  }

  quota_manager_impl_->GetUsageAndQuota(
      storage_key, type,
      base::BindOnce(&DidGetUsageAndQuota, std::move(callback_task_runner),
                     std::move(callback)));
}

void QuotaManagerProxy::IsStorageUnlimited(
    const StorageKey& storage_key,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(bool)> callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::IsStorageUnlimited, this,
                       storage_key, type, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  bool is_storage_unlimited =
      quota_manager_impl_
          ? quota_manager_impl_->IsStorageUnlimited(storage_key, type)
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

void QuotaManagerProxy::OverrideQuotaForStorageKey(
    int handle_id,
    const StorageKey& storage_key,
    absl::optional<int64_t> quota_size,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::OverrideQuotaForStorageKey, this,
                       handle_id, storage_key, quota_size,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_)
    quota_manager_impl_->OverrideQuotaForStorageKey(handle_id, storage_key,
                                                    quota_size);

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
