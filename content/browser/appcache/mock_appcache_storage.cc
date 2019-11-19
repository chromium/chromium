// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/mock_appcache_storage.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

// This is a quick and easy 'mock' implementation of the storage interface
// that doesn't put anything to disk.
//
// We simply add an extra reference to objects when they're put in storage,
// and remove the extra reference when they are removed from storage.
// Responses are never really removed from the in-memory disk cache.
// Delegate callbacks are made asyncly to appropiately mimic what will
// happen with a real disk-backed storage impl that involves IO on a
// background thread.

namespace content {

MockAppCacheStorage::MockAppCacheStorage(AppCacheServiceImpl* service)
    : AppCacheStorage(service),
      simulate_make_group_obsolete_failure_(false),
      simulate_store_group_and_newest_cache_failure_(false),
      simulate_find_main_resource_(false),
      simulate_find_sub_resource_(false),
      simulated_found_cache_id_(blink::mojom::kAppCacheNoCacheId),
      simulated_found_group_id_(0),
      simulated_found_network_namespace_(false) {
  last_cache_id_ = 0;
  last_group_id_ = 0;
  last_response_id_ = 0;
}

MockAppCacheStorage::~MockAppCacheStorage() = default;

void MockAppCacheStorage::GetAllInfo(Delegate* delegate) {
  ScheduleTask(base::BindOnce(
      &MockAppCacheStorage::ProcessGetAllInfo, weak_factory_.GetWeakPtr(),
      base::WrapRefCounted(GetOrCreateDelegateReference(delegate))));
}

void MockAppCacheStorage::LoadCache(int64_t id, Delegate* delegate) {
  DCHECK(delegate);
  AppCache* cache = working_set_.GetCache(id);
  if (ShouldCacheLoadAppearAsync(cache)) {
    ScheduleTask(base::BindOnce(
        &MockAppCacheStorage::ProcessLoadCache, weak_factory_.GetWeakPtr(), id,
        base::WrapRefCounted(GetOrCreateDelegateReference(delegate))));
    return;
  }
  ProcessLoadCache(id, GetOrCreateDelegateReference(delegate));
}

void MockAppCacheStorage::LoadOrCreateGroup(
    const GURL& manifest_url, Delegate* delegate) {
  DCHECK(delegate);
  AppCacheGroup* group = working_set_.GetGroup(manifest_url);
  if (ShouldGroupLoadAppearAsync(group)) {
    ScheduleTask(base::BindOnce(
        &MockAppCacheStorage::ProcessLoadOrCreateGroup,
        weak_factory_.GetWeakPtr(), manifest_url,
        base::WrapRefCounted(GetOrCreateDelegateReference(delegate))));
    return;
  }
  ProcessLoadOrCreateGroup(
      manifest_url, GetOrCreateDelegateReference(delegate));
}

void MockAppCacheStorage::StoreGroupAndNewestCache(
    AppCacheGroup* group, AppCache* newest_cache, Delegate* delegate) {
  DCHECK(group && delegate && newest_cache);

  // Always make this operation look async.
  ScheduleTask(base::BindOnce(
      &MockAppCacheStorage::ProcessStoreGroupAndNewestCache,
      weak_factory_.GetWeakPtr(), base::WrapRefCounted(group),
      base::WrapRefCounted(newest_cache),
      base::WrapRefCounted(GetOrCreateDelegateReference(delegate))));
}

void MockAppCacheStorage::FindResponseForMainRequest(
    const GURL& url, const GURL& preferred_manifest_url, Delegate* delegate) {
  DCHECK(delegate);

  // Note: MockAppCacheStorage does not respect the preferred_manifest_url.

  // Always make this operation look async.
  ScheduleTask(base::BindOnce(
      &MockAppCacheStorage::ProcessFindResponseForMainRequest,
      weak_factory_.GetWeakPtr(), url,
      base::WrapRefCounted(GetOrCreateDelegateReference(delegate))));
}

void MockAppCacheStorage::FindResponseForSubRequest(
    AppCache* cache, const GURL& url,
    AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
    bool* found_network_namespace) {
  DCHECK(cache && cache->is_complete());

  // This layer of indirection is here to facilitate testing.
  if (simulate_find_sub_resource_) {
    *found_entry = simulated_found_entry_;
    *found_fallback_entry = simulated_found_fallback_entry_;
    *found_network_namespace = simulated_found_network_namespace_;
    simulate_find_sub_resource_ = false;
    return;
  }

  GURL fallback_namespace_not_used;
  GURL intercept_namespace_not_used;
  cache->FindResponseForRequest(
      url, found_entry, &intercept_namespace_not_used,
      found_fallback_entry,  &fallback_namespace_not_used,
      found_network_namespace);
}

void MockAppCacheStorage::MarkEntryAsForeign(const GURL& entry_url,
                                             int64_t cache_id) {
  AppCache* cache = working_set_.GetCache(cache_id);
  if (cache) {
    AppCacheEntry* entry = cache->GetEntry(entry_url);
    DCHECK(entry);
    if (entry)
      entry->add_types(AppCacheEntry::FOREIGN);
  }
}

void MockAppCacheStorage::MakeGroupObsolete(AppCacheGroup* group,
                                            Delegate* delegate,
                                            int response_code) {
  DCHECK(group && delegate);

  // Always make this method look async.
  ScheduleTask(base::BindOnce(
      &MockAppCacheStorage::ProcessMakeGroupObsolete,
      weak_factory_.GetWeakPtr(), base::WrapRefCounted(group),
      base::WrapRefCounted(GetOrCreateDelegateReference(delegate)),
      response_code));
}

void MockAppCacheStorage::StoreEvictionTimes(AppCacheGroup* group) {
  stored_eviction_times_[group->group_id()] =
      std::make_pair(group->last_full_update_check_time(),
                     group->first_evictable_error_time());
}

std::unique_ptr<AppCacheResponseReader>
MockAppCacheStorage::CreateResponseReader(const GURL& manifest_url,
                                          int64_t response_id) {
  if (simulated_reader_)
    return std::move(simulated_reader_);
  return std::make_unique<AppCacheResponseReader>(response_id,
                                                  disk_cache()->GetWeakPtr());
}

std::unique_ptr<AppCacheResponseWriter>
MockAppCacheStorage::CreateResponseWriter(const GURL& manifest_url) {
  return std::make_unique<AppCacheResponseWriter>(NewResponseId(),
                                                  disk_cache()->GetWeakPtr());
}

std::unique_ptr<AppCacheResponseMetadataWriter>
MockAppCacheStorage::CreateResponseMetadataWriter(int64_t response_id) {
  return std::make_unique<AppCacheResponseMetadataWriter>(
      response_id, disk_cache()->GetWeakPtr());
}

void MockAppCacheStorage::DoomResponses(
    const GURL& manifest_url,
    const std::vector<int64_t>& response_ids) {
  DeleteResponses(manifest_url, response_ids);
}

void MockAppCacheStorage::DeleteResponses(
    const GURL& manifest_url,
    const std::vector<int64_t>& response_ids) {
  // We don't bother with actually removing responses from the disk-cache,
  // just keep track of which ids have been doomed or deleted
  for (const auto& id : response_ids)
    doomed_response_ids_.insert(id);
}

bool MockAppCacheStorage::IsInitialized() {
  return false;
}

void MockAppCacheStorage::ProcessGetAllInfo(
    scoped_refptr<DelegateReference> delegate_ref) {
  if (delegate_ref->delegate)
    delegate_ref->delegate->OnAllInfo(simulated_appcache_info_.get());
}

void MockAppCacheStorage::ProcessLoadCache(
    int64_t id,
    scoped_refptr<DelegateReference> delegate_ref) {
  AppCache* cache = working_set_.GetCache(id);
  if (delegate_ref->delegate)
    delegate_ref->delegate->OnCacheLoaded(cache, id);
}

void MockAppCacheStorage::ProcessLoadOrCreateGroup(
    const GURL& manifest_url, scoped_refptr<DelegateReference> delegate_ref) {
  scoped_refptr<AppCacheGroup> group(working_set_.GetGroup(manifest_url));

  // Newly created groups are not put in the stored_groups collection
  // until StoreGroupAndNewestCache is called.
  if (!group.get()) {
    group = base::MakeRefCounted<AppCacheGroup>(service_->storage(),
                                                manifest_url, NewGroupId());
  }

  if (delegate_ref->delegate)
    delegate_ref->delegate->OnGroupLoaded(group.get(), manifest_url);
}

void MockAppCacheStorage::ProcessStoreGroupAndNewestCache(
    scoped_refptr<AppCacheGroup> group,
    scoped_refptr<AppCache> newest_cache,
    scoped_refptr<DelegateReference> delegate_ref) {
  Delegate* delegate = delegate_ref->delegate;
  if (simulate_store_group_and_newest_cache_failure_) {
    if (delegate)
      delegate->OnGroupAndNewestCacheStored(
          group.get(), newest_cache.get(), false, false);
    return;
  }

  AddStoredGroup(group.get());
  if (newest_cache.get() != group->newest_complete_cache()) {
    newest_cache->set_complete(true);
    group->AddCache(newest_cache.get());
    AddStoredCache(newest_cache.get());

    // Copy the collection prior to removal, on final release
    // of a cache the group's collection will change.
    std::vector<AppCache*> copy = group->old_caches();
    RemoveStoredCaches(copy);
  }

  if (delegate)
    delegate->OnGroupAndNewestCacheStored(
        group.get(), newest_cache.get(), true, false);
}

namespace {

struct FoundCandidate {
  GURL namespace_entry_url;
  AppCacheEntry entry;
  int64_t cache_id;
  int64_t group_id;
  GURL manifest_url;
  bool is_cache_in_use;

  FoundCandidate()
      : cache_id(blink::mojom::kAppCacheNoCacheId),
        group_id(0),
        is_cache_in_use(false) {}
};

void MaybeTakeNewNamespaceEntry(
    AppCacheNamespaceType namespace_type,
    const AppCacheEntry &entry,
    const GURL& namespace_url,
    bool cache_is_in_use,
    FoundCandidate* best_candidate,
    GURL* best_candidate_namespace,
    AppCache* cache,
    AppCacheGroup* group) {
  DCHECK(entry.has_response_id());

  bool take_new_entry = true;

  // Does the new candidate entry trump our current best candidate?
  if (best_candidate->entry.has_response_id()) {
    // Longer namespace prefix matches win.
    size_t candidate_length =
        namespace_url.spec().length();
    size_t best_length =
        best_candidate_namespace->spec().length();

    if (candidate_length > best_length) {
      take_new_entry = true;
    } else if (candidate_length == best_length &&
               cache_is_in_use && !best_candidate->is_cache_in_use) {
      take_new_entry = true;
    } else {
      take_new_entry = false;
    }
  }

  if (take_new_entry) {
    if (namespace_type == APPCACHE_FALLBACK_NAMESPACE) {
      best_candidate->namespace_entry_url =
          cache->GetFallbackEntryUrl(namespace_url);
    } else {
      best_candidate->namespace_entry_url =
          cache->GetInterceptEntryUrl(namespace_url);
    }
    best_candidate->entry = entry;
    best_candidate->cache_id = cache->cache_id();
    best_candidate->group_id = group->group_id();
    best_candidate->manifest_url = group->manifest_url();
    best_candidate->is_cache_in_use = cache_is_in_use;
    *best_candidate_namespace = namespace_url;
  }
}
}  // namespace

void MockAppCacheStorage::ProcessFindResponseForMainRequest(
    const GURL& url, scoped_refptr<DelegateReference> delegate_ref) {
  if (simulate_find_main_resource_) {
    simulate_find_main_resource_ = false;
    if (delegate_ref->delegate) {
      delegate_ref->delegate->OnMainResponseFound(
          url, simulated_found_entry_,
          simulated_found_fallback_url_, simulated_found_fallback_entry_,
          simulated_found_cache_id_, simulated_found_group_id_,
          simulated_found_manifest_url_);
    }
    return;
  }

  // This call has no persistent side effects, if the delegate has gone
  // away, we can just bail out early.
  if (!delegate_ref->delegate)
    return;

  // TODO(michaeln): The heuristics around choosing amoungst
  // multiple candidates is under specified, and just plain
  // not fully understood. Refine these over time. In particular,
  // * prefer candidates from newer caches
  // * take into account the cache associated with the document
  //   that initiated the navigation
  // * take into account the cache associated with the document
  //   currently residing in the frame being navigated
  FoundCandidate found_candidate;
  GURL found_intercept_candidate_namespace;
  FoundCandidate found_fallback_candidate;
  GURL found_fallback_candidate_namespace;

  for (const auto& pair : stored_groups_) {
    AppCacheGroup* group = pair.second.get();
    AppCache* cache = group->newest_complete_cache();
    if (group->is_obsolete() || !cache ||
        (url.GetOrigin() != group->manifest_url().GetOrigin())) {
      continue;
    }

    AppCacheEntry found_entry;
    AppCacheEntry found_fallback_entry;
    GURL found_intercept_namespace;
    GURL found_fallback_namespace;
    bool ignore_found_network_namespace = false;
    bool found = cache->FindResponseForRequest(
                            url, &found_entry, &found_intercept_namespace,
                            &found_fallback_entry, &found_fallback_namespace,
                            &ignore_found_network_namespace);

    // 6.11.1 Navigating across documents, Step 10.
    // Network namespacing doesn't apply to main resource loads,
    // and foreign entries are excluded.
    if (!found || ignore_found_network_namespace ||
        (found_entry.has_response_id() && found_entry.IsForeign()) ||
        (found_fallback_entry.has_response_id() &&
         found_fallback_entry.IsForeign())) {
      continue;
    }

    // We have a bias for hits from caches that are in use.
    bool is_in_use = IsCacheStored(cache) && !cache->HasOneRef();

    if (found_entry.has_response_id() &&
        found_intercept_namespace.is_empty()) {
      found_candidate.namespace_entry_url = GURL();
      found_candidate.entry = found_entry;
      found_candidate.cache_id = cache->cache_id();
      found_candidate.group_id = group->group_id();
      found_candidate.manifest_url = group->manifest_url();
      found_candidate.is_cache_in_use = is_in_use;
      if (is_in_use)
        break;  // We break out of the loop with this direct hit.
    } else if (found_entry.has_response_id() &&
               !found_intercept_namespace.is_empty()) {
      MaybeTakeNewNamespaceEntry(
          APPCACHE_INTERCEPT_NAMESPACE,
          found_entry, found_intercept_namespace, is_in_use,
          &found_candidate, &found_intercept_candidate_namespace,
          cache, group);
    } else {
      DCHECK(found_fallback_entry.has_response_id());
      MaybeTakeNewNamespaceEntry(
          APPCACHE_FALLBACK_NAMESPACE,
          found_fallback_entry, found_fallback_namespace, is_in_use,
          &found_fallback_candidate, &found_fallback_candidate_namespace,
          cache, group);
    }
  }

  // Found a direct hit or an intercept namespace hit.
  if (found_candidate.entry.has_response_id()) {
    delegate_ref->delegate->OnMainResponseFound(
        url, found_candidate.entry, found_candidate.namespace_entry_url,
        AppCacheEntry(),  found_candidate.cache_id, found_candidate.group_id,
        found_candidate.manifest_url);
    return;
  }

  // Found a fallback namespace.
  if (found_fallback_candidate.entry.has_response_id()) {
    delegate_ref->delegate->OnMainResponseFound(
        url, AppCacheEntry(),
        found_fallback_candidate.namespace_entry_url,
        found_fallback_candidate.entry,
        found_fallback_candidate.cache_id,
        found_fallback_candidate.group_id,
        found_fallback_candidate.manifest_url);
    return;
  }

  // Didn't find anything.
  delegate_ref->delegate->OnMainResponseFound(
      url, AppCacheEntry(), GURL(), AppCacheEntry(),
      blink::mojom::kAppCacheNoCacheId, 0, GURL());
}

void MockAppCacheStorage::ProcessMakeGroupObsolete(
    scoped_refptr<AppCacheGroup> group,
    scoped_refptr<DelegateReference> delegate_ref,
    int response_code) {
  if (simulate_make_group_obsolete_failure_) {
    if (delegate_ref->delegate)
      delegate_ref->delegate->OnGroupMadeObsolete(
          group.get(), false, response_code);
    return;
  }

  RemoveStoredGroup(group.get());
  if (group->newest_complete_cache())
    RemoveStoredCache(group->newest_complete_cache());

  // Copy the collection prior to removal, on final release
  // of a cache the group's collection will change.
  std::vector<AppCache*> copy = group->old_caches();
  RemoveStoredCaches(copy);

  group->set_obsolete(true);

  // Also remove from the working set, caches for an 'obsolete' group
  // may linger in use, but the group itself cannot be looked up by
  // 'manifest_url' in the working set any longer.
  working_set()->RemoveGroup(group.get());

  if (delegate_ref->delegate)
    delegate_ref->delegate->OnGroupMadeObsolete(
        group.get(), true, response_code);
}

void MockAppCacheStorage::ScheduleTask(base::OnceClosure task) {
  pending_tasks_.push_back(std::move(task));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MockAppCacheStorage::RunOnePendingTask,
                                weak_factory_.GetWeakPtr()));
}

void MockAppCacheStorage::RunOnePendingTask() {
  DCHECK(!pending_tasks_.empty());
  base::OnceClosure task = std::move(pending_tasks_.front());
  pending_tasks_.pop_front();
  std::move(task).Run();
}

void MockAppCacheStorage::AddStoredCache(AppCache* cache) {
  int64_t cache_id = cache->cache_id();
  if (stored_caches_.find(cache_id) == stored_caches_.end()) {
    stored_caches_.insert(
        StoredCacheMap::value_type(cache_id, base::WrapRefCounted(cache)));
  }
}

void MockAppCacheStorage::RemoveStoredCache(AppCache* cache) {
  // Do not remove from the working set, active caches are still usable
  // and may be looked up by id until they fall out of use.
  stored_caches_.erase(cache->cache_id());
}

void MockAppCacheStorage::RemoveStoredCaches(
    const std::vector<AppCache*>& caches) {
  for (AppCache* cache : caches)
    RemoveStoredCache(cache);
}

void MockAppCacheStorage::AddStoredGroup(AppCacheGroup* group) {
  const GURL& url = group->manifest_url();
  if (stored_groups_.find(url) == stored_groups_.end()) {
    stored_groups_.insert(
        StoredGroupMap::value_type(url, base::WrapRefCounted(group)));
  }
}

void MockAppCacheStorage::RemoveStoredGroup(AppCacheGroup* group) {
  stored_groups_.erase(group->manifest_url());
}

bool MockAppCacheStorage::ShouldGroupLoadAppearAsync(
    const AppCacheGroup* group) {
  // We'll have to query the database to see if a group for the
  // manifest_url exists on disk. So return true for async.
  if (!group)
    return true;

  // Groups without a newest cache can't have been put to disk yet, so
  // we can synchronously return a reference we have in the working set.
  if (!group->newest_complete_cache())
    return false;

  // The LoadGroup interface implies also loading the newest cache, so
  // if loading the newest cache should appear async, so too must the
  // loading of this group.
  if (!ShouldCacheLoadAppearAsync(group->newest_complete_cache()))
    return false;


  // If any of the old caches are "in use", then the group must also
  // be memory resident and not require async loading.
  for (const AppCache* cache : group->old_caches()) {
    // "in use" caches don't require async loading
    if (!ShouldCacheLoadAppearAsync(cache))
      return false;
  }

  return true;
}

bool MockAppCacheStorage::ShouldCacheLoadAppearAsync(const AppCache* cache) {
  if (!cache)
    return true;

  // If the 'stored' ref is the only ref, real storage will have to load from
  // the database.
  return IsCacheStored(cache) && cache->HasOneRef();
}

}  // namespace content
