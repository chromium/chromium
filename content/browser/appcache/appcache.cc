// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "content/browser/appcache/appcache_database.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/common/appcache_interfaces.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "url/origin.h"

namespace content {

// static
bool AppCache::CheckValidManifestScope(const GURL& manifest_url,
                                       const std::string& manifest_scope) {
  if (manifest_scope.empty())
    return false;
  const GURL url = manifest_url.Resolve(manifest_scope);
  return url.is_valid() && !url.has_ref() && !url.has_query() &&
         url.spec().back() == '/';
}

// static
std::string AppCache::GetManifestScope(const GURL& manifest_url,
                                       std::string optional_scope) {
  DCHECK(manifest_url.is_valid());
  if (!optional_scope.empty()) {
    std::string scope = manifest_url.Resolve(optional_scope).path();
    if (CheckValidManifestScope(manifest_url, scope)) {
      return optional_scope;
    }
  }

  // The default manifest scope is the path to the manifest URL's containing
  // directory.
  const GURL manifest_scope_url = manifest_url.GetWithoutFilename();
  DCHECK(manifest_scope_url.is_valid());
  DCHECK(CheckValidManifestScope(manifest_url, manifest_scope_url.path()));
  return manifest_scope_url.path();
}

AppCache::AppCache(AppCacheStorage* storage, int64_t cache_id)
    : cache_id_(cache_id),
      owning_group_(nullptr),
      online_safelist_all_(false),
      is_complete_(false),
      cache_size_(0),
      padding_size_(0),
      manifest_parser_version_(-1),
      manifest_scope_(""),
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
  padding_size_ += entry.padding_size();
}

bool AppCache::AddOrModifyEntry(const GURL& url, const AppCacheEntry& entry) {
  std::pair<EntryMap::iterator, bool> ret =
      entries_.insert(EntryMap::value_type(url, entry));

  // Entry already exists.  Merge the types and token expiration of the new and
  // existing entries.
  if (!ret.second) {
    ret.first->second.add_types(entry.types());
  } else {
    cache_size_ += entry.response_size();  // New entry. Add to cache size.
    padding_size_ += entry.padding_size();
  }
  return ret.second;
}

void AppCache::RemoveEntry(const GURL& url) {
  auto found = entries_.find(url);
  DCHECK(found != entries_.end());
  DCHECK_GE(cache_size_, found->second.response_size());
  DCHECK_GE(padding_size_, found->second.padding_size());
  cache_size_ -= found->second.response_size();
  padding_size_ -= found->second.padding_size();
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
  manifest_parser_version_ = manifest->parser_version;
  manifest_scope_ = manifest->scope;
  intercept_namespaces_.swap(manifest->intercept_namespaces);
  fallback_namespaces_.swap(manifest->fallback_namespaces);
  online_safelist_namespaces_.swap(manifest->online_safelist_namespaces);
  online_safelist_all_ = manifest->online_safelist_all;
  token_expires_ = manifest->token_expires;

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
    const std::vector<AppCacheDatabase::OnlineSafeListRecord>& safelists) {
  DCHECK_EQ(cache_id_, cache_record.cache_id);
  manifest_parser_version_ = cache_record.manifest_parser_version;
  manifest_scope_ = cache_record.manifest_scope;
  online_safelist_all_ = cache_record.online_wildcard;
  update_time_ = cache_record.update_time;
  token_expires_ = cache_record.token_expires;

  for (const AppCacheDatabase::EntryRecord& entry : entries) {
    AddEntry(entry.url, AppCacheEntry(entry.flags, entry.response_id,
                                      entry.response_size, entry.padding_size));
  }
  DCHECK_EQ(cache_size_, cache_record.cache_size);
  DCHECK_EQ(padding_size_, cache_record.padding_size);

  for (const auto& intercept : intercepts)
    intercept_namespaces_.push_back(intercept.namespace_);

  for (const auto& fallback : fallbacks)
    fallback_namespaces_.push_back(fallback.namespace_);

  // Sort the fallback namespaces by url string length, longest to shortest,
  // since longer matches trump when matching a url to a namespace.
  std::sort(intercept_namespaces_.begin(), intercept_namespaces_.end(),
            SortNamespacesByLength);
  std::sort(fallback_namespaces_.begin(), fallback_namespaces_.end(),
            SortNamespacesByLength);

  for (const auto& record : safelists) {
    online_safelist_namespaces_.emplace_back(APPCACHE_NETWORK_NAMESPACE,
                                             record.namespace_url, GURL());
  }
}

void AppCache::ToDatabaseRecords(
    const AppCacheGroup* group,
    AppCacheDatabase::CacheRecord* cache_record,
    std::vector<AppCacheDatabase::EntryRecord>* entries,
    std::vector<AppCacheDatabase::NamespaceRecord>* intercepts,
    std::vector<AppCacheDatabase::NamespaceRecord>* fallbacks,
    std::vector<AppCacheDatabase::OnlineSafeListRecord>* safelists) {
  DCHECK(group && cache_record && entries && fallbacks && safelists);
  DCHECK(entries->empty() && fallbacks->empty() && safelists->empty());

  cache_record->cache_id = cache_id_;
  cache_record->group_id = group->group_id();
  cache_record->online_wildcard = online_safelist_all_;
  cache_record->update_time = update_time_;
  cache_record->cache_size = cache_size_;
  cache_record->padding_size = padding_size_;
  cache_record->manifest_parser_version = manifest_parser_version_;
  cache_record->manifest_scope = manifest_scope_;
  cache_record->token_expires = token_expires_;

  for (const auto& pair : entries_) {
    entries->push_back(AppCacheDatabase::EntryRecord());
    AppCacheDatabase::EntryRecord& record = entries->back();
    record.url = pair.first;
    record.cache_id = cache_id_;
    record.flags = pair.second.types();
    record.response_id = pair.second.response_id();
    record.response_size = pair.second.response_size();
    record.padding_size = pair.second.padding_size();
  }

  const url::Origin origin = url::Origin::Create(group->manifest_url());

  for (const AppCacheNamespace& intercept_namespace : intercept_namespaces_) {
    intercepts->push_back(AppCacheDatabase::NamespaceRecord());
    AppCacheDatabase::NamespaceRecord& record = intercepts->back();
    record.cache_id = cache_id_;
    record.origin = origin;
    record.namespace_ = intercept_namespace;
  }

  for (const AppCacheNamespace& fallback_namespace : fallback_namespaces_) {
    fallbacks->push_back(AppCacheDatabase::NamespaceRecord());
    AppCacheDatabase::NamespaceRecord& record = fallbacks->back();
    record.cache_id = cache_id_;
    record.origin = origin;
    record.namespace_ = fallback_namespace;
  }

  for (const AppCacheNamespace& online_namespace :
       online_safelist_namespaces_) {
    safelists->push_back(AppCacheDatabase::OnlineSafeListRecord());
    AppCacheDatabase::OnlineSafeListRecord& record = safelists->back();
    record.cache_id = cache_id_;
    record.namespace_url = online_namespace.namespace_url;
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

  *found_network_namespace = online_safelist_all_;
  return *found_network_namespace;
}

void AppCache::ToResourceInfoVector(
    std::vector<blink::mojom::AppCacheResourceInfo>* infos) const {
  DCHECK(infos && infos->empty());
  for (const auto& pair : entries_) {
    infos->push_back(blink::mojom::AppCacheResourceInfo());
    blink::mojom::AppCacheResourceInfo& info = infos->back();
    info.url = pair.first;
    info.is_master = pair.second.IsMaster();
    info.is_manifest = pair.second.IsManifest();
    info.is_intercept = pair.second.IsIntercept();
    info.is_fallback = pair.second.IsFallback();
    info.is_foreign = pair.second.IsForeign();
    info.is_explicit = pair.second.IsExplicit();
    info.response_size = pair.second.response_size();
    info.padding_size = pair.second.padding_size();
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
