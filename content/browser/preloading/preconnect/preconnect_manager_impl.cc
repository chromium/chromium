// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preconnect/preconnect_manager_impl.h"

#include <ranges>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_util.h"
#include "content/browser/preloading/proxy_lookup_client_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/preconnect_request.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/request_priority.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

const bool kAllowCredentialsOnPreconnectByDefault = true;

std::unique_ptr<PreconnectManager> PreconnectManager::Create(
    base::WeakPtr<PreconnectManager::Delegate> delegate,
    content::BrowserContext* browser_context) {
  return std::make_unique<PreconnectManagerImpl>(std::move(delegate),
                                                 browser_context);
}

PreconnectedRequestStats::PreconnectedRequestStats(const url::Origin& origin,
                                                   bool was_preconnected)
    : origin(origin), was_preconnected(was_preconnected) {}

PreconnectedRequestStats::PreconnectedRequestStats(
    const PreconnectedRequestStats& other) = default;
PreconnectedRequestStats::~PreconnectedRequestStats() = default;

PreconnectStats::PreconnectStats(const GURL& url)
    : url(url), start_time(base::TimeTicks::Now()) {}
PreconnectStats::~PreconnectStats() = default;

PreresolveInfo::PreresolveInfo(const GURL& url, size_t count)
    : url(url),
      queued_count(count),
      inflight_count(0),
      was_canceled(false),
      stats(std::make_unique<PreconnectStats>(url)) {}

PreresolveInfo::~PreresolveInfo() = default;

PreresolveJob::PreresolveJob(
    const GURL& url,
    int num_sockets,
    bool allow_credentials,
    net::NetworkAnonymizationKey network_anonymization_key,
    net::NetworkTrafficAnnotationTag traffic_annotation_tag,
    std::optional<content::StoragePartitionConfig> storage_partition_config,
    std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
    mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
        connection_change_observer_client,
    PreresolveInfo* info,
    base::UnguessableToken network_restrictions_id)
    : url(url),
      num_sockets(num_sockets),
      allow_credentials(allow_credentials),
      network_anonymization_key(std::move(network_anonymization_key)),
      traffic_annotation_tag(std::move(traffic_annotation_tag)),
      storage_partition_config(std::move(storage_partition_config)),
      keepalive_config(std::move(keepalive_config)),
      connection_change_observer_client(
          std::move(connection_change_observer_client)),
      info(info),
      network_restrictions_id(network_restrictions_id) {
  CHECK_GE(num_sockets, 0);

  CHECK(!this->network_anonymization_key.IsEmpty() ||
        !net::NetworkAnonymizationKey::IsPartitioningEnabled());
}

PreresolveJob::PreresolveJob(
    content::PreconnectRequest preconnect_request,
    PreresolveInfo* info,
    net::NetworkTrafficAnnotationTag traffic_annotation_tag)
    : PreresolveJob(preconnect_request.origin.GetURL(),
                    preconnect_request.num_sockets,
                    preconnect_request.allow_credentials,
                    std::move(preconnect_request.network_anonymization_key),
                    traffic_annotation_tag,
                    /*storage_partition_config=*/std::nullopt,
                    std::nullopt,
                    mojo::NullRemote(),
                    info,
                    preconnect_request.network_restrictions_id) {}

PreresolveJob::PreresolveJob(PreresolveJob&& other) = default;
PreresolveJob::~PreresolveJob() = default;

PreconnectManagerImpl::PreconnectManagerImpl(
    base::WeakPtr<Delegate> delegate,
    content::BrowserContext* browser_context)
    : delegate_(std::move(delegate)), browser_context_(browser_context) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);
  CHECK(browser_context_, base::NotFatalUntil::M159);
  CheckAndUpdatePreconnectEnabledOnUI();
  if (base::FeatureList::IsEnabled(features::kPreconnectOffTheMainThread)) {
    omt_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE});
    cross_thread_network_context_ =
        static_cast<StoragePartitionImpl*>(
            browser_context_->GetDefaultStoragePartition())
            ->CreateCrossThreadNetworkContext();
  }
}

PreconnectManagerImpl::~PreconnectManagerImpl() = default;

base::WeakPtr<PreconnectManager> PreconnectManagerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PreconnectManagerImpl::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  network_context_for_testing_ = network_context;
}

void PreconnectManagerImpl::SetCrossThreadNetworkContextForTesting(
    scoped_refptr<CrossThreadNetworkContext> context) {
  if (!omt_task_runner_) {
    omt_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE});
  }
  cross_thread_network_context_ = std::move(context);
}

void PreconnectManagerImpl::SetObserverForTesting(Observer* observer) {
  observer_ = observer;
}

void PreconnectManagerImpl::Start(
    const GURL& url,
    std::vector<content::PreconnectRequest> requests,
    net::NetworkTrafficAnnotationTag traffic_annotation) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);
  if (!CheckAndUpdatePreconnectEnabledOnUI()) {
    return;
  }
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  auto [it, inserted] = preresolve_info_.try_emplace(url);
  if (inserted) {
    it->second = std::make_unique<PreresolveInfo>(url, requests.size());
  } else {
    it->second->queued_count += requests.size();
  }
  PreresolveInfo* const info = it->second.get();

  for (auto& request : requests) {
    PreresolveJobId job_id =
        preresolve_jobs_.Add(std::make_unique<PreresolveJob>(
            std::move(request), info, traffic_annotation));
    queued_jobs_.push_back(job_id);
  }

  TryToLaunchPreresolveJobs();
}

void PreconnectManagerImpl::StartPreresolveHost(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const content::StoragePartitionConfig* storage_partition_config,
    const base::UnguessableToken& network_restrictions_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);
  CHECK(!network_restrictions_id.is_empty(), base::NotFatalUntil::M165);
  if (!CheckAndUpdatePreconnectEnabledOnUI()) {
    return;
  }
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  PreresolveJobId job_id = preresolve_jobs_.Add(std::make_unique<PreresolveJob>(
      url.DeprecatedGetOriginAsURL(), 0, kAllowCredentialsOnPreconnectByDefault,
      network_anonymization_key, traffic_annotation,
      base::OptionalFromPtr(storage_partition_config), std::nullopt,
      mojo::NullRemote(), nullptr, network_restrictions_id));
  queued_jobs_.push_front(job_id);

  TryToLaunchPreresolveJobs();
}

void PreconnectManagerImpl::StartPreresolveHosts(
    const std::vector<GURL>& urls,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const content::StoragePartitionConfig* storage_partition_config,
    const base::UnguessableToken& network_restrictions_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);
  CHECK(!network_restrictions_id.is_empty(), base::NotFatalUntil::M165);
  if (!CheckAndUpdatePreconnectEnabledOnUI()) {
    return;
  }
  // Push jobs in front of the queue due to higher priority.
  for (const GURL& url : std::views::reverse(urls)) {
    if (!url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    PreresolveJobId job_id =
        preresolve_jobs_.Add(std::make_unique<PreresolveJob>(
            url.DeprecatedGetOriginAsURL(), 0,
            kAllowCredentialsOnPreconnectByDefault, network_anonymization_key,
            traffic_annotation, base::OptionalFromPtr(storage_partition_config),
            std::nullopt, mojo::NullRemote(), nullptr,
            network_restrictions_id));
    queued_jobs_.push_front(job_id);
  }

  TryToLaunchPreresolveJobs();
}

void PreconnectManagerImpl::StartPreconnectUrl(
    const GURL& url,
    bool allow_credentials,
    net::NetworkAnonymizationKey network_anonymization_key,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const content::StoragePartitionConfig* storage_partition_config,
    const base::UnguessableToken& network_restrictions_id,
    std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
    mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
        connection_change_observer_client) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    StartPreconnectUrlOnUi(url, allow_credentials,
                           std::move(network_anonymization_key),
                           traffic_annotation, storage_partition_config,
                           network_restrictions_id, std::move(keepalive_config),
                           std::move(connection_change_observer_client));
  } else {
    // Off-the-main-thread preconnect only supports the default storage
    // partition because `cross_thread_network_context_` is bound to the default
    // partition. Require `storage_partition_config` to be nullptr so
    // `StoragePartitionConfig` is never accessed off the UI thread.
    CHECK(!storage_partition_config);
    StartPreconnectUrlOnNonUi(url, allow_credentials,
                              std::move(network_anonymization_key),
                              traffic_annotation, network_restrictions_id,
                              std::move(keepalive_config),
                              std::move(connection_change_observer_client));
  }
}

void PreconnectManagerImpl::StartPreconnectUrlOnUi(
    const GURL& url,
    bool allow_credentials,
    net::NetworkAnonymizationKey network_anonymization_key,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const content::StoragePartitionConfig* storage_partition_config,
    const base::UnguessableToken& network_restrictions_id,
    std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
    mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
        connection_change_observer_client) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!CheckAndUpdatePreconnectEnabledOnUI()) {
    return;
  }
  CHECK(!network_restrictions_id.is_empty(), base::NotFatalUntil::M165);

  if (base::FeatureList::IsEnabled(features::kPreconnectOffTheMainThread) ||
      base::FeatureList::IsEnabled(
          features::kPreconnectManagerDirectFastPath)) {
    PreconnectUrl(url.DeprecatedGetOriginAsURL(), /*num_sockets=*/1,
                  allow_credentials, network_anonymization_key,
                  traffic_annotation, storage_partition_config,
                  network_restrictions_id, std::move(keepalive_config),
                  std::move(connection_change_observer_client));
    return;
  }

  PreresolveJobId job_id = preresolve_jobs_.Add(std::make_unique<PreresolveJob>(
      url.DeprecatedGetOriginAsURL(), 1, allow_credentials,
      std::move(network_anonymization_key), traffic_annotation,
      base::OptionalFromPtr(storage_partition_config),
      std::move(keepalive_config), std::move(connection_change_observer_client),
      nullptr, network_restrictions_id));
  queued_jobs_.push_front(job_id);

  TryToLaunchPreresolveJobs();
}

void PreconnectManagerImpl::StartPreconnectUrlOnNonUi(
    const GURL& url,
    bool allow_credentials,
    net::NetworkAnonymizationKey network_anonymization_key,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const base::UnguessableToken& network_restrictions_id,
    std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
    mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
        connection_change_observer_client) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // TODO(crbug.com/556493449): `is_preconnect_enabled_` is only refreshed on
  // the UI thread when UI-thread preconnect APIs are called. If preconnect is
  // initiated purely from non-UI threads and the delegate's policy changes
  // dynamically, this could read a stale value. Consider observing delegate
  // policy changes or periodically syncing from the UI thread.
  if (!is_preconnect_enabled_.load(std::memory_order_relaxed)) {
    return;
  }
  CHECK(!network_restrictions_id.is_empty(), base::NotFatalUntil::M165);
  CHECK(!network_anonymization_key.IsEmpty() ||
        !net::NetworkAnonymizationKey::IsPartitioningEnabled());
  if (!base::FeatureList::IsEnabled(features::kPreconnectOffTheMainThread)) {
    return;
  }
  if (!cross_thread_network_context_) {
    return;
  }
  CHECK(omt_task_runner_);

  omt_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CrossThreadNetworkContext::PreconnectSockets,
          cross_thread_network_context_, /*num_streams=*/1,
          url.DeprecatedGetOriginAsURL(),
          allow_credentials ? network::mojom::CredentialsMode::kInclude
                            : network::mojom::CredentialsMode::kOmit,
          network_anonymization_key, network_restrictions_id,
          net::MutableNetworkTrafficAnnotationTag(traffic_annotation),
          std::move(keepalive_config),
          std::move(connection_change_observer_client)));
}

void PreconnectManagerImpl::Stop(const GURL& url) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);
  auto it = preresolve_info_.find(url);
  if (it == preresolve_info_.end()) {
    return;
  }

  it->second->was_canceled = true;
}

void PreconnectManagerImpl::PreconnectUrl(
    const GURL& url,
    int num_sockets,
    bool allow_credentials,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const content::StoragePartitionConfig* storage_partition_config,
    const base::UnguessableToken& network_restrictions_id,
    std::optional<net::ConnectionKeepAliveConfig> keepalive_config,
    mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
        connection_change_observer_client) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!network_restrictions_id.is_empty(), base::NotFatalUntil::M165);
  CHECK(url.DeprecatedGetOriginAsURL() == url, base::NotFatalUntil::M159);
  CHECK(url.SchemeIsHTTPOrHTTPS(), base::NotFatalUntil::M159);
  CHECK(!network_anonymization_key.IsEmpty() ||
        !net::NetworkAnonymizationKey::IsPartitioningEnabled());
  if (observer_) {
    observer_->OnPreconnectUrl(url, num_sockets, allow_credentials,
                               network_anonymization_key,
                               connection_change_observer_client);
  }

  network::mojom::NetworkContext* network_context =
      GetNetworkContext(storage_partition_config);
  CHECK(network_context);

  if (num_sockets > 1 &&
      base::FeatureList::IsEnabled(
          features::kLoadingPredictorLimitPreconnectSocketCount)) {
    // Adjust the number of socket here because LoadingPredictor is the only
    // call site that sets `num_sockets` to a non-one value.
    num_sockets = 1;
  }

  // TODO(crbug.com/406022435): pass the actual `keepalive_config` from the
  // caller.
  network_context->PreconnectSockets(
      num_sockets, url,
      allow_credentials ? network::mojom::CredentialsMode::kInclude
                        : network::mojom::CredentialsMode::kOmit,
      network_anonymization_key, network_restrictions_id,
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation),
      std::move(keepalive_config),
      std::move(connection_change_observer_client));
}

std::unique_ptr<ResolveHostClientImpl> PreconnectManagerImpl::PreresolveUrl(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const content::StoragePartitionConfig* storage_partition_config,
    const base::UnguessableToken& network_restrictions_id,
    net::RequestPriority initial_priority,
    ResolveHostCallback callback) const {
  CHECK(url.DeprecatedGetOriginAsURL() == url, base::NotFatalUntil::M159);
  // TODO(crbug.com/573950459): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  auto* network_context = GetNetworkContext(storage_partition_config);

  return std::make_unique<ResolveHostClientImpl>(
      url, network_anonymization_key, network_restrictions_id,
      std::move(callback), network_context, initial_priority);
}

void PreconnectManagerImpl::LookupProxyForUrl(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const content::StoragePartitionConfig* storage_partition_config,
    ProxyLookupClientImpl::ProxyLookupCallback callback) const {
  CHECK(url.DeprecatedGetOriginAsURL() == url, base::NotFatalUntil::M159);
  // TODO(crbug.com/571031313): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  auto* network_context = GetNetworkContext(storage_partition_config);

  ProxyLookupClientImpl::CreateAndStart(url, network_anonymization_key,
                                        std::move(callback), network_context);
}

void PreconnectManagerImpl::TryToLaunchPreresolveJobs() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);

  while (!queued_jobs_.empty() &&
         inflight_preresolves_count_ < kMaxInflightPreresolves) {
    auto job_id = queued_jobs_.front();
    queued_jobs_.pop_front();
    PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
    CHECK(job, base::NotFatalUntil::M159);

    PreresolveInfo* info = job->info;

    if (!(info && info->was_canceled)) {
      // This is used to avoid issuing DNS requests when a fixed proxy
      // configuration is in place, which improves efficiency, and is also
      // important if the unproxied DNS may contain incorrect entries.
      LookupProxyForUrl(
          job->url, job->network_anonymization_key,
          base::OptionalToPtr(job->storage_partition_config),
          base::BindOnce(&PreconnectManagerImpl::OnProxyLookupFinished,
                         weak_factory_.GetWeakPtr(), job_id));
      if (info) {
        ++info->inflight_count;
        if (delegate_) {
          delegate_->PreconnectInitiated(info->url, job->url);
        }
      }
      ++inflight_preresolves_count_;
    } else {
      preresolve_jobs_.Remove(job_id);
    }

    if (info) {
      CHECK_LE(1u, info->queued_count, base::NotFatalUntil::M159);
      --info->queued_count;
      if (info->is_done()) {
        AllPreresolvesForUrlFinished(info);
      }
    }
  }
}

void PreconnectManagerImpl::OnPreresolveFinished(PreresolveJobId job_id,
                                                 bool success) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);
  PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
  CHECK(job, base::NotFatalUntil::M159);

  if (observer_) {
    observer_->OnPreresolveFinished(job->url, job->network_anonymization_key,
                                    job->connection_change_observer_client,
                                    success);
  }

  job->resolve_host_client = nullptr;
  FinishPreresolveJob(job_id, success);
}

void PreconnectManagerImpl::OnProxyLookupFinished(PreresolveJobId job_id,
                                                  bool success) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);
  PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
  // TODO(crbug.com/573950459): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK(job);

  if (observer_) {
    observer_->OnProxyLookupFinished(job->url, job->network_anonymization_key,
                                     success);
  }

  if (success) {
    FinishPreresolveJob(job_id, success);
  } else {
    net::RequestPriority initial_priority =
        job->keepalive_config.has_value()
            ? job->keepalive_config->preconnect_priority
            : net::RequestPriority::IDLE;
    job->resolve_host_client = PreresolveUrl(
        job->url, job->network_anonymization_key,
        base::OptionalToPtr(job->storage_partition_config),
        job->network_restrictions_id, initial_priority,
        base::BindOnce(&PreconnectManagerImpl::OnPreresolveFinished,
                       weak_factory_.GetWeakPtr(), job_id));
  }
}

void PreconnectManagerImpl::FinishPreresolveJob(PreresolveJobId job_id,
                                                bool success) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M159);
  PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
  CHECK(job, base::NotFatalUntil::M159);

  bool need_preconnect = success && job->need_preconnect();
  if (need_preconnect) {
    PreconnectUrl(job->url, job->num_sockets, job->allow_credentials,
                  job->network_anonymization_key, job->traffic_annotation_tag,
                  base::OptionalToPtr(job->storage_partition_config),
                  job->network_restrictions_id,
                  std::move(job->keepalive_config),
                  std::move(job->connection_change_observer_client));
  }

  PreresolveInfo* info = job->info;
  if (info) {
    info->stats->requests_stats.emplace_back(url::Origin::Create(job->url),
                                             need_preconnect);
  }
  preresolve_jobs_.Remove(job_id);
  --inflight_preresolves_count_;
  if (info) {
    CHECK_LE(1u, info->inflight_count, base::NotFatalUntil::M159);
    --info->inflight_count;
  }
  if (info && info->is_done()) {
    AllPreresolvesForUrlFinished(info);
  }
  TryToLaunchPreresolveJobs();
}

void PreconnectManagerImpl::AllPreresolvesForUrlFinished(PreresolveInfo* info) {
  CHECK(info, base::NotFatalUntil::M159);
  CHECK(info->is_done(), base::NotFatalUntil::M159);
  auto it = preresolve_info_.find(info->url);
  CHECK(it != preresolve_info_.end());
  CHECK(info == it->second.get(), base::NotFatalUntil::M159);
  if (delegate_) {
    delegate_->PreconnectFinished(std::move(info->stats));
  }
  preresolve_info_.erase(it);
}

network::mojom::NetworkContext* PreconnectManagerImpl::GetNetworkContext(
    const content::StoragePartitionConfig* storage_partition_config) const {
  if (network_context_for_testing_) {
    return network_context_for_testing_;
  }

  auto* network_context =
      browser_context_
          ->GetStoragePartition(
              storage_partition_config
                  ? *storage_partition_config
                  : content::StoragePartitionConfig::CreateDefault(
                        browser_context_))
          ->GetNetworkContext();
  CHECK(network_context, base::NotFatalUntil::M159);
  return network_context;
}

bool PreconnectManagerImpl::CheckAndUpdatePreconnectEnabledOnUI() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!delegate_) {
    is_preconnect_enabled_.store(false, std::memory_order_relaxed);
    return false;
  }
  bool is_enabled = delegate_->IsPreconnectEnabled();
  is_preconnect_enabled_.store(is_enabled, std::memory_order_relaxed);
  return is_enabled;
}

}  // namespace content
