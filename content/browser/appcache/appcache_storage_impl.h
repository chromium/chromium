// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_STORAGE_IMPL_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_STORAGE_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/browser/appcache/appcache_database.h"
#include "content/browser/appcache/appcache_disk_cache.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class AppCacheStorageImplTest;
class ChromeAppCacheServiceTest;

// Task scheduler for database read/write operations.
class AppCacheStorageImpl : public AppCacheStorage {
 public:
  explicit AppCacheStorageImpl(AppCacheServiceImpl* service);
  ~AppCacheStorageImpl() override;

  void Initialize(
      const base::FilePath& cache_directory,
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner);
  void Disable();
  bool is_disabled() const { return is_disabled_; }

  // AppCacheStorage methods, see the base class for doc comments.
  void GetAllInfo(Delegate* delegate) override;
  void LoadCache(int64_t id, Delegate* delegate) override;
  void LoadOrCreateGroup(const GURL& manifest_url, Delegate* delegate) override;
  void StoreGroupAndNewestCache(AppCacheGroup* group,
                                AppCache* newest_cache,
                                Delegate* delegate) override;
  void FindResponseForMainRequest(const GURL& url,
                                  const GURL& preferred_manifest_url,
                                  Delegate* delegate) override;
  void FindResponseForSubRequest(AppCache* cache,
                                 const GURL& url,
                                 AppCacheEntry* found_entry,
                                 AppCacheEntry* found_fallback_entry,
                                 bool* found_network_namespace) override;
  void MarkEntryAsForeign(const GURL& entry_url, int64_t cache_id) override;
  void MakeGroupObsolete(AppCacheGroup* group,
                         Delegate* delegate,
                         int response_code) override;
  void StoreEvictionTimes(AppCacheGroup* group) override;
  std::unique_ptr<AppCacheResponseReader> CreateResponseReader(
      const GURL& manifest_url,
      int64_t response_id) override;
  std::unique_ptr<AppCacheResponseWriter> CreateResponseWriter(
      const GURL& manifest_url) override;
  std::unique_ptr<AppCacheResponseMetadataWriter> CreateResponseMetadataWriter(
      int64_t response_id) override;
  void DoomResponses(const GURL& manifest_url,
                     const std::vector<int64_t>& response_ids) override;
  void DeleteResponses(const GURL& manifest_url,
                       const std::vector<int64_t>& response_ids) override;
  bool IsInitialized() override;

 private:
  // The AppCacheStorageImpl class methods and datamembers may only be
  // accessed on the IO thread. This class manufactures seperate DatabaseTasks
  // which access the DB on a seperate background thread.
  class DatabaseTask;
  class InitTask;
  class DisableDatabaseTask;
  class GetAllInfoTask;
  class StoreOrLoadTask;
  class CacheLoadTask;
  class GroupLoadTask;
  class StoreGroupAndCacheTask;
  class FindMainResponseTask;
  class MarkEntryAsForeignTask;
  class MakeGroupObsoleteTask;
  class GetDeletableResponseIdsTask;
  class InsertDeletableResponseIdsTask;
  class DeleteDeletableResponseIdsTask;
  class LazyUpdateLastAccessTimeTask;
  class CommitLastAccessTimesTask;
  class UpdateEvictionTimesTask;

  using DatabaseTaskQueue = base::circular_deque<DatabaseTask*>;
  using PendingCacheLoads = std::map<int64_t, CacheLoadTask*>;
  using PendingGroupLoads = std::map<GURL, GroupLoadTask*>;
  using PendingForeignMarkings = base::circular_deque<std::pair<GURL, int64_t>>;
  using PendingQuotaQueries = std::set<StoreGroupAndCacheTask*>;

  bool IsInitTaskComplete() {
    return last_cache_id_ != AppCacheStorage::kUnitializedId;
  }

  CacheLoadTask* GetPendingCacheLoadTask(int64_t cache_id);
  GroupLoadTask* GetPendingGroupLoadTask(const GURL& manifest_url);
  std::vector<GURL> GetPendingForeignMarkingsForCache(int64_t cache_id);

  void ScheduleSimpleTask(base::OnceClosure task);
  void RunOnePendingSimpleTask();

  void DelayedStartDeletingUnusedResponses();
  void StartDeletingResponses(const std::vector<int64_t>& response_ids);
  void ScheduleDeleteOneResponse();
  void DeleteOneResponse();
  void OnDeletedOneResponse(int rv);
  void OnDiskCacheInitialized(int rv);
  void OnDiskCacheCleanupComplete();

  void DeleteAndStartOver();
  void CallScheduleReinitialize();
  void LazilyCommitLastAccessTimes();
  void OnLazyCommitTimer();

  // If there is appcache data to be deleted (|force_keep_session_state| is
  // false), deletes session-only appcache data.
  static void ClearSessionOnlyOrigins(
      std::unique_ptr<AppCacheDatabase> database,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      bool force_keep_session_state);

  // Sometimes we can respond without having to query the database.
  bool FindResponseForMainRequestInGroup(
      AppCacheGroup* group,  const GURL& url, Delegate* delegate);
  void DeliverShortCircuitedFindMainResponse(
      const GURL& url,
      const AppCacheEntry& found_entry,
      scoped_refptr<AppCacheGroup> group,
      scoped_refptr<AppCache> newest_cache,
      scoped_refptr<DelegateReference> delegate_ref);

  void CallOnMainResponseFound(
      std::vector<scoped_refptr<DelegateReference>>* delegates,
      const GURL& url,
      const AppCacheEntry& entry,
      const GURL& namespace_entry_url,
      const AppCacheEntry& fallback_entry,
      int64_t cache_id,
      int64_t group_id,
      const GURL& manifest_url);

  // Don't call this when |is_disabled_| is true.
  CONTENT_EXPORT AppCacheDiskCache* disk_cache();

  // The directory in which we place files in the file system.
  base::FilePath cache_directory_;
  bool is_incognito_;

  // This class operates primarily on the IO thread, but schedules
  // its DatabaseTasks on the db thread.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // Structures to keep track of DatabaseTasks that are in-flight.
  DatabaseTaskQueue scheduled_database_tasks_;
  PendingCacheLoads pending_cache_loads_;
  PendingGroupLoads pending_group_loads_;
  PendingForeignMarkings pending_foreign_markings_;
  PendingQuotaQueries pending_quota_queries_;

  // Structures to keep track of lazy response deletion.
  base::circular_deque<int64_t> deletable_response_ids_;
  std::vector<int64_t> deleted_response_ids_;
  bool is_response_deletion_scheduled_;
  bool did_start_deleting_responses_;
  int64_t last_deletable_response_rowid_;

  // Created on the IO thread, but only used on the DB thread.
  std::unique_ptr<AppCacheDatabase> database_;

  // Set if we discover a fatal error like a corrupt SQL database or
  // disk cache and cannot continue.
  bool is_disabled_;

  // This is set when we want to use the post-cleanup callback to initiate
  // directory deletion.
  bool delete_and_start_over_pending_;

  // This is set when we know that a call to Disable() will result in
  // OnDiskCacheCleanupComplete() eventually called.
  bool expecting_cleanup_complete_on_disable_;

  std::unique_ptr<AppCacheDiskCache> disk_cache_;
  base::OneShotTimer lazy_commit_timer_;

  // Used to short-circuit certain operations without having to schedule
  // any tasks on the background database thread.
  base::circular_deque<base::OnceClosure> pending_simple_tasks_;
  base::WeakPtrFactory<AppCacheStorageImpl> weak_factory_{this};

  friend class content::AppCacheStorageImplTest;
  friend class content::ChromeAppCacheServiceTest;
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_STORAGE_IMPL_H_
