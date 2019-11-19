// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_storage_impl.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_database.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/browser/appcache/appcache_quota_client.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "sql/database.h"
#include "sql/transaction.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {

namespace {

constexpr const int kMB = 1024 * 1024;

// Hard coded default when not using quota management.
constexpr const int kDefaultQuota = 5 * kMB;

constexpr base::FilePath::CharType kDiskCacheDirectoryName[] =
    FILE_PATH_LITERAL("Cache");
constexpr base::FilePath::CharType kAppCacheDatabaseName[] =
    FILE_PATH_LITERAL("Index");

// Helpers for clearing data from the AppCacheDatabase.
bool DeleteGroupAndRelatedRecords(
    AppCacheDatabase* database,
    int64_t group_id,
    std::vector<int64_t>* deletable_response_ids) {
  AppCacheDatabase::CacheRecord cache_record;
  bool success = false;
  if (database->FindCacheForGroup(group_id, &cache_record)) {
    database->FindResponseIdsForCacheAsVector(cache_record.cache_id,
                                              deletable_response_ids);
    success =
        database->DeleteGroup(group_id) &&
        database->DeleteCache(cache_record.cache_id) &&
        database->DeleteEntriesForCache(cache_record.cache_id) &&
        database->DeleteNamespacesForCache(cache_record.cache_id) &&
        database->DeleteOnlineWhiteListForCache(cache_record.cache_id) &&
        database->InsertDeletableResponseIds(*deletable_response_ids);
  } else {
    NOTREACHED() << "A existing group without a cache is unexpected";
    success = database->DeleteGroup(group_id);
  }
  return success;
}

}  // namespace

// static
void AppCacheStorageImpl::ClearSessionOnlyOrigins(
    std::unique_ptr<AppCacheDatabase> database,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    bool force_keep_session_state) {
  // If saving session state, only delete the database.
  if (force_keep_session_state)
    return;

  bool has_session_only_appcaches =
      special_storage_policy.get() &&
      special_storage_policy->HasSessionOnlyOrigins();

  // Clearning only session-only databases, and there are none.
  if (!has_session_only_appcaches)
    return;

  std::set<url::Origin> origins;
  database->FindOriginsWithGroups(&origins);
  if (origins.empty())
    return;  // nothing to delete

  sql::Database* connection = database->db_connection();
  if (!connection) {
    NOTREACHED() << "Missing database connection.";
    return;
  }

  for (const url::Origin& origin : origins) {
    if (!special_storage_policy->IsStorageSessionOnly(origin.GetURL()))
      continue;
    if (special_storage_policy->IsStorageProtected(origin.GetURL()))
      continue;

    std::vector<AppCacheDatabase::GroupRecord> groups;
    database->FindGroupsForOrigin(origin, &groups);
    for (const auto& group : groups) {
      sql::Transaction transaction(connection);
      if (!transaction.Begin()) {
        NOTREACHED() << "Failed to start transaction";
        return;
      }
      std::vector<int64_t> deletable_response_ids;
      bool success = DeleteGroupAndRelatedRecords(
          database.get(), group.group_id, &deletable_response_ids);
      success = success && transaction.Commit();
      DCHECK(success);
    }  // for each group
  }  // for each origin
}

// DatabaseTask -----------------------------------------

class AppCacheStorageImpl::DatabaseTask
    : public base::RefCountedThreadSafe<DatabaseTask> {
 public:
  explicit DatabaseTask(AppCacheStorageImpl* storage)
      : storage_(storage),
        database_(storage->database_.get()),
        io_thread_(base::SequencedTaskRunnerHandle::Get()) {
    DCHECK(io_thread_.get());
  }

  void AddDelegate(DelegateReference* delegate_reference) {
    delegates_.push_back(base::WrapRefCounted(delegate_reference));
  }

  // Schedules a task to be Run() on the DB thread. Tasks
  // are run in the order in which they are scheduled.
  void Schedule();

  // Called on the DB thread.
  virtual void Run() = 0;

  // Called on the IO thread after Run() has completed.
  virtual void RunCompleted() {}

  // Once scheduled a task cannot be cancelled, but the
  // call to RunCompleted may be. This method should only be
  // called on the IO thread. This is used by AppCacheStorageImpl
  // to cancel the completion calls when AppCacheStorageImpl is
  // destructed. This method may be overriden to release or delete
  // additional data associated with the task that is not DB thread
  // safe. If overriden, this base class method must be called from
  // within the override.
  virtual void CancelCompletion();

 protected:
  friend class base::RefCountedThreadSafe<DatabaseTask>;
  virtual ~DatabaseTask() {}

  AppCacheStorageImpl* storage_;
  AppCacheDatabase* const database_;
  std::vector<scoped_refptr<DelegateReference>> delegates_;

 private:
  void CallRun();
  void CallRunCompleted();
  void OnFatalError();

  const scoped_refptr<base::SequencedTaskRunner> io_thread_;
};

void AppCacheStorageImpl::DatabaseTask::Schedule() {
  DCHECK(storage_);
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  if (!storage_->database_)
    return;

  if (storage_->db_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&DatabaseTask::CallRun, this))) {
    storage_->scheduled_database_tasks_.push_back(this);
  } else {
    NOTREACHED() << "Thread for database tasks is not running.";
  }
}

void AppCacheStorageImpl::DatabaseTask::CancelCompletion() {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  delegates_.clear();
  storage_ = nullptr;
}

void AppCacheStorageImpl::DatabaseTask::CallRun() {
  if (!database_->is_disabled()) {
    Run();
    if (database_->was_corruption_detected()) {
      database_->Disable();
    }
    if (database_->is_disabled()) {
      io_thread_->PostTask(FROM_HERE,
                           base::BindOnce(&DatabaseTask::OnFatalError, this));
    }
  }
  io_thread_->PostTask(FROM_HERE,
                       base::BindOnce(&DatabaseTask::CallRunCompleted, this));
}

void AppCacheStorageImpl::DatabaseTask::CallRunCompleted() {
  if (storage_) {
    DCHECK(io_thread_->RunsTasksInCurrentSequence());
    DCHECK(storage_->scheduled_database_tasks_.front() == this);
    storage_->scheduled_database_tasks_.pop_front();
    RunCompleted();
    delegates_.clear();
  }
}

void AppCacheStorageImpl::DatabaseTask::OnFatalError() {
  if (storage_) {
    DCHECK(io_thread_->RunsTasksInCurrentSequence());
    storage_->Disable();
    storage_->DeleteAndStartOver();
  }
}

// InitTask -------

class AppCacheStorageImpl::InitTask : public DatabaseTask {
 public:
  explicit InitTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage), last_group_id_(0),
        last_cache_id_(0), last_response_id_(0),
        last_deletable_response_rowid_(0) {
    if (!storage->is_incognito_) {
      db_file_path_ =
          storage->cache_directory_.Append(kAppCacheDatabaseName);
      disk_cache_directory_ =
          storage->cache_directory_.Append(kDiskCacheDirectoryName);
    }
  }

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;

 protected:
  ~InitTask() override {}

 private:
  base::FilePath db_file_path_;
  base::FilePath disk_cache_directory_;
  int64_t last_group_id_;
  int64_t last_cache_id_;
  int64_t last_response_id_;
  int64_t last_deletable_response_rowid_;
  std::map<url::Origin, int64_t> usage_map_;
};

void AppCacheStorageImpl::InitTask::Run() {
  // If there is no sql database, ensure there is no disk cache either.
  if (!db_file_path_.empty() &&
      !base::PathExists(db_file_path_) &&
      base::DirectoryExists(disk_cache_directory_)) {
    base::DeleteFile(disk_cache_directory_, true);
    if (base::DirectoryExists(disk_cache_directory_)) {
      database_->Disable();  // This triggers OnFatalError handling.
      return;
    }
  }

  database_->FindLastStorageIds(
      &last_group_id_, &last_cache_id_, &last_response_id_,
      &last_deletable_response_rowid_);
  database_->GetAllOriginUsage(&usage_map_);
}

void AppCacheStorageImpl::InitTask::RunCompleted() {
  storage_->last_group_id_ = last_group_id_;
  storage_->last_cache_id_ = last_cache_id_;
  storage_->last_response_id_ = last_response_id_;
  storage_->last_deletable_response_rowid_ = last_deletable_response_rowid_;

  if (!storage_->is_disabled()) {
    storage_->usage_map_.swap(usage_map_);
    const base::TimeDelta kDelay = base::TimeDelta::FromMinutes(5);
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AppCacheStorageImpl::DelayedStartDeletingUnusedResponses,
            storage_->weak_factory_.GetWeakPtr()),
        kDelay);
  }

  if (storage_->service()->quota_manager_proxy()) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&AppCacheQuotaClient::NotifyAppCacheReady,
                                  storage_->service()->quota_client()));
  }
}

// DisableDatabaseTask -------

class AppCacheStorageImpl::DisableDatabaseTask : public DatabaseTask {
 public:
  explicit DisableDatabaseTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}

  // DatabaseTask:
  void Run() override { database_->Disable(); }

 protected:
  ~DisableDatabaseTask() override {}
};

// GetAllInfoTask -------

class AppCacheStorageImpl::GetAllInfoTask : public DatabaseTask {
 public:
  explicit GetAllInfoTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage),
        info_collection_(base::MakeRefCounted<AppCacheInfoCollection>()) {}

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;

 protected:
  ~GetAllInfoTask() override {}

 private:
  scoped_refptr<AppCacheInfoCollection> info_collection_;
};

void AppCacheStorageImpl::GetAllInfoTask::Run() {
  std::set<url::Origin> origins;
  database_->FindOriginsWithGroups(&origins);
  for (const url::Origin& origin : origins) {
    std::vector<blink::mojom::AppCacheInfo>& infos =
        info_collection_->infos_by_origin[origin];
    std::vector<AppCacheDatabase::GroupRecord> groups;
    database_->FindGroupsForOrigin(origin, &groups);
    for (const auto& group : groups) {
      AppCacheDatabase::CacheRecord cache_record;
      database_->FindCacheForGroup(group.group_id, &cache_record);
      blink::mojom::AppCacheInfo info;
      info.manifest_url = group.manifest_url;
      info.creation_time = group.creation_time;
      info.response_sizes = cache_record.cache_size;
      info.padding_sizes = cache_record.padding_size;
      info.last_access_time = group.last_access_time;
      info.last_update_time = cache_record.update_time;
      info.cache_id = cache_record.cache_id;
      info.group_id = group.group_id;
      info.is_complete = true;
      infos.push_back(info);
    }
  }
}

void AppCacheStorageImpl::GetAllInfoTask::RunCompleted() {
  DCHECK_EQ(1U, delegates_.size());
  AppCacheStorage::ForEachDelegate(
      delegates_, [&](AppCacheStorage::Delegate* delegate) {
        delegate->OnAllInfo(info_collection_.get());
      });
}

// StoreOrLoadTask -------

class AppCacheStorageImpl::StoreOrLoadTask : public DatabaseTask {
 protected:
  explicit StoreOrLoadTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}
  ~StoreOrLoadTask() override {}

  bool FindRelatedCacheRecords(int64_t cache_id);
  void CreateCacheAndGroupFromRecords(
      scoped_refptr<AppCache>* cache, scoped_refptr<AppCacheGroup>* group);

  AppCacheDatabase::GroupRecord group_record_;
  AppCacheDatabase::CacheRecord cache_record_;
  std::vector<AppCacheDatabase::EntryRecord> entry_records_;
  std::vector<AppCacheDatabase::NamespaceRecord>
      intercept_namespace_records_;
  std::vector<AppCacheDatabase::NamespaceRecord>
      fallback_namespace_records_;
  std::vector<AppCacheDatabase::OnlineWhiteListRecord>
      online_whitelist_records_;
};

bool AppCacheStorageImpl::StoreOrLoadTask::FindRelatedCacheRecords(
    int64_t cache_id) {
  return database_->FindEntriesForCache(cache_id, &entry_records_) &&
         database_->FindNamespacesForCache(
             cache_id, &intercept_namespace_records_,
             &fallback_namespace_records_) &&
         database_->FindOnlineWhiteListForCache(
             cache_id, &online_whitelist_records_);
}

void AppCacheStorageImpl::StoreOrLoadTask::CreateCacheAndGroupFromRecords(
    scoped_refptr<AppCache>* cache, scoped_refptr<AppCacheGroup>* group) {
  DCHECK(storage_ && cache && group);

  (*cache) = storage_->working_set_.GetCache(cache_record_.cache_id);
  if (cache->get()) {
    (*group) = cache->get()->owning_group();
    DCHECK(group->get());
    DCHECK_EQ(group_record_.group_id, group->get()->group_id());

    // TODO(pwnall): A removed histogram shows that, very rarely,
    //               cache->get()->GetEntry(group_record_.manifest_url))
    //               return null here. This was supposed to help investigate
    //               https://crbug.com/95101
    storage_->NotifyStorageAccessed(group_record_.origin);
    return;
  }

  *cache = base::MakeRefCounted<AppCache>(storage_, cache_record_.cache_id);
  cache->get()->InitializeWithDatabaseRecords(
      cache_record_, entry_records_,
      intercept_namespace_records_,
      fallback_namespace_records_,
      online_whitelist_records_);
  cache->get()->set_complete(true);

  *group = storage_->working_set_.GetGroup(group_record_.manifest_url);
  if (group->get()) {
    DCHECK(group_record_.group_id == group->get()->group_id());
    group->get()->AddCache(cache->get());
  } else {
    *group = base::MakeRefCounted<AppCacheGroup>(
        storage_, group_record_.manifest_url, group_record_.group_id);
    group->get()->set_creation_time(group_record_.creation_time);
    group->get()->set_last_full_update_check_time(
        group_record_.last_full_update_check_time);
    group->get()->set_first_evictable_error_time(
        group_record_.first_evictable_error_time);
    group->get()->AddCache(cache->get());

    // TODO(pwnall): A removed histogram shows that, very rarely,
    //               cache->get()->GetEntry(group_record_.manifest_url))
    //               return null here. This was supposed to help investigate
    //               https://crbug.com/95101
  }
  DCHECK(group->get()->newest_complete_cache() == cache->get());

  // We have to update foriegn entries if MarkEntryAsForeignTasks
  // are in flight.
  std::vector<GURL> urls =
      storage_->GetPendingForeignMarkingsForCache(cache->get()->cache_id());
  for (const auto& url : urls) {
    // Skip any entries that were marked as foreign but that don't actually
    // exist. This shouldn't happen other than with misbehaving renderers, but
    // we've always just ignored these when the cache already exists when
    // MarkEntryAsForeign is called, so also ignore them here when the cache
    // still had to be created.
    // If AppCache wouldn't be in maintenance mode only, we might want to
    // (async) ReportBadMessage here and in MarkEntryAsForeign, and deal
    // with any resulting crashes, but for now just keep the existing behavior.
    if (!cache->get()->GetEntry(url))
      continue;
    cache->get()->GetEntry(url)->add_types(AppCacheEntry::FOREIGN);
  }

  storage_->NotifyStorageAccessed(group_record_.origin);

  // TODO(michaeln): Maybe verify that the responses we expect to exist
  // do actually exist in the disk_cache (and if not then what?)
}

// CacheLoadTask -------

class AppCacheStorageImpl::CacheLoadTask : public StoreOrLoadTask {
 public:
  CacheLoadTask(int64_t cache_id, AppCacheStorageImpl* storage)
      : StoreOrLoadTask(storage), cache_id_(cache_id), success_(false) {}

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;

 protected:
  ~CacheLoadTask() override {}

 private:
  int64_t cache_id_;
  bool success_;
};

void AppCacheStorageImpl::CacheLoadTask::Run() {
  success_ =
      database_->FindCache(cache_id_, &cache_record_) &&
      database_->FindGroup(cache_record_.group_id, &group_record_) &&
      FindRelatedCacheRecords(cache_id_);

  if (success_)
    database_->LazyUpdateLastAccessTime(group_record_.group_id,
                                        base::Time::Now());
}

void AppCacheStorageImpl::CacheLoadTask::RunCompleted() {
  storage_->pending_cache_loads_.erase(cache_id_);
  scoped_refptr<AppCache> cache;
  scoped_refptr<AppCacheGroup> group;
  if (success_ && !storage_->is_disabled()) {
    storage_->LazilyCommitLastAccessTimes();
    DCHECK(cache_record_.cache_id == cache_id_);
    CreateCacheAndGroupFromRecords(&cache, &group);
  }
  AppCacheStorage::ForEachDelegate(
      delegates_, [&](AppCacheStorage::Delegate* delegate) {
        delegate->OnCacheLoaded(cache.get(), cache_id_);
      });
}

// GroupLoadTask -------

class AppCacheStorageImpl::GroupLoadTask : public StoreOrLoadTask {
 public:
  GroupLoadTask(GURL manifest_url, AppCacheStorageImpl* storage)
      : StoreOrLoadTask(storage), manifest_url_(manifest_url),
        success_(false) {}

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;

 protected:
  ~GroupLoadTask() override {}

 private:
  GURL manifest_url_;
  bool success_;
};

void AppCacheStorageImpl::GroupLoadTask::Run() {
  success_ =
      database_->FindGroupForManifestUrl(manifest_url_, &group_record_) &&
      database_->FindCacheForGroup(group_record_.group_id, &cache_record_) &&
      FindRelatedCacheRecords(cache_record_.cache_id);

  if (success_) {
    database_->LazyUpdateLastAccessTime(group_record_.group_id,
                                        base::Time::Now());
  }
}

void AppCacheStorageImpl::GroupLoadTask::RunCompleted() {
  storage_->pending_group_loads_.erase(manifest_url_);
  scoped_refptr<AppCacheGroup> group;
  scoped_refptr<AppCache> cache;
  if (!storage_->is_disabled()) {
    if (success_) {
      storage_->LazilyCommitLastAccessTimes();
      DCHECK(group_record_.manifest_url == manifest_url_);
      CreateCacheAndGroupFromRecords(&cache, &group);
    } else {
      group = storage_->working_set_.GetGroup(manifest_url_);
      if (!group.get()) {
        group = base::MakeRefCounted<AppCacheGroup>(storage_, manifest_url_,
                                                    storage_->NewGroupId());
      }
    }
  }
  AppCacheStorage::ForEachDelegate(
      delegates_, [&](AppCacheStorage::Delegate* delegate) {
        delegate->OnGroupLoaded(group.get(), manifest_url_);
      });
}

// StoreGroupAndCacheTask -------

class AppCacheStorageImpl::StoreGroupAndCacheTask : public StoreOrLoadTask {
 public:
  StoreGroupAndCacheTask(AppCacheStorageImpl* storage, AppCacheGroup* group,
                         AppCache* newest_cache);

  void GetQuotaThenSchedule();
  void OnQuotaCallback(blink::mojom::QuotaStatusCode status,
                       int64_t usage,
                       int64_t quota);

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;
  void CancelCompletion() override;

 protected:
  ~StoreGroupAndCacheTask() override {}

 private:
  scoped_refptr<AppCacheGroup> group_;
  scoped_refptr<AppCache> cache_;
  bool success_;
  bool would_exceed_quota_;
  int64_t space_available_;
  int64_t new_origin_usage_;
  std::vector<int64_t> newly_deletable_response_ids_;
};

AppCacheStorageImpl::StoreGroupAndCacheTask::StoreGroupAndCacheTask(
    AppCacheStorageImpl* storage,
    AppCacheGroup* group,
    AppCache* newest_cache)
    : StoreOrLoadTask(storage),
      group_(group),
      cache_(newest_cache),
      success_(false),
      would_exceed_quota_(false),
      space_available_(-1),
      new_origin_usage_(-1) {
  group_record_.group_id = group->group_id();
  group_record_.manifest_url = group->manifest_url();
  group_record_.origin = url::Origin::Create(group_record_.manifest_url);
  group_record_.last_full_update_check_time =
      group->last_full_update_check_time();
  group_record_.first_evictable_error_time =
      group->first_evictable_error_time();
  newest_cache->ToDatabaseRecords(
      group,
      &cache_record_, &entry_records_,
      &intercept_namespace_records_,
      &fallback_namespace_records_,
      &online_whitelist_records_);
}

void AppCacheStorageImpl::StoreGroupAndCacheTask::GetQuotaThenSchedule() {
  if (!storage_->service()->quota_manager_proxy()) {
    if (storage_->service()->special_storage_policy() &&
        storage_->service()->special_storage_policy()->IsStorageUnlimited(
            group_record_.origin.GetURL()))
      space_available_ = std::numeric_limits<int64_t>::max();
    Schedule();
    return;
  }

  // We have to ask the quota manager for the value.
  storage_->pending_quota_queries_.insert(this);
  storage_->service()->quota_manager_proxy()->GetUsageAndQuota(
      base::ThreadTaskRunnerHandle::Get().get(), group_record_.origin,
      blink::mojom::StorageType::kTemporary,
      base::BindOnce(&StoreGroupAndCacheTask::OnQuotaCallback, this));
}

void AppCacheStorageImpl::StoreGroupAndCacheTask::OnQuotaCallback(
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  if (storage_) {
    if (status == blink::mojom::QuotaStatusCode::kOk)
      space_available_ = std::max(static_cast<int64_t>(0), quota - usage);
    else
      space_available_ = 0;
    storage_->pending_quota_queries_.erase(this);
    Schedule();
  }
}

void AppCacheStorageImpl::StoreGroupAndCacheTask::Run() {
  DCHECK(!success_);
  sql::Database* const connection = database_->db_connection();
  if (!connection)
    return;

  sql::Transaction transaction(connection);
  if (!transaction.Begin())
    return;

  int64_t old_origin_usage = database_->GetOriginUsage(group_record_.origin);

  AppCacheDatabase::GroupRecord existing_group;
  success_ = database_->FindGroup(group_record_.group_id, &existing_group);
  if (!success_) {
    group_record_.creation_time = base::Time::Now();
    group_record_.last_access_time = base::Time::Now();
    success_ = database_->InsertGroup(&group_record_);
  } else {
    DCHECK(group_record_.group_id == existing_group.group_id);
    DCHECK(group_record_.manifest_url == existing_group.manifest_url);
    DCHECK(group_record_.origin == existing_group.origin);

    database_->UpdateLastAccessTime(group_record_.group_id,
                                    base::Time::Now());

    database_->UpdateEvictionTimes(
        group_record_.group_id,
        group_record_.last_full_update_check_time,
        group_record_.first_evictable_error_time);

    AppCacheDatabase::CacheRecord cache;
    if (database_->FindCacheForGroup(group_record_.group_id, &cache)) {
      // Get the set of response ids in the old cache.
      std::set<int64_t> existing_response_ids;
      database_->FindResponseIdsForCacheAsSet(cache.cache_id,
                                              &existing_response_ids);

      // Remove those that remain in the new cache.
      for (const auto& entry : entry_records_)
        existing_response_ids.erase(entry.response_id);

      // The rest are deletable.
      for (const auto& id : existing_response_ids)
        newly_deletable_response_ids_.push_back(id);

      success_ =
          database_->DeleteCache(cache.cache_id) &&
          database_->DeleteEntriesForCache(cache.cache_id) &&
          database_->DeleteNamespacesForCache(cache.cache_id) &&
          database_->DeleteOnlineWhiteListForCache(cache.cache_id) &&
          database_->InsertDeletableResponseIds(newly_deletable_response_ids_);
          // TODO(michaeln): store group_id too with deletable ids
    } else {
      NOTREACHED() << "A existing group without a cache is unexpected";
    }
  }

  success_ =
      success_ &&
      database_->InsertCache(&cache_record_) &&
      database_->InsertEntryRecords(entry_records_) &&
      database_->InsertNamespaceRecords(intercept_namespace_records_) &&
      database_->InsertNamespaceRecords(fallback_namespace_records_) &&
      database_->InsertOnlineWhiteListRecords(online_whitelist_records_);

  if (!success_)
    return;

  new_origin_usage_ = database_->GetOriginUsage(group_record_.origin);

  // Only check quota when the new usage exceeds the old usage.
  if (new_origin_usage_ <= old_origin_usage) {
    success_ = transaction.Commit();
    return;
  }

  // Use a simple hard-coded value when not using quota management.
  if (space_available_ == -1) {
    if (new_origin_usage_ > kDefaultQuota) {
      would_exceed_quota_ = true;
      success_ = false;
      return;
    }
    success_ = transaction.Commit();
    return;
  }

  // Check limits based on the space availbable given to us via the
  // quota system.
  int64_t delta = new_origin_usage_ - old_origin_usage;
  if (delta > space_available_) {
    would_exceed_quota_ = true;
    success_ = false;
    return;
  }

  success_ = transaction.Commit();
}

void AppCacheStorageImpl::StoreGroupAndCacheTask::RunCompleted() {
  if (success_) {
    storage_->UpdateUsageMapAndNotify(
        url::Origin::Create(group_->manifest_url()), new_origin_usage_);
    if (cache_.get() != group_->newest_complete_cache()) {
      cache_->set_complete(true);
      group_->AddCache(cache_.get());
    }
    if (group_->creation_time().is_null())
      group_->set_creation_time(group_record_.creation_time);
    group_->AddNewlyDeletableResponseIds(&newly_deletable_response_ids_);
  }
  AppCacheStorage::ForEachDelegate(
      delegates_, [&](AppCacheStorage::Delegate* delegate) {
        delegate->OnGroupAndNewestCacheStored(group_.get(), cache_.get(),
                                              success_, would_exceed_quota_);
      });
  group_ = nullptr;
  cache_ = nullptr;

  // TODO(michaeln): if (would_exceed_quota_) what if the current usage
  // also exceeds the quota? http://crbug.com/83968
}

void AppCacheStorageImpl::StoreGroupAndCacheTask::CancelCompletion() {
  // Overriden to safely drop our reference to the group and cache
  // which are not thread safe refcounted.
  DatabaseTask::CancelCompletion();
  group_ = nullptr;
  cache_ = nullptr;
}

// FindMainResponseTask -------

// Helpers for FindMainResponseTask::Run()
namespace {
class SortByCachePreference {
 public:
  SortByCachePreference(int64_t preferred_id,
                        const std::set<int64_t>& in_use_ids)
      : preferred_id_(preferred_id), in_use_ids_(in_use_ids) {}
  bool operator()(
      const AppCacheDatabase::EntryRecord& lhs,
      const AppCacheDatabase::EntryRecord& rhs) {
    return compute_value(lhs) > compute_value(rhs);
  }
 private:
  int compute_value(const AppCacheDatabase::EntryRecord& entry) {
    if (entry.cache_id == preferred_id_)
      return 100;
    else if (in_use_ids_.find(entry.cache_id) != in_use_ids_.end())
      return 50;
    return 0;
  }
  int64_t preferred_id_;
  const std::set<int64_t>& in_use_ids_;
};

bool SortByLength(
    const AppCacheDatabase::NamespaceRecord& lhs,
    const AppCacheDatabase::NamespaceRecord& rhs) {
  return lhs.namespace_.namespace_url.spec().length() >
         rhs.namespace_.namespace_url.spec().length();
}

class NetworkNamespaceHelper {
 public:
  explicit NetworkNamespaceHelper(AppCacheDatabase* database)
      : database_(database) {
  }

  bool IsInNetworkNamespace(const GURL& url, int64_t cache_id) {
    std::pair<WhiteListMap::iterator, bool> result = namespaces_map_.insert(
        WhiteListMap::value_type(cache_id, std::vector<AppCacheNamespace>()));
    if (result.second)
      GetOnlineWhiteListForCache(cache_id, &result.first->second);
    return AppCache::FindNamespace(result.first->second, url) != nullptr;
  }

 private:
  void GetOnlineWhiteListForCache(int64_t cache_id,
                                  std::vector<AppCacheNamespace>* namespaces) {
    DCHECK(namespaces && namespaces->empty());
    using WhiteListVector =
        std::vector<AppCacheDatabase::OnlineWhiteListRecord>;
    WhiteListVector records;
    if (!database_->FindOnlineWhiteListForCache(cache_id, &records))
      return;

    for (const auto& record : records) {
      namespaces->push_back(AppCacheNamespace(APPCACHE_NETWORK_NAMESPACE,
                                              record.namespace_url, GURL(),
                                              record.is_pattern));
    }
  }

  // Key is cache id
  using WhiteListMap = std::map<int64_t, std::vector<AppCacheNamespace>>;
  WhiteListMap namespaces_map_;
  AppCacheDatabase* const database_;
};

}  // namespace

class AppCacheStorageImpl::FindMainResponseTask : public DatabaseTask {
 public:
  FindMainResponseTask(AppCacheStorageImpl* storage,
                       const GURL& url,
                       const GURL& preferred_manifest_url,
                       const AppCacheWorkingSet::GroupMap* groups_in_use)
      : DatabaseTask(storage),
        url_(url),
        preferred_manifest_url_(preferred_manifest_url),
        cache_id_(blink::mojom::kAppCacheNoCacheId),
        group_id_(0) {
    if (groups_in_use) {
      for (const auto& pair : *groups_in_use) {
        AppCacheGroup* group = pair.second;
        AppCache* cache = group->newest_complete_cache();
        if (group->is_obsolete() || !cache)
          continue;
        cache_ids_in_use_.insert(cache->cache_id());
      }
    }
  }

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;

 protected:
  ~FindMainResponseTask() override {}

 private:
  using NamespaceRecordPtrVector =
      std::vector<AppCacheDatabase::NamespaceRecord*>;

  bool FindExactMatch(int64_t preferred_id);
  bool FindNamespaceMatch(int64_t preferred_id);
  bool FindNamespaceHelper(int64_t preferred_cache_id,
                           AppCacheDatabase::NamespaceRecordVector* namespaces,
                           NetworkNamespaceHelper* network_namespace_helper);
  bool FindFirstValidNamespace(const NamespaceRecordPtrVector& namespaces);

  GURL url_;
  GURL preferred_manifest_url_;
  std::set<int64_t> cache_ids_in_use_;
  AppCacheEntry entry_;
  AppCacheEntry fallback_entry_;
  GURL namespace_entry_url_;
  int64_t cache_id_;
  int64_t group_id_;
  GURL manifest_url_;
};

void AppCacheStorageImpl::FindMainResponseTask::Run() {
  // NOTE: The heuristics around choosing amoungst multiple candidates
  // is underspecified, and just plain not fully understood. This needs
  // to be refined.

  // The 'preferred_manifest_url' is the url of the manifest associated
  // with the page that opened or embedded the page being loaded now.
  // We have a strong preference to use resources from that cache.
  // We also have a lesser bias to use resources from caches that are currently
  // being used by other unrelated pages.
  // TODO(michaeln): come up with a 'preferred_manifest_url' in more cases
  // - when navigating a frame whose current contents are from an appcache
  // - when clicking an href in a frame that is appcached
  int64_t preferred_cache_id = blink::mojom::kAppCacheNoCacheId;
  if (!preferred_manifest_url_.is_empty()) {
    AppCacheDatabase::GroupRecord preferred_group;
    AppCacheDatabase::CacheRecord preferred_cache;
    if (database_->FindGroupForManifestUrl(
            preferred_manifest_url_, &preferred_group) &&
        database_->FindCacheForGroup(
            preferred_group.group_id, &preferred_cache)) {
      preferred_cache_id = preferred_cache.cache_id;
    }
  }

  if (FindExactMatch(preferred_cache_id) ||
      FindNamespaceMatch(preferred_cache_id)) {
    // We found something.
    DCHECK(cache_id_ != blink::mojom::kAppCacheNoCacheId &&
           !manifest_url_.is_empty() && group_id_ != 0);
    return;
  }

  // We didn't find anything.
  DCHECK(cache_id_ == blink::mojom::kAppCacheNoCacheId &&
         manifest_url_.is_empty() && group_id_ == 0);
}

bool AppCacheStorageImpl::FindMainResponseTask::FindExactMatch(
    int64_t preferred_cache_id) {
  std::vector<AppCacheDatabase::EntryRecord> entries;
  if (database_->FindEntriesForUrl(url_, &entries) && !entries.empty()) {
    // Sort them in order of preference, from the preferred_cache first,
    // followed by hits from caches that are 'in use', then the rest.
    std::sort(entries.begin(), entries.end(),
              SortByCachePreference(preferred_cache_id, cache_ids_in_use_));

    // Take the first with a valid, non-foreign entry.
    for (const auto& entry : entries) {
      AppCacheDatabase::GroupRecord group_record;
      if ((entry.flags & AppCacheEntry::FOREIGN) ||
          !database_->FindGroupForCache(entry.cache_id, &group_record)) {
        continue;
      }
      manifest_url_ = group_record.manifest_url;
      group_id_ = group_record.group_id;
      entry_ = AppCacheEntry(entry.flags, entry.response_id);
      cache_id_ = entry.cache_id;
      return true;  // We found an exact match.
    }
  }
  return false;
}

bool AppCacheStorageImpl::FindMainResponseTask::FindNamespaceMatch(
    int64_t preferred_cache_id) {
  AppCacheDatabase::NamespaceRecordVector all_intercepts;
  AppCacheDatabase::NamespaceRecordVector all_fallbacks;
  if (!database_->FindNamespacesForOrigin(url::Origin::Create(url_),
                                          &all_intercepts, &all_fallbacks) ||
      (all_intercepts.empty() && all_fallbacks.empty())) {
    return false;
  }

  NetworkNamespaceHelper network_namespace_helper(database_);
  if (FindNamespaceHelper(preferred_cache_id,
                          &all_intercepts,
                          &network_namespace_helper) ||
      FindNamespaceHelper(preferred_cache_id,
                          &all_fallbacks,
                          &network_namespace_helper)) {
    return true;
  }
  return false;
}

bool AppCacheStorageImpl::FindMainResponseTask::FindNamespaceHelper(
    int64_t preferred_cache_id,
    AppCacheDatabase::NamespaceRecordVector* namespaces,
    NetworkNamespaceHelper* network_namespace_helper) {
  // Sort them by length, longer matches within the same cache/bucket take
  // precedence.
  std::sort(namespaces->begin(), namespaces->end(), SortByLength);

  NamespaceRecordPtrVector preferred_namespaces;
  NamespaceRecordPtrVector inuse_namespaces;
  NamespaceRecordPtrVector other_namespaces;
  for (auto& namespace_record : *namespaces) {
    // Skip those that aren't a match.
    if (!namespace_record.namespace_.IsMatch(url_))
      continue;

    // Skip namespaces where the requested url falls into a network
    // namespace of its containing appcache.
    if (network_namespace_helper->IsInNetworkNamespace(
            url_, namespace_record.cache_id))
      continue;

    // Bin them into one of our three buckets.
    if (namespace_record.cache_id == preferred_cache_id)
      preferred_namespaces.push_back(&namespace_record);
    else if (cache_ids_in_use_.find(namespace_record.cache_id) !=
             cache_ids_in_use_.end())
      inuse_namespaces.push_back(&namespace_record);
    else
      other_namespaces.push_back(&namespace_record);
  }

  if (FindFirstValidNamespace(preferred_namespaces) ||
      FindFirstValidNamespace(inuse_namespaces) ||
      FindFirstValidNamespace(other_namespaces))
    return true;  // We found one.

  // We didn't find anything.
  return false;
}

bool AppCacheStorageImpl::
FindMainResponseTask::FindFirstValidNamespace(
    const NamespaceRecordPtrVector& namespaces) {
  // Take the first with a valid, non-foreign entry.
  for (auto* namespace_record : namespaces) {
    AppCacheDatabase::EntryRecord entry_record;
    if (database_->FindEntry(namespace_record->cache_id,
                             namespace_record->namespace_.target_url,
                             &entry_record)) {
      AppCacheDatabase::GroupRecord group_record;
      if ((entry_record.flags & AppCacheEntry::FOREIGN) ||
          !database_->FindGroupForCache(entry_record.cache_id, &group_record)) {
        continue;
      }
      manifest_url_ = group_record.manifest_url;
      group_id_ = group_record.group_id;
      cache_id_ = namespace_record->cache_id;
      namespace_entry_url_ = namespace_record->namespace_.target_url;
      if (namespace_record->namespace_.type == APPCACHE_FALLBACK_NAMESPACE)
        fallback_entry_ = AppCacheEntry(entry_record.flags,
                                        entry_record.response_id);
      else
        entry_ = AppCacheEntry(entry_record.flags, entry_record.response_id);
      return true;  // We found one.
    }
  }
  return false;  // We didn't find a match.
}

void AppCacheStorageImpl::FindMainResponseTask::RunCompleted() {
  storage_->CallOnMainResponseFound(
      &delegates_, url_, entry_, namespace_entry_url_, fallback_entry_,
      cache_id_, group_id_, manifest_url_);
}

// MarkEntryAsForeignTask -------

class AppCacheStorageImpl::MarkEntryAsForeignTask : public DatabaseTask {
 public:
  MarkEntryAsForeignTask(AppCacheStorageImpl* storage,
                         const GURL& url,
                         int64_t cache_id)
      : DatabaseTask(storage), cache_id_(cache_id), entry_url_(url) {}

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;

 protected:
  ~MarkEntryAsForeignTask() override {}

 private:
  int64_t cache_id_;
  GURL entry_url_;
};

void AppCacheStorageImpl::MarkEntryAsForeignTask::Run() {
  database_->AddEntryFlags(entry_url_, cache_id_, AppCacheEntry::FOREIGN);
}

void AppCacheStorageImpl::MarkEntryAsForeignTask::RunCompleted() {
  DCHECK(storage_->pending_foreign_markings_.front().first == entry_url_ &&
         storage_->pending_foreign_markings_.front().second == cache_id_);
  storage_->pending_foreign_markings_.pop_front();
}

// MakeGroupObsoleteTask -------

class AppCacheStorageImpl::MakeGroupObsoleteTask : public DatabaseTask {
 public:
  MakeGroupObsoleteTask(AppCacheStorageImpl* storage,
                        AppCacheGroup* group,
                        int response_code);

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;
  void CancelCompletion() override;

 protected:
  ~MakeGroupObsoleteTask() override {}

 private:
  scoped_refptr<AppCacheGroup> group_;
  int64_t group_id_;
  url::Origin origin_;
  bool success_;
  int response_code_;
  int64_t new_origin_usage_;
  std::vector<int64_t> newly_deletable_response_ids_;
};

AppCacheStorageImpl::MakeGroupObsoleteTask::MakeGroupObsoleteTask(
    AppCacheStorageImpl* storage,
    AppCacheGroup* group,
    int response_code)
    : DatabaseTask(storage),
      group_(group),
      group_id_(group->group_id()),
      origin_(url::Origin::Create(group->manifest_url())),
      success_(false),
      response_code_(response_code),
      new_origin_usage_(-1) {}

void AppCacheStorageImpl::MakeGroupObsoleteTask::Run() {
  DCHECK(!success_);
  sql::Database* connection = database_->db_connection();
  if (!connection)
    return;

  sql::Transaction transaction(connection);
  if (!transaction.Begin())
    return;

  AppCacheDatabase::GroupRecord group_record;
  if (!database_->FindGroup(group_id_, &group_record)) {
    // This group doesn't exists in the database, nothing todo here.
    new_origin_usage_ = database_->GetOriginUsage(origin_);
    success_ = true;
    return;
  }

  DCHECK_EQ(group_record.origin, origin_);
  success_ = DeleteGroupAndRelatedRecords(database_,
                                          group_id_,
                                          &newly_deletable_response_ids_);

  new_origin_usage_ = database_->GetOriginUsage(origin_);
  success_ = success_ && transaction.Commit();
}

void AppCacheStorageImpl::MakeGroupObsoleteTask::RunCompleted() {
  if (success_) {
    group_->set_obsolete(true);
    if (!storage_->is_disabled()) {
      storage_->UpdateUsageMapAndNotify(origin_, new_origin_usage_);
      group_->AddNewlyDeletableResponseIds(&newly_deletable_response_ids_);

      // Also remove from the working set, caches for an 'obsolete' group
      // may linger in use, but the group itself cannot be looked up by
      // 'manifest_url' in the working set any longer.
      storage_->working_set()->RemoveGroup(group_.get());
    }
  }

  AppCacheStorage::ForEachDelegate(
      delegates_, [&](AppCacheStorage::Delegate* delegate) {
        delegate->OnGroupMadeObsolete(group_.get(), success_, response_code_);
      });
  group_ = nullptr;
}

void AppCacheStorageImpl::MakeGroupObsoleteTask::CancelCompletion() {
  // Overriden to safely drop our reference to the group
  // which is not thread safe refcounted.
  DatabaseTask::CancelCompletion();
  group_ = nullptr;
}

// GetDeletableResponseIdsTask -------

class AppCacheStorageImpl::GetDeletableResponseIdsTask : public DatabaseTask {
 public:
  GetDeletableResponseIdsTask(AppCacheStorageImpl* storage, int64_t max_rowid)
      : DatabaseTask(storage), max_rowid_(max_rowid) {}

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;

 protected:
  ~GetDeletableResponseIdsTask() override {}

 private:
  int64_t max_rowid_;
  std::vector<int64_t> response_ids_;
};

void AppCacheStorageImpl::GetDeletableResponseIdsTask::Run() {
  const int kSqlLimit = 1000;
  database_->GetDeletableResponseIds(&response_ids_, max_rowid_, kSqlLimit);
  // TODO(michaeln): retrieve group_ids too
}

void AppCacheStorageImpl::GetDeletableResponseIdsTask::RunCompleted() {
  if (!response_ids_.empty())
    storage_->StartDeletingResponses(response_ids_);
}

// InsertDeletableResponseIdsTask -------

class AppCacheStorageImpl::InsertDeletableResponseIdsTask
    : public DatabaseTask {
 public:
  explicit InsertDeletableResponseIdsTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}

  // DatabaseTask:
  void Run() override;

  std::vector<int64_t> response_ids_;

 protected:
  ~InsertDeletableResponseIdsTask() override {}
};

void AppCacheStorageImpl::InsertDeletableResponseIdsTask::Run() {
  database_->InsertDeletableResponseIds(response_ids_);
  // TODO(michaeln): store group_ids too
}

// DeleteDeletableResponseIdsTask -------

class AppCacheStorageImpl::DeleteDeletableResponseIdsTask
    : public DatabaseTask {
 public:
  explicit DeleteDeletableResponseIdsTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}

  // DatabaseTask:
  void Run() override;

  std::vector<int64_t> response_ids_;

 protected:
  ~DeleteDeletableResponseIdsTask() override {}
};

void AppCacheStorageImpl::DeleteDeletableResponseIdsTask::Run() {
  database_->DeleteDeletableResponseIds(response_ids_);
}

// LazyUpdateLastAccessTimeTask -------

class AppCacheStorageImpl::LazyUpdateLastAccessTimeTask
    : public DatabaseTask {
 public:
  LazyUpdateLastAccessTimeTask(
      AppCacheStorageImpl* storage, AppCacheGroup* group, base::Time time)
      : DatabaseTask(storage), group_id_(group->group_id()),
        last_access_time_(time) {
    storage->NotifyStorageAccessed(url::Origin::Create(group->manifest_url()));
  }

  // DatabaseTask:
  void Run() override;
  void RunCompleted() override;

 protected:
  ~LazyUpdateLastAccessTimeTask() override {}

 private:
  int64_t group_id_;
  base::Time last_access_time_;
};

void AppCacheStorageImpl::LazyUpdateLastAccessTimeTask::Run() {
  database_->LazyUpdateLastAccessTime(group_id_, last_access_time_);
}

void AppCacheStorageImpl::LazyUpdateLastAccessTimeTask::RunCompleted() {
  storage_->LazilyCommitLastAccessTimes();
}

// CommitLastAccessTimesTask -------

class AppCacheStorageImpl::CommitLastAccessTimesTask
    : public DatabaseTask {
 public:
  CommitLastAccessTimesTask(AppCacheStorageImpl* storage)
      : DatabaseTask(storage) {}

  // DatabaseTask:
  void Run() override {
    database_->CommitLazyLastAccessTimes();
  }

 protected:
  ~CommitLastAccessTimesTask() override {}
};

// UpdateEvictionTimes -------

class AppCacheStorageImpl::UpdateEvictionTimesTask
    : public DatabaseTask {
 public:
  UpdateEvictionTimesTask(
      AppCacheStorageImpl* storage, AppCacheGroup* group)
      : DatabaseTask(storage), group_id_(group->group_id()),
        last_full_update_check_time_(group->last_full_update_check_time()),
        first_evictable_error_time_(group->first_evictable_error_time()) {
  }

  // DatabaseTask:
  void Run() override;

 protected:
  ~UpdateEvictionTimesTask() override {}

 private:
  int64_t group_id_;
  base::Time last_full_update_check_time_;
  base::Time first_evictable_error_time_;
};

void AppCacheStorageImpl::UpdateEvictionTimesTask::Run() {
  database_->UpdateEvictionTimes(group_id_,
                                 last_full_update_check_time_,
                                 first_evictable_error_time_);
}

// AppCacheStorageImpl ---------------------------------------------------

AppCacheStorageImpl::AppCacheStorageImpl(AppCacheServiceImpl* service)
    : AppCacheStorage(service),
      is_incognito_(false),
      is_response_deletion_scheduled_(false),
      did_start_deleting_responses_(false),
      last_deletable_response_rowid_(0),
      database_(nullptr),
      is_disabled_(false),
      delete_and_start_over_pending_(false),
      expecting_cleanup_complete_on_disable_(false) {}

AppCacheStorageImpl::~AppCacheStorageImpl() {
  for (StoreGroupAndCacheTask* task : pending_quota_queries_)
    task->CancelCompletion();
  for (DatabaseTask* task : scheduled_database_tasks_)
    task->CancelCompletion();

  if (database_ &&
      !db_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ClearSessionOnlyOrigins, std::move(database_),
              base::WrapRefCounted(service_->special_storage_policy()),
              service()->force_keep_session_state()))) {
  }
}

void AppCacheStorageImpl::Initialize(
    const base::FilePath& cache_directory,
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner) {
  cache_directory_ = cache_directory;
  is_incognito_ = cache_directory_.empty();

  base::FilePath db_file_path;
  if (!is_incognito_)
    db_file_path = cache_directory_.Append(kAppCacheDatabaseName);
  database_ = std::make_unique<AppCacheDatabase>(db_file_path);

  db_task_runner_ = db_task_runner;

  auto task = base::MakeRefCounted<InitTask>(this);
  task->Schedule();
}

void AppCacheStorageImpl::Disable() {
  if (is_disabled_)
    return;
  VLOG(1) << "Disabling appcache storage.";
  is_disabled_ = true;
  ClearUsageMapAndNotify();
  working_set()->Disable();
  if (disk_cache_)
    disk_cache_->Disable();
  auto task = base::MakeRefCounted<DisableDatabaseTask>(this);
  task->Schedule();
}

void AppCacheStorageImpl::GetAllInfo(Delegate* delegate) {
  DCHECK(delegate);
  auto task = base::MakeRefCounted<GetAllInfoTask>(this);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
}

void AppCacheStorageImpl::LoadCache(int64_t id, Delegate* delegate) {
  DCHECK(delegate);
  if (is_disabled_) {
    delegate->OnCacheLoaded(nullptr, id);
    return;
  }

  AppCache* cache = working_set_.GetCache(id);
  if (cache) {
    delegate->OnCacheLoaded(cache, id);
    if (cache->owning_group()) {
      auto update_task = base::MakeRefCounted<LazyUpdateLastAccessTimeTask>(
          this, cache->owning_group(), base::Time::Now());
      update_task->Schedule();
    }
    return;
  }
  scoped_refptr<CacheLoadTask> task(GetPendingCacheLoadTask(id));
  if (task.get()) {
    task->AddDelegate(GetOrCreateDelegateReference(delegate));
    return;
  }
  task = base::MakeRefCounted<CacheLoadTask>(id, this);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
  pending_cache_loads_[id] = task.get();
}

void AppCacheStorageImpl::LoadOrCreateGroup(
    const GURL& manifest_url, Delegate* delegate) {
  DCHECK(delegate);
  if (is_disabled_) {
    delegate->OnGroupLoaded(nullptr, manifest_url);
    return;
  }

  AppCacheGroup* group = working_set_.GetGroup(manifest_url);
  if (group) {
    delegate->OnGroupLoaded(group, manifest_url);
    auto update_task = base::MakeRefCounted<LazyUpdateLastAccessTimeTask>(
        this, group, base::Time::Now());
    update_task->Schedule();
    return;
  }

  scoped_refptr<GroupLoadTask> task(GetPendingGroupLoadTask(manifest_url));
  if (task.get()) {
    task->AddDelegate(GetOrCreateDelegateReference(delegate));
    return;
  }

  if (usage_map_.find(url::Origin::Create(manifest_url)) == usage_map_.end()) {
    // No need to query the database, return a new group immediately.
    auto new_group =
        base::MakeRefCounted<AppCacheGroup>(this, manifest_url, NewGroupId());
    delegate->OnGroupLoaded(new_group.get(), manifest_url);
    return;
  }

  task = base::MakeRefCounted<GroupLoadTask>(manifest_url, this);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
  pending_group_loads_[manifest_url] = task.get();
}

void AppCacheStorageImpl::StoreGroupAndNewestCache(
    AppCacheGroup* group, AppCache* newest_cache, Delegate* delegate) {
  // TODO(michaeln): distinguish between a simple update of an existing
  // cache that just adds new master entry(s), and the insertion of a
  // whole new cache. The StoreGroupAndCacheTask as written will handle
  // the simple update case in a very heavy weight way (delete all and
  // the reinsert all over again).
  DCHECK(group && delegate && newest_cache);
  auto task =
      base::MakeRefCounted<StoreGroupAndCacheTask>(this, group, newest_cache);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->GetQuotaThenSchedule();
}

void AppCacheStorageImpl::FindResponseForMainRequest(
    const GURL& url, const GURL& preferred_manifest_url,
    Delegate* delegate) {
  DCHECK(delegate);

  const GURL* url_ptr = &url;
  GURL url_no_ref;
  if (url.has_ref()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    url_no_ref = url.ReplaceComponents(replacements);
    url_ptr = &url_no_ref;
  }

  const url::Origin origin(url::Origin::Create(url));

  // First look in our working set for a direct hit without having to query
  // the database.
  const AppCacheWorkingSet::GroupMap* groups_in_use =
      working_set()->GetGroupsInOrigin(origin);
  if (groups_in_use) {
    if (!preferred_manifest_url.is_empty()) {
      auto found = groups_in_use->find(preferred_manifest_url);
      if (found != groups_in_use->end() &&
          FindResponseForMainRequestInGroup(
              found->second, *url_ptr, delegate)) {
          return;
      }
    } else {
      for (const auto& pair : *groups_in_use) {
        if (FindResponseForMainRequestInGroup(pair.second, *url_ptr,
                                              delegate)) {
          return;
        }
      }
    }
  }

  if (IsInitTaskComplete() &&  usage_map_.find(origin) == usage_map_.end()) {
    // No need to query the database, return async'ly but without going thru
    // the DB thread.
    scoped_refptr<AppCacheGroup> no_group;
    scoped_refptr<AppCache> no_cache;
    ScheduleSimpleTask(base::BindOnce(
        &AppCacheStorageImpl::DeliverShortCircuitedFindMainResponse,
        weak_factory_.GetWeakPtr(), url, AppCacheEntry(), no_group, no_cache,
        base::WrapRefCounted(GetOrCreateDelegateReference(delegate))));
    return;
  }

  // We have to query the database, schedule a database task to do so.
  auto task = base::MakeRefCounted<FindMainResponseTask>(
      this, *url_ptr, preferred_manifest_url, groups_in_use);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
}

bool AppCacheStorageImpl::FindResponseForMainRequestInGroup(
    AppCacheGroup* group,  const GURL& url, Delegate* delegate) {
  AppCache* cache = group->newest_complete_cache();
  if (group->is_obsolete() || !cache)
    return false;

  AppCacheEntry* entry = cache->GetEntry(url);
  if (!entry || entry->IsForeign())
    return false;

  ScheduleSimpleTask(base::BindOnce(
      &AppCacheStorageImpl::DeliverShortCircuitedFindMainResponse,
      weak_factory_.GetWeakPtr(), url, *entry, base::WrapRefCounted(group),
      base::WrapRefCounted(cache),
      base::WrapRefCounted(GetOrCreateDelegateReference(delegate))));
  return true;
}

void AppCacheStorageImpl::DeliverShortCircuitedFindMainResponse(
    const GURL& url,
    const AppCacheEntry& found_entry,
    scoped_refptr<AppCacheGroup> group,
    scoped_refptr<AppCache> cache,
    scoped_refptr<DelegateReference> delegate_ref) {
  if (delegate_ref->delegate) {
    std::vector<scoped_refptr<DelegateReference>> delegates(1, delegate_ref);
    CallOnMainResponseFound(
        &delegates, url, found_entry, GURL(), AppCacheEntry(),
        cache.get() ? cache->cache_id() : blink::mojom::kAppCacheNoCacheId,
        group.get() ? group->group_id() : blink::mojom::kAppCacheNoCacheId,
        group.get() ? group->manifest_url() : GURL());
  }
}

void AppCacheStorageImpl::CallOnMainResponseFound(
    std::vector<scoped_refptr<DelegateReference>>* delegates,
    const GURL& url,
    const AppCacheEntry& entry,
    const GURL& namespace_entry_url,
    const AppCacheEntry& fallback_entry,
    int64_t cache_id,
    int64_t group_id,
    const GURL& manifest_url) {
  AppCacheStorage::ForEachDelegate(
      *delegates, [&](AppCacheStorage::Delegate* delegate) {
        delegate->OnMainResponseFound(url, entry, namespace_entry_url,
                                      fallback_entry, cache_id, group_id,
                                      manifest_url);
      });
}

void AppCacheStorageImpl::FindResponseForSubRequest(
    AppCache* cache, const GURL& url,
    AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
    bool* found_network_namespace) {
  DCHECK(cache && cache->is_complete());

  // When a group is forcibly deleted, all subresource loads for pages
  // using caches in the group will result in a synthesized network errors.
  // Forcible deletion is not a function that is covered by the HTML5 spec.
  if (cache->owning_group()->is_being_deleted()) {
    *found_entry = AppCacheEntry();
    *found_fallback_entry = AppCacheEntry();
    *found_network_namespace = false;
    return;
  }

  GURL fallback_namespace_not_used;
  GURL intercept_namespace_not_used;
  cache->FindResponseForRequest(
      url, found_entry, &intercept_namespace_not_used,
      found_fallback_entry, &fallback_namespace_not_used,
      found_network_namespace);
}

void AppCacheStorageImpl::MarkEntryAsForeign(const GURL& entry_url,
                                             int64_t cache_id) {
  AppCache* cache = working_set_.GetCache(cache_id);
  if (cache) {
    AppCacheEntry* entry = cache->GetEntry(entry_url);
    if (entry)
      entry->add_types(AppCacheEntry::FOREIGN);
  }
  auto task =
      base::MakeRefCounted<MarkEntryAsForeignTask>(this, entry_url, cache_id);
  task->Schedule();
  pending_foreign_markings_.push_back(std::make_pair(entry_url, cache_id));
}

void AppCacheStorageImpl::MakeGroupObsolete(AppCacheGroup* group,
                                            Delegate* delegate,
                                            int response_code) {
  DCHECK(group && delegate);
  auto task =
      base::MakeRefCounted<MakeGroupObsoleteTask>(this, group, response_code);
  task->AddDelegate(GetOrCreateDelegateReference(delegate));
  task->Schedule();
}

void AppCacheStorageImpl::StoreEvictionTimes(AppCacheGroup* group) {
  auto task = base::MakeRefCounted<UpdateEvictionTimesTask>(this, group);
  task->Schedule();
}

std::unique_ptr<AppCacheResponseReader>
AppCacheStorageImpl::CreateResponseReader(const GURL& manifest_url,
                                          int64_t response_id) {
  return std::make_unique<AppCacheResponseReader>(
      response_id, is_disabled_ ? nullptr : disk_cache()->GetWeakPtr());
}

std::unique_ptr<AppCacheResponseWriter>
AppCacheStorageImpl::CreateResponseWriter(const GURL& manifest_url) {
  return std::make_unique<AppCacheResponseWriter>(
      NewResponseId(), is_disabled_ ? nullptr : disk_cache()->GetWeakPtr());
}

std::unique_ptr<AppCacheResponseMetadataWriter>
AppCacheStorageImpl::CreateResponseMetadataWriter(int64_t response_id) {
  return std::make_unique<AppCacheResponseMetadataWriter>(
      response_id, is_disabled_ ? nullptr : disk_cache()->GetWeakPtr());
}

void AppCacheStorageImpl::DoomResponses(
    const GURL& manifest_url,
    const std::vector<int64_t>& response_ids) {
  if (response_ids.empty())
    return;

  // Start deleting them from the disk cache lazily.
  StartDeletingResponses(response_ids);

  // Also schedule a database task to record these ids in the
  // deletable responses table.
  // TODO(michaeln): There is a race here. If the browser crashes
  // prior to committing these rows to the database and prior to us
  // having deleted them from the disk cache, we'll never delete them.
  auto task = base::MakeRefCounted<InsertDeletableResponseIdsTask>(this);
  task->response_ids_ = response_ids;
  task->Schedule();
}

void AppCacheStorageImpl::DeleteResponses(
    const GURL& manifest_url,
    const std::vector<int64_t>& response_ids) {
  if (response_ids.empty())
    return;
  StartDeletingResponses(response_ids);
}

bool AppCacheStorageImpl::IsInitialized() {
  return IsInitTaskComplete();
}

void AppCacheStorageImpl::DelayedStartDeletingUnusedResponses() {
  // Only if we haven't already begun.
  if (!did_start_deleting_responses_) {
    auto task = base::MakeRefCounted<GetDeletableResponseIdsTask>(
        this, last_deletable_response_rowid_);
    task->Schedule();
  }
}

void AppCacheStorageImpl::StartDeletingResponses(
    const std::vector<int64_t>& response_ids) {
  DCHECK(!response_ids.empty());
  did_start_deleting_responses_ = true;
  deletable_response_ids_.insert(
      deletable_response_ids_.end(),
      response_ids.begin(), response_ids.end());
  if (!is_response_deletion_scheduled_)
    ScheduleDeleteOneResponse();
}

void AppCacheStorageImpl::ScheduleDeleteOneResponse() {
  DCHECK(!is_response_deletion_scheduled_);
  const base::TimeDelta kBriefDelay = base::TimeDelta::FromMilliseconds(10);
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppCacheStorageImpl::DeleteOneResponse,
                     weak_factory_.GetWeakPtr()),
      kBriefDelay);
  is_response_deletion_scheduled_ = true;
}

void AppCacheStorageImpl::DeleteOneResponse() {
  DCHECK(is_response_deletion_scheduled_);
  DCHECK(!deletable_response_ids_.empty());

  if (is_disabled_) {
    deletable_response_ids_.clear();
    deleted_response_ids_.clear();
    is_response_deletion_scheduled_ = false;
    return;
  }

  // TODO(michaeln): add group_id to DoomEntry args
  int64_t id = deletable_response_ids_.front();
  int rv = disk_cache()->DoomEntry(
      id, base::BindOnce(&AppCacheStorageImpl::OnDeletedOneResponse,
                         base::Unretained(this)));
  if (rv != net::ERR_IO_PENDING)
    OnDeletedOneResponse(rv);
}

void AppCacheStorageImpl::OnDeletedOneResponse(int rv) {
  is_response_deletion_scheduled_ = false;
  if (is_disabled_)
    return;

  int64_t id = deletable_response_ids_.front();
  deletable_response_ids_.pop_front();
  if (rv != net::ERR_ABORTED)
    deleted_response_ids_.push_back(id);

  const size_t kBatchSize = 50U;
  if (deleted_response_ids_.size() >= kBatchSize ||
      deletable_response_ids_.empty()) {
    auto task = base::MakeRefCounted<DeleteDeletableResponseIdsTask>(this);
    task->response_ids_.swap(deleted_response_ids_);
    task->Schedule();
  }

  if (deletable_response_ids_.empty()) {
    auto task = base::MakeRefCounted<GetDeletableResponseIdsTask>(
        this, last_deletable_response_rowid_);
    task->Schedule();
    return;
  }

  ScheduleDeleteOneResponse();
}

AppCacheStorageImpl::CacheLoadTask*
AppCacheStorageImpl::GetPendingCacheLoadTask(int64_t cache_id) {
  auto found = pending_cache_loads_.find(cache_id);
  if (found != pending_cache_loads_.end())
    return found->second;
  return nullptr;
}

AppCacheStorageImpl::GroupLoadTask*
AppCacheStorageImpl::GetPendingGroupLoadTask(const GURL& manifest_url) {
  auto found = pending_group_loads_.find(manifest_url);
  if (found != pending_group_loads_.end())
    return found->second;
  return nullptr;
}

std::vector<GURL> AppCacheStorageImpl::GetPendingForeignMarkingsForCache(
    int64_t cache_id) {
  std::vector<GURL> urls;
  for (const auto& pair : pending_foreign_markings_) {
    if (pair.second == cache_id)
      urls.push_back(pair.first);
  }
  return urls;
}

void AppCacheStorageImpl::ScheduleSimpleTask(base::OnceClosure task) {
  pending_simple_tasks_.push_back(std::move(task));
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppCacheStorageImpl::RunOnePendingSimpleTask,
                                weak_factory_.GetWeakPtr()));
}

void AppCacheStorageImpl::RunOnePendingSimpleTask() {
  DCHECK(!pending_simple_tasks_.empty());
  base::OnceClosure task = std::move(pending_simple_tasks_.front());
  pending_simple_tasks_.pop_front();
  std::move(task).Run();
}

AppCacheDiskCache* AppCacheStorageImpl::disk_cache() {
  DCHECK(IsInitTaskComplete());
  DCHECK(!is_disabled_);

  if (!disk_cache_) {
    int rv = net::OK;
    disk_cache_ = std::make_unique<AppCacheDiskCache>();
    if (is_incognito_) {
      rv = disk_cache_->InitWithMemBackend(
          0, base::BindOnce(&AppCacheStorageImpl::OnDiskCacheInitialized,
                            base::Unretained(this)));
    } else {
      expecting_cleanup_complete_on_disable_ = true;

      rv = disk_cache_->InitWithDiskBackend(
          cache_directory_.Append(kDiskCacheDirectoryName), false,
          base::BindOnce(&AppCacheStorageImpl::OnDiskCacheCleanupComplete,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&AppCacheStorageImpl::OnDiskCacheInitialized,
                         base::Unretained(this)));
    }

    if (rv != net::ERR_IO_PENDING)
      OnDiskCacheInitialized(rv);
  }
  return disk_cache_.get();
}

void AppCacheStorageImpl::OnDiskCacheInitialized(int rv) {
  if (rv != net::OK) {
    // We're unable to open the disk cache, this is a fatal error that we can't
    // really recover from. We handle it by temporarily disabling the appcache
    // deleting the directory on disk and reinitializing the appcache system.
    Disable();
    if (rv != net::ERR_ABORTED)
      DeleteAndStartOver();
  }
}

void AppCacheStorageImpl::DeleteAndStartOver() {
  DCHECK(is_disabled_);
  if (!is_incognito_) {
    VLOG(1) << "Deleting existing appcache data and starting over.";

    // We can have tasks in flight to close file handles on both the db
    // and cache threads, we need to allow those tasks to cycle thru
    // prior to deleting the files and calling reinit.  We will know that the
    // cache ones will be finished once we get into OnDiskCacheCleanupComplete,
    // so let that known to synchronize with the DB thread.
    delete_and_start_over_pending_ = true;

    // Won't get a callback about cleanup being done, so call it ourselves.
    if (!expecting_cleanup_complete_on_disable_)
      OnDiskCacheCleanupComplete();
  }
}

void AppCacheStorageImpl::OnDiskCacheCleanupComplete() {
  expecting_cleanup_complete_on_disable_ = false;
  if (delete_and_start_over_pending_) {
    delete_and_start_over_pending_ = false;
    db_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), cache_directory_,
                       true),
        base::BindOnce(&AppCacheStorageImpl::CallScheduleReinitialize,
                       weak_factory_.GetWeakPtr()));
  }
}

void AppCacheStorageImpl::CallScheduleReinitialize() {
  service_->ScheduleReinitialize();
  // note: 'this' may be deleted at this point.
}

void AppCacheStorageImpl::LazilyCommitLastAccessTimes() {
  if (lazy_commit_timer_.IsRunning())
    return;
  const base::TimeDelta kDelay = base::TimeDelta::FromMinutes(5);
  lazy_commit_timer_.Start(
      FROM_HERE, kDelay,
      base::BindOnce(&AppCacheStorageImpl::OnLazyCommitTimer,
                     weak_factory_.GetWeakPtr()));
}

void AppCacheStorageImpl::OnLazyCommitTimer() {
  lazy_commit_timer_.Stop();
  if (is_disabled())
    return;
  auto task = base::MakeRefCounted<CommitLastAccessTimesTask>(this);
  task->Schedule();
}

}  // namespace content
