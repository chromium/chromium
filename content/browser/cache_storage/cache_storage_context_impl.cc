// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_context_impl.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/cache_storage/blob_storage_context_wrapper.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

BASE_FEATURE(kCacheStorageTaskPriority,
             "CacheStorageTaskPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

CacheStorageContextImpl::CacheStorageContextImpl(
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : quota_manager_proxy_(std::move(quota_manager_proxy)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CacheStorageContextImpl::~CacheStorageContextImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!origins_to_purge_on_shutdown_.empty()) {
    cache_manager_->DeleteOriginData(
        origins_to_purge_on_shutdown_,
        storage::mojom::CacheStorageOwner::kCacheAPI,

        // Retain a reference to the manager until the deletion is
        // complete, since it internally uses weak pointers for
        // the various stages of deletion and nothing else will
        // keep it alive during shutdown.
        base::BindOnce([](scoped_refptr<CacheStorageManager> cache_manager,
                          blink::mojom::QuotaStatusCode) {},
                       cache_manager_));
  }
}

// static
scoped_refptr<base::SequencedTaskRunner>
CacheStorageContextImpl::CreateSchedulerTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::FeatureList::IsEnabled(kCacheStorageTaskPriority)
           ? base::TaskPriority::USER_BLOCKING
           : base::TaskPriority::USER_VISIBLE});
}

void CacheStorageContextImpl::Init(
    mojo::PendingReceiver<storage::mojom::CacheStorageControl> control,
    const base::FilePath& user_data_directory,
    mojo::PendingReceiver<storage::mojom::QuotaClient>
        cache_storage_client_remote,
    mojo::PendingReceiver<storage::mojom::QuotaClient>
        background_fetch_client_remote,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(control));
  is_incognito_ = user_data_directory.empty();

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           base::FeatureList::IsEnabled(kCacheStorageTaskPriority)
               ? base::TaskPriority::USER_BLOCKING
               : base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  DCHECK(!dispatcher_host_);
  dispatcher_host_ =
      std::make_unique<CacheStorageDispatcherHost>(this, quota_manager_proxy_);

  DCHECK(!cache_manager_);
  cache_manager_ = CacheStorageManager::Create(
      user_data_directory, std::move(cache_task_runner),
      base::SequencedTaskRunner::GetCurrentDefault(), quota_manager_proxy_,
      base::MakeRefCounted<BlobStorageContextWrapper>(
          std::move(blob_storage_context)),
      dispatcher_host_->AsWeakPtr());

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CacheStorageQuotaClient>(
          cache_manager_, storage::mojom::CacheStorageOwner::kCacheAPI),
      std::move(cache_storage_client_remote));
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CacheStorageQuotaClient>(
          cache_manager_, storage::mojom::CacheStorageOwner::kBackgroundFetch),
      std::move(background_fetch_client_remote));
}

void CacheStorageContextImpl::AddReceiver(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    const network::DocumentIsolationPolicy& document_isolation_policy,
    const storage::BucketLocator& bucket_locator,
    storage::mojom::CacheStorageOwner owner,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto add_receiver =
      base::BindOnce(&CacheStorageContextImpl::AddReceiverWithBucketInfo,
                     weak_factory_.GetWeakPtr(), cross_origin_embedder_policy,
                     std::move(coep_reporter), document_isolation_policy,
                     bucket_locator.storage_key, owner, std::move(receiver));

  if (bucket_locator.is_default) {
    DCHECK_EQ(storage::BucketId(), bucket_locator.id);
    quota_manager_proxy_->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(bucket_locator.storage_key),
        base::SequencedTaskRunner::GetCurrentDefault(),
        std::move(add_receiver));
  } else if (!bucket_locator.is_null()) {
    quota_manager_proxy_->GetBucketById(
        bucket_locator.id, base::SequencedTaskRunner::GetCurrentDefault(),
        std::move(add_receiver));
  } else {
    std::move(add_receiver)
        .Run(base::unexpected(storage::QuotaError::kNotFound));
  }
}

void CacheStorageContextImpl::AddObserver(
    mojo::PendingRemote<storage::mojom::CacheStorageObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_manager_->AddObserver(std::move(observer));
}

void CacheStorageContextImpl::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& update : policy_updates) {
    if (!update->purge_on_shutdown)
      origins_to_purge_on_shutdown_.erase(update->origin);
    else
      origins_to_purge_on_shutdown_.insert(std::move(update->origin));
  }
}

void CacheStorageContextImpl::AddReceiverWithBucketInfo(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    const network::DocumentIsolationPolicy& document_isolation_policy,
    const blink::StorageKey& storage_key,
    storage::mojom::CacheStorageOwner owner,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::optional<storage::BucketLocator> bucket =
      result.has_value() ? std::make_optional(result->ToBucketLocator())
                         : std::nullopt;

  dispatcher_host_->AddReceiver(cross_origin_embedder_policy,
                                std::move(coep_reporter),
                                document_isolation_policy, storage_key, bucket,
                                owner, std::move(receiver));
}

}  // namespace content
