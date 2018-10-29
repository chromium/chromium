// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_job.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/browser/appcache/appcache_update_request_base.h"
#include "content/browser/appcache/appcache_update_url_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "url/origin.h"

namespace content {

namespace {

const int kAppCacheFetchBufferSize = 32768;
const size_t kMaxConcurrentUrlFetches = 2;

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
                      const AppCacheErrorDetails& details) {
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
      return details.reason == AppCacheErrorReason::APPCACHE_SIGNATURE_ERROR;

    default:
      NOTREACHED();
      return true;
  }
}

bool CanUseExistingResource(const net::HttpResponseInfo* http_info) {
  // Check HTTP caching semantics based on max-age and expiration headers.
  if (!http_info->headers || http_info->headers->RequiresValidation(
                                 http_info->request_time,
                                 http_info->response_time, base::Time::Now())) {
    return false;
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
      return false;
    }
  }
  return true;
}

void EmptyCompletionCallback(int result) {}

}  // namespace

// Helper class for collecting hosts per frontend when sending notifications
// so that only one notification is sent for all hosts using the same frontend.
class HostNotifier {
 public:
  using HostIds = std::vector<int>;
  using NotifyHostMap = std::map<AppCacheFrontend*, HostIds>;

  // Caller is responsible for ensuring there will be no duplicate hosts.
  void AddHost(AppCacheHost* host) {
    std::pair<NotifyHostMap::iterator, bool> ret = hosts_to_notify_.insert(
        NotifyHostMap::value_type(host->frontend(), HostIds()));
    ret.first->second.push_back(host->host_id());
  }

  void AddHosts(const std::set<AppCacheHost*>& hosts) {
    for (AppCacheHost* host : hosts)
      AddHost(host);
  }

  void SendNotifications(AppCacheEventID event_id) {
    for (auto& pair : hosts_to_notify_) {
      AppCacheFrontend* frontend = pair.first;
      frontend->OnEventRaised(pair.second, event_id);
    }
  }

  void SendProgressNotifications(const GURL& url,
                                 int num_total,
                                 int num_complete) {
    for (const auto& pair : hosts_to_notify_) {
      AppCacheFrontend* frontend = pair.first;
      frontend->OnProgressEventRaised(pair.second, url, num_total,
                                      num_complete);
    }
  }

  void SendErrorNotifications(const AppCacheErrorDetails& details) {
    DCHECK(!details.message.empty());
    for (const auto& pair : hosts_to_notify_) {
      AppCacheFrontend* frontend = pair.first;
      frontend->OnErrorEventRaised(pair.second, details);
    }
  }

  void SendLogMessage(const std::string& message) {
    for (const auto& pair : hosts_to_notify_) {
      AppCacheFrontend* frontend = pair.first;
      for (const auto& id : pair.second)
        frontend->OnLogMessage(id, APPCACHE_LOG_WARNING, message);
    }
  }

 private:
  NotifyHostMap hosts_to_notify_;
};
AppCacheUpdateJob::UrlToFetch::UrlToFetch(const GURL& url,
                                          bool checked,
                                          AppCacheResponseInfo* info)
    : url(url),
      storage_checked(checked),
      existing_response_info(info) {
}

AppCacheUpdateJob::UrlToFetch::UrlToFetch(const UrlToFetch& other) = default;

AppCacheUpdateJob::UrlToFetch::~UrlToFetch() {
}

AppCacheUpdateJob::AppCacheUpdateJob(AppCacheServiceImpl* service,
                                     AppCacheGroup* group)
    : service_(service),
      manifest_url_(group->manifest_url()),
      group_(group),
      update_type_(UNKNOWN_TYPE),
      internal_state_(FETCH_MANIFEST),
      doing_full_update_check_(false),
      master_entries_completed_(0),
      url_fetches_completed_(0),
      manifest_fetcher_(nullptr),
      manifest_has_valid_mime_type_(false),
      stored_state_(UNSTORED),
      storage_(service->storage()),
      weak_factory_(this) {
  service_->AddObserver(this);
}

AppCacheUpdateJob::~AppCacheUpdateJob() {
  if (service_)
    service_->RemoveObserver(this);
  if (internal_state_ != COMPLETED)
    Cancel();

  DCHECK(!inprogress_cache_.get());
  DCHECK(pending_master_entries_.empty());

  // The job must not outlive any of its fetchers.
  CHECK(!manifest_fetcher_);
  CHECK(pending_url_fetches_.empty());
  CHECK(master_entry_fetches_.empty());

  if (group_)
    group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);
}

void AppCacheUpdateJob::StartUpdate(AppCacheHost* host,
                                    const GURL& new_master_resource) {
  DCHECK(group_->update_job() == this);
  DCHECK(!group_->is_obsolete());

  bool is_new_pending_master_entry = false;
  if (!new_master_resource.is_empty()) {
    DCHECK(new_master_resource == host->pending_master_entry_url());
    DCHECK(!new_master_resource.has_ref());
    DCHECK(new_master_resource.GetOrigin() == manifest_url_.GetOrigin());

    if (base::ContainsKey(failed_master_entries_, new_master_resource))
      return;

    // Cannot add more to this update if already terminating.
    if (IsTerminating()) {
      group_->QueueUpdate(host, new_master_resource);
      return;
    }

    std::pair<PendingMasters::iterator, bool> ret =
        pending_master_entries_.insert(
            PendingMasters::value_type(new_master_resource, PendingHosts()));
    is_new_pending_master_entry = ret.second;
    ret.first->second.push_back(host);
    host->AddObserver(this);
  }

  // Notify host (if any) if already checking or downloading.
  AppCacheGroup::UpdateAppCacheStatus update_status = group_->update_status();
  if (update_status == AppCacheGroup::CHECKING ||
      update_status == AppCacheGroup::DOWNLOADING) {
    if (host) {
      NotifySingleHost(host, AppCacheEventID::APPCACHE_CHECKING_EVENT);
      if (update_status == AppCacheGroup::DOWNLOADING)
        NotifySingleHost(host, AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);

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
    base::TimeDelta time_since_last_check =
        base::Time::Now() - group_->last_full_update_check_time();
    doing_full_update_check_ = time_since_last_check > kFullUpdateInterval;
    NotifyAllAssociatedHosts(AppCacheEventID::APPCACHE_CHECKING_EVENT);
  } else {
    update_type_ = CACHE_ATTEMPT;
    doing_full_update_check_ = true;
    DCHECK(host);
    NotifySingleHost(host, AppCacheEventID::APPCACHE_CHECKING_EVENT);
  }

  if (!new_master_resource.is_empty()) {
    AddMasterEntryToFetchList(host, new_master_resource,
                              is_new_pending_master_entry);
  }

  BrowserThread::PostAfterStartupTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(&AppCacheUpdateJob::FetchManifest,
                     weak_factory_.GetWeakPtr(), true));
}

std::unique_ptr<AppCacheResponseWriter>
AppCacheUpdateJob::CreateResponseWriter() {
  std::unique_ptr<AppCacheResponseWriter> writer =
      storage_->CreateResponseWriter(manifest_url_);
  stored_response_ids_.push_back(writer->response_id());
  return writer;
}

void AppCacheUpdateJob::HandleCacheFailure(
    const AppCacheErrorDetails& error_details,
    ResultType result,
    const GURL& failed_resource_url) {
  // 6.9.4 cache failure steps 2-8.
  DCHECK(internal_state_ != CACHE_FAILURE);
  DCHECK(!error_details.message.empty());
  DCHECK(result != UPDATE_OK);
  internal_state_ = CACHE_FAILURE;
  LogHistogramStats(result, failed_resource_url);
  CancelAllUrlFetches();
  CancelAllMasterEntryFetches(error_details);
  NotifyAllError(error_details);
  DiscardInprogressCache();
  internal_state_ = COMPLETED;

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

void AppCacheUpdateJob::FetchManifest(bool is_first_fetch) {
  DCHECK(!manifest_fetcher_);
  manifest_fetcher_ =
      new URLFetcher(manifest_url_,
                     is_first_fetch ? URLFetcher::MANIFEST_FETCH
                                    : URLFetcher::MANIFEST_REFETCH,
                     this, kAppCacheFetchBufferSize);

  if (is_first_fetch) {
    // Maybe load the cached headers to make a condiditional request.
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

  DCHECK(internal_state_ == REFETCH_MANIFEST);
  DCHECK(manifest_response_info_.get());
  manifest_fetcher_->set_existing_response_headers(
      manifest_response_info_->headers.get());
  manifest_fetcher_->Start();
}

void AppCacheUpdateJob::HandleManifestFetchCompleted(URLFetcher* fetcher,
                                                     int net_error) {
  DCHECK_EQ(internal_state_, FETCH_MANIFEST);
  DCHECK_EQ(manifest_fetcher_, fetcher);

  manifest_fetcher_ = nullptr;

  UpdateRequestBase* request = fetcher->request();
  int response_code = -1;
  bool is_valid_response_code = false;
  if (net_error == net::OK) {
    response_code = request->GetResponseCode();
    is_valid_response_code = (response_code / 100 == 2);

    std::string mime_type = request->GetMimeType();
    manifest_has_valid_mime_type_ = (mime_type == "text/cache-manifest");
  }

  if (is_valid_response_code) {
    manifest_data_ = fetcher->manifest_data();
    manifest_response_info_.reset(
        new net::HttpResponseInfo(request->GetResponseInfo()));
    if (update_type_ == UPGRADE_ATTEMPT)
      CheckIfManifestChanged();  // continues asynchronously
    else
      ContinueHandleManifestFetchCompleted(true);
  } else if (response_code == 304 && update_type_ == UPGRADE_ATTEMPT) {
    ContinueHandleManifestFetchCompleted(false);
  } else if ((response_code == 404 || response_code == 410) &&
             update_type_ == UPGRADE_ATTEMPT) {
    storage_->MakeGroupObsolete(group_, this, response_code);  // async
  } else {
    const char kFormatString[] = "Manifest fetch failed (%d) %s";
    std::string message = FormatUrlErrorMessage(
        kFormatString, manifest_url_, fetcher->result(), response_code);
    HandleCacheFailure(
        AppCacheErrorDetails(
            message, AppCacheErrorReason::APPCACHE_MANIFEST_ERROR,
            manifest_url_, response_code, false /*is_cross_origin*/),
        fetcher->result(), GURL());
  }
}

void AppCacheUpdateJob::OnGroupMadeObsolete(AppCacheGroup* group,
                                            bool success,
                                            int response_code) {
  DCHECK(master_entry_fetches_.empty());
  CancelAllMasterEntryFetches(
      AppCacheErrorDetails("The cache has been made obsolete, "
                           "the manifest file returned 404 or 410",
                           AppCacheErrorReason::APPCACHE_MANIFEST_ERROR, GURL(),
                           response_code, false /*is_cross_origin*/));
  if (success) {
    DCHECK(group->is_obsolete());
    NotifyAllAssociatedHosts(AppCacheEventID::APPCACHE_OBSOLETE_EVENT);
    internal_state_ = COMPLETED;
    MaybeCompleteUpdate();
  } else {
    // Treat failure to mark group obsolete as a cache failure.
    HandleCacheFailure(
        AppCacheErrorDetails("Failed to mark the cache as obsolete",
                             AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                             GURL(), 0, false /*is_cross_origin*/),
        DB_ERROR, GURL());
  }
}

void AppCacheUpdateJob::ContinueHandleManifestFetchCompleted(bool changed) {
  DCHECK(internal_state_ == FETCH_MANIFEST);

  if (!changed) {
    DCHECK(update_type_ == UPGRADE_ATTEMPT);
    internal_state_ = NO_UPDATE;

    // Wait for pending master entries to download.
    FetchMasterEntries();
    MaybeCompleteUpdate();  // if not done, run async 6.9.4 step 7 substeps
    return;
  }

  AppCacheManifest manifest;
  if (!ParseManifest(manifest_url_, manifest_data_.data(),
                     manifest_data_.length(),
                     manifest_has_valid_mime_type_
                         ? PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES
                         : PARSE_MANIFEST_PER_STANDARD,
                     manifest)) {
    const char kFormatString[] = "Failed to parse manifest %s";
    const std::string message = base::StringPrintf(kFormatString,
        manifest_url_.spec().c_str());
    HandleCacheFailure(
        AppCacheErrorDetails(message,
                             AppCacheErrorReason::APPCACHE_SIGNATURE_ERROR,
                             GURL(), 0, false /*is_cross_origin*/),
        MANIFEST_ERROR, GURL());
    VLOG(1) << message;
    return;
  }

  // Proceed with update process. Section 6.9.4 steps 8-20.
  internal_state_ = DOWNLOADING;
  inprogress_cache_ = new AppCache(storage_, storage_->NewCacheId());
  BuildUrlFileList(manifest);
  inprogress_cache_->InitializeWithManifest(&manifest);

  // Associate all pending master hosts with the newly created cache.
  for (const auto& pair : pending_master_entries_) {
    const PendingHosts& hosts = pair.second;
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
  NotifyAllAssociatedHosts(AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
  FetchUrls();
  FetchMasterEntries();
  MaybeCompleteUpdate();  // if not done, continues when async fetches complete
}

void AppCacheUpdateJob::HandleUrlFetchCompleted(URLFetcher* fetcher,
                                                int net_error) {
  DCHECK(internal_state_ == DOWNLOADING);

  UpdateRequestBase* request = fetcher->request();
  const GURL& url = request->GetURL();
  pending_url_fetches_.erase(url);
  NotifyAllProgress(url);
  ++url_fetches_completed_;

  int response_code = net_error == net::OK ? request->GetResponseCode()
                                           : fetcher->redirect_response_code();

  AppCacheEntry& entry = url_file_list_.find(url)->second;

  if (response_code / 100 == 2) {
    // Associate storage with the new entry.
    DCHECK(fetcher->response_writer());
    entry.set_response_id(fetcher->response_writer()->response_id());
    entry.set_response_size(fetcher->response_writer()->amount_written());
    if (!inprogress_cache_->AddOrModifyEntry(url, entry))
      duplicate_response_ids_.push_back(entry.response_id());

    // TODO(michaeln): Check for <html manifest=xxx>
    // See http://code.google.com/p/chromium/issues/detail?id=97930
    // if (entry.IsMaster() && !(entry.IsExplicit() || fallback || intercept))
    //   if (!manifestAttribute) skip it

    // Foreign entries will be detected during cache selection.
    // Note: 6.9.4, step 17.9 possible optimization: if resource is HTML or XML
    // file whose root element is an html element with a manifest attribute
    // whose value doesn't match the manifest url of the application cache
    // being processed, mark the entry as being foreign.
  } else {
    VLOG(1) << "Request error: " << net_error
            << " response code: " << response_code;
    if (entry.IsExplicit() || entry.IsFallback() || entry.IsIntercept()) {
      if (response_code == 304 && fetcher->existing_entry().has_response_id()) {
        // Keep the existing response.
        entry.set_response_id(fetcher->existing_entry().response_id());
        entry.set_response_size(fetcher->existing_entry().response_size());
        inprogress_cache_->AddOrModifyEntry(url, entry);
      } else {
        const char kFormatString[] = "Resource fetch failed (%d) %s";
        std::string message = FormatUrlErrorMessage(
            kFormatString, url, fetcher->result(), response_code);
        ResultType result = fetcher->result();
        bool is_cross_origin = url.GetOrigin() != manifest_url_.GetOrigin();
        switch (result) {
          case DISKCACHE_ERROR:
            HandleCacheFailure(
                AppCacheErrorDetails(
                    message, AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                    GURL(), 0, is_cross_origin),
                result, url);
            break;
          case NETWORK_ERROR:
            HandleCacheFailure(
                AppCacheErrorDetails(
                    message, AppCacheErrorReason::APPCACHE_RESOURCE_ERROR, url,
                    0, is_cross_origin),
                result, url);
            break;
          default:
            HandleCacheFailure(
                AppCacheErrorDetails(
                    message, AppCacheErrorReason::APPCACHE_RESOURCE_ERROR, url,
                    response_code, is_cross_origin),
                result, url);
            break;
        }
        return;
      }
    } else if (response_code == 404 || response_code == 410) {
      // Entry is skipped.  They are dropped from the cache.
    } else if (update_type_ == UPGRADE_ATTEMPT &&
               fetcher->existing_entry().has_response_id()) {
      // Keep the existing response.
      // TODO(michaeln): Not sure this is a good idea. This is spec compliant
      // but the old resource may or may not be compatible with the new contents
      // of the cache. Impossible to know one way or the other.
      entry.set_response_id(fetcher->existing_entry().response_id());
      entry.set_response_size(fetcher->existing_entry().response_size());
      inprogress_cache_->AddOrModifyEntry(url, entry);
    }
  }

  // Fetch another URL now that one request has completed.
  DCHECK(internal_state_ != CACHE_FAILURE);
  FetchUrls();
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleMasterEntryFetchCompleted(URLFetcher* fetcher,
                                                        int net_error) {
  DCHECK(internal_state_ == NO_UPDATE || internal_state_ == DOWNLOADING);

  // TODO(jennb): Handle downloads completing during cache failure when update
  // no longer fetches master entries directly. For now, we cancel all pending
  // master entry fetches when entering cache failure state so this will never
  // be called in CACHE_FAILURE state.

  UpdateRequestBase* request = fetcher->request();
  const GURL& url = request->GetURL();
  master_entry_fetches_.erase(url);
  ++master_entries_completed_;

  int response_code = net_error == net::OK ? request->GetResponseCode() : -1;

  auto found = pending_master_entries_.find(url);
  DCHECK(found != pending_master_entries_.end());
  PendingHosts& hosts = found->second;

  // Section 6.9.4. No update case: step 7.3, else step 22.
  if (response_code / 100 == 2) {
    // Add fetched master entry to the appropriate cache.
    AppCache* cache = inprogress_cache_.get() ? inprogress_cache_.get()
                                              : group_->newest_complete_cache();
    DCHECK(fetcher->response_writer());
    AppCacheEntry master_entry(AppCacheEntry::MASTER,
                               fetcher->response_writer()->response_id(),
                               fetcher->response_writer()->amount_written());
    if (cache->AddOrModifyEntry(url, master_entry))
      added_master_entries_.push_back(url);
    else
      duplicate_response_ids_.push_back(master_entry.response_id());

    // In no-update case, associate host with the newest cache.
    if (!inprogress_cache_.get()) {
      // TODO(michaeln): defer until the updated cache has been stored
      DCHECK(cache == group_->newest_complete_cache());
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

    const char kFormatString[] = "Manifest fetch failed (%d) %s";
    std::string message = FormatUrlErrorMessage(
        kFormatString, request->GetURL(), fetcher->result(), response_code);
    host_notifier.SendErrorNotifications(AppCacheErrorDetails(
        message, AppCacheErrorReason::APPCACHE_MANIFEST_ERROR,
        request->GetURL(), response_code, false /*is_cross_origin*/));

    // In downloading case, update result is different if all master entries
    // failed vs. only some failing.
    if (inprogress_cache_.get()) {
      // Only count successful downloads to know if all master entries failed.
      pending_master_entries_.erase(found);
      --master_entries_completed_;

      // Section 6.9.4, step 22.3.
      if (update_type_ == CACHE_ATTEMPT && pending_master_entries_.empty()) {
        HandleCacheFailure(
            AppCacheErrorDetails(
                message, AppCacheErrorReason::APPCACHE_MANIFEST_ERROR,
                request->GetURL(), response_code, false /*is_cross_origin*/),
            fetcher->result(), GURL());
        return;
      }
    }
  }

  DCHECK(internal_state_ != CACHE_FAILURE);
  FetchMasterEntries();
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleManifestRefetchCompleted(URLFetcher* fetcher,
                                                       int net_error) {
  DCHECK(internal_state_ == REFETCH_MANIFEST);
  DCHECK(manifest_fetcher_ == fetcher);
  manifest_fetcher_ = nullptr;

  int response_code =
      net_error == net::OK ? fetcher->request()->GetResponseCode() : -1;
  if (response_code == 304 || manifest_data_ == fetcher->manifest_data()) {
    // Only need to store response in storage if manifest is not already
    // an entry in the cache.
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
          AppCacheErrorDetails("Manifest changed during update",
                               AppCacheErrorReason::APPCACHE_CHANGED_ERROR,
                               GURL(), 0, false /*is_cross_origin*/),
          MANIFEST_ERROR, GURL());
    } else {
      const char kFormatString[] = "Manifest re-fetch failed (%d) %s";
      std::string message = FormatUrlErrorMessage(
          kFormatString, manifest_url_, fetcher->result(), response_code);
      HandleCacheFailure(
          AppCacheErrorDetails(
              message, AppCacheErrorReason::APPCACHE_MANIFEST_ERROR, GURL(),
              response_code, false /*is_cross_origin*/),
          fetcher->result(), GURL());
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
        AppCacheErrorDetails("Failed to write the manifest headers to storage",
                             AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                             GURL(), 0, false /*is_cross_origin*/),
        DISKCACHE_ERROR, GURL());
  }
}

void AppCacheUpdateJob::OnManifestDataWriteComplete(int result) {
  if (result > 0) {
    AppCacheEntry entry(AppCacheEntry::MANIFEST,
        manifest_response_writer_->response_id(),
        manifest_response_writer_->amount_written());
    if (!inprogress_cache_->AddOrModifyEntry(manifest_url_, entry))
      duplicate_response_ids_.push_back(entry.response_id());
    StoreGroupAndCache();
  } else {
    HandleCacheFailure(
        AppCacheErrorDetails("Failed to write the manifest data to storage",
                             AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                             GURL(), 0, false /*is_cross_origin*/),
        DISKCACHE_ERROR, GURL());
  }
}

void AppCacheUpdateJob::StoreGroupAndCache() {
  DCHECK(stored_state_ == UNSTORED);
  stored_state_ = STORING;

  scoped_refptr<AppCache> newest_cache;
  if (inprogress_cache_.get())
    newest_cache.swap(inprogress_cache_);
  else
    newest_cache = group_->newest_complete_cache();
  newest_cache->set_update_time(base::Time::Now());

  group_->set_first_evictable_error_time(base::Time());
  if (doing_full_update_check_)
    group_->set_last_full_update_check_time(base::Time::Now());

  storage_->StoreGroupAndNewestCache(group_, newest_cache.get(), this);
}

void AppCacheUpdateJob::OnGroupAndNewestCacheStored(AppCacheGroup* group,
                                                    AppCache* newest_cache,
                                                    bool success,
                                                    bool would_exceed_quota) {
  DCHECK(stored_state_ == STORING);
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
  AppCacheErrorReason reason = AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR;
  std::string message("Failed to commit new cache to storage");
  if (would_exceed_quota) {
    message.append(", would exceed quota");
    result = QUOTA_ERROR;
    reason = AppCacheErrorReason::APPCACHE_QUOTA_ERROR;
  }
  HandleCacheFailure(
      AppCacheErrorDetails(message, reason, GURL(), 0,
          false /*is_cross_origin*/),
      result,
      GURL());
}

void AppCacheUpdateJob::NotifySingleHost(AppCacheHost* host,
                                         AppCacheEventID event_id) {
  std::vector<int> ids(1, host->host_id());
  host->frontend()->OnEventRaised(ids, event_id);
}

void AppCacheUpdateJob::NotifyAllAssociatedHosts(AppCacheEventID event_id) {
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
  DCHECK(url_file_list_.size() == url_fetches_completed_);
  NotifyAllProgress(GURL());
}

void AppCacheUpdateJob::NotifyAllError(const AppCacheErrorDetails& details) {
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
    DCHECK(internal_state_ == DOWNLOADING || internal_state_ == CACHE_FAILURE);
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
  PendingHosts& hosts = found->second;
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
  DCHECK(update_type_ == UPGRADE_ATTEMPT);
  AppCacheEntry* entry = nullptr;
  if (group_->newest_complete_cache())
    entry = group_->newest_complete_cache()->GetEntry(manifest_url_);
  if (!entry) {
    // TODO(michaeln): This is just a bandaid to avoid a crash.
    // http://code.google.com/p/chromium/issues/detail?id=95101
    if (service_->storage() == storage_) {
      // Use a local variable because service_ is reset in HandleCacheFailure.
      AppCacheServiceImpl* service = service_;
      HandleCacheFailure(
          AppCacheErrorDetails("Manifest entry not found in existing cache",
                               AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                               GURL(), 0, false /*is_cross_origin*/),
          DB_ERROR, GURL());
      AppCacheHistograms::AddMissingManifestEntrySample();
      service->DeleteAppCacheGroup(manifest_url_,
                                   net::CompletionOnceCallback());
    }
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
  if (result > 0) {
    loaded_manifest_data_.append(read_manifest_buffer_->data(), result);
    manifest_response_reader_->ReadData(
        read_manifest_buffer_.get(), kAppCacheFetchBufferSize,
        base::BindOnce(&AppCacheUpdateJob::OnManifestDataReadComplete,
                       base::Unretained(this)));  // read more
  } else {
    read_manifest_buffer_ = nullptr;
    manifest_response_reader_.reset();
    ContinueHandleManifestFetchCompleted(
        result < 0 || manifest_data_ != loaded_manifest_data_);
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
  std::pair<AppCache::EntryMap::iterator, bool> ret = url_file_list_.insert(
      AppCache::EntryMap::value_type(url, AppCacheEntry(type)));

  if (ret.second)
    urls_to_fetch_.push_back(UrlToFetch(url, false, nullptr));
  else
    ret.first->second.add_types(type);  // URL already exists. Merge types.
}

void AppCacheUpdateJob::FetchUrls() {
  DCHECK(internal_state_ == DOWNLOADING);

  // Fetch each URL in the list according to section 6.9.4 step 17.1-17.3.
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
      URLFetcher* fetcher =
          new URLFetcher(url_to_fetch.url, URLFetcher::URL_FETCH, this,
                         kAppCacheFetchBufferSize);
      if (url_to_fetch.existing_response_info.get() &&
          group_->newest_complete_cache()) {
        AppCacheEntry* existing_entry =
            group_->newest_complete_cache()->GetEntry(url_to_fetch.url);
        DCHECK(existing_entry);
        DCHECK(existing_entry->response_id() ==
               url_to_fetch.existing_response_info->response_id());
        fetcher->set_existing_response_headers(
            url_to_fetch.existing_response_info->http_response_info()
                .headers.get());
        fetcher->set_existing_entry(*existing_entry);
      }
      fetcher->Start();
      pending_url_fetches_.insert(
          PendingUrlFetches::value_type(url_to_fetch.url, fetcher));
    }
  }
}

void AppCacheUpdateJob::CancelAllUrlFetches() {
  // Cancel any pending URL requests.
  for (auto& pair : pending_url_fetches_)
    delete pair.second;

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
  DCHECK(internal_state_ == DOWNLOADING || internal_state_ == NO_UPDATE);
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

  if (internal_state_ == DOWNLOADING || internal_state_ == NO_UPDATE) {
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
      if (internal_state_ == NO_UPDATE && !inprogress_cache_.get()) {
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
    if (internal_state_ == DOWNLOADING || internal_state_ == NO_UPDATE)
      FetchMasterEntries();
  }
}

void AppCacheUpdateJob::FetchMasterEntries() {
  DCHECK(internal_state_ == NO_UPDATE || internal_state_ == DOWNLOADING);

  // Fetch each master entry in the list, up to the concurrent limit.
  // Additional fetches will be triggered as each fetch completes.
  while (master_entry_fetches_.size() < kMaxConcurrentUrlFetches &&
         !master_entries_to_fetch_.empty()) {
    const GURL& url = *master_entries_to_fetch_.begin();

    if (AlreadyFetchedEntry(url, AppCacheEntry::MASTER)) {
      ++master_entries_completed_;  // saved a URL request

      // In no update case, associate hosts to newest cache in group
      // now that master entry has been "successfully downloaded".
      if (internal_state_ == NO_UPDATE) {
        // TODO(michaeln): defer until the updated cache has been stored.
        DCHECK(!inprogress_cache_.get());
        AppCache* cache = group_->newest_complete_cache();
        auto found = pending_master_entries_.find(url);
        DCHECK(found != pending_master_entries_.end());
        PendingHosts& hosts = found->second;
        for (AppCacheHost* host : hosts)
          host->AssociateCompleteCache(cache);
      }
    } else {
      URLFetcher* fetcher = new URLFetcher(url, URLFetcher::MASTER_ENTRY_FETCH,
                                           this, kAppCacheFetchBufferSize);
      fetcher->Start();
      master_entry_fetches_.insert(PendingUrlFetches::value_type(url, fetcher));
    }

    master_entries_to_fetch_.erase(master_entries_to_fetch_.begin());
  }
}

void AppCacheUpdateJob::CancelAllMasterEntryFetches(
    const AppCacheErrorDetails& error_details) {
  // For now, cancel all in-progress fetches for master entries and pretend
  // all master entries fetches have completed.
  // TODO(jennb): Delete this when update no longer fetches master entries
  // directly.

  // Cancel all in-progress fetches.
  for (auto& pair : master_entry_fetches_) {
    delete pair.second;
    master_entries_to_fetch_.insert(pair.first);  // back in unfetched list
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
    PendingHosts& hosts = found->second;
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
  loading_responses_.insert(
      LoadingResponses::value_type(copy_me->response_id(), url));
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
  if (internal_state_ == FETCH_MANIFEST) {
    if (http_info)
      manifest_fetcher_->set_existing_response_headers(
          http_info->headers.get());
    manifest_fetcher_->Start();
    return;
  }

  auto found = loading_responses_.find(response_id);
  DCHECK(found != loading_responses_.end());
  const GURL& url = found->second;

  if (!http_info) {
    LoadFromNewestCacheFailed(url, nullptr);  // no response found
  } else if (!CanUseExistingResource(http_info)) {
    LoadFromNewestCacheFailed(url, response_info);
  } else {
    DCHECK(group_->newest_complete_cache());
    AppCacheEntry* copy_me = group_->newest_complete_cache()->GetEntry(url);
    DCHECK(copy_me);
    DCHECK_EQ(copy_me->response_id(), response_id);

    auto it = url_file_list_.find(url);
    DCHECK(it != url_file_list_.end());
    AppCacheEntry& entry = it->second;
    entry.set_response_id(response_id);
    entry.set_response_size(copy_me->response_size());
    inprogress_cache_->AddOrModifyEntry(url, entry);
    NotifyAllProgress(url);
    ++url_fetches_completed_;
  }

  loading_responses_.erase(found);
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::LoadFromNewestCacheFailed(
    const GURL& url, AppCacheResponseInfo* response_info) {
  if (internal_state_ == CACHE_FAILURE)
    return;

  // Re-insert url at front of fetch list. Indicate storage has been checked.
  urls_to_fetch_.push_front(UrlToFetch(url, true, response_info));
  FetchUrls();
}

void AppCacheUpdateJob::MaybeCompleteUpdate() {
  DCHECK(internal_state_ != CACHE_FAILURE);

  // Must wait for any pending master entries or url fetches to complete.
  if (master_entries_completed_ != pending_master_entries_.size() ||
      url_fetches_completed_ != url_file_list_.size()) {
    DCHECK(internal_state_ != COMPLETED);
    return;
  }

  switch (internal_state_) {
    case NO_UPDATE:
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
      // 6.9.4 steps 7.3-7.7.
      NotifyAllAssociatedHosts(AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);
      DiscardDuplicateResponses();
      internal_state_ = COMPLETED;
      break;
    case DOWNLOADING:
      internal_state_ = REFETCH_MANIFEST;
      FetchManifest(false);
      break;
    case REFETCH_MANIFEST:
      DCHECK(stored_state_ == STORED);
      NotifyAllFinalProgress();
      group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);
      if (update_type_ == CACHE_ATTEMPT)
        NotifyAllAssociatedHosts(AppCacheEventID::APPCACHE_CACHED_EVENT);
      else
        NotifyAllAssociatedHosts(AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);
      DiscardDuplicateResponses();
      internal_state_ = COMPLETED;
      LogHistogramStats(UPDATE_OK, GURL());
      break;
    case CACHE_FAILURE:
      NOTREACHED();  // See HandleCacheFailure
      break;
    default:
      break;
  }

  // Let the stack unwind before deletion to make it less risky as this
  // method is called from multiple places in this file.
  if (internal_state_ == COMPLETED)
    DeleteSoon();
}

void AppCacheUpdateJob::ScheduleUpdateRetry(int delay_ms) {
  // TODO(jennb): post a delayed task with the "same parameters" as this job
  // to retry the update at a later time. Need group, URLs of pending master
  // entries and their hosts.
}

void AppCacheUpdateJob::Cancel() {
  internal_state_ = CANCELLED;

  LogHistogramStats(CANCELLED_ERROR, GURL());

  if (manifest_fetcher_) {
    delete manifest_fetcher_;
    manifest_fetcher_ = nullptr;
  }

  for (auto& pair : pending_url_fetches_)
    delete pair.second;
  pending_url_fetches_.clear();

  for (auto& pair : master_entry_fetches_)
    delete pair.second;
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
    PendingHosts& hosts = pair.second;
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

void AppCacheUpdateJob::LogHistogramStats(
      ResultType result, const GURL& failed_resource_url) {
  AppCacheHistograms::CountUpdateJobResult(result,
                                           url::Origin::Create(manifest_url_));
  if (result == UPDATE_OK)
    return;

  int percent_complete = 0;
  if (url_file_list_.size() > 0) {
    size_t actual_fetches_completed = url_fetches_completed_;
    if (!failed_resource_url.is_empty() && actual_fetches_completed)
      --actual_fetches_completed;
    percent_complete = (static_cast<double>(actual_fetches_completed) /
                            static_cast<double>(url_file_list_.size())) * 100.0;
    percent_complete = std::min(percent_complete, 99);
  }

  bool was_making_progress =
      base::Time::Now() - last_progress_time_ <
          base::TimeDelta::FromMinutes(5);

  bool off_origin_resource_failure =
      !failed_resource_url.is_empty() &&
          (failed_resource_url.GetOrigin() != manifest_url_.GetOrigin());

  AppCacheHistograms::LogUpdateFailureStats(
      url::Origin::Create(manifest_url_), percent_complete, was_making_progress,
      off_origin_resource_failure);
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

}  // namespace content
