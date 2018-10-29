// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_impl.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

// Limits on the cache size and number of areas in memory, over which the areas
// are purged.
#if defined(OS_ANDROID)
const unsigned kMaxDomStorageAreaCount = 20;
const size_t kMaxDomStorageCacheSize = 2 * 1024 * 1024;
#else
const unsigned kMaxDomStorageAreaCount = 100;
const size_t kMaxDomStorageCacheSize = 20 * 1024 * 1024;
#endif

const int kSessionStoraceScavengingSeconds = 60;

// Aggregates statistics from all the namespaces.
DOMStorageNamespace::UsageStatistics GetTotalNamespaceStatistics(
    const DOMStorageContextImpl::StorageNamespaceMap& namespace_map) {
  DOMStorageNamespace::UsageStatistics total_stats = {0};
  for (const auto& it : namespace_map) {
    DOMStorageNamespace::UsageStatistics stats =
        it.second->GetUsageStatistics();
    total_stats.total_cache_size += stats.total_cache_size;
    total_stats.total_area_count += stats.total_area_count;
    total_stats.inactive_area_count += stats.inactive_area_count;
  }
  return total_stats;
}

}  // namespace

DOMStorageContextImpl::DOMStorageContextImpl(
    const base::FilePath& sessionstorage_directory,
    storage::SpecialStoragePolicy* special_storage_policy,
    scoped_refptr<DOMStorageTaskRunner> task_runner)
    : sessionstorage_directory_(sessionstorage_directory),
      task_runner_(std::move(task_runner)),
      is_shutdown_(false),
      force_keep_session_state_(false),
      special_storage_policy_(special_storage_policy),
      scavenging_started_(false),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()) {
  // Tests may run without task runners.
  if (task_runner_) {
    // Registering dump provider is safe even outside the task runner.
    base::trace_event::MemoryDumpManager::GetInstance()
        ->RegisterDumpProviderWithSequencedTaskRunner(
            this, "DOMStorage", task_runner_->GetSequencedTaskRunner(
                                    DOMStorageTaskRunner::PRIMARY_SEQUENCE),
            base::trace_event::MemoryDumpProvider::Options());
  }
}

DOMStorageContextImpl::~DOMStorageContextImpl() {
  DCHECK(is_shutdown_);
  if (session_storage_database_.get()) {
    // SessionStorageDatabase shouldn't be deleted right away: deleting it will
    // potentially involve waiting in leveldb::DBImpl::~DBImpl, and waiting
    // shouldn't happen on this thread.
    SessionStorageDatabase* to_release = session_storage_database_.get();
    to_release->AddRef();
    session_storage_database_ = nullptr;
    task_runner_->PostShutdownBlockingTask(
        FROM_HERE, DOMStorageTaskRunner::COMMIT_SEQUENCE,
        base::BindOnce(&SessionStorageDatabase::Release,
                       base::Unretained(to_release)));
  }
}

DOMStorageNamespace* DOMStorageContextImpl::GetStorageNamespace(
    const std::string& namespace_id) {
  if (is_shutdown_)
    return nullptr;
  auto found = namespaces_.find(namespace_id);
  if (found == namespaces_.end())
    return nullptr;
  return found->second.get();
}

void DOMStorageContextImpl::GetSessionStorageUsage(
    std::vector<SessionStorageUsageInfo>* infos) {
  if (!session_storage_database_.get()) {
    for (const auto& entry : namespaces_) {
      std::vector<url::Origin> origins;
      entry.second->GetOriginsWithAreas(&origins);
      for (const url::Origin& origin : origins) {
        SessionStorageUsageInfo info;
        info.namespace_id = entry.second->namespace_id();
        info.origin = origin.GetURL();
        infos->push_back(info);
      }
    }
    return;
  }

  std::map<std::string, std::vector<url::Origin>> namespaces_and_origins;
  session_storage_database_->ReadNamespacesAndOrigins(
      &namespaces_and_origins);
  for (auto it = namespaces_and_origins.cbegin();
       it != namespaces_and_origins.cend(); ++it) {
    for (auto origin_it = it->second.cbegin(); origin_it != it->second.cend();
         ++origin_it) {
      SessionStorageUsageInfo info;
      info.namespace_id = it->first;
      info.origin = origin_it->GetURL();
      infos->push_back(info);
    }
  }
}

void DOMStorageContextImpl::DeleteSessionStorage(
    const SessionStorageUsageInfo& usage_info) {
  DCHECK(!is_shutdown_);
  DOMStorageNamespace* dom_storage_namespace = nullptr;

  auto it = namespaces_.find(usage_info.namespace_id);
  if (it != namespaces_.end()) {
    dom_storage_namespace = it->second.get();
  } else {
    CreateSessionNamespace(usage_info.namespace_id);
    dom_storage_namespace = GetStorageNamespace(usage_info.namespace_id);
  }
  dom_storage_namespace->DeleteSessionStorageOrigin(
      url::Origin::Create(usage_info.origin));
  // Synthesize a 'cleared' event if the area is open so CachedAreas in
  // renderers get emptied out too.
  DOMStorageArea* area = dom_storage_namespace->GetOpenStorageArea(
      url::Origin::Create(usage_info.origin));
  if (area)
    NotifyAreaCleared(area, usage_info.origin);
}

void DOMStorageContextImpl::Flush() {
  for (auto& entry : namespaces_)
    entry.second->Flush();
}

void DOMStorageContextImpl::Shutdown() {
  if (task_runner_)
    task_runner_->AssertIsRunningOnPrimarySequence();
  is_shutdown_ = true;
  StorageNamespaceMap::const_iterator it = namespaces_.begin();
  for (; it != namespaces_.end(); ++it)
    it->second->Shutdown();

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  if (!session_storage_database_.get())
    return;

  // Respect the content policy settings about what to
  // keep and what to discard.
  if (force_keep_session_state_)
    return;  // Keep everything.

  bool has_session_only_origins =
      special_storage_policy_.get() &&
      special_storage_policy_->HasSessionOnlyOrigins();

  if (has_session_only_origins) {
    // We may have to delete something. We continue on the
    // commit sequence after area shutdown tasks have cycled
    // thru that sequence (and closed their database files).
    bool success = task_runner_->PostShutdownBlockingTask(
        FROM_HERE, DOMStorageTaskRunner::COMMIT_SEQUENCE,
        base::BindOnce(&DOMStorageContextImpl::ClearSessionOnlyOrigins, this));
    DCHECK(success);
  }
}

void DOMStorageContextImpl::AddEventObserver(EventObserver* observer) {
  event_observers_.AddObserver(observer);
}

void DOMStorageContextImpl::RemoveEventObserver(EventObserver* observer) {
  event_observers_.RemoveObserver(observer);
}

void DOMStorageContextImpl::NotifyItemSet(
    const DOMStorageArea* area,
    const base::string16& key,
    const base::string16& new_value,
    const base::NullableString16& old_value,
    const GURL& page_url) {
  for (auto& observer : event_observers_)
    observer.OnDOMStorageItemSet(area, key, new_value, old_value, page_url);
}

void DOMStorageContextImpl::NotifyItemRemoved(
    const DOMStorageArea* area,
    const base::string16& key,
    const base::string16& old_value,
    const GURL& page_url) {
  for (auto& observer : event_observers_)
    observer.OnDOMStorageItemRemoved(area, key, old_value, page_url);
}

void DOMStorageContextImpl::NotifyAreaCleared(
    const DOMStorageArea* area,
    const GURL& page_url) {
  for (auto& observer : event_observers_)
    observer.OnDOMStorageAreaCleared(area, page_url);
}

// Used to diagnose unknown namespace_ids given to the ipc message filter.
base::Optional<bad_message::BadMessageReason>
DOMStorageContextImpl::DiagnoseSessionNamespaceId(
    const std::string& namespace_id) {
  if (base::ContainsValue(recently_deleted_session_ids_, namespace_id))
    return bad_message::DSH_DELETED_SESSION_ID;
  return bad_message::DSH_NOT_ALLOCATED_SESSION_ID;
}

void DOMStorageContextImpl::CreateSessionNamespace(
    const std::string& namespace_id) {
  if (is_shutdown_)
    return;
  DCHECK(!namespace_id.empty());
  // There are many browser tests that 'quit and restore' the browser but not
  // the profile (and instead just reuse the old profile). This is unfortunate
  // and doesn't actually test the 'restore' feature of session storage.
  if (namespaces_.find(namespace_id) != namespaces_.end())
    return;
  namespaces_[namespace_id] = new DOMStorageNamespace(
      namespace_id, session_storage_database_.get(), task_runner_.get());
}

void DOMStorageContextImpl::DeleteSessionNamespace(
    const std::string& namespace_id,
    bool should_persist_data) {
  DCHECK(!namespace_id.empty());
  StorageNamespaceMap::const_iterator it = namespaces_.find(namespace_id);
  if (it == namespaces_.end())
    return;
  if (session_storage_database_.get()) {
    if (!should_persist_data) {
      task_runner_->PostShutdownBlockingTask(
          FROM_HERE, DOMStorageTaskRunner::COMMIT_SEQUENCE,
          base::BindOnce(
              base::IgnoreResult(&SessionStorageDatabase::DeleteNamespace),
              session_storage_database_, namespace_id));
    } else {
      // Ensure that the data gets committed before we shut down.
      it->second->Shutdown();
      if (!scavenging_started_) {
        // Protect the persistent namespace ID from scavenging.
        protected_session_ids_.insert(namespace_id);
      }
    }
  }
  namespaces_.erase(namespace_id);

  recently_deleted_session_ids_.push_back(namespace_id);
  if (recently_deleted_session_ids_.size() > 10)
    recently_deleted_session_ids_.pop_front();
}

void DOMStorageContextImpl::CloneSessionNamespace(
    const std::string& existing_id,
    const std::string& new_id) {
  if (is_shutdown_)
    return;
  DCHECK(!existing_id.empty());
  DCHECK(!new_id.empty());
  auto found = namespaces_.find(existing_id);
  if (found != namespaces_.end()) {
    namespaces_[new_id] = found->second->Clone(new_id);
    return;
  }
  CreateSessionNamespace(new_id);
}

void DOMStorageContextImpl::ClearSessionOnlyOrigins() {
  if (session_storage_database_.get()) {
    std::vector<SessionStorageUsageInfo> infos;
    GetSessionStorageUsage(&infos);
    for (size_t i = 0; i < infos.size(); ++i) {
      const url::Origin& origin = url::Origin::Create(infos[i].origin);
      if (special_storage_policy_->IsStorageProtected(origin.GetURL()))
        continue;
      if (!special_storage_policy_->IsStorageSessionOnly(origin.GetURL()))
        continue;
      session_storage_database_->DeleteArea(infos[i].namespace_id, origin);
    }
  }
}

void DOMStorageContextImpl::SetSaveSessionStorageOnDisk() {
  DCHECK(namespaces_.empty());
  if (!sessionstorage_directory_.empty()) {
    session_storage_database_ = new SessionStorageDatabase(
        sessionstorage_directory_, task_runner_->GetSequencedTaskRunner(
                                       DOMStorageTaskRunner::COMMIT_SEQUENCE));
  }
}

void DOMStorageContextImpl::StartScavengingUnusedSessionStorage() {
  if (session_storage_database_.get()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DOMStorageContextImpl::FindUnusedNamespaces, this),
        base::TimeDelta::FromSeconds(kSessionStoraceScavengingSeconds));
  }
}

void DOMStorageContextImpl::PurgeMemory(PurgeOption purge_option) {
  if (is_shutdown_)
    return;

  DOMStorageNamespace::UsageStatistics initial_stats =
      GetTotalNamespaceStatistics(namespaces_);
  if (!initial_stats.total_area_count)
    return;

  // Track the total localStorage cache size.
  UMA_HISTOGRAM_CUSTOM_COUNTS("LocalStorage.BrowserLocalStorageCacheSizeInKB",
                              initial_stats.total_cache_size / 1024, 1, 100000,
                              50);

  const char* purge_reason = nullptr;
  if (purge_option == PURGE_IF_NEEDED) {
    // Purging is done based on the cache sizes without including the database
    // size since it can be expensive trying to estimate the sqlite usage for
    // all databases. For low end devices purge all inactive areas.
    if (initial_stats.total_cache_size > kMaxDomStorageCacheSize)
      purge_reason = "SizeLimitExceeded";
    else if (initial_stats.total_area_count > kMaxDomStorageAreaCount)
      purge_reason = "AreaCountLimitExceeded";
    else if (is_low_end_device_)
      purge_reason = "InactiveOnLowEndDevice";
    if (!purge_reason)
      return;

    purge_option = PURGE_UNOPENED;
  } else {
    if (purge_option == PURGE_AGGRESSIVE)
      purge_reason = "AggressivePurgeTriggered";
    else
      purge_reason = "ModeratePurgeTriggered";
  }

  // Return if no areas can be purged with the given option.
  bool aggressively = purge_option == PURGE_AGGRESSIVE;
  if (!aggressively && !initial_stats.inactive_area_count) {
    return;
  }
  for (const auto& it : namespaces_)
    it.second->PurgeMemory(aggressively);

  // Track the size of cache purged.
  size_t purged_size_kib =
      (initial_stats.total_cache_size -
       GetTotalNamespaceStatistics(namespaces_).total_cache_size) /
      1024;
  std::string full_histogram_name =
      std::string("LocalStorage.BrowserLocalStorageCachePurgedInKB.") +
      purge_reason;
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      full_histogram_name, 1, 100000, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (histogram)
    histogram->Add(purged_size_kib);
  UMA_HISTOGRAM_CUSTOM_COUNTS("LocalStorage.BrowserLocalStorageCachePurgedInKB",
                              purged_size_kib, 1, 100000, 50);
}

bool DOMStorageContextImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (session_storage_database_)
    session_storage_database_->OnMemoryDump(pmd);
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    DOMStorageNamespace::UsageStatistics total_stats =
        GetTotalNamespaceStatistics(namespaces_);
    auto* mad = pmd->CreateAllocatorDump(base::StringPrintf(
        "site_storage/session_storage/0x%" PRIXPTR "/cache_size",
        reinterpret_cast<uintptr_t>(this)));
    mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                   base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                   total_stats.total_cache_size);
    mad->AddScalar("inactive_areas",
                   base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                   total_stats.inactive_area_count);
    mad->AddScalar("total_areas",
                   base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                   total_stats.total_area_count);
    return true;
  }
  for (const auto& it : namespaces_) {
    it.second->OnMemoryDump(pmd);
  }
  return true;
}

void DOMStorageContextImpl::FindUnusedNamespaces() {
  DCHECK(session_storage_database_.get());
  if (scavenging_started_)
    return;
  scavenging_started_ = true;
  std::set<std::string> namespace_ids_in_use;
  for (StorageNamespaceMap::const_iterator it = namespaces_.begin();
       it != namespaces_.end(); ++it)
    namespace_ids_in_use.insert(it->second->namespace_id());
  std::set<std::string> protected_session_ids;
  protected_session_ids.swap(protected_session_ids_);
  task_runner_->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::COMMIT_SEQUENCE,
      base::BindOnce(
          &DOMStorageContextImpl::FindUnusedNamespacesInCommitSequence, this,
          namespace_ids_in_use, protected_session_ids));
}

void DOMStorageContextImpl::FindUnusedNamespacesInCommitSequence(
    const std::set<std::string>& namespace_ids_in_use,
    const std::set<std::string>& protected_session_ids) {
  DCHECK(session_storage_database_.get());
  // Delete all namespaces which don't have an associated DOMStorageNamespace
  // alive.
  std::map<std::string, std::vector<url::Origin>> namespaces_and_origins;
  session_storage_database_->ReadNamespacesAndOrigins(&namespaces_and_origins);
  for (auto it = namespaces_and_origins.cbegin();
       it != namespaces_and_origins.cend(); ++it) {
    if (namespace_ids_in_use.find(it->first) == namespace_ids_in_use.end() &&
        protected_session_ids.find(it->first) == protected_session_ids.end()) {
      deletable_namespace_ids_.push_back(it->first);
    }
  }
  if (!deletable_namespace_ids_.empty()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DOMStorageContextImpl::DeleteNextUnusedNamespace, this),
        base::TimeDelta::FromSeconds(kSessionStoraceScavengingSeconds));
  }
}

void DOMStorageContextImpl::DeleteNextUnusedNamespace() {
  if (is_shutdown_)
    return;
  task_runner_->PostShutdownBlockingTask(
      FROM_HERE, DOMStorageTaskRunner::COMMIT_SEQUENCE,
      base::BindOnce(
          &DOMStorageContextImpl::DeleteNextUnusedNamespaceInCommitSequence,
          this));
}

void DOMStorageContextImpl::DeleteNextUnusedNamespaceInCommitSequence() {
  if (deletable_namespace_ids_.empty())
    return;
  const std::string& persistent_id = deletable_namespace_ids_.back();
  session_storage_database_->DeleteNamespace(persistent_id);
  deletable_namespace_ids_.pop_back();
  if (!deletable_namespace_ids_.empty()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DOMStorageContextImpl::DeleteNextUnusedNamespace, this),
        base::TimeDelta::FromSeconds(kSessionStoraceScavengingSeconds));
  }
}

}  // namespace content
