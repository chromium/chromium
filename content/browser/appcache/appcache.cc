// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/common/appcache_interfaces.h"
#include "url/origin.h"

namespace content {

AppCache::AppCache(AppCacheStorage* storage, int64_t cache_id)
    : cache_id_(cache_id),
      owning_group_(nullptr),
      online_whitelist_all_(false),
      is_complete_(false),
      cache_size_(0),
      storage_(storage) {
  storage_->working_set()->AddCache(this);
}

AppCache::~AppCache() {
  DCHECK(associated_hosts_.empty());
  if (owning_group_.get()) {
    DCHECK(is_complete_);
    owning_group_->RemoveCache(this);
  }
  DCHECK(!owning_group_.get());
  storage_->working_set()->RemoveCache(this);
}

void AppCache::UnassociateHost(AppCacheHost* host) {
  associated_hosts_.erase(host);
}

void AppCache::AddEntry(const GURL& url, const AppCacheEntry& entry) {
  DCHECK(entries_.find(url) == entries_.end());
  entries_.insert(EntryMap::value_type(url, entry));
  cache_size_ += entry.response_size();
}

bool AppCache::AddOrModifyEntry(const GURL& url, const AppCacheEntry& entry) {
  std::pair<EntryMap::iterator, bool> ret =
      entries_.insert(EntryMap::value_type(url, entry));

  // Entry already exists.  Merge the types of the new and existing entries.
  if (!ret.second)
    ret.first->second.add_types(entry.types());
  else
    cache_size_ += entry.response_size();  // New entry. Add to cache size.
  return ret.second;
}

void AppCache::RemoveEntry(const GURL& url) {
  auto found = entries_.find(url);
  DCHECK(found != entries_.end());
  cache_size_ -= found->second.response_size();
  entries_.erase(found);
}

AppCacheEntry* AppCache::GetEntry(const GURL& url) {
  auto it = entries_.find(url);
  return (it != entries_.end()) ? &(it->second) : nullptr;
}

const AppCacheEntry* AppCache::GetEntryAndUrlWithResponseId(
    int64_t response_id,
    GURL* optional_url_out) {
  for (const auto& pair : entries_) {
    if (pair.second.response_id() == response_id) {
      if (optional_url_out)
        *optional_url_out = pair.first;
      return &pair.second;
    }
  }
  return nullptr;
}

GURL AppCache::GetNamespaceEntryUrl(
    const std::vector<AppCacheNamespace>& namespaces,
    const GURL& namespace_url) const {
  size_t count = namespaces.size();
  for (size_t i = 0; i < count; ++i) {
    if (namespaces[i].namespace_url == namespace_url)
      return namespaces[i].target_url;
  }
  NOTREACHED();
  return GURL();
}

namespace {
bool SortNamespacesByLength(
    const AppCacheNamespace& lhs, const AppCacheNamespace& rhs) {
  return lhs.namespace_url.spec().length() > rhs.namespace_url.spec().length();
}
}

void AppCache::InitializeWithManifest(AppCacheManifest* manifest) {
  DCHECK(manifest);
  intercept_namespaces_.swap(manifest->intercept_namespaces);
  fallback_namespaces_.swap(manifest->fallback_namespaces);
  online_whitelist_namespaces_.swap(manifest->online_whitelist_namespaces);
  online_whitelist_all_ = manifest->online_whitelist_all;

  // Sort the namespaces by url string length, longest to shortest,
  // since longer matches trump when matching a url to a namespace.
  std::sort(intercept_namespaces_.begin(), intercept_namespaces_.end(),
            SortNamespacesByLength);
  std::sort(fallback_namespaces_.begin(), fallback_namespaces_.end(),
            SortNamespacesByLength);
}

void AppCache::InitializeWithDatabaseRecords(
    const AppCacheDatabase::CacheRecord& cache_record,
    const std::vector<AppCacheDatabase::EntryRecord>& entries,
    const std::vector<AppCacheDatabase::NamespaceRecord>& intercepts,
    const std::vector<AppCacheDatabase::NamespaceRecord>& fallbacks,
    const std::vector<AppCacheDatabase::OnlineWhiteListRecord>& whitelists) {
  DCHECK(cache_id_ == cache_record.cache_id);
  online_whitelist_all_ = cache_record.online_wildcard;
  update_time_ = cache_record.update_time;

  for (size_t i = 0; i < entries.size(); ++i) {
    const AppCacheDatabase::EntryRecord& entry = entries.at(i);
    AddEntry(entry.url, AppCacheEntry(entry.flags, entry.response_id,
                                      entry.response_size));
  }
  DCHECK(cache_size_ == cache_record.cache_size);

  for (size_t i = 0; i < intercepts.size(); ++i)
    intercept_namespaces_.push_back(intercepts.at(i).namespace_);

  for (size_t i = 0; i < fallbacks.size(); ++i)
    fallback_namespaces_.push_back(fallbacks.at(i).namespace_);

  // Sort the fallback namespaces by url string length, longest to shortest,
  // since longer matches trump when matching a url to a namespace.
  std::sort(intercept_namespaces_.begin(), intercept_namespaces_.end(),
            SortNamespacesByLength);
  std::sort(fallback_namespaces_.begin(), fallback_namespaces_.end(),
            SortNamespacesByLength);

  for (size_t i = 0; i < whitelists.size(); ++i) {
    const AppCacheDatabase::OnlineWhiteListRecord& record = whitelists.at(i);
    online_whitelist_namespaces_.push_back(
        AppCacheNamespace(APPCACHE_NETWORK_NAMESPACE,
                  record.namespace_url,
                  GURL(),
                  record.is_pattern));
  }
}

void AppCache::ToDatabaseRecords(
    const AppCacheGroup* group,
    AppCacheDatabase::CacheRecord* cache_record,
    std::vector<AppCacheDatabase::EntryRecord>* entries,
    std::vector<AppCacheDatabase::NamespaceRecord>* intercepts,
    std::vector<AppCacheDatabase::NamespaceRecord>* fallbacks,
    std::vector<AppCacheDatabase::OnlineWhiteListRecord>* whitelists) {
  DCHECK(group && cache_record && entries && fallbacks && whitelists);
  DCHECK(entries->empty() && fallbacks->empty() && whitelists->empty());

  cache_record->cache_id = cache_id_;
  cache_record->group_id = group->group_id();
  cache_record->online_wildcard = online_whitelist_all_;
  cache_record->update_time = update_time_;
  cache_record->cache_size = 0;

  for (const auto& pair : entries_) {
    entries->push_back(AppCacheDatabase::EntryRecord());
    AppCacheDatabase::EntryRecord& record = entries->back();
    record.url = pair.first;
    record.cache_id = cache_id_;
    record.flags = pair.second.types();
    record.response_id = pair.second.response_id();
    record.response_size = pair.second.response_size();
    cache_record->cache_size += record.response_size;
  }

  const url::Origin origin = url::Origin::Create(group->manifest_url());

  for (size_t i = 0; i < intercept_namespaces_.size(); ++i) {
    intercepts->push_back(AppCacheDatabase::NamespaceRecord());
    AppCacheDatabase::NamespaceRecord& record = intercepts->back();
    record.cache_id = cache_id_;
    record.origin = origin;
    record.namespace_ = intercept_namespaces_[i];
  }

  for (size_t i = 0; i < fallback_namespaces_.size(); ++i) {
    fallbacks->push_back(AppCacheDatabase::NamespaceRecord());
    AppCacheDatabase::NamespaceRecord& record = fallbacks->back();
    record.cache_id = cache_id_;
    record.origin = origin;
    record.namespace_ = fallback_namespaces_[i];
  }

  for (size_t i = 0; i < online_whitelist_namespaces_.size(); ++i) {
    whitelists->push_back(AppCacheDatabase::OnlineWhiteListRecord());
    AppCacheDatabase::OnlineWhiteListRecord& record = whitelists->back();
    record.cache_id = cache_id_;
    record.namespace_url = online_whitelist_namespaces_[i].namespace_url;
    record.is_pattern = online_whitelist_namespaces_[i].is_pattern;
  }
}

bool AppCache::FindResponseForRequest(const GURL& url,
    AppCacheEntry* found_entry, GURL* found_intercept_namespace,
    AppCacheEntry* found_fallback_entry, GURL* found_fallback_namespace,
    bool* found_network_namespace) {
  // Ignore fragments when looking up URL in the cache.
  GURL url_no_ref;
  if (url.has_ref()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    url_no_ref = url.ReplaceComponents(replacements);
  } else {
    url_no_ref = url;
  }

  // 6.6.6 Changes to the networking model

  AppCacheEntry* entry = GetEntry(url_no_ref);
  if (entry) {
    *found_entry = *entry;
    return true;
  }

  *found_network_namespace = IsInNetworkNamespace(url_no_ref);
  if (*found_network_namespace)
    return true;

  const AppCacheNamespace* intercept_namespace =
      FindInterceptNamespace(url_no_ref);
  if (intercept_namespace) {
    entry = GetEntry(intercept_namespace->target_url);
    DCHECK(entry);
    *found_entry = *entry;
    *found_intercept_namespace = intercept_namespace->namespace_url;
    return true;
  }

  const AppCacheNamespace* fallback_namespace =
      FindFallbackNamespace(url_no_ref);
  if (fallback_namespace) {
    entry = GetEntry(fallback_namespace->target_url);
    DCHECK(entry);
    *found_fallback_entry = *entry;
    *found_fallback_namespace = fallback_namespace->namespace_url;
    return true;
  }

  *found_network_namespace = online_whitelist_all_;
  return *found_network_namespace;
}

void AppCache::ToResourceInfoVector(
    std::vector<AppCacheResourceInfo>* infos) const {
  DCHECK(infos && infos->empty());
  for (const auto& pair : entries_) {
    infos->push_back(AppCacheResourceInfo());
    AppCacheResourceInfo& info = infos->back();
    info.url = pair.first;
    info.is_master = pair.second.IsMaster();
    info.is_manifest = pair.second.IsManifest();
    info.is_intercept = pair.second.IsIntercept();
    info.is_fallback = pair.second.IsFallback();
    info.is_foreign = pair.second.IsForeign();
    info.is_explicit = pair.second.IsExplicit();
    info.size = pair.second.response_size();
    info.response_id = pair.second.response_id();
  }
}

// static
const AppCacheNamespace* AppCache::FindNamespace(
    const std::vector<AppCacheNamespace>& namespaces,
    const GURL& url) {
  size_t count = namespaces.size();
  for (size_t i = 0; i < count; ++i) {
    if (namespaces[i].IsMatch(url))
      return &namespaces[i];
  }
  return nullptr;
}

}  // namespace content
