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
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "storage/browser/quota/storage_directory_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using ::blink::StorageKey;

namespace storage {

namespace {

// Wraps `callback` such that it:
// 1. when run, is posted to the sequence on which `PostWithFallback` is
// invoked.
// 2. has a fallback value which is used if the callback is destroyed without
// being invoked.
template <typename... Args, typename... DefaultArgs>
base::OnceCallback<void(Args...)> PostWithFallback(
    base::OnceCallback<void(Args...)> callback,
    DefaultArgs&&... default_args) {
  return mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindPostTaskToCurrentDefault(std::move(callback)),
      std::forward<DefaultArgs>(default_args)...);
}

}  // namespace

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
  CHECK(quota_manager_impl_task_runner_.get());

  DETACH_FROM_SEQUENCE(quota_manager_impl_sequence_checker_);
}

void QuotaManagerProxy::PostTaskWithQuotaManager(
    base::OnceCallback<void(base::WeakPtr<QuotaManagerImpl>)> callback) {
  quota_manager_impl_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<QuotaManagerProxy> proxy,
             base::OnceCallback<void(base::WeakPtr<QuotaManagerImpl>)>
                 callback) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(
                proxy->quota_manager_impl_sequence_checker_);
            base::WeakPtr<QuotaManagerImpl> quota_manager;
            if (proxy->quota_manager_impl_) {
              quota_manager =
                  proxy->quota_manager_impl_->weak_factory_.GetWeakPtr();
            }
            std::move(callback).Run(std::move(quota_manager));
          },
          base::WrapRefCounted(this), std::move(callback)));
}

void QuotaManagerProxy::MaybeRunTask(
    base::OnceCallback<void(QuotaManagerImpl&)> task) {
  if (!quota_manager_impl_task_runner_->RunsTasksInCurrentSequence()) {
    quota_manager_impl_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaManagerProxy::MaybeRunTask, this,
                                  std::move(task)));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);
  if (quota_manager_impl_) {
    std::move(task).Run(*quota_manager_impl_);
  }
}

base::FilePath QuotaManagerProxy::GetClientBucketPath(
    const BucketLocator& bucket,
    QuotaClientType client_type) {
  return CreateClientBucketPath(profile_path_, bucket, client_type);
}

void QuotaManagerProxy::RegisterClient(
    mojo::PendingRemote<mojom::QuotaClient> client,
    QuotaClientType client_type) {
  MaybeRunTask(base::BindOnce(
      [](mojo::PendingRemote<mojom::QuotaClient> client,
         QuotaClientType client_type, QuotaManagerImpl& manager) {
        manager.RegisterClient(std::move(client), client_type);
      },
      std::move(client), client_type));
}

void QuotaManagerProxy::BindInternalsHandler(
    mojo::PendingReceiver<mojom::QuotaInternalsHandler> receiver) {
  MaybeRunTask(base::BindOnce(
      [](mojo::PendingReceiver<mojom::QuotaInternalsHandler> receiver,
         QuotaManagerImpl& manager) {
        manager.BindInternalsHandler(std::move(receiver));
      },
      std::move(receiver)));
}

void QuotaManagerProxy::UpdateOrCreateBucket(
    const BucketInitParams& bucket_params,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  MaybeRunTask(base::BindOnce(
      [](const BucketInitParams& bucket_params,
         base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.UpdateOrCreateBucket(bucket_params, std::move(callback));
      },
      bucket_params,
      PostWithFallback(std::move(callback),
                       base::unexpected(QuotaError::kUnknownError))));
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

void QuotaManagerProxy::CreateBucketForTesting(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  MaybeRunTask(base::BindOnce(
      [](const StorageKey& storage_key, const std::string& bucket_name,
         base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.CreateBucketForTesting(  // IN-TEST
            storage_key, bucket_name, std::move(callback));
      },
      storage_key, bucket_name,
      PostWithFallback(std::move(callback),
                       base::unexpected(QuotaError::kUnknownError))));
}

void QuotaManagerProxy::GetBucketByNameUnsafe(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  MaybeRunTask(base::BindOnce(
      [](const StorageKey& storage_key, const std::string& bucket_name,
         base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.GetBucketByNameUnsafe(  // IN-TEST
            storage_key, bucket_name, std::move(callback));
      },
      storage_key, bucket_name,
      PostWithFallback(std::move(callback),
                       base::unexpected(QuotaError::kUnknownError))));
}

void QuotaManagerProxy::GetBucketsForStorageKey(
    const blink::StorageKey& storage_key,
    bool delete_expired,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback) {
  MaybeRunTask(base::BindOnce(
      [](const blink::StorageKey& storage_key, bool delete_expired,
         base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.GetBucketsForStorageKey(storage_key, std::move(callback),
                                              delete_expired);
      },
      storage_key, delete_expired,
      PostWithFallback(std::move(callback),
                       base::unexpected(QuotaError::kUnknownError))));
}

void QuotaManagerProxy::GetBucketsModifiedBetween(base::Time begin,
                                                  base::Time end,
                                                  GetBucketsCallback callback) {
  MaybeRunTask(base::BindOnce(
      [](base::Time begin, base::Time end, GetBucketsCallback callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.GetBucketsModifiedBetween(begin, end,
                                                std::move(callback));
      },
      begin, end,
      PostWithFallback(std::move(callback), std::set<BucketLocator>())));
}

void QuotaManagerProxy::DeleteStorageKeyData(const StorageKey& storage_key,
                                             StatusCallback callback) {
  MaybeRunTask(base::BindOnce(
      [](const StorageKey& storage_key, StatusCallback callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.DeleteStorageKeyData(storage_key, std::move(callback));
      },
      storage_key,
      PostWithFallback(std::move(callback),
                       blink::mojom::QuotaStatusCode::kErrorAbort)));
}

void QuotaManagerProxy::GetBucketById(
    const BucketId& bucket_id,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  MaybeRunTask(base::BindOnce(
      [](const BucketId& bucket_id,
         base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.GetBucketById(bucket_id, std::move(callback));
      },
      bucket_id,
      PostWithFallback(std::move(callback),
                       base::unexpected(QuotaError::kUnknownError))));
}

void QuotaManagerProxy::DeleteBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    base::OnceCallback<void(blink::mojom::QuotaStatusCode)> callback) {
  MaybeRunTask(base::BindOnce(
      [](const StorageKey& storage_key, const std::string& bucket_name,
         base::OnceCallback<void(blink::mojom::QuotaStatusCode)> callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.FindAndDeleteBucketData(storage_key, bucket_name,
                                              std::move(callback));
      },
      storage_key, bucket_name,
      PostWithFallback(std::move(callback),
                       blink::mojom::QuotaStatusCode::kUnknown)));
}

void QuotaManagerProxy::UpdateBucketExpiration(
    BucketId bucket,
    const base::Time& expiration,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  MaybeRunTask(base::BindOnce(
      [](BucketId bucket, const base::Time& expiration,
         base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.UpdateBucketExpiration(bucket, expiration,
                                             std::move(callback));
      },
      bucket, expiration,
      PostWithFallback(std::move(callback),
                       base::unexpected(QuotaError::kUnknownError))));
}

void QuotaManagerProxy::UpdateBucketPersistence(
    BucketId bucket,
    bool persistent,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  MaybeRunTask(base::BindOnce(
      [](BucketId bucket, bool persistent,
         base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.UpdateBucketPersistence(bucket, persistent,
                                              std::move(callback));
      },
      bucket, persistent,
      PostWithFallback(std::move(callback),
                       base::unexpected(QuotaError::kUnknownError))));
}

void QuotaManagerProxy::NotifyBucketAccessed(const BucketLocator& bucket,
                                             base::Time access_time) {
  MaybeRunTask(base::BindOnce(
      [](const BucketLocator& bucket, base::Time access_time,
         QuotaManagerImpl& manager) {
        manager.NotifyBucketAccessed(bucket, access_time);
      },
      bucket, access_time));
}

void QuotaManagerProxy::NotifyBucketModified(QuotaClientType client_id,
                                             const BucketLocator& bucket,
                                             std::optional<int64_t> delta,
                                             base::Time modification_time,
                                             base::OnceClosure callback) {
  MaybeRunTask(base::BindOnce(
      [](QuotaClientType client_id, const BucketLocator& bucket,
         std::optional<int64_t> delta, base::Time modification_time,
         base::OnceClosure callback, QuotaManagerImpl& quota_manager) {
        quota_manager.NotifyBucketModified(
            client_id, bucket, delta, modification_time, std::move(callback));
      },
      client_id, bucket, delta, modification_time,
      PostWithFallback(std::move(callback))));
}

void QuotaManagerProxy::OnClientWriteFailed(const StorageKey& storage_key) {
  MaybeRunTask(base::BindOnce(
      [](const StorageKey& storage_key, QuotaManagerImpl& manager) {
        manager.OnClientWriteFailed(storage_key);
      },
      storage_key));
}

void QuotaManagerProxy::SetUsageCacheEnabled(QuotaClientType client_id,
                                             const StorageKey& storage_key,
                                             bool enabled) {
  MaybeRunTask(base::BindOnce(
      [](QuotaClientType client_id, const StorageKey& storage_key, bool enabled,
         QuotaManagerImpl& manager) {
        manager.SetUsageCacheEnabled(client_id, storage_key, enabled);
      },
      client_id, storage_key, enabled));
}

void QuotaManagerProxy::GetUsageAndQuota(const StorageKey& storage_key,
                                         UsageAndQuotaCallback callback) {
  MaybeRunTask(base::BindOnce(
      [](const StorageKey& storage_key, UsageAndQuotaCallback callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.GetUsageAndQuota(storage_key, std::move(callback));
      },
      storage_key,
      PostWithFallback(std::move(callback),
                       blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0)));
}

void QuotaManagerProxy::GetUsageAndQuotaForWebApps(
    const StorageKey& storage_key,
    UsageAndQuotaCallback callback) {
  MaybeRunTask(base::BindOnce(
      [](const StorageKey& storage_key, UsageAndQuotaCallback callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.GetUsageAndQuotaForWebApps(storage_key,
                                                 std::move(callback));
      },
      storage_key,
      PostWithFallback(std::move(callback),
                       blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0)));
}

void QuotaManagerProxy::GetUsageAndQuotaForDevtools(
    const StorageKey& storage_key,
    UsageAndQuotaWithBreakdownAndOverrideFlagCallback callback) {
  MaybeRunTask(base::BindOnce(
      [](const StorageKey& storage_key,
         UsageAndQuotaWithBreakdownAndOverrideFlagCallback callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.GetUsageAndQuotaForDevtools(storage_key,
                                                  std::move(callback));
      },
      storage_key,
      PostWithFallback(std::move(callback),
                       blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0, false,
                       nullptr)));
}

void QuotaManagerProxy::GetBucketUsageAndReportedQuota(
    BucketId bucket,
    UsageAndQuotaCallback callback) {
  MaybeRunTask(base::BindOnce(
      [](BucketId bucket, UsageAndQuotaCallback callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.GetBucketUsageAndReportedQuota(bucket,
                                                     std::move(callback));
      },
      bucket,
      PostWithFallback(std::move(callback),
                       blink::mojom::QuotaStatusCode::kErrorAbort, 0, 0)));
}

void QuotaManagerProxy::GetBucketSpaceRemaining(
    const BucketLocator& bucket,
    base::OnceCallback<void(QuotaErrorOr<int64_t>)> callback) {
  MaybeRunTask(base::BindOnce(
      [](const BucketLocator& bucket,
         base::OnceCallback<void(QuotaErrorOr<int64_t>)> callback,
         QuotaManagerImpl& quota_manager) {
        quota_manager.GetBucketSpaceRemaining(bucket, std::move(callback));
      },
      bucket,
      PostWithFallback(std::move(callback),
                       base::unexpected(QuotaError::kUnknownError))));
}

void QuotaManagerProxy::GetStorageKeyUsageWithBreakdown(
    const blink::StorageKey& storage_key,
    UsageWithBreakdownCallback callback) {
  MaybeRunTask(base::BindOnce(
      [](const blink::StorageKey& storage_key,
         UsageWithBreakdownCallback callback, QuotaManagerImpl& quota_manager) {
        quota_manager.GetStorageKeyUsageWithBreakdown(storage_key,
                                                      std::move(callback));
      },
      storage_key, PostWithFallback(std::move(callback), 0, nullptr)));
}

std::unique_ptr<QuotaOverrideHandle>
QuotaManagerProxy::GetQuotaOverrideHandle() {
  return std::make_unique<QuotaOverrideHandle>(this);
}

void QuotaManagerProxy::OverrideQuotaForStorageKey(
    int handle_id,
    const StorageKey& storage_key,
    std::optional<int64_t> quota_size,
    base::OnceClosure callback) {
  MaybeRunTask(base::BindOnce(
                   [](int handle_id, const StorageKey& storage_key,
                      std::optional<int64_t> quota_size,
                      QuotaManagerImpl& quota_manager) {
                     quota_manager.OverrideQuotaForStorageKey(
                         handle_id, storage_key, quota_size);
                   },
                   handle_id, storage_key, quota_size)
                   .Then(PostWithFallback(std::move(callback))));
}

void QuotaManagerProxy::WithdrawOverridesForHandle(int handle_id) {
  MaybeRunTask(base::BindOnce(
      [](int handle_id, QuotaManagerImpl& manager) {
        manager.WithdrawOverridesForHandle(handle_id);
      },
      handle_id));
}

void QuotaManagerProxy::GetOverrideHandleId(
    base::OnceCallback<void(int)> callback) {
  MaybeRunTask(base::BindOnce([](QuotaManagerImpl& quota_manager) {
                 return quota_manager.GetOverrideHandleId();
               }).Then(PostWithFallback(std::move(callback), 0)));
}

QuotaManagerProxy::~QuotaManagerProxy() = default;

void QuotaManagerProxy::InvalidateQuotaManagerImpl(
    base::PassKey<QuotaManagerImpl>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_manager_impl_sequence_checker_);

  CHECK(quota_manager_impl_) << __func__ << " called multiple times";
  quota_manager_impl_ = nullptr;
}

void QuotaManagerProxy::AddObserver(
    mojo::PendingRemote<storage::mojom::QuotaManagerObserver> observer) {
  MaybeRunTask(base::BindOnce(
      [](mojo::PendingRemote<storage::mojom::QuotaManagerObserver> observer,
         QuotaManagerImpl& manager) {
        manager.AddObserver(std::move(observer));
      },
      std::move(observer)));
}

}  // namespace storage
