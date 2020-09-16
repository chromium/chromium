// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_job.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_disk_cache_ops.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/browser/appcache/appcache_policy.h"
#include "content/browser/appcache/appcache_response_info.h"
#include "content/browser/appcache/appcache_update_url_fetcher.h"
#include "content/browser/appcache/appcache_update_url_loader_request.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "storage/browser/quota/padding_key.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

const int kAppCacheFetchBufferSize = 32768;
const size_t kMaxConcurrentUrlFetches = 2;

enum class ResourceCheck {
  kValid,
  kInvalid,
  kCorrupt,
};

std::string FormatUrlErrorMessage(
      const char* format, const GURL& url,
      AppCacheUpdateJob::ResultType error,
      int response_code) {
    // Show the net response code if we have one.
    int code = response_code;
    if (error != AppCacheUpdateJob::SERVER_ERROR)
      code = static_cast<int>(error);
    return base::StringPrintf(format, code, url.spec().c_str());
}

bool IsEvictableError(AppCacheUpdateJob::ResultType result,
                      const blink::mojom::AppCacheErrorDetails& details) {
  switch (result) {
    case AppCacheUpdateJob::DB_ERROR:
    case AppCacheUpdateJob::DISKCACHE_ERROR:
    case AppCacheUpdateJob::QUOTA_ERROR:
    case AppCacheUpdateJob::NETWORK_ERROR:
    case AppCacheUpdateJob::CANCELLED_ERROR:
      return false;

    case AppCacheUpdateJob::REDIRECT_ERROR:
    case AppCacheUpdateJob::SERVER_ERROR:
    case AppCacheUpdateJob::SECURITY_ERROR:
      return true;

    case AppCacheUpdateJob::MANIFEST_ERROR:
      return details.reason ==
             blink::mojom::AppCacheErrorReason::APPCACHE_SIGNATURE_ERROR;

    default:
      NOTREACHED();
      return true;
  }
}

ResourceCheck CanUseExistingResource(
    const net::HttpResponseInfo* http_info,
    AppCacheUpdateMetricsRecorder& update_metrics) {
  update_metrics.IncrementExistingResourceCheck();

  if (!http_info->headers)
    return ResourceCheck::kInvalid;

  base::Time request_time = http_info->request_time;
  base::Time response_time = http_info->response_time;

  // The logic below works around the following confluence of problems.
  //
  // 1) If a cached response contains a Last-Modified header,
  // AppCacheUpdateJob::URLFetcher::AddConditionalHeaders() adds an
  // If-Modified-Since header, so the server may return an HTTP 304 Not Modified
  // response. AppCacheUpdateJob::HandleResourceFetchCompleted() reuses the
  // existing cache entry when a 304 is received, even though the HTTP
  // specification mandates updating the cached headers with the headers in the
  // 304 response.
  //
  // This deviation from the HTTP specification is Web-observable when AppCache
  // resources are served with Last-Modified and Cache-Control: max-age headers.
  // Specifically, if a server returns a 304 with a Cache-Control: max-age
  // header, the response stored in AppCache should be updated to reflect the
  // new cache expiration time. Instead, Chrome ignores all the headers in the
  // 304 response, so the Cache-Control: max-age directive is discarded.
  //
  // In other words, once a cached resource's lifetime expires, 304 responses
  // won't refresh its lifetime. Chrome gets stuck in a cycle where it sends
  // If-Modified-Since requests, the server responds with 304, and the response
  // headers are discarded.
  //
  // 2) The implementation of
  // AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveResponse() introduced
  // in https://crrev.com/c/599359 did not populate |request_time| and
  // |response_time|. When the Network Service was enabled, caches got populated
  // with the default value of base::Time, which is the Windows epoch. So,
  // cached entries with max-age values below ~40 years will require
  // re-validation. https://crrev.com/c/1636266 fixed the cache population bug,
  // but did not address the incorrect times that have already been written to
  // users' disks.
  //
  // The 1st problem, on its own, hasn't had a large impact. This is likely
  // because we have been advising sites to set max-age=31536000 (~1 year) for
  // immutable resources, and most AppCache caches have been getting evicted
  // before the entries' max-age expired. However, the 2nd problem caused us to
  // create a large number of expired cache entries, and the unnecessary
  // If-Modified-Since requests are causing noticeable levels of traffic.
  //
  // There is currently a Finch-controlled kAppCacheCorruptionRecoveryFeature
  // that is turned on for some users.  Once this has been rolled out fully,
  // then this workaround below can be removed.
  //
  // The logic below is a workaround to prevent refetching these corrupted
  // cache entries while this kAppCacheCorruptionRecoveryFeature is rolled out
  // to all users.  We'll consider all cache entries with invalid times to have
  // been created on Tue, Jun 30 2020.  This date has been moved several times
  // to prevent resources that haven't yet expired from being refetched all at
  // once.
  //
  bool found_corruption = false;
  static constexpr base::Time::Exploded kInvalidTimePlaceholderExploded = {
      2020, 6, 2, 30, 0, 0, 0, 0};
  if (request_time.is_null()) {
    bool conversion_succeeded = base::Time::FromUTCExploded(
        kInvalidTimePlaceholderExploded, &request_time);
    DCHECK(conversion_succeeded);
    found_corruption = true;
  }
  if (response_time.is_null()) {
    bool conversion_succeeded = base::Time::FromUTCExploded(
        kInvalidTimePlaceholderExploded, &response_time);
    DCHECK(conversion_succeeded);
    found_corruption = true;
  }

  if (found_corruption) {
    update_metrics.IncrementExistingResourceCorrupt();
    if (base::FeatureList::IsEnabled(kAppCacheCorruptionRecoveryFeature)) {
      update_metrics.IncrementExistingResourceCorruptionRecovery();
      return ResourceCheck::kCorrupt;
    }
  } else {
    update_metrics.IncrementExistingResourceNotCorrupt();
  }

  // Record the max age / expiry value on this entry in days.
  net::HttpResponseHeaders::FreshnessLifetimes lifetimes =
      http_info->headers->GetFreshnessLifetimes(response_time);
  base::UmaHistogramCounts10000("appcache.UpdateJob.ResourceFreshness",
                                lifetimes.freshness.InDays());

  // Check HTTP caching semantics based on max-age and expiration headers.
  if (http_info->headers->RequiresValidation(request_time, response_time,
                                             base::Time::Now())) {
    return ResourceCheck::kInvalid;
  }

  // Responses with a "vary" header generally get treated as expired,
  // but we special case the "Origin" header since we know it's invariant.
  // Also, content decoding is handled by the network library, the appcache
  // stores decoded response bodies, so we can safely ignore varying on
  // the "Accept-Encoding" header.
  std::string value;
  size_t iter = 0;
  while (http_info->headers->EnumerateHeader(&iter, "vary", &value)) {
    if (!base::EqualsCaseInsensitiveASCII(value, "Accept-Encoding") &&
        !base::EqualsCaseInsensitiveASCII(value, "Origin")) {
      return ResourceCheck::kInvalid;
    }
  }

  update_metrics.IncrementExistingResourceReused();
  return ResourceCheck::kValid;
}

void EmptyCompletionCallback(int result) {}

int64_t ComputeAppCacheResponsePadding(const GURL& response_url,
                                       const GURL& manifest_url) {
  // All cross-origin resources should have their size padded in response to
  // queries regarding quota usage.
  if (response_url.GetOrigin() == manifest_url.GetOrigin())
    return 0;

  return storage::ComputeResponsePadding(
      response_url.spec(), storage::GetDefaultPaddingKey(),
      /*has_metadata=*/false, /*loaded_with_credentials=*/false,
      net::HttpRequestHeaders::kGetMethod);
}

}  // namespace

const base::Feature kAppCacheCorruptionRecoveryFeature{
    "AppCacheCorruptionRecovery", base::FEATURE_DISABLED_BY_DEFAULT};

// Helper class for collecting hosts per frontend when sending notifications
// so that only one notification is sent for all hosts using the same frontend.
class HostNotifier {
 public:
  // Caller is responsible for ensuring there will be no duplicate hosts.
  void AddHost(AppCacheHost* host) {
    hosts_to_notify_.insert(host->frontend());
  }

  void AddHosts(const std::set<AppCacheHost*>& hosts) {
    for (AppCacheHost* host : hosts)
      AddHost(host);
  }

  void SendNotifications(blink::mojom::AppCacheEventID event_id) {
    for (auto* frontend : hosts_to_notify_)
      frontend->EventRaised(event_id);
  }

  void SendProgressNotifications(const GURL& url,
                                 int num_total,
                                 int num_complete) {
    for (auto* frontend : hosts_to_notify_)
      frontend->ProgressEventRaised(url, num_total, num_complete);
  }

  void SendErrorNotifications(
      const blink::mojom::AppCacheErrorDetails& details) {
    DCHECK(!details.message.empty());
    for (auto* frontend : hosts_to_notify_)
      frontend->ErrorEventRaised(details.Clone());
  }

  void SendLogMessage(const std::string& message) {
    for (auto* frontend : hosts_to_notify_)
      frontend->LogMessage(blink::mojom::ConsoleMessageLevel::kWarning,
                           message);
  }

 private:
  std::set<blink::mojom::AppCacheFrontend*> hosts_to_notify_;
};
AppCacheUpdateJob::UrlToFetch::UrlToFetch(const GURL& url,
                                          bool checked,
                                          AppCacheResponseInfo* info)
    : url(url),
      storage_checked(checked),
      existing_response_info(info) {
}

AppCacheUpdateJob::UrlToFetch::UrlToFetch(const UrlToFetch& other) = default;

AppCacheUpdateJob::UrlToFetch::~UrlToFetch() = default;

AppCacheUpdateJob::AppCacheUpdateJob(AppCacheServiceImpl* service,
                                     AppCacheGroup* group)
    : service_(service),
      manifest_url_(group->manifest_url()),
      cached_manifest_parser_version_(-1),
      fetched_manifest_parser_version_(-1),
      cached_manifest_scope_(""),
      fetched_manifest_scope_(""),
      refetched_manifest_scope_(""),
      group_(group),
      update_type_(UNKNOWN_TYPE),
      internal_state_(AppCacheUpdateJobState::FETCH_MANIFEST),
      doing_full_update_check_(false),
      master_entries_completed_(0),
      url_fetches_completed_(0),
      manifest_fetcher_(nullptr),
      manifest_has_valid_mime_type_(false),
      stored_state_(UNSTORED),
      storage_(service->storage()) {
  service_->AddObserver(this);
  is_origin_trial_required_ =
      service_->appcache_policy()->IsOriginTrialRequiredForAppCache();
}

AppCacheUpdateJob::~AppCacheUpdateJob() {
  update_metrics_.RecordFinalInternalState(internal_state_);
  if (service_)
    service_->RemoveObserver(this);
  if (internal_state_ != AppCacheUpdateJobState::COMPLETED)
    Cancel();

  DCHECK(!inprogress_cache_.get());
  DCHECK(pending_master_entries_.empty());

  // No fetcher may outlive the job.
  CHECK(!manifest_fetcher_);
  CHECK(pending_url_fetches_.empty());
  CHECK(master_entry_fetches_.empty());

  if (group_)
    group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);

  // Upload accumulated update job metrics to UMA.  We expect at this point the
  // update job has finalized its work and no external references exist back to
  // it that may trigger more metrics to be logged.  Especially,
  // SetUpdateAppCacheStatus() causes the cache group's update job reference to
  // be set to nullptr.
  update_metrics_.UploadMetrics();
}

void AppCacheUpdateJob::StartUpdate(AppCacheHost* host,
                                    const GURL& new_master_resource) {
  DCHECK_EQ(group_->update_job(), this);
  DCHECK(!group_->is_obsolete());

  bool is_new_pending_master_entry = false;
  if (!new_master_resource.is_empty()) {
    DCHECK_EQ(new_master_resource, host->pending_master_entry_url());
    DCHECK(!new_master_resource.has_ref());
    DCHECK_EQ(new_master_resource.GetOrigin(), manifest_url_.GetOrigin());

    if (base::Contains(failed_master_entries_, new_master_resource))
      return;

    // Cannot add more to this update if already terminating.
    if (IsTerminating()) {
      group_->QueueUpdate(host, new_master_resource);
      return;
    }

    auto emplace_result = pending_master_entries_.emplace(
        new_master_resource, std::vector<AppCacheHost*>());
    is_new_pending_master_entry = emplace_result.second;
    emplace_result.first->second.push_back(host);
    host->AddObserver(this);
  }

  // Notify host (if any) if already checking or downloading.
  AppCacheGroup::UpdateAppCacheStatus update_status = group_->update_status();
  if (update_status == AppCacheGroup::CHECKING ||
      update_status == AppCacheGroup::DOWNLOADING) {
    if (host) {
      NotifySingleHost(host,
                       blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
      if (update_status == AppCacheGroup::DOWNLOADING)
        NotifySingleHost(
            host, blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);

      // Add to fetch list or an existing entry if already fetched.
      if (!new_master_resource.is_empty()) {
        AddMasterEntryToFetchList(host, new_master_resource,
                                  is_new_pending_master_entry);
      }
    }
    return;
  }

  // Begin update process for the group.
  MadeProgress();
  group_->SetUpdateAppCacheStatus(AppCacheGroup::CHECKING);
  if (group_->HasCache()) {
    base::TimeDelta kFullUpdateInterval = base::TimeDelta::FromHours(24);
    update_type_ = UPGRADE_ATTEMPT;
    AppCache* cache = group_->newest_complete_cache();
    cached_manifest_parser_version_ = cache->manifest_parser_version();
    cached_manifest_scope_ = cache->manifest_scope();
    base::TimeDelta time_since_last_check =
        base::Time::Now() - group_->last_full_update_check_time();
    doing_full_update_check_ = time_since_last_check > kFullUpdateInterval;
    NotifyAllAssociatedHosts(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
  } else {
    update_type_ = CACHE_ATTEMPT;
    doing_full_update_check_ = true;
    DCHECK(host);
    NotifySingleHost(host,
                     blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
  }

  if (!new_master_resource.is_empty()) {
    AddMasterEntryToFetchList(host, new_master_resource,
                              is_new_pending_master_entry);
  }

  BrowserThread::PostBestEffortTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(&AppCacheUpdateJob::FetchManifest,
                     weak_factory_.GetWeakPtr()));
}

std::unique_ptr<AppCacheResponseWriter>
AppCacheUpdateJob::CreateResponseWriter() {
  std::unique_ptr<AppCacheResponseWriter> writer =
      storage_->CreateResponseWriter(manifest_url_);
  stored_response_ids_.push_back(writer->response_id());
  return writer;
}

void AppCacheUpdateJob::HandleCacheFailure(
    const blink::mojom::AppCacheErrorDetails& error_details,
    ResultType result,
    const GURL& failed_resource_url) {
  // 7.9.4 cache failure steps 2-8.
  DCHECK(internal_state_ != AppCacheUpdateJobState::CACHE_FAILURE);
  DCHECK(!error_details.message.empty());
  DCHECK(result != UPDATE_OK);
  internal_state_ = AppCacheUpdateJobState::CACHE_FAILURE;
  CancelAllUrlFetches();
  CancelAllMasterEntryFetches(error_details);
  NotifyAllError(error_details);
  DiscardInprogressCache();
  internal_state_ = AppCacheUpdateJobState::COMPLETED;

  if (update_type_ == CACHE_ATTEMPT ||
      !IsEvictableError(result, error_details) ||
      service_->storage() != storage_) {
    DeleteSoon();
    return;
  }

  if (group_->first_evictable_error_time().is_null()) {
    group_->set_first_evictable_error_time(base::Time::Now());
    storage_->StoreEvictionTimes(group_);
    DeleteSoon();
    return;
  }

  base::TimeDelta kMaxEvictableErrorDuration = base::TimeDelta::FromDays(14);
  base::TimeDelta error_duration =
      base::Time::Now() - group_->first_evictable_error_time();
  if (error_duration > kMaxEvictableErrorDuration) {
    // Break the connection with the group prior to calling
    // DeleteAppCacheGroup, otherwise that method would delete |this|
    // and we need the stack to unwind prior to deletion.
    group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);
    group_ = nullptr;
    service_->DeleteAppCacheGroup(manifest_url_,
                                  base::BindOnce(EmptyCompletionCallback));
  }

  DeleteSoon();  // To unwind the stack prior to deletion.
}

void AppCacheUpdateJob::FetchManifest() {
  DCHECK(!manifest_fetcher_);
  manifest_fetcher_ = std::make_unique<URLFetcher>(
      manifest_url_, URLFetcher::FetchType::kManifest, this,
      kAppCacheFetchBufferSize);

  // Maybe load the cached headers to make a conditional request.
  AppCacheEntry* entry =
      (update_type_ == UPGRADE_ATTEMPT)
          ? group_->newest_complete_cache()->GetEntry(manifest_url_)
          : nullptr;
  if (entry && !doing_full_update_check_) {
    // Asynchronously load response info for manifest from newest cache.
    storage_->LoadResponseInfo(manifest_url_, entry->response_id(), this);
    return;
  }
  manifest_fetcher_->Start();
  return;
}

void AppCacheUpdateJob::RefetchManifest() {
  DCHECK(!manifest_fetcher_);
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::REFETCH_MANIFEST);
  DCHECK(manifest_response_info_.get());

  manifest_fetcher_ = std::make_unique<URLFetcher>(
      manifest_url_, URLFetcher::FetchType::kManifestRefetch, this,
      kAppCacheFetchBufferSize);
  manifest_fetcher_->set_existing_response_headers(
      manifest_response_info_->headers.get());
  manifest_fetcher_->Start();
}

void AppCacheUpdateJob::HandleManifestFetchCompleted(URLFetcher* url_fetcher,
                                                     int net_error) {
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::FETCH_MANIFEST);
  DCHECK_EQ(manifest_fetcher_.get(), url_fetcher);

  std::unique_ptr<URLFetcher> manifest_fetcher = std::move(manifest_fetcher_);
  UpdateURLLoaderRequest* request = manifest_fetcher->request();

  int response_code = -1;
  bool is_valid_response_code = false;
  std::string optional_manifest_scope;
  if (net_error == net::OK) {
    response_code = request->GetResponseCode();
    is_valid_response_code = (response_code / 100 == 2);

    std::string mime_type = request->GetMimeType();
    manifest_has_valid_mime_type_ = (mime_type == "text/cache-manifest");

    optional_manifest_scope = request->GetAppCacheAllowedHeader();
  }
  fetched_manifest_scope_ =
      AppCache::GetManifestScope(manifest_url_, optional_manifest_scope);

  if (is_valid_response_code) {
    manifest_data_ = manifest_fetcher->manifest_data();
    manifest_response_info_ =
        std::make_unique<net::HttpResponseInfo>(request->GetResponseInfo());
    if (update_type_ == UPGRADE_ATTEMPT) {
      CheckIfManifestChanged();  // continues asynchronously
    } else {
      HandleFetchedManifestChanged();
    }
    return;
  }

  if (response_code == 304 && update_type_ == UPGRADE_ATTEMPT) {
    if (cached_manifest_parser_version_ >= 2 &&
        fetched_manifest_scope_ == cached_manifest_scope_) {
      HandleFetchedManifestIsUnchanged();
    } else {
      // We don't check if |cached_manifest_parser_version_| is less than 2 here
      // since in that case we didn't add conditional headers and don't expect a
      // 304 response.
      ReadManifestFromCacheAndContinue();
    }
    return;
  }

  if ((response_code == 404 || response_code == 410) &&
      update_type_ == UPGRADE_ATTEMPT) {
    storage_->MakeGroupObsolete(group_, this, response_code);  // async
    return;
  }

  static const char kFormatString[] = "Manifest fetch failed (%d) %s";
  std::string message = FormatUrlErrorMessage(
      kFormatString, manifest_url_, manifest_fetcher->result(), response_code);
  HandleCacheFailure(
      blink::mojom::AppCacheErrorDetails(
          message, blink::mojom::AppCacheErrorReason::APPCACHE_MANIFEST_ERROR,
          manifest_url_, response_code, /*is_cross_origin=*/false),
      manifest_fetcher->result(), GURL());
}

void AppCacheUpdateJob::OnGroupMadeObsolete(AppCacheGroup* group,
                                            bool success,
                                            int response_code) {
  DCHECK(master_entry_fetches_.empty());
  CancelAllMasterEntryFetches(blink::mojom::AppCacheErrorDetails(
      "The cache has been made obsolete, "
      "the manifest file returned 404 or 410",
      blink::mojom::AppCacheErrorReason::APPCACHE_MANIFEST_ERROR, GURL(),
      response_code, /*is_cross_origin=*/false));
  if (success) {
    DCHECK(group->is_obsolete());
    NotifyAllAssociatedHosts(
        blink::mojom::AppCacheEventID::APPCACHE_OBSOLETE_EVENT);
    internal_state_ = AppCacheUpdateJobState::COMPLETED;
    MaybeCompleteUpdate();
  } else {
    // Treat failure to mark group obsolete as a cache failure.
    HandleCacheFailure(
        blink::mojom::AppCacheErrorDetails(
            "Failed to mark the cache as obsolete",
            blink::mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR, GURL(),
            0, /*is_cross_origin=*/false),
        DB_ERROR, GURL());
  }
}

void AppCacheUpdateJob::HandleFetchedManifestIsUnchanged() {
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::FETCH_MANIFEST);

  DCHECK_EQ(update_type_, UPGRADE_ATTEMPT);
  internal_state_ = AppCacheUpdateJobState::NO_UPDATE;

  // We should only ever allow AppCaches to remain unchanged if their parser
  // version is 2 or higher.
  DCHECK_GE(cached_manifest_parser_version_, 2);

  // No manifest update is planned.  Set the fetched manifest parser version
  // and scope to match their initial values.
  fetched_manifest_parser_version_ = cached_manifest_parser_version_;
  fetched_manifest_scope_ = cached_manifest_scope_;

  // Set |refetched_manifest_scope_| to match |fetched_manifest_scope_| so
  // StoreGroupAndCache() can verify the overall state of the
  // AppCacheUpdateJob is correct.
  refetched_manifest_scope_ = fetched_manifest_scope_;

  // Wait for pending master entries to download.
  FetchMasterEntries();
  MaybeCompleteUpdate();  // if not done, run async 7.9.4 step 7 substeps
}

void AppCacheUpdateJob::HandleFetchedManifestChanged() {
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::FETCH_MANIFEST);

  AppCacheManifest manifest;
  if (!ParseManifest(manifest_url_, fetched_manifest_scope_,
                     manifest_data_.data(), manifest_data_.length(),
                     manifest_has_valid_mime_type_
                         ? PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES
                         : PARSE_MANIFEST_PER_STANDARD,
                     manifest)) {
    static const char kFormatString[] = "Failed to parse manifest %s";
    const std::string message = base::StringPrintf(kFormatString,
        manifest_url_.spec().c_str());
    HandleCacheFailure(
        blink::mojom::AppCacheErrorDetails(
            message,
            blink::mojom::AppCacheErrorReason::APPCACHE_SIGNATURE_ERROR, GURL(),
            0, /*is_cross_origin=*/false),
        MANIFEST_ERROR, GURL());
    VLOG(1) << message;
    return;
  }

  if (is_origin_trial_required_ &&
      manifest.token_expires <= base::Time::Now()) {
    static const char kFormatString[] =
        "Invalid or missing manifest origin trial token: %s";
    const std::string message =
        base::StringPrintf(kFormatString, manifest_url_.spec().c_str());
    HandleCacheFailure(
        blink::mojom::AppCacheErrorDetails(
            message, blink::mojom::AppCacheErrorReason::APPCACHE_MANIFEST_ERROR,
            manifest_url_, 0, /*is_cross_origin=*/false),
        MANIFEST_ERROR, GURL());
    VLOG(1) << message;
    return;
  }

  // Ensure the manifest parser version matches what we configured.
  DCHECK_EQ(manifest.parser_version, 2);
  fetched_manifest_parser_version_ = manifest.parser_version;

  // Ensure the manifest scope matches what we configured.
  DCHECK_EQ(manifest.scope, fetched_manifest_scope_);

  // Proceed with update process. Section 7.9.4 steps 8-20.
  internal_state_ = AppCacheUpdateJobState::DOWNLOADING;
  inprogress_cache_ =
      base::MakeRefCounted<AppCache>(storage_, storage_->NewCacheId());
  BuildUrlFileList(manifest);

  inprogress_cache_->InitializeWithManifest(&manifest);

  // Associate all pending master hosts with the newly created cache.
  for (const auto& pair : pending_master_entries_) {
    const std::vector<AppCacheHost*>& hosts = pair.second;
    for (AppCacheHost* host : hosts) {
      host->AssociateIncompleteCache(inprogress_cache_.get(), manifest_url_);
    }
  }

  // Warn about dangerous features being ignored due to the wrong content-type
  // Must be done after associating all pending master hosts.
  if (manifest.did_ignore_intercept_namespaces) {
    std::string message(
        "Ignoring the INTERCEPT section of the application cache manifest "
        "because the content type is not text/cache-manifest");
    LogConsoleMessageToAll(message);
  }
  if (manifest.did_ignore_fallback_namespaces) {
    std::string message(
        "Ignoring out of scope FALLBACK entries of the application cache "
        "manifest because the content-type is not text/cache-manifest");
    LogConsoleMessageToAll(message);
  }

  group_->SetUpdateAppCacheStatus(AppCacheGroup::DOWNLOADING);
  NotifyAllAssociatedHosts(
      blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
  FetchUrls();
  FetchMasterEntries();
  MaybeCompleteUpdate();  // if not done, continues when async fetches complete
}

void AppCacheUpdateJob::HandleResourceFetchCompleted(URLFetcher* url_fetcher,
                                                     int net_error) {
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::DOWNLOADING);

  UpdateURLLoaderRequest* request = url_fetcher->request();
  const GURL& url = request->GetURL();

  auto it = pending_url_fetches_.find(url);
  if (it == pending_url_fetches_.end()) {
    NOTREACHED() << "Entry URL not found in pending_url_fetches_";
    return;
  }
  DCHECK_EQ(it->second.get(), url_fetcher);
  std::unique_ptr<URLFetcher> entry_fetcher = std::move(it->second);
  pending_url_fetches_.erase(it);

  // URLFetcher should only trigger this for resources, even if those entries
  // happen to be manifest entries.
  DCHECK_EQ(entry_fetcher->fetch_type(), URLFetcher::FetchType::kResource);

  int response_code = net_error == net::OK
                          ? request->GetResponseCode()
                          : entry_fetcher->redirect_response_code();

  AppCacheEntry& entry = url_file_list_.find(url)->second;

  NotifyAllProgress(url);
  ++url_fetches_completed_;

  if (response_code / 100 == 2) {
    // Associate storage with the new entry.
    DCHECK(entry_fetcher->response_writer());
    entry.set_response_id(entry_fetcher->response_writer()->response_id());
    entry.SetResponseAndPaddingSizes(
        entry_fetcher->response_writer()->amount_written(),
        ComputeAppCacheResponsePadding(url, manifest_url_));
    if (!inprogress_cache_->AddOrModifyEntry(url, entry))
      duplicate_response_ids_.push_back(entry.response_id());

    // TODO(michaeln): Check for <html manifest=xxx>
    // See http://code.google.com/p/chromium/issues/detail?id=97930
    // if (entry.IsMaster() && !(entry.IsExplicit() || fallback || intercept))
    //   if (!manifestAttribute) skip it

    // Foreign entries will be detected during cache selection.
    // Note: 7.9.4, step 17.9 possible optimization: if resource is HTML or XML
    // file whose root element is an html element with a manifest attribute
    // whose value doesn't match the manifest url of the application cache
    // being processed, mark the entry as being foreign.
  } else if ((entry.IsExplicit() || entry.IsFallback() ||
              entry.IsIntercept()) &&
             response_code == 304 &&
             entry_fetcher->existing_entry().has_response_id()) {
    VLOG(1) << "Request error: " << net_error
            << " response code: " << response_code;
    // Keep the existing response.
    entry.set_response_id(entry_fetcher->existing_entry().response_id());
    entry.SetResponseAndPaddingSizes(
        entry_fetcher->existing_entry().response_size(),
        entry_fetcher->existing_entry().padding_size());
    inprogress_cache_->AddOrModifyEntry(url, entry);
  } else if (entry.IsExplicit() || entry.IsFallback() || entry.IsIntercept()) {
    VLOG(1) << "Request error: " << net_error
            << " response code: " << response_code;
    static const char kFormatString[] = "Resource fetch failed (%d) %s";
    std::string message = FormatUrlErrorMessage(
        kFormatString, url, entry_fetcher->result(), response_code);
    ResultType result = entry_fetcher->result();
    bool is_cross_origin = url.GetOrigin() != manifest_url_.GetOrigin();
    switch (result) {
      case DISKCACHE_ERROR:
        HandleCacheFailure(
            blink::mojom::AppCacheErrorDetails(
                message,
                blink::mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                GURL(), 0, is_cross_origin),
            result, url);
        break;
      case NETWORK_ERROR:
        HandleCacheFailure(
            blink::mojom::AppCacheErrorDetails(
                message,
                blink::mojom::AppCacheErrorReason::APPCACHE_RESOURCE_ERROR, url,
                0, is_cross_origin),
            result, url);
        break;
      default:
        HandleCacheFailure(
            blink::mojom::AppCacheErrorDetails(
                message,
                blink::mojom::AppCacheErrorReason::APPCACHE_RESOURCE_ERROR, url,
                response_code, is_cross_origin),
            result, url);
        break;
    }
    return;
  } else if (response_code == 404 || response_code == 410) {
    VLOG(1) << "Request error: " << net_error
            << " response code: " << response_code;
    // Entry is skipped.  They are dropped from the cache.
  } else if (update_type_ == UPGRADE_ATTEMPT &&
             entry_fetcher->existing_entry().has_response_id()) {
    VLOG(1) << "Request error: " << net_error
            << " response code: " << response_code;
    // Keep the existing response.
    // TODO(michaeln): Not sure this is a good idea. This is spec compliant
    // but the old resource may or may not be compatible with the new contents
    // of the cache. Impossible to know one way or the other.
    entry.set_response_id(entry_fetcher->existing_entry().response_id());
    entry.SetResponseAndPaddingSizes(
        entry_fetcher->existing_entry().response_size(),
        entry_fetcher->existing_entry().padding_size());
    inprogress_cache_->AddOrModifyEntry(url, entry);
  }

  // Fetch another URL now that one request has completed.
  DCHECK(internal_state_ != AppCacheUpdateJobState::CACHE_FAILURE);
  FetchUrls();
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleNewMasterEntryFetchCompleted(
    URLFetcher* url_fetcher,
    int net_error) {
  DCHECK(internal_state_ == AppCacheUpdateJobState::NO_UPDATE ||
         internal_state_ == AppCacheUpdateJobState::DOWNLOADING);

  // TODO(jennb): Handle downloads completing during cache failure when update
  // no longer fetches master entries directly. For now, we cancel all pending
  // master entry fetches when entering cache failure state so this will never
  // be called in CACHE_FAILURE state.

  UpdateURLLoaderRequest* request = url_fetcher->request();
  const GURL& url = request->GetURL();

  auto it = master_entry_fetches_.find(url);
  if (it == master_entry_fetches_.end()) {
    NOTREACHED() << "Entry URL not found in master_entry_fetches_";
    return;
  }
  DCHECK_EQ(it->second.get(), url_fetcher);
  std::unique_ptr<URLFetcher> entry_fetcher = std::move(it->second);
  master_entry_fetches_.erase(it);

  // URLFetcher triggers this function, so we verify here that the fetch type
  // in URLFetcher is what we expect: kNewMasterEntry.
  DCHECK_EQ(entry_fetcher->fetch_type(),
            URLFetcher::FetchType::kNewMasterEntry);

  ++master_entries_completed_;

  int response_code = net_error == net::OK ? request->GetResponseCode() : -1;

  auto found = pending_master_entries_.find(url);
  DCHECK(found != pending_master_entries_.end());
  std::vector<AppCacheHost*>& hosts = found->second;

  // Section 7.9.4. No update case: step 7.3, else step 22.
  if (response_code / 100 == 2) {
    // Add fetched master entry to the appropriate cache.
    AppCache* cache = inprogress_cache_.get() ? inprogress_cache_.get()
                                              : group_->newest_complete_cache();
    DCHECK(entry_fetcher->response_writer());
    // Master entries cannot be cross-origin by definition, so they do not
    // require padding.
    AppCacheEntry master_entry(
        AppCacheEntry::MASTER, entry_fetcher->response_writer()->response_id(),
        entry_fetcher->response_writer()->amount_written(),
        /*padding_size=*/0);
    if (cache->AddOrModifyEntry(url, master_entry))
      added_master_entries_.push_back(url);
    else
      duplicate_response_ids_.push_back(master_entry.response_id());

    // In no-update case, associate host with the newest cache.
    if (!inprogress_cache_.get()) {
      // TODO(michaeln): defer until the updated cache has been stored
      DCHECK_EQ(cache, group_->newest_complete_cache());
      for (AppCacheHost* host : hosts)
        host->AssociateCompleteCache(cache);
    }
  } else {
    HostNotifier host_notifier;
    for (AppCacheHost* host : hosts) {
      host_notifier.AddHost(host);

      // In downloading case, disassociate host from inprogress cache.
      if (inprogress_cache_.get())
        host->AssociateNoCache(GURL());

      host->RemoveObserver(this);
    }
    hosts.clear();

    failed_master_entries_.insert(url);

    static const char kFormatString[] = "Manifest fetch failed (%d) %s";
    std::string message =
        FormatUrlErrorMessage(kFormatString, request->GetURL(),
                              entry_fetcher->result(), response_code);
    host_notifier.SendErrorNotifications(blink::mojom::AppCacheErrorDetails(
        message, blink::mojom::AppCacheErrorReason::APPCACHE_MANIFEST_ERROR,
        request->GetURL(), response_code, /*is_cross_origin=*/false));

    // In downloading case, update result is different if all master entries
    // failed vs. only some failing.
    if (inprogress_cache_.get()) {
      // Only count successful downloads to know if all master entries failed.
      pending_master_entries_.erase(found);
      --master_entries_completed_;

      // Section 7.9.4, step 22.3.
      if (update_type_ == CACHE_ATTEMPT && pending_master_entries_.empty()) {
        HandleCacheFailure(
            blink::mojom::AppCacheErrorDetails(
                message,
                blink::mojom::AppCacheErrorReason::APPCACHE_MANIFEST_ERROR,
                request->GetURL(), response_code, /*is_cross_origin=*/false),
            entry_fetcher->result(), GURL());
        return;
      }
    }
  }

  DCHECK(internal_state_ != AppCacheUpdateJobState::CACHE_FAILURE);
  FetchMasterEntries();
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleManifestRefetchCompleted(URLFetcher* url_fetcher,
                                                       int net_error) {
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::REFETCH_MANIFEST);
  DCHECK_EQ(manifest_fetcher_.get(), url_fetcher);
  std::unique_ptr<URLFetcher> manifest_fetcher = std::move(manifest_fetcher_);

  UpdateURLLoaderRequest* request = manifest_fetcher->request();
  int response_code = -1;
  std::string optional_manifest_scope;
  if (net_error == net::OK) {
    response_code = request->GetResponseCode();
    optional_manifest_scope = request->GetAppCacheAllowedHeader();
  }
  refetched_manifest_scope_ =
      AppCache::GetManifestScope(manifest_url_, optional_manifest_scope);

  if ((response_code == 304 &&
       fetched_manifest_scope_ == refetched_manifest_scope_) ||
      (manifest_data_ == manifest_fetcher->manifest_data())) {
    // Only need to store response in storage if manifest is not already an
    // entry in the cache.
    AppCacheEntry* entry = nullptr;
    if (inprogress_cache_)
      entry = inprogress_cache_->GetEntry(manifest_url_);
    if (entry) {
      entry->add_types(AppCacheEntry::MANIFEST);
      StoreGroupAndCache();
    } else {
      manifest_response_writer_ = CreateResponseWriter();
      scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
          base::MakeRefCounted<HttpResponseInfoIOBuffer>(
              std::move(manifest_response_info_));
      manifest_response_writer_->WriteInfo(
          io_buffer.get(),
          base::BindOnce(&AppCacheUpdateJob::OnManifestInfoWriteComplete,
                         base::Unretained(this)));
    }
  } else {
    VLOG(1) << "Request error: " << net_error
            << " response code: " << response_code;
    ScheduleUpdateRetry(kRerunDelayMs);
    if (response_code == 200) {
      HandleCacheFailure(
          blink::mojom::AppCacheErrorDetails(
              "Manifest changed during update",
              blink::mojom::AppCacheErrorReason::APPCACHE_CHANGED_ERROR, GURL(),
              0, /*is_cross_origin=*/false),
          MANIFEST_ERROR, GURL());
    } else {
      static const char kFormatString[] = "Manifest re-fetch failed (%d) %s";
      std::string message =
          FormatUrlErrorMessage(kFormatString, manifest_url_,
                                manifest_fetcher->result(), response_code);
      ResultType result = manifest_fetcher->result();
      if (result == UPDATE_OK) {
        // URLFetcher considers any 2xx response a success, however in this
        // particular case we want to treat any non 200 responses as failures.
        result = SERVER_ERROR;
      }
      HandleCacheFailure(
          blink::mojom::AppCacheErrorDetails(
              message,
              blink::mojom::AppCacheErrorReason::APPCACHE_MANIFEST_ERROR,
              GURL(), response_code, /*is_cross_origin=*/false),
          result, GURL());
    }
  }
}

void AppCacheUpdateJob::OnManifestInfoWriteComplete(int result) {
  if (result > 0) {
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(manifest_data_);
    manifest_response_writer_->WriteData(
        io_buffer.get(), manifest_data_.length(),
        base::BindOnce(&AppCacheUpdateJob::OnManifestDataWriteComplete,
                       base::Unretained(this)));
  } else {
    HandleCacheFailure(
        blink::mojom::AppCacheErrorDetails(
            "Failed to write the manifest headers to storage",
            blink::mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR, GURL(),
            0, /*is_cross_origin=*/false),
        DISKCACHE_ERROR, GURL());
  }
}

void AppCacheUpdateJob::OnManifestDataWriteComplete(int result) {
  if (result > 0) {
    // The manifest determines the cache's origin, so the manifest entry is
    // always same-origin, and thus does not require padding.
    AppCacheEntry entry(AppCacheEntry::MANIFEST,
                        manifest_response_writer_->response_id(),
                        manifest_response_writer_->amount_written(),
                        /*padding_size=*/0);
    if (!inprogress_cache_->AddOrModifyEntry(manifest_url_, entry))
      duplicate_response_ids_.push_back(entry.response_id());

    StoreGroupAndCache();
  } else {
    HandleCacheFailure(
        blink::mojom::AppCacheErrorDetails(
            "Failed to write the manifest data to storage",
            blink::mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR, GURL(),
            0, /*is_cross_origin=*/false),
        DISKCACHE_ERROR, GURL());
  }
}

void AppCacheUpdateJob::StoreGroupAndCache() {
  DCHECK_EQ(stored_state_, UNSTORED);
  stored_state_ = STORING;

  scoped_refptr<AppCache> newest_cache;
  if (inprogress_cache_.get())
    newest_cache.swap(inprogress_cache_);
  else
    newest_cache = group_->newest_complete_cache();
  newest_cache->set_update_time(base::Time::Now());

  // Verify that cache contains the associated manifest parser version and
  // scope values.
  DCHECK_EQ(fetched_manifest_parser_version_,
            newest_cache->manifest_parser_version());
  DCHECK_EQ(fetched_manifest_scope_, newest_cache->manifest_scope());

  // Verify fetched manifest parser version and scope:
  // 1. Values must be initialized and valid:
  //    - For parser version, the version must not be -1.
  //    - For scope, the the value must not be the empty string.
  DCHECK_NE(fetched_manifest_parser_version_, -1);
  DCHECK_NE(fetched_manifest_scope_, "");

  // 2. Check that the UpdateJob value state is correct:
  //    - For parser version, the newly fetched parser version must be greater
  //      than or equal to the version we began with.
  //    - For scope, the fetched manifest scope must be valid.
  DCHECK_GE(fetched_manifest_parser_version_, cached_manifest_parser_version_);
  DCHECK_EQ(fetched_manifest_scope_, refetched_manifest_scope_);
  DCHECK(AppCache::CheckValidManifestScope(manifest_url_,
                                           fetched_manifest_scope_));

  group_->set_first_evictable_error_time(base::Time());
  if (doing_full_update_check_)
    group_->set_last_full_update_check_time(base::Time::Now());
  storage_->StoreGroupAndNewestCache(group_, newest_cache.get(), this);
}

void AppCacheUpdateJob::OnGroupAndNewestCacheStored(AppCacheGroup* group,
                                                    AppCache* newest_cache,
                                                    bool success,
                                                    bool would_exceed_quota) {
  DCHECK_EQ(stored_state_, STORING);
  if (success) {
    stored_state_ = STORED;
    MaybeCompleteUpdate();  // will definitely complete
    return;
  }

  stored_state_ = UNSTORED;

  // Restore inprogress_cache_ to get the proper events delivered
  // and the proper cleanup to occur.
  if (newest_cache != group->newest_complete_cache())
    inprogress_cache_ = newest_cache;

  ResultType result = DB_ERROR;
  blink::mojom::AppCacheErrorReason reason =
      blink::mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR;
  std::string message("Failed to commit new cache to storage");
  if (would_exceed_quota) {
    message.append(", would exceed quota");
    result = QUOTA_ERROR;
    reason = blink::mojom::AppCacheErrorReason::APPCACHE_QUOTA_ERROR;
  }
  HandleCacheFailure(blink::mojom::AppCacheErrorDetails(
                         message, reason, GURL(), 0, /*is_cross_origin=*/false),
                     result, GURL());
}

void AppCacheUpdateJob::NotifySingleHost(
    AppCacheHost* host,
    blink::mojom::AppCacheEventID event_id) {
  host->frontend()->EventRaised(event_id);
}

void AppCacheUpdateJob::NotifyAllAssociatedHosts(
    blink::mojom::AppCacheEventID event_id) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendNotifications(event_id);
}

void AppCacheUpdateJob::NotifyAllProgress(const GURL& url) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendProgressNotifications(
      url, url_file_list_.size(), url_fetches_completed_);
}

void AppCacheUpdateJob::NotifyAllFinalProgress() {
  DCHECK_EQ(url_file_list_.size(), url_fetches_completed_);
  NotifyAllProgress(GURL());
}

void AppCacheUpdateJob::NotifyAllError(
    const blink::mojom::AppCacheErrorDetails& details) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendErrorNotifications(details);
}

void AppCacheUpdateJob::LogConsoleMessageToAll(const std::string& message) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendLogMessage(message);
}

void AppCacheUpdateJob::AddAllAssociatedHostsToNotifier(
    HostNotifier* host_notifier) {
  // Collect hosts so we only send one notification per frontend.
  // A host can only be associated with a single cache so no need to worry
  // about duplicate hosts being added to the notifier.
  if (inprogress_cache_.get()) {
    DCHECK(internal_state_ == AppCacheUpdateJobState::DOWNLOADING ||
           internal_state_ == AppCacheUpdateJobState::CACHE_FAILURE);
    host_notifier->AddHosts(inprogress_cache_->associated_hosts());
  }

  for (AppCache* cache : group_->old_caches())
    host_notifier->AddHosts(cache->associated_hosts());

  AppCache* newest_cache = group_->newest_complete_cache();
  if (newest_cache)
    host_notifier->AddHosts(newest_cache->associated_hosts());
}

void AppCacheUpdateJob::OnDestructionImminent(AppCacheHost* host) {
  // The host is about to be deleted; remove from our collection.
  auto found = pending_master_entries_.find(host->pending_master_entry_url());
  CHECK(found != pending_master_entries_.end());
  std::vector<AppCacheHost*>& hosts = found->second;
  auto it = std::find(hosts.begin(), hosts.end(), host);
  CHECK(it != hosts.end());
  hosts.erase(it);
}

void AppCacheUpdateJob::OnServiceReinitialized(
    AppCacheStorageReference* old_storage_ref) {
  // We continue to use the disabled instance, but arrange for its
  // deletion when its no longer needed.
  if (old_storage_ref->storage() == storage_)
    disabled_storage_reference_ = old_storage_ref;
}

void AppCacheUpdateJob::CheckIfManifestChanged() {
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::FETCH_MANIFEST);
  DCHECK_EQ(update_type_, UPGRADE_ATTEMPT);
  AppCacheEntry* entry = nullptr;
  if (group_->newest_complete_cache())
    entry = group_->newest_complete_cache()->GetEntry(manifest_url_);
  if (!entry) {
    // TODO(pwnall): Old documentation said this avoided the crash at
    //               https://crbug.com/95101. A removed histogram shows that
    //               this path is hit very rarely.
    if (service_->storage() == storage_) {
      // Use a local variable because service_ is reset in HandleCacheFailure.
      AppCacheServiceImpl* service = service_;
      HandleCacheFailure(
          blink::mojom::AppCacheErrorDetails(
              "Manifest entry not found in existing cache",
              blink::mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR, GURL(),
              0, /*is_cross_origin=*/false),
          DB_ERROR, GURL());
      service->DeleteAppCacheGroup(manifest_url_,
                                   net::CompletionOnceCallback());
    }
    return;
  }

  if (fetched_manifest_scope_ != cached_manifest_scope_) {
    HandleFetchedManifestChanged();
    return;
  }

  if (cached_manifest_parser_version_ < 2) {
    HandleFetchedManifestChanged();
    return;
  }

  // Load manifest data from storage to compare against fetched manifest.
  manifest_response_reader_ =
      storage_->CreateResponseReader(manifest_url_, entry->response_id());
  read_manifest_buffer_ =
      base::MakeRefCounted<net::IOBuffer>(kAppCacheFetchBufferSize);
  manifest_response_reader_->ReadData(
      read_manifest_buffer_.get(), kAppCacheFetchBufferSize,
      base::BindOnce(&AppCacheUpdateJob::OnManifestDataReadComplete,
                     base::Unretained(this)));  // async read
}

void AppCacheUpdateJob::OnManifestDataReadComplete(int result) {
  DCHECK_GE(cached_manifest_parser_version_, 2);
  DCHECK_EQ(fetched_manifest_scope_, cached_manifest_scope_);
  if (result > 0) {
    loaded_manifest_data_.append(read_manifest_buffer_->data(), result);
    manifest_response_reader_->ReadData(
        read_manifest_buffer_.get(), kAppCacheFetchBufferSize,
        base::BindOnce(&AppCacheUpdateJob::OnManifestDataReadComplete,
                       base::Unretained(this)));  // read more
  } else {
    read_manifest_buffer_ = nullptr;
    manifest_response_reader_.reset();
    if (result < 0 || manifest_data_ != loaded_manifest_data_) {
      HandleFetchedManifestChanged();
    } else {
      HandleFetchedManifestIsUnchanged();
    }
  }
}

void AppCacheUpdateJob::ReadManifestFromCacheAndContinue() {
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::FETCH_MANIFEST);
  DCHECK_EQ(update_type_, UPGRADE_ATTEMPT);
  // |manifest_response_info_| should have been saved in OnResponseInfoLoaded(),
  // we'll reuse it later in ContinueHandleManifestFetchCompleted() so make sure
  // it's still there.
  DCHECK(manifest_response_info_.get());
  AppCacheEntry* entry = nullptr;
  if (group_->newest_complete_cache())
    entry = group_->newest_complete_cache()->GetEntry(manifest_url_);
  if (!entry) {
    // TODO(pwnall): Old documentation said this avoided the crash at
    //               https://crbug.com/95101. A removed histogram shows that
    //               this path is hit very rarely.
    if (service_->storage() == storage_) {
      // Use a local variable because service_ is reset in HandleCacheFailure.
      AppCacheServiceImpl* service = service_;
      HandleCacheFailure(
          blink::mojom::AppCacheErrorDetails(
              "Manifest entry not found in existing cache",
              blink::mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR, GURL(),
              0, /*is_cross_origin=*/false),
          DB_ERROR, GURL());
      service->DeleteAppCacheGroup(manifest_url_,
                                   net::CompletionOnceCallback());
    }
    return;
  }

  // Load manifest data from storage so we can continue parsing using the new
  // scope.
  manifest_response_reader_ =
      storage_->CreateResponseReader(manifest_url_, entry->response_id());
  read_manifest_buffer_ =
      base::MakeRefCounted<net::IOBuffer>(kAppCacheFetchBufferSize);
  manifest_response_reader_->ReadData(
      read_manifest_buffer_.get(), kAppCacheFetchBufferSize,
      base::BindOnce(&AppCacheUpdateJob::OnManifestFromCacheDataReadComplete,
                     base::Unretained(this)));  // async read
}

void AppCacheUpdateJob::OnManifestFromCacheDataReadComplete(int result) {
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::FETCH_MANIFEST);
  DCHECK_EQ(update_type_, UPGRADE_ATTEMPT);
  if (result > 0) {
    loaded_manifest_data_.append(read_manifest_buffer_->data(), result);
    manifest_response_reader_->ReadData(
        read_manifest_buffer_.get(), kAppCacheFetchBufferSize,
        base::BindOnce(&AppCacheUpdateJob::OnManifestFromCacheDataReadComplete,
                       base::Unretained(this)));  // read more
  } else {
    manifest_data_ = loaded_manifest_data_;
    read_manifest_buffer_ = nullptr;
    manifest_response_reader_.reset();
    HandleFetchedManifestChanged();
  }
}

void AppCacheUpdateJob::BuildUrlFileList(const AppCacheManifest& manifest) {
  for (const std::string& explicit_url : manifest.explicit_urls)
    AddUrlToFileList(GURL(explicit_url), AppCacheEntry::EXPLICIT);

  for (const auto& intercept : manifest.intercept_namespaces)
    AddUrlToFileList(intercept.target_url, AppCacheEntry::INTERCEPT);

  for (const auto& fallback : manifest.fallback_namespaces)
    AddUrlToFileList(fallback.target_url, AppCacheEntry::FALLBACK);

  // Add all master entries from newest complete cache.
  if (update_type_ == UPGRADE_ATTEMPT) {
    for (const auto& pair : group_->newest_complete_cache()->entries()) {
      const AppCacheEntry& entry = pair.second;
      if (entry.IsMaster())
        AddUrlToFileList(pair.first, AppCacheEntry::MASTER);
    }
  }
}

void AppCacheUpdateJob::AddUrlToFileList(const GURL& url, int type) {
  auto emplace_result = url_file_list_.emplace(url, AppCacheEntry(type));

  if (emplace_result.second) {
    urls_to_fetch_.emplace_back(url, false, nullptr);
  } else {
    // URL already exists. Merge types.
    emplace_result.first->second.add_types(type);
  }
}

void AppCacheUpdateJob::FetchUrls() {
  DCHECK_EQ(internal_state_, AppCacheUpdateJobState::DOWNLOADING);

  // Fetch each URL in the list according to section 7.9.4 step 18.1-18.3.
  // Fetch up to the concurrent limit. Other fetches will be triggered as each
  // each fetch completes.
  while (pending_url_fetches_.size() < kMaxConcurrentUrlFetches &&
         !urls_to_fetch_.empty()) {
    UrlToFetch url_to_fetch = urls_to_fetch_.front();
    urls_to_fetch_.pop_front();

    auto it = url_file_list_.find(url_to_fetch.url);
    DCHECK(it != url_file_list_.end());
    AppCacheEntry& entry = it->second;
    if (ShouldSkipUrlFetch(entry)) {
      NotifyAllProgress(url_to_fetch.url);
      ++url_fetches_completed_;
    } else if (AlreadyFetchedEntry(url_to_fetch.url, entry.types())) {
      NotifyAllProgress(url_to_fetch.url);
      ++url_fetches_completed_;  // saved a URL request
    } else if (!url_to_fetch.storage_checked &&
               MaybeLoadFromNewestCache(url_to_fetch.url, entry)) {
      // Continues asynchronously after data is loaded from newest cache.
    } else {
      auto fetcher = std::make_unique<URLFetcher>(
          url_to_fetch.url, URLFetcher::FetchType::kResource, this,
          kAppCacheFetchBufferSize);
      if (url_to_fetch.existing_response_info.get() &&
          group_->newest_complete_cache()) {
        AppCacheEntry* existing_entry =
            group_->newest_complete_cache()->GetEntry(url_to_fetch.url);
        DCHECK(existing_entry);
        DCHECK_EQ(existing_entry->response_id(),
                  url_to_fetch.existing_response_info->response_id());
        fetcher->set_existing_response_headers(
            url_to_fetch.existing_response_info->http_response_info()
                .headers.get());
        fetcher->set_existing_entry(*existing_entry);
      }
      fetcher->Start();
      pending_url_fetches_.emplace(url_to_fetch.url, std::move(fetcher));
    }
  }
}

void AppCacheUpdateJob::CancelAllUrlFetches() {
  // Cancel any pending URL requests.
  url_fetches_completed_ +=
      pending_url_fetches_.size() + urls_to_fetch_.size();
  pending_url_fetches_.clear();
  urls_to_fetch_.clear();
}

bool AppCacheUpdateJob::ShouldSkipUrlFetch(const AppCacheEntry& entry) {
  // 6.6.4 Step 17
  // If the resource URL being processed was flagged as neither an
  // "explicit entry" nor or a "fallback entry", then the user agent
  // may skip this URL.
  if (entry.IsExplicit() || entry.IsFallback() || entry.IsIntercept())
    return false;

  // TODO(jennb): decide if entry should be skipped to expire it from cache
  return false;
}

bool AppCacheUpdateJob::AlreadyFetchedEntry(const GURL& url,
                                            int entry_type) {
  DCHECK(internal_state_ == AppCacheUpdateJobState::DOWNLOADING ||
         internal_state_ == AppCacheUpdateJobState::NO_UPDATE);
  AppCacheEntry* existing =
      inprogress_cache_.get() ? inprogress_cache_->GetEntry(url)
                              : group_->newest_complete_cache()->GetEntry(url);
  if (existing) {
    existing->add_types(entry_type);
    return true;
  }
  return false;
}

void AppCacheUpdateJob::AddMasterEntryToFetchList(AppCacheHost* host,
                                                  const GURL& url,
                                                  bool is_new) {
  DCHECK(!IsTerminating());

  if (internal_state_ == AppCacheUpdateJobState::DOWNLOADING ||
      internal_state_ == AppCacheUpdateJobState::NO_UPDATE) {
    AppCache* cache;
    if (inprogress_cache_.get()) {
      // always associate
      host->AssociateIncompleteCache(inprogress_cache_.get(), manifest_url_);
      cache = inprogress_cache_.get();
    } else {
      cache = group_->newest_complete_cache();
    }

    // Update existing entry if it has already been fetched.
    AppCacheEntry* entry = cache->GetEntry(url);
    if (entry) {
      entry->add_types(AppCacheEntry::MASTER);
      if (internal_state_ == AppCacheUpdateJobState::NO_UPDATE &&
          !inprogress_cache_.get()) {
        // only associate if have entry
        host->AssociateCompleteCache(cache);
      }
      if (is_new)
        ++master_entries_completed_;  // pretend fetching completed
      return;
    }
  }

  // Add to fetch list if not already fetching.
  if (master_entry_fetches_.find(url) == master_entry_fetches_.end()) {
    master_entries_to_fetch_.insert(url);
    if (internal_state_ == AppCacheUpdateJobState::DOWNLOADING ||
        internal_state_ == AppCacheUpdateJobState::NO_UPDATE)
      FetchMasterEntries();
  }
}

void AppCacheUpdateJob::FetchMasterEntries() {
  DCHECK(internal_state_ == AppCacheUpdateJobState::NO_UPDATE ||
         internal_state_ == AppCacheUpdateJobState::DOWNLOADING);

  // Fetch each master entry in the list, up to the concurrent limit.
  // Additional fetches will be triggered as each fetch completes.
  while (master_entry_fetches_.size() < kMaxConcurrentUrlFetches &&
         !master_entries_to_fetch_.empty()) {
    const GURL& url = *master_entries_to_fetch_.begin();

    if (AlreadyFetchedEntry(url, AppCacheEntry::MASTER)) {
      ++master_entries_completed_;  // saved a URL request

      // In no update case, associate hosts to newest cache in group
      // now that master entry has been "successfully downloaded".
      if (internal_state_ == AppCacheUpdateJobState::NO_UPDATE) {
        // TODO(michaeln): defer until the updated cache has been stored.
        DCHECK(!inprogress_cache_.get());
        AppCache* cache = group_->newest_complete_cache();
        auto found = pending_master_entries_.find(url);
        DCHECK(found != pending_master_entries_.end());
        std::vector<AppCacheHost*>& hosts = found->second;
        for (AppCacheHost* host : hosts)
          host->AssociateCompleteCache(cache);
      }
    } else {
      auto fetcher = std::make_unique<URLFetcher>(
          url, URLFetcher::FetchType::kNewMasterEntry, this,
          kAppCacheFetchBufferSize);
      fetcher->Start();
      master_entry_fetches_.emplace(url, std::move(fetcher));
    }

    master_entries_to_fetch_.erase(master_entries_to_fetch_.begin());
  }
}

void AppCacheUpdateJob::CancelAllMasterEntryFetches(
    const blink::mojom::AppCacheErrorDetails& error_details) {
  // For now, cancel all in-progress fetches for master entries and pretend
  // all master entries fetches have completed.
  // TODO(jennb): Delete this when update no longer fetches master entries
  // directly.

  // Cancel all in-progress fetches.
  for (auto& pair : master_entry_fetches_) {
    // Move URLs back to the unfetched list.
    master_entries_to_fetch_.emplace(std::move(pair.first));
  }
  master_entry_fetches_.clear();

  master_entries_completed_ += master_entries_to_fetch_.size();

  // Cache failure steps, step 2.
  // Pretend all master entries that have not yet been fetched have completed
  // downloading. Unassociate hosts from any appcache and send ERROR event.
  HostNotifier host_notifier;
  while (!master_entries_to_fetch_.empty()) {
    const GURL& url = *master_entries_to_fetch_.begin();
    auto found = pending_master_entries_.find(url);
    DCHECK(found != pending_master_entries_.end());
    std::vector<AppCacheHost*>& hosts = found->second;
    for (AppCacheHost* host : hosts) {
      host->AssociateNoCache(GURL());
      host_notifier.AddHost(host);
      host->RemoveObserver(this);
    }
    hosts.clear();

    master_entries_to_fetch_.erase(master_entries_to_fetch_.begin());
  }
  host_notifier.SendErrorNotifications(error_details);
}

bool AppCacheUpdateJob::MaybeLoadFromNewestCache(const GURL& url,
                                                 AppCacheEntry& entry) {
  if (update_type_ != UPGRADE_ATTEMPT)
    return false;

  AppCache* newest = group_->newest_complete_cache();
  AppCacheEntry* copy_me = newest->GetEntry(url);
  if (!copy_me || !copy_me->has_response_id())
    return false;

  // Load HTTP headers for entry from newest cache.
  loading_responses_.emplace(copy_me->response_id(), url);
  storage_->LoadResponseInfo(manifest_url_, copy_me->response_id(), this);
  // Async: wait for OnResponseInfoLoaded to complete.
  return true;
}

void AppCacheUpdateJob::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info,
    int64_t response_id) {
  const net::HttpResponseInfo* http_info =
      response_info ? &response_info->http_response_info() : nullptr;

  // Needed response info for a manifest fetch request.
  if (internal_state_ == AppCacheUpdateJobState::FETCH_MANIFEST) {
    if (http_info) {
      // Save a copy of the HttpResponseInfo in case we need it later. We would
      // use it if we attach conditional headers and the server replies with a
      // 304. In that case, we would use these same headers again to refetch the
      // manifest. In the case that the server replies with 200 OK, this
      // manifest_response_info_ will be overwritten with that response's
      // HttpResponseInfo and since it's a unique_ptr this HttpResponseInfo will
      // be deleted.
      manifest_response_info_ =
          std::make_unique<net::HttpResponseInfo>(*http_info);
      if (cached_manifest_parser_version_ >= 2) {
        manifest_fetcher_->set_existing_response_headers(
            http_info->headers.get());
      }
    }
    manifest_fetcher_->Start();
    return;
  }

  auto found = loading_responses_.find(response_id);
  DCHECK(found != loading_responses_.end());
  const GURL& url = found->second;

  if (!http_info) {
    LoadFromNewestCacheFailed(url, nullptr);  // no response found
  } else {
    ResourceCheck result = CanUseExistingResource(http_info, update_metrics_);
    if (result == ResourceCheck::kCorrupt) {
      // A corrupt resource was found.  In this case, we want to cause the next
      // fetch attempt for this resource to be issued without conditional
      // headers so a 200 OK response is the only result.  We do that by not
      // passing along |response_info| here.  This case can only occur when the
      // AppCacheCorruptionRecovery feature is enabled.
      LoadFromNewestCacheFailed(url, nullptr);
    } else if (result == ResourceCheck::kInvalid) {
      // An invalid resource was found, but we may want to add conditional
      // headers that could result in a 304 NOT MODIFIED response.
      LoadFromNewestCacheFailed(url, response_info);
    } else {
      DCHECK(result == ResourceCheck::kValid);
      DCHECK(group_->newest_complete_cache());
      AppCacheEntry* copy_me = group_->newest_complete_cache()->GetEntry(url);
      DCHECK(copy_me);
      DCHECK_EQ(copy_me->response_id(), response_id);

      auto it = url_file_list_.find(url);
      DCHECK(it != url_file_list_.end());
      AppCacheEntry& entry = it->second;
      entry.set_response_id(response_id);
      entry.SetResponseAndPaddingSizes(copy_me->response_size(),
                                       copy_me->padding_size());
      inprogress_cache_->AddOrModifyEntry(url, entry);
      NotifyAllProgress(url);
      ++url_fetches_completed_;
    }
  }

  loading_responses_.erase(found);
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::LoadFromNewestCacheFailed(
    const GURL& url, AppCacheResponseInfo* response_info) {
  if (internal_state_ == AppCacheUpdateJobState::CACHE_FAILURE)
    return;

  // Re-insert url at front of fetch list. Indicate storage has been checked.
  urls_to_fetch_.push_front(UrlToFetch(url, true, response_info));
  FetchUrls();
}

void AppCacheUpdateJob::MaybeCompleteUpdate() {
  DCHECK(internal_state_ != AppCacheUpdateJobState::CACHE_FAILURE);

  // Must wait for any pending master entries or url fetches to complete.
  if (master_entries_completed_ != pending_master_entries_.size() ||
      url_fetches_completed_ != url_file_list_.size()) {
    DCHECK(internal_state_ != AppCacheUpdateJobState::COMPLETED);
    return;
  }

  switch (internal_state_) {
    case AppCacheUpdateJobState::NO_UPDATE:
      if (master_entries_completed_ > 0) {
        switch (stored_state_) {
          case UNSTORED:
            StoreGroupAndCache();
            return;
          case STORING:
            return;
          case STORED:
            break;
        }
      } else {
        bool times_changed = false;
        if (!group_->first_evictable_error_time().is_null()) {
          group_->set_first_evictable_error_time(base::Time());
          times_changed = true;
        }
        if (doing_full_update_check_) {
          group_->set_last_full_update_check_time(base::Time::Now());
          times_changed = true;
        }
        if (times_changed)
          storage_->StoreEvictionTimes(group_);
      }
      group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);
      // 7.9.4 steps 7.3-7.7.
      NotifyAllAssociatedHosts(
          blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);
      DiscardDuplicateResponses();
      internal_state_ = AppCacheUpdateJobState::COMPLETED;
      break;
    case AppCacheUpdateJobState::DOWNLOADING:
      internal_state_ = AppCacheUpdateJobState::REFETCH_MANIFEST;
      RefetchManifest();
      break;
    case AppCacheUpdateJobState::REFETCH_MANIFEST:
      DCHECK_EQ(stored_state_, STORED);
      NotifyAllFinalProgress();
      group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);
      if (update_type_ == CACHE_ATTEMPT)
        NotifyAllAssociatedHosts(
            blink::mojom::AppCacheEventID::APPCACHE_CACHED_EVENT);
      else
        NotifyAllAssociatedHosts(
            blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);
      DiscardDuplicateResponses();
      internal_state_ = AppCacheUpdateJobState::COMPLETED;
      break;
    case AppCacheUpdateJobState::CACHE_FAILURE:
      NOTREACHED();  // See HandleCacheFailure
      break;
    default:
      break;
  }

  // Let the stack unwind before deletion to make it less risky as this
  // method is called from multiple places in this file.
  if (internal_state_ == AppCacheUpdateJobState::COMPLETED)
    DeleteSoon();
}

void AppCacheUpdateJob::ScheduleUpdateRetry(int delay_ms) {
  // TODO(jennb): post a delayed task with the "same parameters" as this job
  // to retry the update at a later time. Need group, URLs of pending master
  // entries and their hosts.
}

void AppCacheUpdateJob::Cancel() {
  update_metrics_.RecordCanceled();
  internal_state_ = AppCacheUpdateJobState::CANCELLED;

  manifest_fetcher_.reset();
  pending_url_fetches_.clear();
  master_entry_fetches_.clear();

  ClearPendingMasterEntries();
  DiscardInprogressCache();

  // Delete response writer to avoid any callbacks.
  if (manifest_response_writer_)
    manifest_response_writer_.reset();

  storage_->CancelDelegateCallbacks(this);
}

void AppCacheUpdateJob::ClearPendingMasterEntries() {
  for (auto& pair : pending_master_entries_) {
    std::vector<AppCacheHost*>& hosts = pair.second;
    for (AppCacheHost* host : hosts)
      host->RemoveObserver(this);
  }

  pending_master_entries_.clear();
}

void AppCacheUpdateJob::DiscardInprogressCache() {
  if (stored_state_ == STORING) {
    // We can make no assumptions about whether the StoreGroupAndCacheTask
    // actually completed or not. This condition should only be reachable
    // during shutdown. Free things up and return to do no harm.
    inprogress_cache_ = nullptr;
    added_master_entries_.clear();
    return;
  }

  storage_->DoomResponses(manifest_url_, stored_response_ids_);

  if (!inprogress_cache_.get()) {
    // We have to undo the changes we made, if any, to the existing cache.
    if (group_ && group_->newest_complete_cache()) {
      for (auto& url : added_master_entries_)
        group_->newest_complete_cache()->RemoveEntry(url);
    }
    added_master_entries_.clear();
    return;
  }

  AppCache::AppCacheHosts& hosts = inprogress_cache_->associated_hosts();
  while (!hosts.empty())
    (*hosts.begin())->AssociateNoCache(GURL());

  inprogress_cache_ = nullptr;
  added_master_entries_.clear();
}

void AppCacheUpdateJob::DiscardDuplicateResponses() {
  storage_->DoomResponses(manifest_url_, duplicate_response_ids_);
}

void AppCacheUpdateJob::DeleteSoon() {
  ClearPendingMasterEntries();
  manifest_response_writer_.reset();
  storage_->CancelDelegateCallbacks(this);
  service_->RemoveObserver(this);
  service_ = nullptr;

  // Break the connection with the group so the group cannot call delete
  // on this object after we've posted a task to delete ourselves.
  if (group_) {
    group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);
    group_ = nullptr;
  }

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

bool AppCacheUpdateJob::IsFinished() const {
  return (internal_state_ == AppCacheUpdateJobState::CACHE_FAILURE ||
          internal_state_ == AppCacheUpdateJobState::CANCELLED ||
          internal_state_ == AppCacheUpdateJobState::COMPLETED);
}

}  // namespace content
