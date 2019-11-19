// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_MOCK_APPCACHE_STORAGE_H_
#define CONTENT_BROWSER_APPCACHE_MOCK_APPCACHE_STORAGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_disk_cache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_storage.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {
FORWARD_DECLARE_TEST(AppCacheServiceImplTest, DeleteAppCachesForOrigin);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest, BasicFindMainResponse);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest,
                     BasicFindMainFallbackResponse);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest, CreateGroup);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest, FindMainResponseExclusions);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest,
                     FindMainResponseWithMultipleCandidates);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest, LoadCache_FarHit);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest, LoadGroupAndCache_FarHit);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest, MakeGroupObsolete);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest, StoreNewGroup);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest, StoreExistingGroup);
FORWARD_DECLARE_TEST(MockAppCacheStorageTest,
                     StoreExistingGroupExistingCache);
class AppCacheRequestHandlerTest;
class AppCacheServiceImplTest;
class MockAppCacheStorageTest;

namespace appcache_update_job_unittest {
class AppCacheUpdateJobTest;
}

// For use in unit tests.
// Note: This class is also being used to bootstrap our development efforts.
// We can get web tests up and running, and back fill with real storage
// somewhat in parallel.
class MockAppCacheStorage : public AppCacheStorage {
 public:
  explicit MockAppCacheStorage(AppCacheServiceImpl* service);
  ~MockAppCacheStorage() override;

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
  friend class AppCacheRequestHandlerTest;
  friend class AppCacheServiceImplTest;
  friend class appcache_update_job_unittest::AppCacheUpdateJobTest;
  friend class MockAppCacheStorageTest;

  using StoredCacheMap = std::unordered_map<int64_t, scoped_refptr<AppCache>>;
  using StoredGroupMap = std::map<GURL, scoped_refptr<AppCacheGroup>>;
  using DoomedResponseIds = std::set<int64_t>;
  using StoredEvictionTimesMap =
      std::map<int64_t, std::pair<base::Time, base::Time>>;

  void ProcessGetAllInfo(scoped_refptr<DelegateReference> delegate_ref);
  void ProcessLoadCache(int64_t id,
                        scoped_refptr<DelegateReference> delegate_ref);
  void ProcessLoadOrCreateGroup(
      const GURL& manifest_url, scoped_refptr<DelegateReference> delegate_ref);
  void ProcessStoreGroupAndNewestCache(
      scoped_refptr<AppCacheGroup> group, scoped_refptr<AppCache> newest_cache,
      scoped_refptr<DelegateReference> delegate_ref);
  void ProcessMakeGroupObsolete(scoped_refptr<AppCacheGroup> group,
                                scoped_refptr<DelegateReference> delegate_ref,
                                int response_code);
  void ProcessFindResponseForMainRequest(
      const GURL& url, scoped_refptr<DelegateReference> delegate_ref);

  void ScheduleTask(base::OnceClosure task);
  void RunOnePendingTask();

  void AddStoredCache(AppCache* cache);
  void RemoveStoredCache(AppCache* cache);
  void RemoveStoredCaches(const std::vector<AppCache*>& caches);
  bool IsCacheStored(const AppCache* cache) {
    return stored_caches_.find(cache->cache_id()) != stored_caches_.end();
  }

  void AddStoredGroup(AppCacheGroup* group);
  void RemoveStoredGroup(AppCacheGroup* group);
  bool IsGroupStored(const AppCacheGroup* group) {
    return IsGroupForManifestStored(group->manifest_url());
  }
  bool IsGroupForManifestStored(const GURL& manifest_url) {
    return stored_groups_.find(manifest_url) != stored_groups_.end();
  }

  // These helpers determine when certain operations should complete
  // asynchronously vs synchronously to faithfully mimic, or mock,
  // the behavior of the real implemenation of the AppCacheStorage
  // interface.
  bool ShouldGroupLoadAppearAsync(const AppCacheGroup* group);
  bool ShouldCacheLoadAppearAsync(const AppCache* cache);

  // Lazily constructed in-memory disk cache.
  AppCacheDiskCache* disk_cache() {
    if (!disk_cache_) {
      const int kMaxCacheSize = 10 * 1024 * 1024;
      disk_cache_ = std::make_unique<AppCacheDiskCache>();
      disk_cache_->InitWithMemBackend(kMaxCacheSize,
                                      net::CompletionOnceCallback());
    }
    return disk_cache_.get();
  }

  // Simulate failures for testing. Once set all subsequent calls
  // to MakeGroupObsolete or StorageGroupAndNewestCache will fail.
  void SimulateMakeGroupObsoleteFailure() {
    simulate_make_group_obsolete_failure_ = true;
  }
  void SimulateStoreGroupAndNewestCacheFailure() {
    simulate_store_group_and_newest_cache_failure_ = true;
  }

  // Simulate FindResponseFor results for testing. These
  // provided values will be return on the next call to
  // the corresponding Find method, subsequent calls are
  // unaffected.
  void SimulateFindMainResource(const AppCacheEntry& entry,
                                const GURL& fallback_url,
                                const AppCacheEntry& fallback_entry,
                                int64_t cache_id,
                                int64_t group_id,
                                const GURL& manifest_url) {
    simulate_find_main_resource_ = true;
    simulate_find_sub_resource_ = false;
    simulated_found_entry_ = entry;
    simulated_found_fallback_url_ = fallback_url;
    simulated_found_fallback_entry_ = fallback_entry;
    simulated_found_cache_id_ = cache_id;
    simulated_found_group_id_ = group_id;
    simulated_found_manifest_url_ = manifest_url,
    simulated_found_network_namespace_ = false;  // N/A to main resource loads
  }
  void SimulateFindSubResource(
      const AppCacheEntry& entry,
      const AppCacheEntry& fallback_entry,
      bool network_namespace) {
    simulate_find_main_resource_ = false;
    simulate_find_sub_resource_ = true;
    simulated_found_entry_ = entry;
    simulated_found_fallback_entry_ = fallback_entry;
    simulated_found_cache_id_ =
        blink::mojom::kAppCacheNoCacheId;    // N/A to sub resource loads
    simulated_found_manifest_url_ = GURL();  // N/A to sub resource loads
    simulated_found_group_id_ = 0;  // N/A to sub resource loads
    simulated_found_network_namespace_ = network_namespace;
  }

  void SimulateGetAllInfo(scoped_refptr<AppCacheInfoCollection> info) {
    simulated_appcache_info_ = std::move(info);
  }

  void SimulateResponseReader(std::unique_ptr<AppCacheResponseReader> reader) {
    simulated_reader_ = std::move(reader);
  }

  StoredCacheMap stored_caches_;
  StoredGroupMap stored_groups_;
  StoredEvictionTimesMap stored_eviction_times_;
  DoomedResponseIds doomed_response_ids_;
  std::unique_ptr<AppCacheDiskCache> disk_cache_;
  base::circular_deque<base::OnceClosure> pending_tasks_;

  bool simulate_make_group_obsolete_failure_;
  bool simulate_store_group_and_newest_cache_failure_;

  bool simulate_find_main_resource_;
  bool simulate_find_sub_resource_;
  AppCacheEntry simulated_found_entry_;
  AppCacheEntry simulated_found_fallback_entry_;
  int64_t simulated_found_cache_id_;
  int64_t simulated_found_group_id_;
  GURL simulated_found_fallback_url_;
  GURL simulated_found_manifest_url_;
  bool simulated_found_network_namespace_;
  scoped_refptr<AppCacheInfoCollection> simulated_appcache_info_;
  std::unique_ptr<AppCacheResponseReader> simulated_reader_;

  base::WeakPtrFactory<MockAppCacheStorage> weak_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           BasicFindMainResponse);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           BasicFindMainFallbackResponse);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, CreateGroup);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           FindMainResponseExclusions);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           FindMainResponseWithMultipleCandidates);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, LoadCache_FarHit);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           LoadGroupAndCache_FarHit);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, MakeGroupObsolete);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, StoreNewGroup);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           StoreExistingGroup);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           StoreExistingGroupExistingCache);
  FRIEND_TEST_ALL_PREFIXES(AppCacheServiceImplTest,
                           DeleteAppCachesForOrigin);

  DISALLOW_COPY_AND_ASSIGN(MockAppCacheStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_MOCK_APPCACHE_STORAGE_H_
