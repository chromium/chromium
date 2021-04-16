// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_GROUP_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_GROUP_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

namespace appcache_update_job_unittest {
class AppCacheUpdateJobTest;
FORWARD_DECLARE_TEST(AppCacheUpdateJobTest, AlreadyChecking);
FORWARD_DECLARE_TEST(AppCacheUpdateJobTest, AlreadyDownloading);
}  // namespace appcache_update_job_unittest

namespace appcache_cache_helper_unittest {
class AppCacheCacheHelperTest;
FORWARD_DECLARE_TEST(AppCacheCacheHelperTest,
                     IfModifiedSinceUpgradeParserVersion0);
}  // namespace appcache_cache_helper_unittest

FORWARD_DECLARE_TEST(AppCacheGroupTest, StartUpdate);
FORWARD_DECLARE_TEST(AppCacheGroupTest, CancelUpdate);
FORWARD_DECLARE_TEST(AppCacheGroupTest, QueueUpdate);
class AppCache;
class AppCacheHost;
class AppCacheStorage;
class AppCacheUpdateJob;
class HostObserver;
class MockAppCacheStorage;

// Collection of application caches identified by the same manifest URL.
// A group exists as long as it is in use by a host or is being updated.
class CONTENT_EXPORT AppCacheGroup
    : public base::RefCounted<AppCacheGroup> {
 public:

  class CONTENT_EXPORT UpdateObserver {
   public:
    UpdateObserver(const UpdateObserver&) = delete;
    UpdateObserver& operator=(const UpdateObserver&) = delete;

    // Called just after an appcache update has completed.
    virtual void OnUpdateComplete(AppCacheGroup* group) = 0;

   protected:
    // The constructor and destructor exist to facilitate subclassing, and
    // should not be called directly.
    UpdateObserver() noexcept = default;
    virtual ~UpdateObserver() = default;
  };

  enum UpdateAppCacheStatus {
    IDLE,
    CHECKING,
    DOWNLOADING,
  };

  AppCacheGroup(AppCacheStorage* storage,
                const GURL& manifest_url,
                int64_t group_id);

  // Adds/removes an update observer, the AppCacheGroup does not take
  // ownership of the observer.
  void AddUpdateObserver(UpdateObserver* observer);
  void RemoveUpdateObserver(UpdateObserver* observer);

  int64_t group_id() const { return group_id_; }
  const GURL& manifest_url() const { return manifest_url_; }
  base::Time creation_time() const { return creation_time_; }
  void set_creation_time(base::Time time) { creation_time_ = time; }
  bool is_obsolete() const { return is_obsolete_; }
  void set_obsolete(bool value) { is_obsolete_ = value; }
  bool is_being_deleted() const { return is_being_deleted_; }
  void set_being_deleted(bool value) { is_being_deleted_ = value; }
  base::Time last_full_update_check_time() const {
    return last_full_update_check_time_;
  }
  void set_last_full_update_check_time(base::Time time) {
    last_full_update_check_time_ = time;
  }
  base::Time first_evictable_error_time() const {
    return first_evictable_error_time_;
  }
  void set_first_evictable_error_time(base::Time time) {
    first_evictable_error_time_ = time;
  }

  AppCache* newest_complete_cache() const { return newest_complete_cache_; }

  void AddCache(AppCache* complete_cache);
  void RemoveCache(AppCache* cache);
  bool HasCache() const { return newest_complete_cache_ != nullptr; }

  void AddNewlyDeletableResponseIds(std::vector<int64_t>* response_ids);

  UpdateAppCacheStatus update_status() const { return update_status_; }

  // Starts an update via update() javascript API.
  void StartUpdate() { StartUpdateWithHost(nullptr); }

  // Starts an update for a doc loaded from an application cache.
  void StartUpdateWithHost(AppCacheHost* host)  {
    StartUpdateWithNewMasterEntry(host, GURL());
  }

  // Starts an update for a doc loaded using HTTP GET or equivalent with
  // an <html> tag manifest attribute value that matches this group's
  // manifest url.
  void StartUpdateWithNewMasterEntry(AppCacheHost* host,
                                     const GURL& new_master_resource);

  // Cancels an update if one is running.
  void CancelUpdate();

 private:
  class HostObserver;

  friend class base::RefCounted<AppCacheGroup>;
  friend class content::appcache_update_job_unittest::AppCacheUpdateJobTest;
  friend class content::MockAppCacheStorage;  // for old_caches()
  friend class AppCacheUpdateJob;

  ~AppCacheGroup();

  using QueuedUpdates =
      std::map<UpdateObserver*, std::pair<AppCacheHost*, GURL>>;

  static const int kUpdateRestartDelayMs = 1000;

  AppCacheUpdateJob* update_job() { return update_job_; }
  void SetUpdateAppCacheStatus(UpdateAppCacheStatus status);

  void NotifyContentBlocked();

  const std::vector<AppCache*>& old_caches() const { return old_caches_; }

  // Update cannot be processed at this time. Queue it for a later run.
  void QueueUpdate(AppCacheHost* host, const GURL& new_master_resource);
  void RunQueuedUpdates();
  static bool FindObserver(
      const UpdateObserver* find_me,
      const base::ObserverList<UpdateObserver>::Unchecked& observer_list);
  void ScheduleUpdateRestart(int delay_ms);
  void HostDestructionImminent(AppCacheHost* host);

  const int64_t group_id_;
  const GURL manifest_url_;
  base::Time creation_time_;
  UpdateAppCacheStatus update_status_;
  bool is_obsolete_;
  bool is_being_deleted_;
  std::vector<int64_t> newly_deletable_response_ids_;

  // Most update checks respect the cache control headers of the manifest
  // resource, but we bypass the http cache for a "full" update check after 24
  // hours since the last full check was successfully performed.
  base::Time last_full_update_check_time_;

  // Groups that fail to update for a sufficiently long time are evicted. This
  // value is reset after a successful update or update check.
  base::Time first_evictable_error_time_;

  // Old complete app caches.
  std::vector<AppCache*> old_caches_;

  // Newest cache in this group to be complete, aka relevant cache.
  AppCache* newest_complete_cache_;

  // Current update job for this group, if any.
  AppCacheUpdateJob* update_job_;

  // Central storage object.
  AppCacheStorage* storage_;

  // List of objects observing this group.
  base::ObserverList<UpdateObserver>::Unchecked observers_;

  // Updates that have been queued for the next run.
  QueuedUpdates queued_updates_;
  base::ObserverList<UpdateObserver>::Unchecked queued_observers_;
  base::CancelableOnceClosure restart_update_task_;
  std::unique_ptr<HostObserver> host_observer_;

  // True if we're in our destructor.
  bool is_in_dtor_;

  FRIEND_TEST_ALL_PREFIXES(content::AppCacheGroupTest, StartUpdate);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheGroupTest, CancelUpdate);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheGroupTest, QueueUpdate);
  FRIEND_TEST_ALL_PREFIXES(
      content::appcache_update_job_unittest::AppCacheUpdateJobTest,
      AlreadyChecking);
  FRIEND_TEST_ALL_PREFIXES(
      content::appcache_update_job_unittest::AppCacheUpdateJobTest,
      AlreadyDownloading);
  FRIEND_TEST_ALL_PREFIXES(
      content::appcache_cache_helper_unittest::AppCacheCacheHelperTest,
      IfModifiedSinceUpgradeParserVersion0);

  DISALLOW_COPY_AND_ASSIGN(AppCacheGroup);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_GROUP_H_
