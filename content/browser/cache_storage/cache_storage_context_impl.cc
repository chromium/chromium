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
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/cache_storage/cross_sequence/cross_sequence_cache_storage_manager.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/origin.h"

namespace content {

namespace {

scoped_refptr<base::SequencedTaskRunner> CreateSchedulerTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE});
}

}  // namespace

CacheStorageContextImpl::CacheStorageContextImpl()
    : task_runner_(CreateSchedulerTaskRunner()),
      observers_(base::MakeRefCounted<ObserverList>()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CacheStorageContextImpl::~CacheStorageContextImpl() {
  // Can be destroyed on any thread.
  task_runner_->ReleaseSoon(FROM_HERE, std::move(cache_manager_));
}

void CacheStorageContextImpl::Init(
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  is_incognito_ = user_data_directory.empty();
  special_storage_policy_ = std::move(special_storage_policy);

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CacheStorageContextImpl::CreateCacheStorageManagerOnTaskRunner, this,
          user_data_directory, std::move(cache_task_runner),
          quota_manager_proxy));

  // If our target sequence is the IO thread, then the manager is guaranteed to
  // be created before this task fires to create the quota clients.  If we are
  // running with a different target sequence then the quota client code will
  // get a cross-sequence wrapper that is guaranteed to initialize its internal
  // SequenceBound<> object after the real manager is created.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CacheStorageContextImpl::CreateQuotaClientsOnIOThread,
                     base::WrapRefCounted(this),
                     std::move(quota_manager_proxy)));
}

void CacheStorageContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::AutoLock lock(shutdown_lock_);

  // Set an atomic flag indicating shutdown has been entered.  This allows us to
  // avoid creating CrossSequenceCacheStorageManager objects when there will
  // no longer be an underlying manager.
  DCHECK(!shutdown_);
  shutdown_ = true;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CacheStorageContextImpl::ShutdownOnTaskRunner, this));
}

void CacheStorageContextImpl::AddReceiver(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!dispatcher_host_) {
    dispatcher_host_ =
        base::SequenceBound<CacheStorageDispatcherHost>(task_runner_);
    dispatcher_host_.Post(FROM_HERE, &CacheStorageDispatcherHost::Init,
                          base::RetainedRef(this));
  }
  dispatcher_host_.Post(FROM_HERE, &CacheStorageDispatcherHost::AddReceiver,
                        cross_origin_embedder_policy, std::move(coep_reporter),
                        origin, std::move(receiver));
}

scoped_refptr<CacheStorageManager> CacheStorageContextImpl::CacheManager() {
  // If we're already on the target sequence, then just return the real manager.
  //
  // Note, we can't check for nullptr cache_manager_ here because it is not
  // threadsafe.  In addition we may be creating a cross-sequence manager
  // wrapper while the task to set cache_manager_ is waiting to run.  This
  // should be fine since the cross-sequence wrapper will initialize after the
  // manager is set.  See the comment in Init().
  if (task_runner_->RunsTasksInCurrentSequence())
    return cache_manager_;
  // Always return nullptr once shutdown has begun if we are on a different
  // sequence.  This check is necessary to avoid creating
  // CrossSequenceCacheStorageManager wrappers when there will no longer be an
  // underlying manager.
  base::AutoLock lock(shutdown_lock_);
  if (shutdown_)
    return nullptr;
  // Otherwise we have to create a cross-sequence wrapper to provide safe
  // access.
  return base::MakeRefCounted<CrossSequenceCacheStorageManager>(task_runner_,
                                                                this);
}

void CacheStorageContextImpl::SetBlobParametersForCache(
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!blob_storage_context)
    return;

  // TODO(enne): this remote will need to be sent to the storage service when
  // cache storage is moved.
  mojo::PendingRemote<storage::mojom::BlobStorageContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CacheStorageContextImpl::SetBlobParametersForCacheOnTaskRunner, this,
          std::move(remote)));

  // We can only bind a mojo interface for BlobStorageContext on the IO thread.
  // TODO(enne): clean this up in the future to not require this bounce and
  // to have this mojo context live on the cache storage sequence.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CacheStorageContextImpl::BindBlobStorageMojoContextOnIOThread, this,
          base::RetainedRef(blob_storage_context), std::move(receiver)));
}

void CacheStorageContextImpl::GetAllOriginsInfo(
    CacheStorageContext::GetUsageInfoCallback callback) {
  // Can be called on any sequence.

  callback = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> reply_task_runner,
         GetUsageInfoCallback inner,
         const std::vector<StorageUsageInfo>& entries) {
        reply_task_runner->PostTask(FROM_HERE,
                                    base::BindOnce(std::move(inner), entries));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<CacheStorageContextImpl> context,
             GetUsageInfoCallback callback) {
            scoped_refptr<CacheStorageManager> manager =
                context->CacheManager();
            if (!manager) {
              std::move(callback).Run(std::vector<StorageUsageInfo>());
              return;
            }
            manager->GetAllOriginsUsage(CacheStorageOwner::kCacheAPI,
                                        std::move(callback));
          },
          base::RetainedRef(this), std::move(callback)));
}

void CacheStorageContextImpl::DeleteForOrigin(const url::Origin& origin) {
  // Can be called on any sequence.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](scoped_refptr<CacheStorageContextImpl> context,
                        const url::Origin& origin) {
                       scoped_refptr<CacheStorageManager> manager =
                           context->CacheManager();
                       if (!manager)
                         return;
                       manager->DeleteOriginData(origin,
                                                 CacheStorageOwner::kCacheAPI);
                     },
                     base::RetainedRef(this), origin));
}

void CacheStorageContextImpl::AddObserver(
    CacheStorageContextImpl::Observer* observer) {
  // Any sequence
  observers_->AddObserver(observer);
}

void CacheStorageContextImpl::RemoveObserver(
    CacheStorageContextImpl::Observer* observer) {
  // Any sequence
  observers_->RemoveObserver(observer);
}

void CacheStorageContextImpl::CreateCacheStorageManagerOnTaskRunner(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  DCHECK(!cache_manager_);
  cache_manager_ = LegacyCacheStorageManager::Create(
      user_data_directory, std::move(cache_task_runner), task_runner_,
      quota_manager_proxy, observers_);
}

void CacheStorageContextImpl::ShutdownOnTaskRunner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(shutdown_);

  // Delete session-only ("clear on exit") origins.
  if (special_storage_policy_ &&
      special_storage_policy_->HasSessionOnlyOrigins()) {
    cache_manager_->GetAllOriginsUsage(
        CacheStorageOwner::kCacheAPI,
        base::BindOnce(
            [](scoped_refptr<CacheStorageManager> cache_manager,
               scoped_refptr<storage::SpecialStoragePolicy>
                   special_storage_policy,
               const std::vector<StorageUsageInfo>& usage_info) {
              for (const auto& info : usage_info) {
                if (special_storage_policy->IsStorageSessionOnly(
                        info.origin.GetURL()) &&
                    !special_storage_policy->IsStorageProtected(
                        info.origin.GetURL())) {
                  cache_manager->DeleteOriginData(
                      info.origin, CacheStorageOwner::kCacheAPI,

                      // Retain a reference to the manager until the deletion is
                      // complete, since it internally uses weak pointers for
                      // the various stages of deletion and nothing else will
                      // keep it alive during shutdown.
                      base::BindOnce(
                          [](scoped_refptr<CacheStorageManager> cache_manager,
                             blink::mojom::QuotaStatusCode) {},
                          cache_manager));
                }
              }
            },
            cache_manager_, special_storage_policy_));
  }

  // Release |cache_manager_|. New clients will get a nullptr if they request
  // an instance of CacheStorageManager after this. Any other client that
  // ref-wrapped |cache_manager_| will be able to continue using it, and the
  // CacheStorageManager will be destroyed when all the references are
  // destroyed.
  cache_manager_ = nullptr;
}

void CacheStorageContextImpl::BindBlobStorageMojoContextOnIOThread(
    ChromeBlobStorageContext* blob_storage_context,
    mojo::PendingReceiver<storage::mojom::BlobStorageContext> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blob_storage_context);
  DCHECK(receiver.is_valid());

  blob_storage_context->BindMojoContext(std::move(receiver));
}

void CacheStorageContextImpl::SetBlobParametersForCacheOnTaskRunner(
    mojo::PendingRemote<storage::mojom::BlobStorageContext> remote) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!cache_manager_)
    return;
  cache_manager_->SetBlobParametersForCache(
      base::MakeRefCounted<BlobStorageContextWrapper>(std::move(remote)));
}

void CacheStorageContextImpl::CreateQuotaClientsOnIOThread(
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!quota_manager_proxy.get())
    return;
  scoped_refptr<CacheStorageManager> manager = CacheManager();
  if (!manager)
    return;
  quota_manager_proxy->RegisterClient(
      base::MakeRefCounted<CacheStorageQuotaClient>(
          manager, CacheStorageOwner::kCacheAPI),
      storage::QuotaClientType::kServiceWorkerCache,
      {blink::mojom::StorageType::kTemporary});
  quota_manager_proxy->RegisterClient(
      base::MakeRefCounted<CacheStorageQuotaClient>(
          manager, CacheStorageOwner::kBackgroundFetch),
      storage::QuotaClientType::kBackgroundFetch,
      {blink::mojom::StorageType::kTemporary});
}

}  // namespace content
