// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_service_impl.h"

#include <functional>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/browser/appcache/appcache_policy.h"
#include "content/browser/appcache/appcache_quota_client.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/appcache_storage_impl.h"
#include "net/base/io_buffer.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

AppCacheInfoCollection::AppCacheInfoCollection() = default;

AppCacheInfoCollection::~AppCacheInfoCollection() = default;

// AsyncHelper -------

class AppCacheServiceImpl::AsyncHelper : public AppCacheStorage::Delegate {
 public:
  AsyncHelper(AppCacheServiceImpl* service, OnceCompletionCallback callback)
      : service_(service), callback_(std::move(callback)) {
    service_->pending_helpers_[this] = base::WrapUnique(this);
  }

  ~AsyncHelper() override {
    if (service_) {
      service_->pending_helpers_[this].release();
      service_->pending_helpers_.erase(this);
    }
  }

  virtual void Start() = 0;
  virtual void Cancel();

 protected:
  void CallCallback(int rv) {
    if (callback_) {
      // Defer to guarantee async completion.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_), rv));
    }
    DCHECK(!callback_);
  }

  AppCacheServiceImpl* service_;
  OnceCompletionCallback callback_;
};

void AppCacheServiceImpl::AsyncHelper::Cancel() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(net::ERR_ABORTED);
  }
  service_->storage()->CancelDelegateCallbacks(this);
  service_ = nullptr;
}

// DeleteHelper -------

class AppCacheServiceImpl::DeleteHelper : public AsyncHelper {
 public:
  DeleteHelper(AppCacheServiceImpl* service,
               const GURL& manifest_url,
               net::CompletionOnceCallback callback)
      : AsyncHelper(service, std::move(callback)),
        manifest_url_(manifest_url) {}

  void Start() override {
    service_->storage()->LoadOrCreateGroup(manifest_url_, this);
  }

 private:
  // AppCacheStorage::Delegate implementation.
  void OnGroupLoaded(AppCacheGroup* group, const GURL& manifest_url) override;
  void OnGroupMadeObsolete(AppCacheGroup* group,
                           bool success,
                           int response_code) override;

  GURL manifest_url_;
  DISALLOW_COPY_AND_ASSIGN(DeleteHelper);
};

void AppCacheServiceImpl::DeleteHelper::OnGroupLoaded(
      AppCacheGroup* group, const GURL& manifest_url) {
  if (group) {
    group->set_being_deleted(true);
    group->CancelUpdate();
    service_->storage()->MakeGroupObsolete(group, this, 0);
  } else {
    CallCallback(net::ERR_FAILED);
    delete this;
  }
}

void AppCacheServiceImpl::DeleteHelper::OnGroupMadeObsolete(
    AppCacheGroup* group,
    bool success,
    int response_code) {
  CallCallback(success ? net::OK : net::ERR_FAILED);
  delete this;
}

// DeleteOriginHelper -------

class AppCacheServiceImpl::DeleteOriginHelper : public AsyncHelper {
 public:
  DeleteOriginHelper(AppCacheServiceImpl* service,
                     const url::Origin& origin,
                     net::CompletionOnceCallback callback)
      : AsyncHelper(service, std::move(callback)),
        origin_(origin),
        num_caches_to_delete_(0),
        successes_(0),
        failures_(0) {}

  void Start() override {
    // We start by listing all caches, continues in OnAllInfo().
    service_->storage()->GetAllInfo(this);
  }

 private:
  // AppCacheStorage::Delegate implementation.
  void OnAllInfo(AppCacheInfoCollection* collection) override;
  void OnGroupLoaded(AppCacheGroup* group, const GURL& manifest_url) override;
  void OnGroupMadeObsolete(AppCacheGroup* group,
                           bool success,
                           int response_code) override;

  void CacheCompleted(bool success);

  url::Origin origin_;
  int num_caches_to_delete_;
  int successes_;
  int failures_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOriginHelper);
};

void AppCacheServiceImpl::DeleteOriginHelper::OnAllInfo(
    AppCacheInfoCollection* collection) {
  if (!collection) {
    // Failed to get a listing.
    CallCallback(net::ERR_FAILED);
    delete this;
    return;
  }

  auto found = collection->infos_by_origin.find(origin_);
  if (found == collection->infos_by_origin.end() || found->second.empty()) {
    // No caches for this origin.
    CallCallback(net::OK);
    delete this;
    return;
  }

  // We have some caches to delete.
  const AppCacheInfoVector& caches_to_delete = found->second;
  successes_ = 0;
  failures_ = 0;
  num_caches_to_delete_ = static_cast<int>(caches_to_delete.size());
  for (const auto& cache : caches_to_delete) {
    service_->storage()->LoadOrCreateGroup(cache.manifest_url, this);
  }
}

void AppCacheServiceImpl::DeleteOriginHelper::OnGroupLoaded(
      AppCacheGroup* group, const GURL& manifest_url) {
  if (group) {
    group->set_being_deleted(true);
    group->CancelUpdate();
    service_->storage()->MakeGroupObsolete(group, this, 0);
  } else {
    CacheCompleted(false);
  }
}

void AppCacheServiceImpl::DeleteOriginHelper::OnGroupMadeObsolete(
    AppCacheGroup* group,
    bool success,
    int response_code) {
  CacheCompleted(success);
}

void AppCacheServiceImpl::DeleteOriginHelper::CacheCompleted(bool success) {
  if (success)
    ++successes_;
  else
    ++failures_;
  if ((successes_ + failures_) < num_caches_to_delete_)
    return;

  CallCallback(!failures_ ? net::OK : net::ERR_FAILED);
  delete this;
}


// GetInfoHelper -------

class AppCacheServiceImpl::GetInfoHelper : AsyncHelper {
 public:
  GetInfoHelper(AppCacheServiceImpl* service,
                AppCacheInfoCollection* collection,
                OnceCompletionCallback callback)
      : AsyncHelper(service, std::move(callback)), collection_(collection) {}

  void Start() override { service_->storage()->GetAllInfo(this); }

 private:
  // AppCacheStorage::Delegate implementation.
  void OnAllInfo(AppCacheInfoCollection* collection) override;

  scoped_refptr<AppCacheInfoCollection> collection_;

  DISALLOW_COPY_AND_ASSIGN(GetInfoHelper);
};

void AppCacheServiceImpl::GetInfoHelper::OnAllInfo(
      AppCacheInfoCollection* collection) {
  if (collection)
    collection->infos_by_origin.swap(collection_->infos_by_origin);
  CallCallback(collection ? net::OK : net::ERR_FAILED);
  delete this;
}

// CheckResponseHelper -------

class AppCacheServiceImpl::CheckResponseHelper : AsyncHelper {
 public:
  CheckResponseHelper(AppCacheServiceImpl* service,
                      const GURL& manifest_url,
                      int64_t cache_id,
                      int64_t response_id)
      : AsyncHelper(service, net::CompletionOnceCallback()),
        manifest_url_(manifest_url),
        cache_id_(cache_id),
        response_id_(response_id),
        kIOBufferSize(32 * 1024),
        expected_total_size_(0),
        amount_headers_read_(0),
        amount_data_read_(0) {}

  void Start() override {
    service_->storage()->LoadOrCreateGroup(manifest_url_, this);
  }

  void Cancel() override {
    AppCacheHistograms::CountCheckResponseResult(
        AppCacheHistograms::CHECK_CANCELED);
    response_reader_.reset();
    AsyncHelper::Cancel();
  }

 private:
  void OnGroupLoaded(AppCacheGroup* group, const GURL& manifest_url) override;
  void OnReadInfoComplete(int result);
  void OnReadDataComplete(int result);

  // Inputs describing what to check.
  GURL manifest_url_;
  int64_t cache_id_;
  int64_t response_id_;

  // Internals used to perform the checks.
  const int kIOBufferSize;
  scoped_refptr<AppCache> cache_;
  std::unique_ptr<AppCacheResponseReader> response_reader_;
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer_;
  scoped_refptr<net::IOBuffer> data_buffer_;
  int64_t expected_total_size_;
  int amount_headers_read_;
  int amount_data_read_;
  DISALLOW_COPY_AND_ASSIGN(CheckResponseHelper);
};

void AppCacheServiceImpl::CheckResponseHelper::OnGroupLoaded(
    AppCacheGroup* group, const GURL& manifest_url) {
  DCHECK_EQ(manifest_url_, manifest_url);
  if (!group || !group->newest_complete_cache() || group->is_being_deleted() ||
      group->is_obsolete()) {
    AppCacheHistograms::CountCheckResponseResult(
        AppCacheHistograms::MANIFEST_OUT_OF_DATE);
    delete this;
    return;
  }

  cache_ = group->newest_complete_cache();
  const AppCacheEntry* entry = cache_->GetEntryWithResponseId(response_id_);
  if (!entry) {
    if (cache_->cache_id() == cache_id_) {
      AppCacheHistograms::CountCheckResponseResult(
          AppCacheHistograms::ENTRY_NOT_FOUND);
      service_->DeleteAppCacheGroup(manifest_url_,
                                    net::CompletionOnceCallback());
    } else {
      AppCacheHistograms::CountCheckResponseResult(
          AppCacheHistograms::RESPONSE_OUT_OF_DATE);
    }
    delete this;
    return;
  }

  // Verify that we can read the response info and data.
  expected_total_size_ = entry->response_size();
  response_reader_ =
      service_->storage()->CreateResponseReader(manifest_url_, response_id_);
  info_buffer_ = new HttpResponseInfoIOBuffer();
  response_reader_->ReadInfo(
      info_buffer_.get(),
      base::BindOnce(&CheckResponseHelper::OnReadInfoComplete,
                     base::Unretained(this)));
}

void AppCacheServiceImpl::CheckResponseHelper::OnReadInfoComplete(int result) {
  if (result < 0) {
    AppCacheHistograms::CountCheckResponseResult(
        AppCacheHistograms::READ_HEADERS_ERROR);
    service_->DeleteAppCacheGroup(manifest_url_, net::CompletionOnceCallback());
    delete this;
    return;
  }
  amount_headers_read_ = result;

  // Start reading the data.
  data_buffer_ = base::MakeRefCounted<net::IOBuffer>(kIOBufferSize);
  response_reader_->ReadData(
      data_buffer_.get(), kIOBufferSize,
      base::BindOnce(&CheckResponseHelper::OnReadDataComplete,
                     base::Unretained(this)));
}

void AppCacheServiceImpl::CheckResponseHelper::OnReadDataComplete(int result) {
  if (result > 0) {
    // Keep reading until we've read thru everything or failed to read.
    amount_data_read_ += result;
    response_reader_->ReadData(
        data_buffer_.get(), kIOBufferSize,
        base::BindOnce(&CheckResponseHelper::OnReadDataComplete,
                       base::Unretained(this)));
    return;
  }

  AppCacheHistograms::CheckResponseResultType check_result;
  if (result < 0)
    check_result = AppCacheHistograms::READ_DATA_ERROR;
  else if (info_buffer_->response_data_size != amount_data_read_ ||
           expected_total_size_ != amount_data_read_ + amount_headers_read_)
    check_result = AppCacheHistograms::UNEXPECTED_DATA_SIZE;
  else
    check_result = AppCacheHistograms::RESPONSE_OK;
  AppCacheHistograms::CountCheckResponseResult(check_result);

  if (check_result != AppCacheHistograms::RESPONSE_OK)
    service_->DeleteAppCacheGroup(manifest_url_, net::CompletionOnceCallback());
  delete this;
}

// AppCacheStorageReference ------

AppCacheStorageReference::AppCacheStorageReference(
    std::unique_ptr<AppCacheStorage> storage)
    : storage_(std::move(storage)) {}
AppCacheStorageReference::~AppCacheStorageReference() {}

// AppCacheServiceImpl -------

AppCacheServiceImpl::AppCacheServiceImpl(
    storage::QuotaManagerProxy* quota_manager_proxy)
    : db_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      appcache_policy_(nullptr),
      quota_client_(nullptr),
      quota_manager_proxy_(quota_manager_proxy),
      request_context_(nullptr),
      force_keep_session_state_(false),
      weak_factory_(this) {
  if (quota_manager_proxy_.get()) {
    quota_client_ = new AppCacheQuotaClient(this);
    quota_manager_proxy_->RegisterClient(quota_client_);
  }
}

AppCacheServiceImpl::~AppCacheServiceImpl() {
  DCHECK(backends_.empty());
  for (auto& observer : observers_)
    observer.OnServiceDestructionImminent(this);
  for (auto& helper : pending_helpers_)
    helper.first->Cancel();
  pending_helpers_.clear();
  if (quota_client_)
    quota_client_->NotifyAppCacheDestroyed();

  // Destroy storage_ first; ~AppCacheStorageImpl accesses other data members
  // (special_storage_policy_).
  storage_.reset();
}

void AppCacheServiceImpl::Initialize(const base::FilePath& cache_directory) {
  DCHECK(!storage_.get());
  cache_directory_ = cache_directory;
  AppCacheStorageImpl* storage = new AppCacheStorageImpl(this);
  storage->Initialize(cache_directory, db_task_runner_);
  storage_.reset(storage);
}

void AppCacheServiceImpl::ScheduleReinitialize() {
  if (reinit_timer_.IsRunning())
    return;

  // Reinitialization only happens when corruption has been noticed.
  // We don't want to thrash the disk but we also don't want to
  // leave the appcache disabled for an indefinite period of time. Some
  // users never shutdown the browser.

  constexpr base::TimeDelta kZeroDelta;
  constexpr base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);
  constexpr base::TimeDelta kThirtySeconds = base::TimeDelta::FromSeconds(30);

  // If the system managed to stay up for long enough, reset the
  // delay so a new failure won't incur a long wait to get going again.
  base::TimeDelta up_time = base::Time::Now() - last_reinit_time_;
  if (next_reinit_delay_ != kZeroDelta && up_time > kOneHour)
    next_reinit_delay_ = kZeroDelta;

  reinit_timer_.Start(FROM_HERE, next_reinit_delay_,
                      this, &AppCacheServiceImpl::Reinitialize);

  // Adjust the delay for next time.
  base::TimeDelta increment = std::max(kThirtySeconds, next_reinit_delay_);
  next_reinit_delay_ = std::min(next_reinit_delay_ + increment,  kOneHour);
}

void AppCacheServiceImpl::Reinitialize() {
  AppCacheHistograms::CountReinitAttempt(!last_reinit_time_.is_null());
  last_reinit_time_ = base::Time::Now();

  // Inform observers of about this and give them a chance to
  // defer deletion of the old storage object.
  scoped_refptr<AppCacheStorageReference> old_storage_ref(
      new AppCacheStorageReference(std::move(storage_)));
  for (auto& observer : observers_)
    observer.OnServiceReinitialized(old_storage_ref.get());

  Initialize(cache_directory_);
}

void AppCacheServiceImpl::GetAllAppCacheInfo(AppCacheInfoCollection* collection,
                                             OnceCompletionCallback callback) {
  DCHECK(collection);
  GetInfoHelper* helper =
      new GetInfoHelper(this, collection, std::move(callback));
  helper->Start();
}

void AppCacheServiceImpl::DeleteAppCacheGroup(
    const GURL& manifest_url,
    net::CompletionOnceCallback callback) {
  DeleteHelper* helper =
      new DeleteHelper(this, manifest_url, std::move(callback));
  helper->Start();
}

void AppCacheServiceImpl::DeleteAppCachesForOrigin(
    const url::Origin& origin,
    net::CompletionOnceCallback callback) {
  DeleteOriginHelper* helper =
      new DeleteOriginHelper(this, origin, std::move(callback));
  helper->Start();
}

void AppCacheServiceImpl::CheckAppCacheResponse(const GURL& manifest_url,
                                                int64_t cache_id,
                                                int64_t response_id) {
  CheckResponseHelper* helper = new CheckResponseHelper(
      this, manifest_url, cache_id, response_id);
  helper->Start();
}

void AppCacheServiceImpl::set_special_storage_policy(
    storage::SpecialStoragePolicy* policy) {
  special_storage_policy_ = policy;
}

void AppCacheServiceImpl::RegisterBackend(
    AppCacheBackendImpl* backend_impl) {
  DCHECK(backends_.find(backend_impl->process_id()) == backends_.end());
  backends_.insert({backend_impl->process_id(), backend_impl});
}

void AppCacheServiceImpl::UnregisterBackend(
    AppCacheBackendImpl* backend_impl) {
  backends_.erase(backend_impl->process_id());
}

}  // namespace content
