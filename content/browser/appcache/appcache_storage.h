// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_STORAGE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_STORAGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/appcache/appcache_working_set.h"
#include "content/common/content_export.h"
#include "url/origin.h"

class GURL;

namespace content {

namespace appcache_storage_unittest {
class AppCacheStorageTest;
FORWARD_DECLARE_TEST(AppCacheStorageTest, DelegateReferences);
FORWARD_DECLARE_TEST(AppCacheStorageTest, UsageMap);
}  // namespace appcache_storage_unittest

class AppCache;
class AppCacheEntry;
class AppCacheGroup;
class AppCacheQuotaClientTest;
class AppCacheResponseMetadataWriter;
class AppCacheResponseReader;
class AppCacheResponseTest;
class AppCacheResponseWriter;
class AppCacheServiceImpl;
struct AppCacheInfoCollection;
struct HttpResponseInfoIOBuffer;

class CONTENT_EXPORT AppCacheStorage {
 public:
  class CONTENT_EXPORT Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // If retrieval fails, |collection| will be null.
    virtual void OnAllInfo(AppCacheInfoCollection* collection) {}

    // If the load fails, |cache| will be null.
    virtual void OnCacheLoaded(AppCache* cache, int64_t cache_id) {}

    // If the load fails, |group| will be null.
    virtual void OnGroupLoaded(
        AppCacheGroup* group, const GURL& manifest_url) {}

    // If successfully stored, |success| will be true.
    virtual void OnGroupAndNewestCacheStored(
        AppCacheGroup* group, AppCache* newest_cache, bool success,
        bool would_exceed_quota) {}

    // If the operation fails, |success| will be false.
    virtual void OnGroupMadeObsolete(AppCacheGroup* group,
                                     bool success,
                                     int response_code) {}

    // If a load fails, |response_info| will be null.
    virtual void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                                      int64_t response_id) {}

    // If no response is found, |entry|'s response_id() and |fallback_entry|'s
    // response_id() will be kAppCacheNoResponseId.
    //
    // If the response is the entry for an intercept or fallback namespace,
    // |namespace_entry_url| refers to the entry. Otherwise, it is empty.
    //
    // If a response is found, |cache_id|, |group_id|, and |manifest_url|
    // identify the cache containing the response.
    virtual void OnMainResponseFound(const GURL& url,
                                     const AppCacheEntry& entry,
                                     const GURL& namespace_entry_url,
                                     const AppCacheEntry& fallback_entry,
                                     int64_t cache_id,
                                     int64_t group_id,
                                     const GURL& mainfest_url) {}

   protected:
    // The constructor and destructor exist to facilitate subclassing, and
    // should not be called directly.
    Delegate() noexcept = default;
    virtual ~Delegate() = default;
  };

  explicit AppCacheStorage(AppCacheServiceImpl* service);
  virtual ~AppCacheStorage();

  // Schedules a task to retrieve basic info about all groups and caches
  // stored in the system. Upon completion the delegate will be called
  // with the results.
  virtual void GetAllInfo(Delegate* delegate) = 0;

  // Schedules a cache to be loaded from storage. Upon load completion
  // the delegate will be called back. If the cache already resides in
  // memory, the delegate will be called back immediately without returning
  // to the message loop. If the load fails, the delegate will be called
  // back with a NULL cache pointer.
  virtual void LoadCache(int64_t id, Delegate* delegate) = 0;

  // Schedules a group and its newest cache, if any, to be loaded from storage.
  // Upon load completion the delegate will be called back. If the group
  // and newest cache already reside in memory, the delegate will be called
  // back immediately without returning to the message loop. If the load fails,
  // the delegate will be called back with a NULL group pointer.
  virtual void LoadOrCreateGroup(
      const GURL& manifest_url, Delegate* delegate) = 0;

  // Schedules response info to be loaded from storage.
  // Upon load completion the delegate will be called back. If the data
  // already resides in memory, the delegate will be called back
  // immediately without returning to the message loop. If the load fails,
  // the delegate will be called back with a NULL pointer.
  virtual void LoadResponseInfo(const GURL& manifest_url,
                                int64_t response_id,
                                Delegate* delegate);

  // Schedules a group and its newest complete cache to be initially stored or
  // incrementally updated with new changes. Upon completion the delegate
  // will be called back. A group without a newest cache cannot be stored.
  // It's a programming error to call this method without a newest cache. A
  // side effect of storing a new newest cache is the removal of the group's
  // old caches and responses from persistent storage (although they may still
  // linger in the in-memory working set until no longer needed). The new
  // cache will be added as the group's newest complete cache only if storage
  // succeeds.
  virtual void StoreGroupAndNewestCache(
      AppCacheGroup* group, AppCache* newest_cache, Delegate* delegate) = 0;

  // Schedules a query to identify a response for a main request. Upon
  // completion the delegate will be called back.
  virtual void FindResponseForMainRequest(
      const GURL& url,
      const GURL& preferred_manifest_url,
      Delegate* delegate) = 0;

  // Performs an immediate lookup of the in-memory cache to
  // identify a response for a sub resource request.
  virtual void FindResponseForSubRequest(
      AppCache* cache, const GURL& url,
      AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
      bool* found_network_namespace) = 0;

  // Immediately updates in-memory storage, if the cache is in memory,
  // and schedules a task to update persistent storage. If the cache is
  // already scheduled to be loaded, upon loading completion the entry
  // will be marked. There is no delegate completion callback.
  virtual void MarkEntryAsForeign(const GURL& entry_url, int64_t cache_id) = 0;

  // Schedules a task to update persistent storage and doom the group and all
  // related caches and responses for deletion. Upon completion the in-memory
  // instance is marked as obsolete and the delegate callback is called.
  virtual void MakeGroupObsolete(AppCacheGroup* group,
                                 Delegate* delegate,
                                 int response_code) = 0;

  // Schedules a task to update persistent storage with the times of the first
  // evictable error and last successful full update check.
  virtual void StoreEvictionTimes(AppCacheGroup* group) = 0;

  // Cancels all pending callbacks for the delegate. The delegate callbacks
  // will not be invoked after, however any scheduled operations will still
  // take place. The callbacks for subsequently scheduled operations are
  // unaffected.
  void CancelDelegateCallbacks(Delegate* delegate) {
    DelegateReference* delegate_reference = GetDelegateReference(delegate);
    if (delegate_reference)
      delegate_reference->CancelReference();
  }

  // Creates a reader to read a response from storage.
  virtual std::unique_ptr<AppCacheResponseReader> CreateResponseReader(
      const GURL& manifest_url,
      int64_t response_id) = 0;

  // Creates a writer to write a new response to storage. This call
  // establishes a new response id.
  virtual std::unique_ptr<AppCacheResponseWriter> CreateResponseWriter(
      const GURL& manifest_url) = 0;

  // Creates a metadata writer to write metadata of response to storage.
  virtual std::unique_ptr<AppCacheResponseMetadataWriter>
  CreateResponseMetadataWriter(int64_t response_id) = 0;

  // Schedules the lazy deletion of responses and saves the ids
  // persistently such that the responses will be deleted upon restart
  // if they aren't deleted prior to shutdown.
  virtual void DoomResponses(const GURL& manifest_url,
                             const std::vector<int64_t>& response_ids) = 0;

  // Schedules the lazy deletion of responses without persistently saving
  // the response ids.
  virtual void DeleteResponses(const GURL& manifest_url,
                               const std::vector<int64_t>& response_ids) = 0;

  // Returns true if the AppCacheStorage instance is initialized.
  virtual bool IsInitialized() = 0;

  // Generates unique storage ids for different object types.
  int64_t NewCacheId() { return ++last_cache_id_; }
  int64_t NewGroupId() { return ++last_group_id_; }

  // The working set of object instances currently in memory.
  AppCacheWorkingSet* working_set() { return &working_set_; }

  // A map of origins to usage.
  const std::map<url::Origin, int64_t>& usage_map() const { return usage_map_; }

  // Simple ptr back to the service object that owns us.
  AppCacheServiceImpl* service() { return service_; }

  // Returns a weak pointer reference to the AppCacheStorage instance.
  base::WeakPtr<AppCacheStorage> GetWeakPtr();

 protected:
  friend class content::AppCacheQuotaClientTest;
  friend class content::AppCacheResponseTest;
  friend class content::appcache_storage_unittest::AppCacheStorageTest;

  // Helper used to manage multiple references to a 'delegate' and to
  // allow all pending callbacks to that delegate to be easily cancelled.
  struct CONTENT_EXPORT DelegateReference :
      public base::RefCounted<DelegateReference> {
    Delegate* delegate;
    AppCacheStorage* storage;

    DelegateReference(Delegate* delegate, AppCacheStorage* storage);

    void CancelReference() {
      storage->delegate_references_.erase(delegate);
      storage = nullptr;
      delegate = nullptr;
    }

   private:
    friend class base::RefCounted<DelegateReference>;
    ~DelegateReference();
  };

  // Helper for calling a function on a collection of delegates.
  //
  // ForEachCallable: (AppCacheStorage::Delegate*) -> void
  template <typename ForEachCallable>
  static void ForEachDelegate(
      const std::vector<scoped_refptr<DelegateReference>>& delegates,
      const ForEachCallable& callable) {
    for (const scoped_refptr<DelegateReference>& delegate_ref : delegates) {
      Delegate* delegate = delegate_ref->delegate;
      if (delegate != nullptr)
        callable(delegate);
    }
  }

  // Helper used to manage an async LoadResponseInfo calls on behalf of
  // multiple callers.
  class ResponseInfoLoadTask {
   public:
    ResponseInfoLoadTask(const GURL& manifest_url,
                         int64_t response_id,
                         AppCacheStorage* storage);
    ~ResponseInfoLoadTask();

    int64_t response_id() const { return response_id_; }
    const GURL& manifest_url() const { return manifest_url_; }

    void AddDelegate(DelegateReference* delegate_reference) {
      delegates_.push_back(delegate_reference);
    }

    void StartIfNeeded();

   private:
    void OnReadComplete(int result);

    AppCacheStorage* storage_;
    GURL manifest_url_;
    int64_t response_id_;
    std::unique_ptr<AppCacheResponseReader> reader_;
    std::vector<scoped_refptr<DelegateReference>> delegates_;
    scoped_refptr<HttpResponseInfoIOBuffer> info_buffer_;
  };

  DelegateReference* GetDelegateReference(Delegate* delegate) {
    std::map<Delegate*, DelegateReference*>::iterator iter =
        delegate_references_.find(delegate);
    if (iter != delegate_references_.end())
      return iter->second;
    return nullptr;
  }

  DelegateReference* GetOrCreateDelegateReference(Delegate* delegate) {
    DelegateReference* reference = GetDelegateReference(delegate);
    if (reference)
      return reference;
    return new DelegateReference(delegate, this);
  }

  ResponseInfoLoadTask* GetOrCreateResponseInfoLoadTask(
      const GURL& manifest_url,
      int64_t response_id) {
    auto iter = pending_info_loads_.find(response_id);
    if (iter != pending_info_loads_.end())
      return iter->second.get();
    return new ResponseInfoLoadTask(manifest_url, response_id, this);
  }

  // Should only be called when creating a new response writer.
  int64_t NewResponseId() { return ++last_response_id_; }

  // Helpers to query and notify the QuotaManager.
  void UpdateUsageMapAndNotify(const url::Origin& origin, int64_t new_usage);
  void ClearUsageMapAndNotify();
  void NotifyStorageAccessed(const url::Origin& origin);

  // The last storage id used for different object types.
  int64_t last_cache_id_;
  int64_t last_group_id_;
  int64_t last_response_id_;

  // Maps origin to padded usage.
  std::map<url::Origin, int64_t> usage_map_;
  AppCacheWorkingSet working_set_;
  AppCacheServiceImpl* service_;
  std::map<Delegate*, DelegateReference*> delegate_references_;

  // Note that the ResponseInfoLoadTask items add themselves to this map.
  std::map<int64_t, std::unique_ptr<ResponseInfoLoadTask>> pending_info_loads_;

  // The set of last ids must be retrieved from storage prior to being used.
  static const int64_t kUnitializedId;

  FRIEND_TEST_ALL_PREFIXES(
      content::appcache_storage_unittest::AppCacheStorageTest,
      DelegateReferences);
  FRIEND_TEST_ALL_PREFIXES(
      content::appcache_storage_unittest::AppCacheStorageTest,
      UsageMap);

  // The WeakPtrFactory below must occur last in the class definition so they
  // get destroyed last.
  base::WeakPtrFactory<AppCacheStorage> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppCacheStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_STORAGE_H_
