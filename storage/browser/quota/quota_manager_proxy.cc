// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_proxy.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "storage/browser/quota/storage_directory_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

using ::blink::StorageKey;

namespace storage {

// This object signals the given `WaitableEvent` when it goes out of scope.
class ScopedWaitableEvent {
 public:
  explicit ScopedWaitableEvent(base::WaitableEvent& event) : event_(&event) {}
  ScopedWaitableEvent(ScopedWaitableEvent&& other) {
    event_ = std::exchange(other.event_, nullptr);
  }

  ~ScopedWaitableEvent() {
    if (event_) {
      event_->Signal();
    }
  }

  ScopedWaitableEvent(const ScopedWaitableEvent& other) = delete;
  ScopedWaitableEvent& operator=(const ScopedWaitableEvent& other) = delete;

 private:
  raw_ptr<base::WaitableEvent> event_;
};

QuotaManagerProxy::QuotaManagerProxy(
    QuotaManagerImpl* quota_manager_impl,
    scoped_refptr<base::SequencedTaskRunner> quota_manager_impl_task_runner,
    const base::FilePath& profile_path)
    : quota_manager_impl_(quota_manager_impl),
      quota_manager_impl_task_runner_(
          std::move(quota_manager_impl_task_runner)),
      profile_path_(profile_path) {
  DCHECK(quota_manager_impl_task_runner_.get());

  DETACH_FROM_SEQUENCE(quota_manager_impl_sequence_checker_);
}

base::FilePath QuotaManagerProxy::GetBucketPath(const BucketLocator& bucket) {
  return CreateBucketPath(profile_path_, bucket);
}

base::FilePath QuotaManagerProxy::GetClientBucketPath(
    const BucketLocator& bucket,
    QuotaClientType client_type) {
  return CreateClientBucketPath(profile_path_, bucket, client_type);
}

void QuotaManagerProxy::RegisterClient(
    mojo::PendingRemote<mojom::QuotaClient> client,
    QuotaClientType client_type,
    const base::flat_set<blink::mojom::StorageType>& storage_types) {
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

void QuotaManagerProxy::BindInternalsHandler(
    mojo::PendingReceiver<mojom::QuotaInternalsHandler> receiver) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::BindInternalsHandler,
                                  this, std::move(receiver)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_) {
    quota_manager_impl_->BindInternalsHandler(std::move(receiver));
  }
}

void QuotaManagerProxy::UpdateOrCreateBucket(
    const BucketInitParams& bucket_params,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::UpdateOrCreateBucket, this,
                       bucket_params, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(base::unexpected(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->UpdateOrCreateBucket(bucket_params, std::move(respond));
}

QuotaErrorOr<BucketInfo> QuotaManagerProxy::GetOrCreateBucketSync(
    const BucketInitParams& params) {
  // Ensure that the task runner we want is free and can be blocked on.
  DCHECK(!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence());
  QuotaErrorOr<BucketInfo> bucket = base::unexpected(QuotaError::kUnknownError);
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  // Asynchronously call UpdateOrCreateBucket and block until it completes.
  quota_manager_impl_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const scoped_refptr<QuotaManagerProxy>& self,
             const BucketInitParams& params, ScopedWaitableEvent waiter,
             QuotaErrorOr<BucketInfo>* sync_bucket) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(
                self->quota_manager_impl_sequence_checker_);
            // If the database is still bootstrapping, return rather than
            // risking deadlock.
            if (!self->quota_manager_impl_ ||
                self->quota_manager_impl_->is_bootstrapping_database_) {
              return;
            }
            // Otherwise, return the bucket value and resolve the waiter.
            self->quota_manager_impl_->UpdateOrCreateBucket(
                params, base::BindOnce(
                            [](ScopedWaitableEvent waiter,
                               QuotaErrorOr<BucketInfo>* sync_bucket,
                               QuotaErrorOr<BucketInfo> result_bucket) {
                              *sync_bucket = std::move(result_bucket);
                            },
                            std::move(waiter), sync_bucket));
          },
          base::WrapRefCounted(this), params, ScopedWaitableEvent(waiter),
          &bucket));
  waiter.Wait();
  return bucket;
}

void QuotaManagerProxy::GetOrCreateBucketDeprecated(
    const BucketInitParams& params,
    blink::mojom::StorageType storage_type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetOrCreateBucketDeprecated, this,
                       params, storage_type, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(base::unexpected(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->GetOrCreateBucketDeprecated(params, storage_type,
                                                   std::move(respond));
}

void QuotaManagerProxy::CreateBucketForTesting(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::CreateBucketForTesting, this,
                       storage_key, bucket_name, storage_type,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(base::unexpected(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->CreateBucketForTesting(  // IN-TEST
      storage_key, bucket_name, storage_type, std::move(respond));
}

void QuotaManagerProxy::GetBucketByNameUnsafe(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetBucketByNameUnsafe, this,
                       storage_key, bucket_name, type,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(base::unexpected(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->GetBucketByNameUnsafe(  // IN-TEST
      storage_key, bucket_name, type, std::move(respond));
}

void QuotaManagerProxy::GetBucketsForStorageKey(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    bool delete_expired,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetBucketsForStorageKey, this,
                       storage_key, type, delete_expired,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(base::unexpected(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->GetBucketsForStorageKey(
      storage_key, type, std::move(respond), delete_expired);
}

void QuotaManagerProxy::GetBucketById(
    const BucketId& bucket_id,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetBucketById, this, bucket_id,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(base::unexpected(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->GetBucketById(bucket_id, std::move(respond));
}

void QuotaManagerProxy::DeleteBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(blink::mojom::QuotaStatusCode)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::DeleteBucket, this, storage_key,
                       bucket_name, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(blink::mojom::QuotaStatusCode::kUnknown);
    return;
  }

  quota_manager_impl_->FindAndDeleteBucketData(storage_key, bucket_name,
                                               std::move(respond));
}

void QuotaManagerProxy::UpdateBucketExpiration(
    BucketId bucket,
    const base::Time& expiration,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::UpdateBucketExpiration, this, bucket,
                       expiration, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(base::unexpected(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->UpdateBucketExpiration(bucket, expiration,
                                              std::move(respond));
}

void QuotaManagerProxy::UpdateBucketPersistence(
    BucketId bucket,
    bool persistent,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::UpdateBucketPersistence, this,
                       bucket, persistent, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(base::unexpected(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->UpdateBucketPersistence(bucket, persistent,
                                               std::move(respond));
}

void QuotaManagerProxy::NotifyBucketAccessed(const BucketLocator& bucket,
                                             base::Time access_time) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::NotifyBucketAccessed,
                                  this, bucket, access_time));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_) {
    quota_manager_impl_->NotifyBucketAccessed(bucket, access_time);
  }
}

void QuotaManagerProxy::NotifyBucketModified(
    QuotaClientType client_id,
    const BucketLocator& bucket,
    std::optional<int64_t> delta,
    base::Time modification_time,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::NotifyBucketModified, this,
                       client_id, bucket, delta, modification_time,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  auto manager_callback =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));

  if (!quota_manager_impl_) {
    std::move(manager_callback).Run();
    return;
  }

  quota_manager_impl_->NotifyBucketModified(
      client_id, bucket, delta, modification_time, std::move(manager_callback));
}

void QuotaManagerProxy::OnClientWriteFailed(const StorageKey& storage_key) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::OnClientWriteFailed, this,
                                  storage_key));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_) {
    quota_manager_impl_->OnClientWriteFailed(storage_key);
  }
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
  if (quota_manager_impl_) {
    quota_manager_impl_->SetUsageCacheEnabled(client_id, storage_key, type,
                                              enabled);
  }
}

void QuotaManagerProxy::GetUsageAndQuota(
    const StorageKey& storage_key,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    UsageAndQuotaCallback callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetUsageAndQuota, this, storage_key,
                       type, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0);
    return;
  }

  quota_manager_impl_->GetUsageAndQuota(storage_key, type, std::move(respond));
}

void QuotaManagerProxy::GetBucketUsageAndQuota(
    BucketId bucket,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    UsageAndQuotaCallback callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetBucketUsageAndQuota, this, bucket,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0);
    return;
  }

  quota_manager_impl_->GetBucketUsageAndQuota(bucket, std::move(respond));
}

void QuotaManagerProxy::GetBucketSpaceRemaining(
    const BucketLocator& bucket,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(QuotaErrorOr<int64_t>)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::GetBucketSpaceRemaining,
                                  this, bucket, std::move(callback_task_runner),
                                  std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));

  if (!quota_manager_impl_) {
    std::move(respond).Run(base::unexpected(QuotaError::kUnknownError));
    return;
  }

  quota_manager_impl_->GetBucketSpaceRemaining(bucket, std::move(respond));
}

void QuotaManagerProxy::IsStorageUnlimited(
    const StorageKey& storage_key,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

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

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  std::move(respond).Run(is_storage_unlimited);
}

void QuotaManagerProxy::GetStorageKeyUsageWithBreakdown(
    const blink::StorageKey& storage_key,
    blink::mojom::StorageType type,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    UsageWithBreakdownCallback callback) {
  CHECK(callback_task_runner);
  CHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::GetStorageKeyUsageWithBreakdown,
                       this, storage_key, type, std::move(callback_task_runner),
                       std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  if (!quota_manager_impl_) {
    std::move(respond).Run(0, nullptr);
    return;
  }

  quota_manager_impl_->GetStorageKeyUsageWithBreakdown(storage_key, type,
                                                       std::move(respond));
}

std::unique_ptr<QuotaOverrideHandle>
QuotaManagerProxy::GetQuotaOverrideHandle() {
  return std::make_unique<QuotaOverrideHandle>(this);
}

void QuotaManagerProxy::OverrideQuotaForStorageKey(
    int handle_id,
    const StorageKey& storage_key,
    std::optional<int64_t> quota_size,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  DCHECK(callback_task_runner);
  DCHECK(callback);

  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuotaManagerProxy::OverrideQuotaForStorageKey, this,
                       handle_id, storage_key, quota_size,
                       std::move(callback_task_runner), std::move(callback)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_) {
    quota_manager_impl_->OverrideQuotaForStorageKey(handle_id, storage_key,
                                                    quota_size);
  }

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  std::move(respond).Run();
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
  if (quota_manager_impl_) {
    quota_manager_impl_->WithdrawOverridesForHandle(handle_id);
  }
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

  auto respond =
      base::BindPostTask(std::move(callback_task_runner), std::move(callback));
  std::move(respond).Run(handle_id);
}

QuotaManagerProxy::~QuotaManagerProxy() = default;

void QuotaManagerProxy::InvalidateQuotaManagerImpl(
    base::PassKey<QuotaManagerImpl>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  DCHECK(quota_manager_impl_) << __func__ << " called multiple times";
  quota_manager_impl_ = nullptr;
}

void QuotaManagerProxy::AddObserver(
    mojo::PendingRemote<storage::mojom::QuotaManagerObserver> observer) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::AddObserver, this,
                                  std::move(observer)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (!quota_manager_impl_) {
    return;
  }

  quota_manager_impl_->AddObserver(std::move(observer));
}

}  // namespace storage
