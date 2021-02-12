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

namespace {

scoped_refptr<base::SequencedTaskRunner> CreateSchedulerTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE});
}

}  // namespace

CacheStorageContextImpl::CacheStorageContextImpl()
    : CacheStorageContext(GetUIThreadTaskRunner({})),
      task_runner_(CreateSchedulerTaskRunner()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CacheStorageContextImpl::~CacheStorageContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_runner_->ReleaseSoon(FROM_HERE, std::move(cache_manager_));
}

void CacheStorageContextImpl::Init(
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  is_incognito_ = user_data_directory.empty();

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CacheStorageContextImpl::CreateCacheStorageManagerOnTaskRunner, this,
          user_data_directory, std::move(cache_task_runner),
          quota_manager_proxy, std::move(blob_storage_context)));
}

void CacheStorageContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Break reference cycle with |this|.
  if (dispatcher_host_)
    dispatcher_host_.AsyncCall(&CacheStorageDispatcherHost::Shutdown);

  receivers_.Clear();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CacheStorageContextImpl::ShutdownOnTaskRunner, this,
                     std::move(origins_to_purge_on_shutdown_)));
}

void CacheStorageContextImpl::Bind(
    mojo::PendingReceiver<storage::mojom::CacheStorageControl> control) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  receivers_.Add(this, std::move(control));
}

void CacheStorageContextImpl::AddReceiver(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    const url::Origin& origin,
    storage::mojom::CacheStorageOwner owner,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!dispatcher_host_) {
    dispatcher_host_ =
        base::SequenceBound<CacheStorageDispatcherHost>(task_runner_);
    dispatcher_host_.AsyncCall(&CacheStorageDispatcherHost::Init)
        .WithArgs(base::RetainedRef(this));
  }
  dispatcher_host_.AsyncCall(&CacheStorageDispatcherHost::AddReceiver)
      .WithArgs(cross_origin_embedder_policy, std::move(coep_reporter), origin,
                owner, std::move(receiver));
}

void CacheStorageContextImpl::GetAllOriginsInfo(
    storage::mojom::CacheStorageControl::GetAllOriginsInfoCallback callback) {
  callback = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
         storage::mojom::CacheStorageControl::GetAllOriginsInfoCallback
             callback,
         std::vector<storage::mojom::StorageUsageInfoPtr> usage_info) {
        reply_task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), std::move(usage_info)));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<CacheStorageContextImpl> context,
             storage::mojom::CacheStorageControl::GetAllOriginsInfoCallback
                 callback) {
            scoped_refptr<CacheStorageManager> manager =
                context->cache_manager();
            if (!manager) {
              std::move(callback).Run(
                  std::vector<storage::mojom::StorageUsageInfoPtr>());
              return;
            }
            manager->GetAllOriginsUsage(
                storage::mojom::CacheStorageOwner::kCacheAPI,
                std::move(callback));
          },
          base::RetainedRef(this), std::move(callback)));
}

void CacheStorageContextImpl::DeleteForOrigin(const url::Origin& origin) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](scoped_refptr<CacheStorageContextImpl> context,
                        const url::Origin& origin) {
                       scoped_refptr<CacheStorageManager> manager =
                           context->cache_manager();
                       if (!manager)
                         return;
                       manager->DeleteOriginData(
                           origin,
                           storage::mojom::CacheStorageOwner::kCacheAPI);
                     },
                     base::RetainedRef(this), origin));
}

void CacheStorageContextImpl::AddObserver(
    mojo::PendingRemote<storage::mojom::CacheStorageObserver> observer) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<CacheStorageContextImpl> context,
             mojo::PendingRemote<storage::mojom::CacheStorageObserver>
                 observer) {
            auto manager = context->cache_manager();
            if (!manager)
              return;
            manager->AddObserver(std::move(observer));
          },
          base::RetainedRef(this), std::move(observer)));
}

void CacheStorageContextImpl::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  for (const auto& update : policy_updates) {
    if (!update->purge_on_shutdown)
      origins_to_purge_on_shutdown_.erase(update->origin);
    else
      origins_to_purge_on_shutdown_.insert(std::move(update->origin));
  }
}

void CacheStorageContextImpl::CreateCacheStorageManagerOnTaskRunner(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  DCHECK(!cache_manager_);
  cache_manager_ = LegacyCacheStorageManager::Create(
      user_data_directory, std::move(cache_task_runner), task_runner_,
      quota_manager_proxy,
      base::MakeRefCounted<BlobStorageContextWrapper>(
          std::move(blob_storage_context)));

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

void CacheStorageContextImpl::ShutdownOnTaskRunner(
    std::set<url::Origin> origins_to_purge_on_shutdown) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  for (const auto& origin : origins_to_purge_on_shutdown) {
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

  // Release |cache_manager_|. New clients will get a nullptr if they request
  // an instance of CacheStorageManager after this. Any other client that
  // ref-wrapped |cache_manager_| will be able to continue using it, and the
  // CacheStorageManager will be destroyed when all the references are
  // destroyed.
  cache_manager_ = nullptr;
}

}  // namespace content
