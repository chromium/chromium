// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_remover_impl.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

using base::UserMetricsAction;

namespace content {

namespace {

// Timeout after which the History.ClearBrowsingData.Duration.SlowTasks180s
// histogram is recorded.
const base::TimeDelta kSlowTaskTimeout = base::TimeDelta::FromSeconds(180);

base::OnceClosure RunsOrPostOnCurrentTaskRunner(base::OnceClosure closure) {
  return base::BindOnce(
      [](base::OnceClosure closure,
         scoped_refptr<base::TaskRunner> task_runner) {
        if (base::ThreadTaskRunnerHandle::Get() == task_runner) {
          std::move(closure).Run();
          return;
        }
        task_runner->PostTask(FROM_HERE, std::move(closure));
      },
      std::move(closure), base::ThreadTaskRunnerHandle::Get());
}

// Returns whether |origin| matches |origin_type_mask| given the special
// storage |policy|; and if |predicate| is not null, then also whether
// it matches |predicate|. If |origin_type_mask| contains embedder-specific
// datatypes, |embedder_matcher| must not be null; the decision for those
// datatypes will be delegated to it.
bool DoesOriginMatchMaskAndURLs(
    int origin_type_mask,
    const base::Callback<bool(const GURL&)>& predicate,
    const BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher&
        embedder_matcher,
    const url::Origin& origin,
    storage::SpecialStoragePolicy* policy) {
  if (!predicate.is_null() && !predicate.Run(origin.GetURL()))
    return false;

  const std::vector<std::string>& schemes = url::GetWebStorageSchemes();
  bool is_web_scheme = base::Contains(schemes, origin.scheme());

  // If a websafe origin is unprotected, it matches iff UNPROTECTED_WEB.
  if ((!policy || !policy->IsStorageProtected(origin.GetURL())) &&
      is_web_scheme &&
      (origin_type_mask & BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB)) {
    return true;
  }
  origin_type_mask &= ~BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;

  // Hosted applications (protected and websafe origins) iff PROTECTED_WEB.
  if (policy && policy->IsStorageProtected(origin.GetURL()) && is_web_scheme &&
      (origin_type_mask & BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB)) {
    return true;
  }
  origin_type_mask &= ~BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;

  DCHECK(embedder_matcher || !origin_type_mask)
      << "The mask contains embedder-defined origin types, but there is no "
      << "embedder delegate matcher to process them.";

  if (!embedder_matcher.is_null())
    return embedder_matcher.Run(origin_type_mask, origin, policy);

  return false;
}

}  // namespace

BrowsingDataRemoverImpl::BrowsingDataRemoverImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      remove_mask_(-1),
      origin_type_mask_(-1),
      is_removing_(false),
      storage_partition_for_testing_(nullptr) {
  DCHECK(browser_context_);
}

BrowsingDataRemoverImpl::~BrowsingDataRemoverImpl() {
  if (!task_queue_.empty()) {
    VLOG(1) << "BrowsingDataRemoverImpl shuts down with " << task_queue_.size()
            << " pending tasks";
  }

  UMA_HISTOGRAM_EXACT_LINEAR("History.ClearBrowsingData.TaskQueueAtShutdown",
                             task_queue_.size(), 10);

  // If we are still removing data, notify observers that their task has been
  // (albeit unsucessfuly) processed, so they can unregister themselves.
  // TODO(bauerb): If it becomes a problem that browsing data might not actually
  // be fully cleared when an observer is notified, add a success flag.
  while (!task_queue_.empty()) {
    if (observer_list_.HasObserver(task_queue_.front().observer))
      task_queue_.front().observer->OnBrowsingDataRemoverDone();
    task_queue_.pop();
  }
}

void BrowsingDataRemoverImpl::SetRemoving(bool is_removing) {
  DCHECK_NE(is_removing_, is_removing);
  is_removing_ = is_removing;
}

void BrowsingDataRemoverImpl::SetEmbedderDelegate(
    BrowsingDataRemoverDelegate* embedder_delegate) {
  embedder_delegate_ = embedder_delegate;
}

bool BrowsingDataRemoverImpl::DoesOriginMatchMask(
    int origin_type_mask,
    const url::Origin& origin,
    storage::SpecialStoragePolicy* policy) {
  BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher embedder_matcher;
  if (embedder_delegate_)
    embedder_matcher = embedder_delegate_->GetOriginTypeMatcher();

  return DoesOriginMatchMaskAndURLs(
      origin_type_mask, base::Callback<bool(const GURL&)>(),
      std::move(embedder_matcher), origin, policy);
}

void BrowsingDataRemoverImpl::Remove(const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     int remove_mask,
                                     int origin_type_mask) {
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::unique_ptr<BrowsingDataFilterBuilder>(), nullptr);
}

void BrowsingDataRemoverImpl::RemoveAndReply(const base::Time& delete_begin,
                                             const base::Time& delete_end,
                                             int remove_mask,
                                             int origin_type_mask,
                                             Observer* observer) {
  DCHECK(observer);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::unique_ptr<BrowsingDataFilterBuilder>(), observer);
}

void BrowsingDataRemoverImpl::RemoveWithFilter(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
  DCHECK(filter_builder);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::move(filter_builder), nullptr);
}

void BrowsingDataRemoverImpl::RemoveWithFilterAndReply(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer) {
  DCHECK(filter_builder);
  DCHECK(observer);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::move(filter_builder), observer);
}

void BrowsingDataRemoverImpl::RemoveInternal(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer) {
  DCHECK(!observer || observer_list_.HasObserver(observer))
      << "Every observer must register itself (by calling AddObserver()) "
      << "before observing a removal task.";

  // Remove() and RemoveAndReply() pass a null pointer to indicate no filter.
  // No filter is equivalent to one that |IsEmptyBlacklist()|.
  if (!filter_builder) {
    filter_builder =
        BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST);
    DCHECK(filter_builder->IsEmptyBlacklist());
  }

  task_queue_.emplace(delete_begin, delete_end, remove_mask, origin_type_mask,
                      std::move(filter_builder), observer);

  // If this is the only scheduled task, execute it immediately. Otherwise,
  // it will be automatically executed when all tasks scheduled before it
  // finish.
  if (task_queue_.size() == 1) {
    SetRemoving(true);
    RunNextTask();
  }
}

void BrowsingDataRemoverImpl::RunNextTask() {
  DCHECK(!task_queue_.empty());
  RemovalTask& removal_task = task_queue_.front();
  removal_task.task_started = base::Time::Now();

  // To detect tasks that are causing slow deletions, record running sub tasks
  // after a delay.
  slow_pending_tasks_closure_.Reset(base::BindRepeating(
      &BrowsingDataRemoverImpl::RecordUnfinishedSubTasks, GetWeakPtr()));
  base::PostDelayedTask(FROM_HERE, {BrowserThread::UI},
                        slow_pending_tasks_closure_.callback(),
                        kSlowTaskTimeout);

  RemoveImpl(removal_task.delete_begin, removal_task.delete_end,
             removal_task.remove_mask, removal_task.filter_builder.get(),
             removal_task.origin_type_mask);
}

void BrowsingDataRemoverImpl::RemoveImpl(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    BrowsingDataFilterBuilder* filter_builder,
    int origin_type_mask) {
  // =============== README before adding more storage backends ===============
  //
  // If you're adding a data storage backend that is included among
  // RemoveDataMask::FILTERABLE_DATATYPES, you must do one of the following:
  // 1. Support one of the filters generated by |filter_builder|.
  // 2. Add a comment explaining why is it acceptable in your case to delete all
  //    data without filtering URLs / origins / domains.
  // 3. Do not support partial deletion, i.e. only delete your data if
  //    |filter_builder.IsEmptyBlacklist()|. Add a comment explaining why this
  //    is acceptable.
  base::ScopedClosureRunner synchronous_clear_operations(
      CreateTaskCompletionClosure(TracingDataType::kSynchronous));

  TRACE_EVENT0("browsing_data", "BrowsingDataRemoverImpl::RemoveImpl");

  // crbug.com/140910: Many places were calling this with base::Time() as
  // delete_end, even though they should've used base::Time::Max().
  DCHECK_NE(base::Time(), delete_end);

  delete_begin_ = delete_begin;
  delete_end_ = delete_end;
  remove_mask_ = remove_mask;
  origin_type_mask_ = origin_type_mask;

  // Record the combined deletion of cookies and cache.
  CookieOrCacheDeletionChoice choice = NEITHER_COOKIES_NOR_CACHE;
  if (remove_mask & DATA_TYPE_COOKIES &&
      origin_type_mask_ & ORIGIN_TYPE_UNPROTECTED_WEB) {
    choice =
        remove_mask & DATA_TYPE_CACHE ? BOTH_COOKIES_AND_CACHE : ONLY_COOKIES;
  } else if (remove_mask & DATA_TYPE_CACHE) {
    choice = ONLY_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCache", choice,
      MAX_CHOICE_VALUE);

  //////////////////////////////////////////////////////////////////////////////
  // INITIALIZATION
  base::RepeatingCallback<bool(const GURL& url)> filter =
      filter_builder->BuildGeneralFilter();

  // Some backends support a filter that |is_null()| to make complete deletion
  // more efficient.
  base::RepeatingCallback<bool(const GURL&)> nullable_filter =
      filter_builder->IsEmptyBlacklist()
          ? base::RepeatingCallback<bool(const GURL&)>()
          : filter;

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_DOWNLOADS
  if ((remove_mask & DATA_TYPE_DOWNLOADS) &&
      (!embedder_delegate_ || embedder_delegate_->MayRemoveDownloadHistory())) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Downloads"));
    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser_context_);
    download_manager->RemoveDownloadsByURLAndTime(filter, delete_begin_,
                                                  delete_end_);
  }

  //////////////////////////////////////////////////////////////////////////////
  // STORAGE PARTITION DATA
  uint32_t storage_partition_remove_mask = 0;

  // We ignore the DATA_TYPE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request DATA_TYPE_SITE_DATA with another origin type
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and other origin types.
  if (remove_mask & DATA_TYPE_COOKIES &&
      origin_type_mask_ & ORIGIN_TYPE_UNPROTECTED_WEB) {
    storage_partition_remove_mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
  }
  if (remove_mask & DATA_TYPE_LOCAL_STORAGE) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  }
  if (remove_mask & DATA_TYPE_INDEXED_DB) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  }
  if (remove_mask & DATA_TYPE_WEB_SQL) {
    storage_partition_remove_mask |= StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  }
  if (remove_mask & DATA_TYPE_APP_CACHE) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  }
  if (remove_mask & DATA_TYPE_SERVICE_WORKERS) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  }
  if (remove_mask & DATA_TYPE_CACHE_STORAGE) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  }
  if (remove_mask & DATA_TYPE_FILE_SYSTEMS) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  }
  if (remove_mask & DATA_TYPE_BACKGROUND_FETCH) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_BACKGROUND_FETCH;
  }
  if (remove_mask & DATA_TYPE_CACHE) {
    // Tell the shader disk cache to clear.
    base::RecordAction(UserMetricsAction("ClearBrowsingData_ShaderCache"));
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  }
  // Content Decryption Modules used by Encrypted Media store licenses in a
  // private filesystem. These are different than content licenses used by
  // Flash (which are deleted father down in this method).
  if (remove_mask & DATA_TYPE_MEDIA_LICENSES) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_PLUGIN_PRIVATE_DATA;
  }

  StoragePartition* storage_partition;
  if (storage_partition_for_testing_) {
    storage_partition = storage_partition_for_testing_;
  } else {
    storage_partition =
        BrowserContext::GetDefaultStoragePartition(browser_context_);
  }

  if (storage_partition_remove_mask) {
    uint32_t quota_storage_remove_mask =
        ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;

    if (delete_begin_ == base::Time() ||
        ((origin_type_mask_ & ~ORIGIN_TYPE_UNPROTECTED_WEB) != 0)) {
      // If we're deleting since the beginning of time, or we're removing
      // protected origins, then remove persistent quota data.
      quota_storage_remove_mask |=
          StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
    }

    // If cookies are supposed to be conditionally deleted from the storage
    // partition, create the deletion info object.
    network::mojom::CookieDeletionFilterPtr deletion_filter;
    if (!filter_builder->IsEmptyBlacklist() &&
        (storage_partition_remove_mask &
         StoragePartition::REMOVE_DATA_MASK_COOKIES)) {
      deletion_filter = filter_builder->BuildCookieDeletionFilter();
    } else {
      deletion_filter = network::mojom::CookieDeletionFilter::New();
    }

    BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher embedder_matcher;
    if (embedder_delegate_)
      embedder_matcher = embedder_delegate_->GetOriginTypeMatcher();
    bool perform_storage_cleanup =
        delete_begin_.is_null() && delete_end_.is_max() &&
        filter_builder->GetMode() == BrowsingDataFilterBuilder::BLACKLIST;

    storage_partition->ClearData(
        storage_partition_remove_mask, quota_storage_remove_mask,
        base::BindRepeating(&DoesOriginMatchMaskAndURLs, origin_type_mask_,
                            filter, std::move(embedder_matcher)),
        std::move(deletion_filter), perform_storage_cleanup, delete_begin_,
        delete_end_,
        CreateTaskCompletionClosure(TracingDataType::kStoragePartition));
  }

  //////////////////////////////////////////////////////////////////////////////
  // CACHE
  if (remove_mask & DATA_TYPE_CACHE) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Cache"));

    network::mojom::NetworkContext* network_context =
        storage_partition->GetNetworkContext();

    // TODO(msramek): Clear the cache of all renderers.

    // TODO(crbug.com/813882): implement retry on network service.

    // The clearing of the HTTP cache happens in the network service process
    // when enabled. Note that we've deprecated the concept of a media cache,
    // and are now using a single cache for both purposes.
    network_context->ClearHttpCache(
        delete_begin, delete_end, filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(TracingDataType::kHttpCache));

    storage_partition->ClearCodeCaches(
        delete_begin, delete_end, nullable_filter,
        CreateTaskCompletionClosureForMojo(TracingDataType::kCodeCaches));

    // When clearing cache, wipe accumulated network related data
    // (TransportSecurityState and HttpServerPropertiesManager data).
    network_context->ClearNetworkingHistorySince(
        delete_begin,
        CreateTaskCompletionClosureForMojo(TracingDataType::kNetworkHistory));

    // Clears the PrefetchedSignedExchangeCache of all RenderFrameHostImpls.
    RenderFrameHostImpl::ClearAllPrefetchedSignedExchangeCache();
  }

#if BUILDFLAG(ENABLE_REPORTING)
  //////////////////////////////////////////////////////////////////////////////
  // Reporting cache.
  if (remove_mask & DATA_TYPE_COOKIES) {
    network::mojom::NetworkContext* network_context =
        BrowserContext::GetDefaultStoragePartition(browser_context_)
            ->GetNetworkContext();
    network_context->ClearReportingCacheClients(
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(TracingDataType::kReportingCache));
    network_context->ClearNetworkErrorLogging(
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(
            TracingDataType::kNetworkErrorLogging));
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

  //////////////////////////////////////////////////////////////////////////////
  // Auth cache.
  if ((remove_mask & DATA_TYPE_COOKIES) &&
      !(remove_mask & DATA_TYPE_AVOID_CLOSING_CONNECTIONS)) {
    BrowserContext::GetDefaultStoragePartition(browser_context_)
        ->GetNetworkContext()
        ->ClearHttpAuthCache(delete_begin, CreateTaskCompletionClosureForMojo(
                                               TracingDataType::kAuthCache));
  }

  //////////////////////////////////////////////////////////////////////////////
  // Embedder data.
  if (embedder_delegate_) {
    embedder_delegate_->RemoveEmbedderData(
        delete_begin_, delete_end_, remove_mask, filter_builder,
        origin_type_mask,
        CreateTaskCompletionClosure(TracingDataType::kEmbedderData));
  }
}

void BrowsingDataRemoverImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BrowsingDataRemoverImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowsingDataRemoverImpl::SetWouldCompleteCallbackForTesting(
    const base::RepeatingCallback<
        void(base::OnceClosure continue_to_completion)>& callback) {
  would_complete_callback_ = callback;
}

void BrowsingDataRemoverImpl::OverrideStoragePartitionForTesting(
    StoragePartition* storage_partition) {
  storage_partition_for_testing_ = storage_partition;
}

const base::Time& BrowsingDataRemoverImpl::GetLastUsedBeginTime() {
  return delete_begin_;
}

const base::Time& BrowsingDataRemoverImpl::GetLastUsedEndTime() {
  return delete_end_;
}

int BrowsingDataRemoverImpl::GetLastUsedRemovalMask() {
  return remove_mask_;
}

int BrowsingDataRemoverImpl::GetLastUsedOriginTypeMask() {
  return origin_type_mask_;
}

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer)
    : delete_begin(delete_begin),
      delete_end(delete_end),
      remove_mask(remove_mask),
      origin_type_mask(origin_type_mask),
      filter_builder(std::move(filter_builder)),
      observer(observer) {}

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(
    RemovalTask&& other) noexcept = default;

BrowsingDataRemoverImpl::RemovalTask::~RemovalTask() {}

void BrowsingDataRemoverImpl::Notify() {
  // Some tests call |RemoveImpl| directly, without using the task scheduler.
  // TODO(msramek): Improve those tests so we don't have to do this. Tests
  // relying on |RemoveImpl| do so because they need to pass in
  // BrowsingDataFilterBuilder while still keeping ownership of it. Making
  // BrowsingDataFilterBuilder copyable would solve this.
  if (!is_removing_) {
    DCHECK(task_queue_.empty());
    return;
  }

  // Inform the observer of the current task unless it has unregistered
  // itself in the meantime.
  DCHECK(!task_queue_.empty());

  const RemovalTask& task = task_queue_.front();
  if (task.observer != nullptr && observer_list_.HasObserver(task.observer)) {
    task.observer->OnBrowsingDataRemoverDone();
  }
  if (task.filter_builder->GetMode() == BrowsingDataFilterBuilder::BLACKLIST) {
    base::TimeDelta delta = base::Time::Now() - task.task_started;
    // Full and partial deletions are often implemented differently, so
    // we track them in seperate metrics.
    if (task.delete_begin.is_null() && task.delete_end.is_max() &&
        task.filter_builder->IsEmptyBlacklist()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "History.ClearBrowsingData.Duration.FullDeletion", delta);
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "History.ClearBrowsingData.Duration.PartialDeletion", delta);
    }
  }

  task_queue_.pop();

  if (task_queue_.empty()) {
    // All removal tasks have finished. Inform the observers that we're idle.
    SetRemoving(false);
    return;
  }

  // Yield to the UI thread before executing the next removal task.
  // TODO(msramek): Consider also adding a backoff if too many tasks
  // are scheduled.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&BrowsingDataRemoverImpl::RunNextTask, GetWeakPtr()));
}

void BrowsingDataRemoverImpl::OnTaskComplete(TracingDataType data_type) {
  // TODO(brettw) http://crbug.com/305259: This should also observe session
  // clearing (what about other things such as passwords, etc.?) and wait for
  // them to complete before continuing.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t num_erased = pending_sub_tasks_.erase(data_type);
  DCHECK_EQ(num_erased, 1U);

  TRACE_EVENT_ASYNC_END1("browsing_data", "BrowsingDataRemoverImpl",
                         static_cast<int>(data_type), "data_type",
                         static_cast<int>(data_type));
  if (!pending_sub_tasks_.empty())
    return;

  slow_pending_tasks_closure_.Cancel();

  if (!would_complete_callback_.is_null()) {
    would_complete_callback_.Run(
        base::BindOnce(&BrowsingDataRemoverImpl::Notify, GetWeakPtr()));
    return;
  }

  Notify();
}

base::OnceClosure BrowsingDataRemoverImpl::CreateTaskCompletionClosure(
    TracingDataType data_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto result = pending_sub_tasks_.insert(data_type);
  DCHECK(result.second) << "Task already started: "
                        << static_cast<int>(data_type);
  TRACE_EVENT_ASYNC_BEGIN1("browsing_data", "BrowsingDataRemoverImpl",
                           static_cast<int>(data_type), "data_type",
                           static_cast<int>(data_type));
  return base::BindOnce(&BrowsingDataRemoverImpl::OnTaskComplete, GetWeakPtr(),
                        data_type);
}

base::OnceClosure BrowsingDataRemoverImpl::CreateTaskCompletionClosureForMojo(
    TracingDataType data_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return RunsOrPostOnCurrentTaskRunner(mojo::WrapCallbackWithDropHandler(
      CreateTaskCompletionClosure(data_type),
      base::BindOnce(&BrowsingDataRemoverImpl::OnTaskComplete, GetWeakPtr(),
                     data_type)));
}

void BrowsingDataRemoverImpl::RecordUnfinishedSubTasks() {
  DCHECK(!pending_sub_tasks_.empty());
  for (TracingDataType task : pending_sub_tasks_) {
    UMA_HISTOGRAM_ENUMERATION(
        "History.ClearBrowsingData.Duration.SlowTasks180s", task);
  }
}

base::WeakPtr<BrowsingDataRemoverImpl> BrowsingDataRemoverImpl::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::WeakPtr<BrowsingDataRemoverImpl> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();

  // Immediately bind the weak pointer to the UI thread. This makes it easier
  // to discover potential misuse on the IO thread.
  weak_ptr.get();

  return weak_ptr;
}

}  // namespace content
