// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/appcache/appcache_database.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_manifest_parser.h"
#include "content/browser/appcache/appcache_namespace.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "url/gurl.h"

namespace content {
FORWARD_DECLARE_TEST(AppCacheTest, InitializeWithManifest);
FORWARD_DECLARE_TEST(AppCacheTest, ToFromDatabaseRecords);
class AppCacheGroup;
class AppCacheHost;
class AppCacheStorage;
class AppCacheTest;
class AppCacheStorageImplTest;

namespace appcache_update_job_unittest {
class AppCacheUpdateJobTest;
}

// Set of cached resources for an application. A cache exists as long as a
// host is associated with it, the cache is in an appcache group or the
// cache is being created during an appcache update.
class CONTENT_EXPORT AppCache
    : public base::RefCounted<AppCache> {
 public:
  using EntryMap = std::map<GURL, AppCacheEntry>;
  using AppCacheHosts = std::set<AppCacheHost*>;

  AppCache(AppCacheStorage* storage, int64_t cache_id);

  int64_t cache_id() const { return cache_id_; }

  AppCacheGroup* owning_group() const { return owning_group_.get(); }

  bool is_complete() const { return is_complete_; }
  void set_complete(bool value) { is_complete_ = value; }

  // Adds a new entry. Entry must not already be in cache.
  void AddEntry(const GURL& url, const AppCacheEntry& entry);

  // Adds a new entry or modifies an existing entry by merging the types
  // of the new entry with the existing entry. Returns true if a new entry
  // is added, false if the flags are merged into an existing entry.
  bool AddOrModifyEntry(const GURL& url, const AppCacheEntry& entry);

  // Removes an entry from the EntryMap, the URL must be in the set.
  void RemoveEntry(const GURL& url);

  // Do not store or delete the returned ptr, they're owned by 'this'.
  AppCacheEntry* GetEntry(const GURL& url);
  const AppCacheEntry* GetEntryWithResponseId(int64_t response_id) {
    return GetEntryAndUrlWithResponseId(response_id, nullptr);
  }
  const AppCacheEntry* GetEntryAndUrlWithResponseId(int64_t response_id,
                                                    GURL* optional_url);
  const EntryMap& entries() const { return entries_; }

  // Returns the URL of the resource used as entry for 'namespace_url'.
  GURL GetFallbackEntryUrl(const GURL& namespace_url) const {
    return GetNamespaceEntryUrl(fallback_namespaces_, namespace_url);
  }
  GURL GetInterceptEntryUrl(const GURL& namespace_url) const {
    return GetNamespaceEntryUrl(intercept_namespaces_, namespace_url);
  }

  AppCacheHosts& associated_hosts() { return associated_hosts_; }

  bool IsNewerThan(AppCache* cache) const {
    // TODO(michaeln): revisit, the system clock can be set
    // back in time which would confuse this logic.
    if (update_time_ > cache->update_time_)
      return true;

    // Tie breaker. Newer caches have a larger cache ID.
    if (update_time_ == cache->update_time_)
      return cache_id_ > cache->cache_id_;

    return false;
  }

  base::Time update_time() const { return update_time_; }

  // The sum of all the sizes of the resources in this cache.
  int64_t cache_size() const { return cache_size_; }

  // The sum of all the padding sizes of the resources in this cache.
  //
  // See AppCacheEntry for a description of how padding size works.
  int64_t padding_size() const { return padding_size_; }

  void set_update_time(base::Time ticks) { update_time_ = ticks; }

  // Initializes the cache with information in the manifest.
  // Do not use the manifest after this call.
  void InitializeWithManifest(AppCacheManifest* manifest);

  // Initializes the cache with the information in the database records.
  void InitializeWithDatabaseRecords(
      const AppCacheDatabase::CacheRecord& cache_record,
      const std::vector<AppCacheDatabase::EntryRecord>& entries,
      const std::vector<AppCacheDatabase::NamespaceRecord>& intercepts,
      const std::vector<AppCacheDatabase::NamespaceRecord>& fallbacks,
      const std::vector<AppCacheDatabase::OnlineWhiteListRecord>& whitelists);

  // Returns the database records to be stored in the AppCacheDatabase
  // to represent this cache.
  void ToDatabaseRecords(
      const AppCacheGroup* group,
      AppCacheDatabase::CacheRecord* cache_record,
      std::vector<AppCacheDatabase::EntryRecord>* entries,
      std::vector<AppCacheDatabase::NamespaceRecord>* intercepts,
      std::vector<AppCacheDatabase::NamespaceRecord>* fallbacks,
      std::vector<AppCacheDatabase::OnlineWhiteListRecord>* whitelists);

  bool FindResponseForRequest(const GURL& url,
      AppCacheEntry* found_entry, GURL* found_intercept_namespace,
      AppCacheEntry* found_fallback_entry, GURL* found_fallback_namespace,
      bool* found_network_namespace);

  // Populates the 'infos' vector with an element per entry in the appcache.
  void ToResourceInfoVector(
      std::vector<blink::mojom::AppCacheResourceInfo>* infos) const;

  static const AppCacheNamespace* FindNamespace(
      const std::vector<AppCacheNamespace>& namespaces,
      const GURL& url);

 private:
  friend class AppCacheGroup;
  friend class AppCacheHost;
  friend class content::AppCacheTest;
  friend class content::AppCacheStorageImplTest;
  friend class content::appcache_update_job_unittest::AppCacheUpdateJobTest;
  friend class base::RefCounted<AppCache>;

  ~AppCache();

  // Use AppCacheGroup::Add/RemoveCache() to manipulate owning group.
  void set_owning_group(AppCacheGroup* group) { owning_group_ = group; }

  // FindResponseForRequest helpers
  const AppCacheNamespace* FindInterceptNamespace(const GURL& url) {
    return FindNamespace(intercept_namespaces_, url);
  }
  const AppCacheNamespace* FindFallbackNamespace(const GURL& url) {
    return FindNamespace(fallback_namespaces_, url);
  }
  bool IsInNetworkNamespace(const GURL& url) {
    return FindNamespace(online_whitelist_namespaces_, url) != nullptr;
  }

  GURL GetNamespaceEntryUrl(const std::vector<AppCacheNamespace>& namespaces,
                            const GURL& namespace_url) const;

  // Use AppCacheHost::Associate*Cache() to manipulate host association.
  void AssociateHost(AppCacheHost* host) {
    associated_hosts_.insert(host);
  }
  void UnassociateHost(AppCacheHost* host);

  const int64_t cache_id_;
  scoped_refptr<AppCacheGroup> owning_group_;
  AppCacheHosts associated_hosts_;

  EntryMap entries_;    // contains entries of all types

  std::vector<AppCacheNamespace> intercept_namespaces_;
  std::vector<AppCacheNamespace> fallback_namespaces_;
  std::vector<AppCacheNamespace> online_whitelist_namespaces_;
  bool online_whitelist_all_;

  bool is_complete_;

  // when this cache was last updated
  base::Time update_time_;

  int64_t cache_size_;
  int64_t padding_size_;

  // to notify storage when cache is deleted
  AppCacheStorage* storage_;

  FRIEND_TEST_ALL_PREFIXES(content::AppCacheTest, InitializeWithManifest);
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheTest, ToFromDatabaseRecords);
  DISALLOW_COPY_AND_ASSIGN(AppCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_H_
