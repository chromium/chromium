// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_context_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/cache_storage/blob_storage_context_wrapper.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "url/origin.h"

namespace content {

CacheStorageContextImpl::CacheStorageContextImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CacheStorageContextImpl::~CacheStorageContextImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& origin : origins_to_purge_on_shutdown_) {
    cache_manager_->DeleteOriginData(
        origin, storage::mojom::CacheStorageOwner::kCacheAPI,

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
      {base::TaskPriority::USER_VISIBLE});
}

void CacheStorageContextImpl::Init(
    mojo::PendingReceiver<storage::mojom::CacheStorageControl> control,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(control));
  is_incognito_ = user_data_directory.empty();

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  DCHECK(!cache_manager_);
  cache_manager_ = LegacyCacheStorageManager::Create(
      user_data_directory, std::move(cache_task_runner),
      base::SequencedTaskRunnerHandle::Get(), quota_manager_proxy,
      base::MakeRefCounted<BlobStorageContextWrapper>(
          std::move(blob_storage_context)));

  if (!quota_manager_proxy)
    return;

  mojo::PendingRemote<storage::mojom::QuotaClient> cache_storage_client;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CacheStorageQuotaClient>(
          cache_manager_, storage::mojom::CacheStorageOwner::kCacheAPI),
      cache_storage_client.InitWithNewPipeAndPassReceiver());
  quota_manager_proxy->RegisterClient(
      std::move(cache_storage_client),
      storage::QuotaClientType::kServiceWorkerCache,
      {blink::mojom::StorageType::kTemporary});

  mojo::PendingRemote<storage::mojom::QuotaClient> background_fetch_client;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CacheStorageQuotaClient>(
          cache_manager_, storage::mojom::CacheStorageOwner::kBackgroundFetch),
      background_fetch_client.InitWithNewPipeAndPassReceiver());
  quota_manager_proxy->RegisterClient(
      std::move(background_fetch_client),
      storage::QuotaClientType::kBackgroundFetch,
      {blink::mojom::StorageType::kTemporary});
}

void CacheStorageContextImpl::AddReceiver(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    const url::Origin& origin,
    storage::mojom::CacheStorageOwner owner,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!dispatcher_host_)
    dispatcher_host_ = std::make_unique<CacheStorageDispatcherHost>(this);
  dispatcher_host_->AddReceiver(cross_origin_embedder_policy,
                                std::move(coep_reporter), origin, owner,
                                std::move(receiver));
}

void CacheStorageContextImpl::GetAllOriginsInfo(
    storage::mojom::CacheStorageControl::GetAllOriginsInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_manager_->GetAllOriginsUsage(
      storage::mojom::CacheStorageOwner::kCacheAPI, std::move(callback));
}

void CacheStorageContextImpl::DeleteForOrigin(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cache_manager_->DeleteOriginData(
      origin, storage::mojom::CacheStorageOwner::kCacheAPI);
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

}  // namespace content
