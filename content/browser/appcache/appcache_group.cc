// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_group.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/browser/appcache/appcache_update_job.h"

namespace content {

class AppCacheGroup;

// Use this helper class because we cannot make AppCacheGroup a derived class
// of AppCacheHost::Observer as it would create a circular dependency between
// AppCacheHost and AppCacheGroup.
class AppCacheGroup::HostObserver : public AppCacheHost::Observer {
 public:
  explicit HostObserver(AppCacheGroup* group) : group_(group) {}

  // Methods for AppCacheHost::Observer.
  void OnCacheSelectionComplete(AppCacheHost* host) override {}  // N/A
  void OnDestructionImminent(AppCacheHost* host) override {
    group_->HostDestructionImminent(host);
  }
 private:
  AppCacheGroup* group_;
};

AppCacheGroup::AppCacheGroup(AppCacheStorage* storage,
                             const GURL& manifest_url,
                             int64_t group_id)
    : group_id_(group_id),
      manifest_url_(manifest_url),
      update_status_(IDLE),
      is_obsolete_(false),
      is_being_deleted_(false),
      newest_complete_cache_(nullptr),
      update_job_(nullptr),
      storage_(storage),
      is_in_dtor_(false) {
  storage_->working_set()->AddGroup(this);
  host_observer_ = std::make_unique<HostObserver>(this);
}

AppCacheGroup::~AppCacheGroup() {
  DCHECK(old_caches_.empty());
  DCHECK(!newest_complete_cache_);
  DCHECK(restart_update_task_.IsCancelled());
  DCHECK(queued_updates_.empty());

  is_in_dtor_ = true;

  if (update_job_)
    delete update_job_;
  DCHECK_EQ(IDLE, update_status_);

  storage_->working_set()->RemoveGroup(this);
  storage_->DeleteResponses(manifest_url_, newly_deletable_response_ids_);
}

void AppCacheGroup::AddUpdateObserver(UpdateObserver* observer) {
  // If observer being added is a host that has been queued for later update,
  // add observer to a different observer list.
  if (queued_updates_.find(observer) != queued_updates_.end())
    queued_observers_.AddObserver(observer);
  else
    observers_.AddObserver(observer);
}

void AppCacheGroup::RemoveUpdateObserver(UpdateObserver* observer) {
  observers_.RemoveObserver(observer);
  queued_observers_.RemoveObserver(observer);
}

void AppCacheGroup::AddCache(AppCache* complete_cache) {
  DCHECK(complete_cache->is_complete());
  complete_cache->set_owning_group(this);

  if (!newest_complete_cache_) {
    newest_complete_cache_ = complete_cache;
    return;
  }

  if (complete_cache->IsNewerThan(newest_complete_cache_)) {
    old_caches_.push_back(newest_complete_cache_);
    newest_complete_cache_ = complete_cache;

    // Update hosts of older caches to add a reference to the newest cache.
    // (This loop mutates |old_caches_| so a range-based for-loop cannot be
    // used, because it caches the end iterator.)
    for (auto it = old_caches_.begin(); it != old_caches_.end(); ++it) {
      AppCache* cache = *it;
      for (AppCacheHost* host : cache->associated_hosts())
        host->SetSwappableCache(this);
    }
  } else {
    old_caches_.push_back(complete_cache);
  }
}

void AppCacheGroup::RemoveCache(AppCache* cache) {
  DCHECK(cache->associated_hosts().empty());
  if (cache == newest_complete_cache_) {
    AppCache* tmp_cache = newest_complete_cache_;
    newest_complete_cache_ = nullptr;
    CancelUpdate();
    tmp_cache->set_owning_group(nullptr);  // may cause this group to be deleted
  } else {
    scoped_refptr<AppCacheGroup> protect(this);

    auto it = std::find(old_caches_.begin(), old_caches_.end(), cache);
    if (it != old_caches_.end()) {
      AppCache* tmp_cache = *it;
      old_caches_.erase(it);
      tmp_cache->set_owning_group(nullptr);  // may cause group to be released
    }

    if (!is_obsolete() && old_caches_.empty() &&
        !newly_deletable_response_ids_.empty()) {
      storage_->DeleteResponses(manifest_url_, newly_deletable_response_ids_);
      newly_deletable_response_ids_.clear();
    }
  }
}

void AppCacheGroup::AddNewlyDeletableResponseIds(
    std::vector<int64_t>* response_ids) {
  if (is_being_deleted() || (!is_obsolete() && old_caches_.empty())) {
    storage_->DeleteResponses(manifest_url_, *response_ids);
    response_ids->clear();
    return;
  }

  if (newly_deletable_response_ids_.empty()) {
    newly_deletable_response_ids_.swap(*response_ids);
    return;
  }
  newly_deletable_response_ids_.insert(
      newly_deletable_response_ids_.end(),
      response_ids->begin(), response_ids->end());
  response_ids->clear();
}

void AppCacheGroup::StartUpdateWithNewMasterEntry(
    AppCacheHost* host, const GURL& new_master_resource) {
  DCHECK(!is_obsolete() && !is_being_deleted());
  if (is_in_dtor_)
    return;

  if (!update_job_)
    update_job_ = new AppCacheUpdateJob(storage_->service(), this);

  update_job_->StartUpdate(host, new_master_resource);

  // Run queued update immediately as an update has been started manually.
  if (!restart_update_task_.IsCancelled()) {
    restart_update_task_.Cancel();
    RunQueuedUpdates();
  }
}

void AppCacheGroup::CancelUpdate() {
  if (update_job_) {
    delete update_job_;
    // The update_job_ destructor calls AppCacheGroup::SetUpdateAppCacheStatus
    // which zeroes update_job_.
    DCHECK(!update_job_);
    DCHECK_EQ(IDLE, update_status_);
  }
}

void AppCacheGroup::QueueUpdate(AppCacheHost* host,
                                const GURL& new_master_resource) {
  DCHECK(update_job_ && host && !new_master_resource.is_empty());
  queued_updates_.insert(QueuedUpdates::value_type(
      host, std::make_pair(host, new_master_resource)));

  // Need to know when host is destroyed.
  host->AddObserver(host_observer_.get());

  // If host is already observing for updates, move host to queued observers
  // list so that host is not notified when the current update completes.
  if (FindObserver(host, observers_)) {
    observers_.RemoveObserver(host);
    queued_observers_.AddObserver(host);
  }
}

void AppCacheGroup::RunQueuedUpdates() {
  if (!restart_update_task_.IsCancelled())
    restart_update_task_.Cancel();

  if (queued_updates_.empty())
    return;

  QueuedUpdates updates_to_run;
  queued_updates_.swap(updates_to_run);
  DCHECK(queued_updates_.empty());

  for (auto& pair : updates_to_run) {
    AppCacheHost* host = pair.second.first;
    host->RemoveObserver(host_observer_.get());
    if (FindObserver(host, queued_observers_)) {
      queued_observers_.RemoveObserver(host);
      observers_.AddObserver(host);
    }

    if (!is_obsolete() && !is_being_deleted())
      StartUpdateWithNewMasterEntry(host, pair.second.second);
  }
}

// static
bool AppCacheGroup::FindObserver(
    const UpdateObserver* find_me,
    const base::ObserverList<UpdateObserver>::Unchecked& observer_list) {
  return observer_list.HasObserver(find_me);
}

void AppCacheGroup::ScheduleUpdateRestart(int delay_ms) {
  DCHECK(restart_update_task_.IsCancelled());
  restart_update_task_.Reset(
      base::BindOnce(&AppCacheGroup::RunQueuedUpdates, this));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, restart_update_task_.callback(),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

void AppCacheGroup::HostDestructionImminent(AppCacheHost* host) {
  queued_updates_.erase(host);
  if (queued_updates_.empty() && !restart_update_task_.IsCancelled())
    restart_update_task_.Cancel();
}

void AppCacheGroup::SetUpdateAppCacheStatus(UpdateAppCacheStatus status) {
  if (status == update_status_)
    return;

  update_status_ = status;

  if (status != IDLE) {
    DCHECK(update_job_);
  } else {
    update_job_ = nullptr;

    // Observers may release us in these callbacks, so we protect against
    // deletion by adding an extra ref in this scope (but only if we're not
    // in our destructor).
    scoped_refptr<AppCacheGroup> protect(is_in_dtor_ ? nullptr : this);
    for (auto& observer : observers_)
      observer.OnUpdateComplete(this);
    if (!queued_updates_.empty())
      ScheduleUpdateRestart(kUpdateRestartDelayMs);
  }
}

}  // namespace content
