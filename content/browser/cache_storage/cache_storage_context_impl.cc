// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_context_impl.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/cache_storage/cross_sequence/cross_sequence_cache_storage_manager.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/origin.h"

namespace content {

namespace {

const base::Feature kCacheStorageSequenceFeature{
    "CacheStorageSequence", base::FEATURE_DISABLED_BY_DEFAULT};

scoped_refptr<base::SequencedTaskRunner> CreateSchedulerTaskRunner() {
  if (!base::FeatureList::IsEnabled(kCacheStorageSequenceFeature))
    return base::CreateSingleThreadTaskRunner({BrowserThread::IO});
  return base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE});
}

}  // namespace

CacheStorageContextImpl::CacheStorageContextImpl(
    BrowserContext* browser_context)
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
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_VISIBLE,
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
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CacheStorageContextImpl::CreateQuotaClientsOnIOThread,
                     base::WrapRefCounted(this),
                     std::move(quota_manager_proxy)));
}

void CacheStorageContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CacheStorageContextImpl::ShutdownOnTaskRunner, this));
}

void CacheStorageContextImpl::AddReceiver(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!dispatcher_host_) {
    dispatcher_host_ =
        base::SequenceBound<CacheStorageDispatcherHost>(task_runner_);
    dispatcher_host_.Post(FROM_HERE, &CacheStorageDispatcherHost::Init,
                          base::RetainedRef(this));
  }
  dispatcher_host_.Post(FROM_HERE, &CacheStorageDispatcherHost::AddReceiver,
                        std::move(receiver), origin);
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
  // We can only get a mojo interface for BlobStorageContext on the IO thread.
  // Bounce there first before setting the context on the manager.
  // TODO(enne): clean this up in the future to not require this bounce and
  // to have this mojo context live on the cache storage sequence.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &CacheStorageContextImpl::GetBlobStorageMojoContextOnIOThread, this,
          base::RetainedRef(blob_storage_context)));
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
            if (!context->CacheManager()) {
              std::move(callback).Run(std::vector<StorageUsageInfo>());
              return;
            }
            context->CacheManager()->GetAllOriginsUsage(
                CacheStorageOwner::kCacheAPI, std::move(callback));
          },
          base::RetainedRef(this), std::move(callback)));
}

void CacheStorageContextImpl::DeleteForOrigin(const GURL& origin) {
  // Can be called on any sequence.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(
                             [](scoped_refptr<CacheStorageContextImpl> context,
                                const GURL& origin) {
                               if (!context->CacheManager())
                                 return;
                               context->CacheManager()->DeleteOriginData(
                                   url::Origin::Create(origin),
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

  // Delete session-only ("clear on exit") origins.
  if (special_storage_policy_ &&
      special_storage_policy_->HasSessionOnlyOrigins()) {
    cache_manager_->GetAllOriginsUsage(
        CacheStorageOwner::kCacheAPI,
        // TODO(jsbell): Make this BindOnce.
        base::BindRepeating(
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

void CacheStorageContextImpl::GetBlobStorageMojoContextOnIOThread(
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blob_storage_context);

  // TODO(enne): this receiver will need to be sent to the storage service when
  // cache storage is moved.
  auto context = blob_storage_context->MojoContext();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CacheStorageContextImpl::SetBlobParametersForCacheOnTaskRunner, this,
          base::MakeRefCounted<BlobStorageContextWrapper>(std::move(context))));
}

void CacheStorageContextImpl::SetBlobParametersForCacheOnTaskRunner(
    scoped_refptr<BlobStorageContextWrapper> blob_storage_context) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (cache_manager_)
    cache_manager_->SetBlobParametersForCache(std::move(blob_storage_context));
}

void CacheStorageContextImpl::CreateQuotaClientsOnIOThread(
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!quota_manager_proxy.get())
    return;
  quota_manager_proxy->RegisterClient(new CacheStorageQuotaClient(
      CacheManager(), CacheStorageOwner::kCacheAPI));
  quota_manager_proxy->RegisterClient(new CacheStorageQuotaClient(
      CacheManager(), CacheStorageOwner::kBackgroundFetch));
}

}  // namespace content
