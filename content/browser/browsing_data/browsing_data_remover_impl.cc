// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_remover_impl.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/browsing_data/core/cookie_or_cache_deletion_choice.h"
#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

using base::UserMetricsAction;

namespace content {

namespace {

// Timeout after which the History.ClearBrowsingData.Duration.SlowTasks180s
// histogram is recorded.
const base::TimeDelta kSlowTaskTimeout = base::Seconds(180);

base::OnceClosure RunsOrPostOnCurrentTaskRunner(base::OnceClosure closure) {
  return base::BindOnce(
      [](base::OnceClosure closure,
         scoped_refptr<base::TaskRunner> task_runner) {
        if (base::SingleThreadTaskRunner::GetCurrentDefault() == task_runner) {
          std::move(closure).Run();
          return;
        }
        task_runner->PostTask(FROM_HERE, std::move(closure));
      },
      std::move(closure), base::SingleThreadTaskRunner::GetCurrentDefault());
}

// Returns whether `storage_key` matches `origin_type_mask` given the special
// storage `policy`. If `origin_type_mask` contains embedder-specific
// datatypes, `embedder_matcher` must not be null; the decision for those
// datatypes will be delegated to it.
bool DoesStorageKeyMatchMask(
    uint64_t origin_type_mask,
    const BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher&
        embedder_matcher,
    const blink::StorageKey& storage_key,
    storage::SpecialStoragePolicy* policy) {
  const std::vector<std::string>& schemes = url::GetWebStorageSchemes();
  bool is_web_scheme = base::Contains(schemes, storage_key.origin().scheme());

  // If a websafe origin is unprotected, it matches iff UNPROTECTED_WEB.
  if ((origin_type_mask & BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB) &&
      is_web_scheme &&
      (!policy || !policy->IsStorageProtected(storage_key.origin().GetURL()))) {
    return true;
  }
  origin_type_mask &= ~BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;

  // Hosted applications (protected and websafe origins) iff PROTECTED_WEB.
  if ((origin_type_mask & BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB) &&
      is_web_scheme && policy &&
      policy->IsStorageProtected(storage_key.origin().GetURL())) {
    return true;
  }
  origin_type_mask &= ~BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;

  DCHECK(embedder_matcher || !origin_type_mask)
      << "The mask contains embedder-defined origin types, but there is no "
      << "embedder delegate matcher to process them.";

  if (!embedder_matcher.is_null())
    return embedder_matcher.Run(origin_type_mask, storage_key.origin(), policy);

  return false;
}

}  // namespace

BrowsingDataRemoverImpl::BrowsingDataRemoverImpl(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      remove_mask_(0xffffffffffffffffull),
      origin_type_mask_(0xffffffffffffffffull),
      storage_partition_config_(std::nullopt),
      is_removing_(false) {
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
  // (albeit unsuccessfully) processed, so they can unregister themselves.
  while (!task_queue_.empty()) {
    const RemovalTask& task = task_queue_.front();
    for (Observer* observer : task.observers) {
      if (observer_list_.HasObserver(observer)) {
        observer->OnBrowsingDataRemoverDone(
            /*failed_data_types=*/task.remove_mask);
      }
    }
    task_queue_.pop_front();
  }
}

void BrowsingDataRemoverImpl::SetRemoving(bool is_removing) {
  DCHECK_NE(is_removing_, is_removing);
  is_removing_ = is_removing;
  if (embedder_delegate_) {
    if (is_removing_) {
      embedder_delegate_->OnStartRemoving();
    } else {
      embedder_delegate_->OnDoneRemoving();
    }
  }
}

void BrowsingDataRemoverImpl::SetEmbedderDelegate(
    BrowsingDataRemoverDelegate* embedder_delegate) {
  embedder_delegate_ = embedder_delegate;
}

bool BrowsingDataRemoverImpl::DoesOriginMatchMaskForTesting(
    uint64_t origin_type_mask,
    const url::Origin& origin,
    storage::SpecialStoragePolicy* policy) {
  BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher embedder_matcher;
  if (embedder_delegate_)
    embedder_matcher = embedder_delegate_->GetOriginTypeMatcher();

  return DoesStorageKeyMatchMask(origin_type_mask, std::move(embedder_matcher),
                                 blink::StorageKey::CreateFirstParty(origin),
                                 policy);
}

void BrowsingDataRemoverImpl::Remove(const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     uint64_t remove_mask,
                                     uint64_t origin_type_mask) {
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::unique_ptr<BrowsingDataFilterBuilder>(), nullptr);
}

void BrowsingDataRemoverImpl::RemoveWithFilter(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    uint64_t origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::move(filter_builder), nullptr);
}

void BrowsingDataRemoverImpl::RemoveAndReply(const base::Time& delete_begin,
                                             const base::Time& delete_end,
                                             uint64_t remove_mask,
                                             uint64_t origin_type_mask,
                                             Observer* observer) {
  DCHECK(observer);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::unique_ptr<BrowsingDataFilterBuilder>(), observer);
}

void BrowsingDataRemoverImpl::RemoveWithFilterAndReply(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    uint64_t origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer) {
  DCHECK(filter_builder);
  DCHECK(observer);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::move(filter_builder), observer);
}

void BrowsingDataRemoverImpl::RemoveStorageBucketsAndReply(
    std::optional<StoragePartitionConfig> storage_partition_config,
    const blink::StorageKey& storage_key,
    const std::set<std::string>& storage_buckets,
    base::OnceClosure callback) {
  DCHECK(callback);
  GetStoragePartition(std::move(storage_partition_config))
      ->ClearDataForBuckets(
          storage_key, storage_buckets,
          base::BindPostTaskToCurrentDefault(
              base::BindOnce(&BrowsingDataRemoverImpl::DidRemoveStorageBuckets,
                             GetWeakPtr(), std::move(callback))));
}

void BrowsingDataRemoverImpl::DidRemoveStorageBuckets(
    base::OnceClosure callback) {
  DCHECK(callback);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(callback).Run();
}

void BrowsingDataRemoverImpl::RemoveInternal(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    uint64_t origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer) {
  DCHECK(!observer || observer_list_.HasObserver(observer))
      << "Every observer must register itself (by calling AddObserver()) "
      << "before observing a removal task.";

  CHECK(!filter_builder || !filter_builder->MatchesNothing())
      << "Filters of type `kDelete` with empty origin and domain lists match "
      << "nothing. To match all origins and domains, use a `kPreserve` filter.";

  // Remove() and RemoveAndReply() pass a null pointer to indicate no filter.
  // No filter is equivalent to one that |MatchesAllOriginsAndDomains()|.
  if (!filter_builder) {
    filter_builder = BrowsingDataFilterBuilder::Create(
        BrowsingDataFilterBuilder::Mode::kPreserve);
    DCHECK(filter_builder->MatchesAllOriginsAndDomains());
  }

  RemovalTask task(delete_begin, delete_end, remove_mask, origin_type_mask,
                   std::move(filter_builder), observer);

  // If there is an identical deletion task that is not already running,
  // we don't have to perform the deletion twice.
  for (size_t i = 1; i < task_queue_.size(); i++) {
    if (task_queue_[i].IsSameDeletion(task)) {
      if (observer)
        task_queue_[i].observers.push_back(observer);
      return;
    }
  }

  task_queue_.push_back(std::move(task));

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
  removal_task.task_started = base::TimeTicks::Now();

  // To detect tasks that are causing slow deletions, record running sub tasks
  // after a delay.
  slow_pending_tasks_closure_.Reset(base::BindOnce(
      &BrowsingDataRemoverImpl::RecordUnfinishedSubTasks, GetWeakPtr()));
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, slow_pending_tasks_closure_.callback(), kSlowTaskTimeout);

  RemoveImpl(removal_task.delete_begin, removal_task.delete_end,
             removal_task.remove_mask, removal_task.filter_builder.get(),
             removal_task.origin_type_mask);
}

void BrowsingDataRemoverImpl::RemoveImpl(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    BrowsingDataFilterBuilder* filter_builder,
    uint64_t origin_type_mask) {
  // =============== README before adding more storage backends ===============
  //
  // If you're adding a data storage backend that is included among
  // RemoveDataMask::FILTERABLE_DATATYPES, you must do one of the following:
  // 1. Support one of the filters generated by |filter_builder|.
  // 2. Add a comment explaining why is it acceptable in your case to delete all
  //    data without filtering URLs / origins / domains.
  // 3. Do not support partial deletion, i.e. only delete your data if
  //    |filter_builder.MatchesAllOriginsAndDomains()|. Add a comment explaining
  //    why this is acceptable.
  base::ScopedClosureRunner synchronous_clear_operations(
      CreateTaskCompletionClosure(TracingDataType::kSynchronous));

  TRACE_EVENT0("browsing_data", "BrowsingDataRemoverImpl::RemoveImpl");

  // Asynchronous removal tasks might end up finishing after an arbitrary
  // delay - this can postpone when OnTaskComplete runs.  Therefore we need to
  // check if destruction of our `browser_context_` might have started in the
  // meantime.  See also https://crbug.com/1216406.
  if (browser_context_->ShutdownStarted()) {
    // Conservatively mark *all* data types as failures.
    failed_data_types_ |= remove_mask_;
    return;
  }

  // crbug.com/140910: Many places were calling this with base::Time() as
  // delete_end, even though they should've used base::Time::Max().
  DCHECK_NE(base::Time(), delete_end);
  DCHECK(domains_for_deferred_cookie_deletion_.empty());

  // If a specific StoragePartition is specified in the filter, only data
  // types that are scoped to a StoragePartition should be removed.
  DCHECK(!filter_builder->GetStoragePartitionConfig().has_value() ||
         !(remove_mask & ~DATA_TYPE_ON_STORAGE_PARTITION));

  delete_begin_ = delete_begin;
  delete_end_ = delete_end;
  remove_mask_ = remove_mask;
  origin_type_mask_ = origin_type_mask;
  storage_partition_config_ = filter_builder->GetStoragePartitionConfig();
  failed_data_types_ = 0;

  // Record the combined deletion of cookies and cache.
  browsing_data::CookieOrCacheDeletionChoice choice =
      browsing_data::CookieOrCacheDeletionChoice::kNeitherCookiesNorCache;
  if (remove_mask & DATA_TYPE_COOKIES &&
      origin_type_mask_ & ORIGIN_TYPE_UNPROTECTED_WEB) {
    choice =
        remove_mask & DATA_TYPE_CACHE
            ? browsing_data::CookieOrCacheDeletionChoice::kBothCookiesAndCache
            : browsing_data::CookieOrCacheDeletionChoice::kOnlyCookies;
  } else if (remove_mask & DATA_TYPE_CACHE) {
    choice = browsing_data::CookieOrCacheDeletionChoice::kOnlyCache;
  }

  base::UmaHistogramEnumeration(
      "History.ClearBrowsingData.UserDeletedCookieOrCache", choice);

  //////////////////////////////////////////////////////////////////////////////
  // INITIALIZATION
  base::RepeatingCallback<bool(const GURL&)> url_filter =
      filter_builder->BuildUrlFilter();

  // Some backends support a filter that |is_null()| to make complete deletion
  // more efficient.
  base::RepeatingCallback<bool(const GURL&)> nullable_url_filter =
      filter_builder->MatchesAllOriginsAndDomains()
          ? base::RepeatingCallback<bool(const GURL&)>()
          : url_filter;

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_DOWNLOADS
  if ((remove_mask & DATA_TYPE_DOWNLOADS) &&
      (!embedder_delegate_ || embedder_delegate_->MayRemoveDownloadHistory())) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Downloads"));
    DownloadManager* download_manager = browser_context_->GetDownloadManager();
    download_manager->RemoveDownloadsByURLAndTime(url_filter, delete_begin_,
                                                  delete_end_);
  }

  //////////////////////////////////////////////////////////////////////////////
  // STORAGE PARTITION DATA
  StoragePartition* storage_partition =
      GetStoragePartition(filter_builder->GetStoragePartitionConfig());

  uint32_t storage_partition_remove_mask = 0;

  // We ignore the DATA_TYPE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request DATA_TYPE_SITE_DATA with another origin type
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and other origin types.
  if (remove_mask & DATA_TYPE_COOKIES &&
      origin_type_mask_ & ORIGIN_TYPE_UNPROTECTED_WEB) {
    storage_partition_remove_mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
    if (!filter_builder->PartitionedCookiesOnly()) {
      // Interest groups should be cleared with cookies for its origin trial as
      // the current FLEDGE implementation has the same privacy characteristics.
      //
      // Interest groups are per-origin, and don't support the concept of
      // partitioning, so we only trigger their deletion if *unpartitioned*
      // cookies are being deleted (hence the not-PartitionedCookiesOnly check
      // above).
      storage_partition_remove_mask |=
          StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUPS;
    }
    if (embedder_delegate_) {
      domains_for_deferred_cookie_deletion_ =
          embedder_delegate_->GetDomainsForDeferredCookieDeletion(
              storage_partition, remove_mask);
    }
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
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUP_PERMISSIONS_CACHE;
    // Tell the shader disk cache to clear.
    base::RecordAction(UserMetricsAction("ClearBrowsingData_ShaderCache"));
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  }
  if (remove_mask & DATA_TYPE_MEDIA_LICENSES ||
      // TODO(crbug.com/40264778): For now, media licenses are part of the quota
      // management system. If all DOM storage types are being removed, remove
      // media licenses as well. When bug is resolved, this condition can be
      // removed.
      (remove_mask & DATA_TYPE_DOM_STORAGE) == DATA_TYPE_DOM_STORAGE) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_MEDIA_LICENSES;
  }
  if (remove_mask & DATA_TYPE_ATTRIBUTION_REPORTING_SITE_CREATED) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_SITE_CREATED;
  }
  if (remove_mask & DATA_TYPE_ATTRIBUTION_REPORTING_INTERNAL) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_ATTRIBUTION_REPORTING_INTERNAL;
  }
  if (remove_mask & DATA_TYPE_AGGREGATION_SERVICE) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_AGGREGATION_SERVICE;
  }
  if (remove_mask & DATA_TYPE_PRIVATE_AGGREGATION_INTERNAL) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_PRIVATE_AGGREGATION_INTERNAL;
  }
  if (remove_mask & DATA_TYPE_INTEREST_GROUPS) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUPS;
  }
  if (remove_mask & DATA_TYPE_INTEREST_GROUPS_INTERNAL) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_INTEREST_GROUPS_INTERNAL;
  }
  if (remove_mask & DATA_TYPE_SHARED_STORAGE) {
    storage_partition_remove_mask |=
        StoragePartition::REMOVE_DATA_MASK_SHARED_STORAGE;
  }

  if (storage_partition_remove_mask) {
    // If cookies are supposed to be conditionally deleted from the storage
    // partition, create the deletion info object.
    network::mojom::CookieDeletionFilterPtr deletion_filter;
    if (!filter_builder->MatchesAllOriginsAndDomains() &&
        (storage_partition_remove_mask &
         StoragePartition::REMOVE_DATA_MASK_COOKIES)) {
      deletion_filter = filter_builder->BuildCookieDeletionFilter();
    } else {
      deletion_filter = network::mojom::CookieDeletionFilter::New();
    }

    if (!domains_for_deferred_cookie_deletion_.empty()) {
      // The data types that require deferred deletion are currently not
      // filterable. If they become filterable we need to check if the
      // selected domains should actually be deleted.
      DCHECK(!deletion_filter->excluding_domains.has_value());
      deletion_filter->excluding_domains =
          domains_for_deferred_cookie_deletion_;
    }

    BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher embedder_matcher;
    if (embedder_delegate_)
      embedder_matcher = embedder_delegate_->GetOriginTypeMatcher();
    // Rewrite leveldb instances to clean up data from disk if almost all data
    // is deleted. Do not perform the cleanup for partial deletions or when only
    // hosted app data is removed as this would be very slow.
    bool perform_storage_cleanup =
        delete_begin_.is_null() && delete_end_.is_max() &&
        origin_type_mask_ & ORIGIN_TYPE_UNPROTECTED_WEB &&
        filter_builder->MatchesMostOriginsAndDomains();

    storage_partition->ClearData(
        storage_partition_remove_mask,
        StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, filter_builder,
        base::BindRepeating(&DoesStorageKeyMatchMask, origin_type_mask_,
                            std::move(embedder_matcher)),
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

    if (filter_builder->MatchesMostOriginsAndDomains()) {
      RenderProcessHostImpl::ClearAllResourceCaches();
    }

    // TODO(crbug.com/40563720): implement retry on network service.

    // The clearing of the HTTP cache happens in the network service process
    // when enabled. Note that we've deprecated the concept of a media cache,
    // and are now using a single cache for both purposes.
    network_context->ClearHttpCache(
        delete_begin, delete_end, filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(TracingDataType::kHttpCache));

    if (base::FeatureList::IsEnabled(
            features::kCodeCacheDeletionWithoutFilter)) {
      // Experimentally perform preservelist deletions without filter and skip
      // origin specific deletions. See crbug.com/1040039#26.
      if (filter_builder->MatchesMostOriginsAndDomains()) {
        storage_partition->ClearCodeCaches(
            delete_begin, delete_end, /*url_matcher=*/base::NullCallback(),
            CreateTaskCompletionClosureForMojo(TracingDataType::kCodeCaches));
      }
    } else {
      storage_partition->ClearCodeCaches(
          delete_begin, delete_end, nullable_url_filter,
          CreateTaskCompletionClosureForMojo(TracingDataType::kCodeCaches));
    }

    // TODO(crbug.com/1985971) : Implement filtering for NetworkHistory.
    if (filter_builder->MatchesMostOriginsAndDomains()) {
      // When clearing cache, wipe accumulated network related data
      // (TransportSecurityState and HttpServerPropertiesManager data).
      network_context->ClearNetworkingHistoryBetween(
          delete_begin, delete_end,
          CreateTaskCompletionClosureForMojo(TracingDataType::kNetworkHistory));
    }

    // Clears the PrefetchedSignedExchangeCache of all RenderFrameHostImpls.
    if (filter_builder->MatchesMostOriginsAndDomains()) {
      RenderFrameHostImpl::ClearAllPrefetchedSignedExchangeCache();
    }

    // Clears the CORS PreFlight cache. We don't support delete_begin,
    // delete_end time range, as the preflight cache max age is capped to 2hrs.
    network_context->ClearCorsPreflightCache(
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(TracingDataType::kPreflightCache));

    // Clears the BFCache entries that match the removal filter for the current
    // browser context.
    auto storage_key_filter = filter_builder->BuildStorageKeyFilter();
    for (WebContentsImpl* web_contents : WebContentsImpl::GetAllWebContents()) {
      if (web_contents->GetBrowserContext() == browser_context_) {
        web_contents->GetController().GetBackForwardCache().Flush(
            storage_key_filter);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Prototype Trust Token API (https://github.com/wicg/trust-token-api).

  // We don't support clearing data for specific time ranges because much Trust
  // Tokens state (e.g. issuers associated with each top-level origin) has no
  // notion of associated creation time. Consequently, like for reporting and
  // network error logging below, a data removal request for certain
  // sites/origins that has the Trust Tokens type in scope will clear all Trust
  // Tokens data associated with the requested sites/origins.
  if (remove_mask & DATA_TYPE_TRUST_TOKENS) {
    network::mojom::NetworkContext* network_context =
        storage_partition->GetNetworkContext();
    network_context->ClearTrustTokenData(
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(TracingDataType::kTrustTokens));
  }

  //////////////////////////////////////////////////////////////////////////////
  // Reporting cache.
  // TODO(crbug.com/40818785): Add unit test to cover this.
  if (remove_mask & DATA_TYPE_COOKIES) {
    network::mojom::NetworkContext* network_context =
        storage_partition->GetNetworkContext();
    network_context->ClearReportingCacheClients(
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(TracingDataType::kReportingCache));
    network_context->ClearNetworkErrorLogging(
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(
            TracingDataType::kNetworkErrorLogging));

    // Clears the BFCache entries that are loaded with "Cache-Control: no-store"
    // header and match the removal filter for the current browser context.
    auto storage_key_filter = filter_builder->BuildStorageKeyFilter();
    for (WebContentsImpl* web_contents : WebContentsImpl::GetAllWebContents()) {
      if (web_contents->GetBrowserContext() == browser_context_) {
        web_contents->GetController()
            .GetBackForwardCache()
            .FlushCacheControlNoStoreEntries(storage_key_filter);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Auth cache.
  if ((remove_mask & DATA_TYPE_COOKIES) &&
      !(remove_mask & DATA_TYPE_AVOID_CLOSING_CONNECTIONS)) {
    storage_partition->GetNetworkContext()->ClearHttpAuthCache(
        delete_begin_.is_null() ? base::Time::Min() : delete_begin_,
        delete_end_.is_null() ? base::Time::Max() : delete_end_,
        filter_builder->BuildNetworkServiceFilter(),
        CreateTaskCompletionClosureForMojo(TracingDataType::kAuthCache));
  }

  //////////////////////////////////////////////////////////////////////////////
  // Shared Dictionaries.
  if ((remove_mask & DATA_TYPE_COOKIES) || (remove_mask & DATA_TYPE_CACHE)) {
    if (base::FeatureList::IsEnabled(
            network::features::kCompressionDictionaryTransportBackend)) {
      network::mojom::NetworkContext* network_context =
          storage_partition->GetNetworkContext();
      network_context->ClearSharedDictionaryCache(
          delete_begin, delete_end, filter_builder->BuildNetworkServiceFilter(),
          CreateTaskCompletionClosureForMojo(
              TracingDataType::kSharedDictionary));
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Embedder data.
  if (embedder_delegate_) {
    embedder_delegate_->RemoveEmbedderData(
        delete_begin_, delete_end_, remove_mask, filter_builder,
        origin_type_mask,
        base::BindOnce(
            &BrowsingDataRemoverImpl::OnDelegateDone, GetWeakPtr(),
            CreateTaskCompletionClosure(TracingDataType::kEmbedderData)));
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
    const StoragePartitionConfig& storage_partition_config,
    StoragePartition* storage_partition) {
  storage_partitions_for_testing_[storage_partition_config] = storage_partition;
}

const base::Time& BrowsingDataRemoverImpl::GetLastUsedBeginTimeForTesting() {
  return delete_begin_;
}

uint64_t BrowsingDataRemoverImpl::GetLastUsedRemovalMaskForTesting() {
  return remove_mask_;
}

uint64_t BrowsingDataRemoverImpl::GetLastUsedOriginTypeMaskForTesting() {
  return origin_type_mask_;
}

std::optional<StoragePartitionConfig>
BrowsingDataRemoverImpl::GetLastUsedStoragePartitionConfigForTesting() {
  return storage_partition_config_;
}

uint64_t BrowsingDataRemoverImpl::GetPendingTaskCountForTesting() {
  return task_queue_.size();
}

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    uint64_t origin_type_mask,
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
    Observer* observer)
    : delete_begin(delete_begin),
      delete_end(delete_end),
      remove_mask(remove_mask),
      origin_type_mask(origin_type_mask),
      filter_builder(std::move(filter_builder)) {
  if (observer)
    observers.push_back(observer);
}

BrowsingDataRemoverImpl::RemovalTask::RemovalTask(
    RemovalTask&& other) noexcept = default;

BrowsingDataRemoverImpl::RemovalTask::~RemovalTask() = default;

bool BrowsingDataRemoverImpl::RemovalTask::IsSameDeletion(
    const RemovalTask& other) {
  return delete_begin == other.delete_begin && delete_end == other.delete_end &&
         remove_mask == other.remove_mask &&
         origin_type_mask == other.origin_type_mask &&
         *filter_builder == *other.filter_builder;
}

StoragePartition* BrowsingDataRemoverImpl::GetStoragePartition(
    std::optional<StoragePartitionConfig> storage_partition_config) {
  DCHECK(!browser_context_->ShutdownStarted());
  if (!storage_partitions_for_testing_.empty()) {
    StoragePartition* storage_partition =
        storage_partitions_for_testing_[storage_partition_config.value_or(
            StoragePartitionConfig::CreateDefault(browser_context_))];
    CHECK(storage_partition);
    return storage_partition;
  }
  return storage_partition_config.has_value()
             ? browser_context_->GetStoragePartition(*storage_partition_config)
             : browser_context_->GetDefaultStoragePartition();
}

void BrowsingDataRemoverImpl::OnDelegateDone(
    base::OnceClosure completion_closure,
    uint64_t failed_data_types) {
  failed_data_types_ |= failed_data_types;
  std::move(completion_closure).Run();
}

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
  for (Observer* observer : task.observers) {
    if (observer_list_.HasObserver(observer)) {
      observer->OnBrowsingDataRemoverDone(failed_data_types_);
    }
  }

  base::TimeDelta delta = base::TimeTicks::Now() - task.task_started;
  if (task.filter_builder->MatchesMostOriginsAndDomains()) {
    // Full, and time based and filtered deletions are often implemented
    // differently, so we track them in separate metrics.
    if (!task.filter_builder->MatchesAllOriginsAndDomains()) {
      base::UmaHistogramMediumTimes(
          "History.ClearBrowsingData.Duration.FilteredDeletion", delta);
    } else if (task.delete_begin.is_null() && task.delete_end.is_max()) {
      base::UmaHistogramMediumTimes(
          "History.ClearBrowsingData.Duration.FullDeletion", delta);
    } else {
      base::UmaHistogramMediumTimes(
          "History.ClearBrowsingData.Duration.TimeRangeDeletion", delta);
    }
  } else {
    base::UmaHistogramMediumTimes(
        "History.ClearBrowsingData.Duration.OriginDeletion", delta);
  }

  task_queue_.pop_front();

  if (task_queue_.empty()) {
    // All removal tasks have finished. Inform the observers that we're idle.
    SetRemoving(false);
    return;
  }

  // Yield to the UI thread before executing the next removal task.
  // TODO(msramek): Consider also adding a backoff if too many tasks
  // are scheduled.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowsingDataRemoverImpl::RunNextTask, GetWeakPtr()));
}

void BrowsingDataRemoverImpl::OnTaskComplete(TracingDataType data_type,
                                             base::TimeTicks started) {
  // TODO(brettw) http://crbug.com/305259: This should also observe session
  // clearing (what about other things such as passwords, etc.?) and wait for
  // them to complete before continuing.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  size_t num_erased = pending_sub_tasks_.erase(data_type);
  DCHECK_EQ(num_erased, 1U);

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "browsing_data", "BrowsingDataRemoverImpl",
      TRACE_ID_WITH_SCOPE("BrowsingDataRemoverImpl",
                          static_cast<int>(data_type)),
      "data_type", static_cast<int>(data_type));

  base::UmaHistogramMediumTimes(
      base::StrCat({"History.ClearBrowsingData.Duration.Task.",
                    GetHistogramSuffix(data_type)}),
      base::TimeTicks::Now() - started);

  if (!pending_sub_tasks_.empty())
    return;

  // If any cookie deletions have been deferred do them now since all other
  // tasks are completed.
  if (!domains_for_deferred_cookie_deletion_.empty()) {
    std::optional<StoragePartitionConfig> storage_partition_config =
        task_queue_.front().filter_builder->GetStoragePartitionConfig();

    DCHECK(remove_mask_ & DATA_TYPE_COOKIES);
    DCHECK(!storage_partition_config.has_value() ||
           storage_partition_config->is_default());

    auto deletion_filter = network::mojom::CookieDeletionFilter::New();
    deletion_filter->including_domains =
        std::move(domains_for_deferred_cookie_deletion_);
    // Moving a vector is defined to empty this vector.
    DCHECK(domains_for_deferred_cookie_deletion_.empty());

    // Asynchronous removal tasks might end up finishing after an arbitrary
    // delay - this can postpone when OnTaskComplete runs.  Therefore we need to
    // check if destruction of our `browser_context_` might have started in the
    // meantime.  See also https://crbug.com/1216406.
    if (browser_context_->ShutdownStarted()) {
      // The tasks related to `domains_for_deferred_cookie_deletion_` and
      // `deletion_filter` are implicitly dropped if we can't clear the data
      // because the StoragePartition's destructor has already started running.
      failed_data_types_ |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
    } else {
      GetStoragePartition(storage_partition_config)
          ->ClearData(
              StoragePartition::REMOVE_DATA_MASK_COOKIES,
              /*quota_storage_remove_mask=*/0,
              /*filter_builder=*/nullptr,
              /*storage_key_policy_matcher=*/base::NullCallback(),
              std::move(deletion_filter),
              /*perform_storage_cleanup=*/false, delete_begin_, delete_end_,
              CreateTaskCompletionClosure(TracingDataType::kDeferredCookies));
      return;
    }
  }

  slow_pending_tasks_closure_.Cancel();

  if (!would_complete_callback_.is_null()) {
    would_complete_callback_.Run(
        base::BindOnce(&BrowsingDataRemoverImpl::Notify, GetWeakPtr()));
    return;
  }

  Notify();
}

const char* BrowsingDataRemoverImpl::GetHistogramSuffix(TracingDataType task) {
  switch (task) {
    case TracingDataType::kSynchronous:
      return "Synchronous";
    case TracingDataType::kEmbedderData:
      return "EmbedderData";
    case TracingDataType::kStoragePartition:
      return "StoragePartition";
    case TracingDataType::kHttpCache:
      return "HttpCache";
    case TracingDataType::kHttpAndMediaCaches:
      return "HttpAndMediaCaches";
    case TracingDataType::kReportingCache:
      return "ReportingCache";
    case TracingDataType::kChannelIds:
      return "ChannelIds";
    case TracingDataType::kNetworkHistory:
      return "NetworkHistory";
    case TracingDataType::kAuthCache:
      return "AuthCache";
    case TracingDataType::kCodeCaches:
      return "CodeCaches";
    case TracingDataType::kNetworkErrorLogging:
      return "NetworkErrorLogging";
    case TracingDataType::kTrustTokens:
      return "TrustTokens";
    case TracingDataType::kConversions:
      return "Conversions";
    case TracingDataType::kDeferredCookies:
      return "DeferredCookies";
    case TracingDataType::kSharedStorage:
      return "SharedStorage";
    case TracingDataType::kPreflightCache:
      return "PreflightCache";
    case TracingDataType::kSharedDictionary:
      return "SharedDictionary";
  }
}

base::OnceClosure BrowsingDataRemoverImpl::CreateTaskCompletionClosure(
    TracingDataType data_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto result = pending_sub_tasks_.insert(data_type);
  DCHECK(result.second) << "Task already started: "
                        << static_cast<int>(data_type);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "browsing_data", "BrowsingDataRemoverImpl",
      TRACE_ID_WITH_SCOPE("BrowsingDataRemoverImpl",
                          static_cast<int>(data_type)),
      "data_type", static_cast<int>(data_type));
  return base::BindOnce(&BrowsingDataRemoverImpl::OnTaskComplete, GetWeakPtr(),
                        data_type, base::TimeTicks::Now());
}

base::OnceClosure BrowsingDataRemoverImpl::CreateTaskCompletionClosureForMojo(
    TracingDataType data_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return RunsOrPostOnCurrentTaskRunner(mojo::WrapCallbackWithDropHandler(
      CreateTaskCompletionClosure(data_type),
      base::BindOnce(&BrowsingDataRemoverImpl::OnTaskComplete, GetWeakPtr(),
                     data_type, base::TimeTicks::Now())));
}

void BrowsingDataRemoverImpl::RecordUnfinishedSubTasks() {
  DCHECK(!pending_sub_tasks_.empty());
  for (TracingDataType task : pending_sub_tasks_) {
    UMA_HISTOGRAM_ENUMERATION(
        "History.ClearBrowsingData.Duration.SlowTasks180s", task);
  }
}

void BrowsingDataRemoverImpl::ClearClientHintCacheAndReply(
    const url::Origin& origin,
    base::OnceClosure callback) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowsingDataRemoverImpl::ClearClientHintCacheAndReplyImpl,
                     GetWeakPtr(), origin, std::move(callback)));
}

void BrowsingDataRemoverImpl::ClearClientHintCacheAndReplyImpl(
    const url::Origin& origin,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback);
  ClientHintsControllerDelegate* delegate =
      browser_context_->GetClientHintsControllerDelegate();
  if (delegate) {
    delegate->PersistClientHints(origin,
                                 /*parent_rfh=*/nullptr,
                                 /*client_hints=*/{});
  }
  std::move(callback).Run();
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
