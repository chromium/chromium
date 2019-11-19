// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_context_mojo.h"

#include <inttypes.h>
#include <cctype>  // for std::isalnum
#include <cstring>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_types.h"
#include "content/browser/dom_storage/session_storage_area_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl_mojo.h"
#include "content/browser/dom_storage/storage_area_impl.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"

namespace content {
namespace {
// After this many consecutive commit errors we'll throw away the entire
// database.
const int kSessionStorageCommitErrorThreshold = 8;

// Limits on the cache size and number of areas in memory, over which the areas
// are purged.
#if defined(OS_ANDROID)
const unsigned kMaxSessionStorageAreaCount = 10;
const size_t kMaxSessionStorageCacheSize = 2 * 1024 * 1024;
#else
const unsigned kMaxSessionStorageAreaCount = 50;
const size_t kMaxSessionStorageCacheSize = 20 * 1024 * 1024;
#endif

enum class SessionStorageCachePurgeReason {
  kNotNeeded,
  kSizeLimitExceeded,
  kAreaCountLimitExceeded,
  kInactiveOnLowEndDevice,
  kAggressivePurgeTriggered
};

void RecordSessionStorageCachePurgedHistogram(
    SessionStorageCachePurgeReason reason,
    size_t purged_size_kib) {
  UMA_HISTOGRAM_COUNTS_100000("SessionStorageContext.CachePurgedInKB",
                              purged_size_kib);
  switch (reason) {
    case SessionStorageCachePurgeReason::kSizeLimitExceeded:
      UMA_HISTOGRAM_COUNTS_100000(
          "SessionStorageContext.CachePurgedInKB.SizeLimitExceeded",
          purged_size_kib);
      break;
    case SessionStorageCachePurgeReason::kAreaCountLimitExceeded:
      UMA_HISTOGRAM_COUNTS_100000(
          "SessionStorageContext.CachePurgedInKB.AreaCountLimitExceeded",
          purged_size_kib);
      break;
    case SessionStorageCachePurgeReason::kInactiveOnLowEndDevice:
      UMA_HISTOGRAM_COUNTS_100000(
          "SessionStorageContext.CachePurgedInKB.InactiveOnLowEndDevice",
          purged_size_kib);
      break;
    case SessionStorageCachePurgeReason::kAggressivePurgeTriggered:
      UMA_HISTOGRAM_COUNTS_100000(
          "SessionStorageContext.CachePurgedInKB.AggressivePurgeTriggered",
          purged_size_kib);
      break;
    case SessionStorageCachePurgeReason::kNotNeeded:
      NOTREACHED();
      break;
  }
}

void SessionStorageErrorResponse(base::OnceClosure callback,
                                 leveldb::Status status) {
  std::move(callback).Run();
}
}  // namespace

SessionStorageContextMojo::SessionStorageContextMojo(
    const base::FilePath& partition_directory,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    scoped_refptr<base::SequencedTaskRunner> memory_dump_task_runner,
    BackingMode backing_mode,
    std::string leveldb_name)
    : backing_mode_(backing_mode),
      leveldb_name_(std::move(leveldb_name)),
      partition_directory_(partition_directory),
      leveldb_task_runner_(std::move(blocking_task_runner)),
      memory_dump_id_(base::StringPrintf("SessionStorage/0x%" PRIXPTR,
                                         reinterpret_cast<uintptr_t>(this))),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "SessionStorage", std::move(memory_dump_task_runner),
          base::trace_event::MemoryDumpProvider::Options());
}

SessionStorageContextMojo::~SessionStorageContextMojo() {
  DCHECK_EQ(connection_state_, CONNECTION_SHUTDOWN);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void SessionStorageContextMojo::OpenSessionStorage(
    int process_id,
    const std::string& namespace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(
        base::BindOnce(&SessionStorageContextMojo::OpenSessionStorage,
                       weak_ptr_factory_.GetWeakPtr(), process_id, namespace_id,
                       std::move(bad_message_callback), std::move(receiver)));
    return;
  }
  auto found = namespaces_.find(namespace_id);
  if (found == namespaces_.end()) {
    std::move(bad_message_callback).Run("Namespace not found: " + namespace_id);
    return;
  }

  if (found->second->state() ==
      SessionStorageNamespaceImplMojo::State::kNotPopulated) {
    found->second->PopulateFromMetadata(
        database_.get(), metadata_.GetOrCreateNamespaceEntry(namespace_id));
  }

  PurgeUnusedAreasIfNeeded();
  found->second->Bind(std::move(receiver), process_id);

  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);
  // Track the total sessionStorage cache size.
  UMA_HISTOGRAM_COUNTS_100000("SessionStorageContext.CacheSizeInKB",
                              total_cache_size / 1024);
}

void SessionStorageContextMojo::CreateSessionNamespace(
    const std::string& namespace_id) {
  if (namespaces_.find(namespace_id) != namespaces_.end())
    return;

  namespaces_.emplace(std::make_pair(
      namespace_id, CreateSessionStorageNamespaceImplMojo(namespace_id)));
}

void SessionStorageContextMojo::CloneSessionNamespace(
    const std::string& clone_from_namespace_id,
    const std::string& clone_to_namespace_id,
    CloneType clone_type) {
  if (namespaces_.find(clone_to_namespace_id) != namespaces_.end()) {
    // Non-immediate clones expect to be paired with a |Clone| from the mojo
    // namespace object. If that clone has already happened, then we don't need
    // to do anything here.
    // However, immediate clones happen without a |Clone| from the mojo
    // namespace object, so there should never be a namespace already populated
    // for an immediate clone.
    DCHECK_NE(clone_type, CloneType::kImmediate);
    return;
  }

  auto clone_from_ns = namespaces_.find(clone_from_namespace_id);
  std::unique_ptr<SessionStorageNamespaceImplMojo> clone_to_namespace_impl =
      CreateSessionStorageNamespaceImplMojo(clone_to_namespace_id);
  switch (clone_type) {
    case CloneType::kImmediate: {
      // If the namespace doesn't exist or it's not populated yet, just create
      // an empty session storage by not marking it as pending a clone.
      if (clone_from_ns == namespaces_.end() ||
          !clone_from_ns->second->IsPopulated()) {
        break;
      }
      clone_from_ns->second->Clone(clone_to_namespace_id);
      return;
    }
    case CloneType::kWaitForCloneOnNamespace:
      if (clone_from_ns != namespaces_.end()) {
        // The namespace exists and is in-use, so wait until receiving a clone
        // call on that mojo binding.
        clone_to_namespace_impl->SetPendingPopulationFromParentNamespace(
            clone_from_namespace_id);
        clone_from_ns->second->AddChildNamespaceWaitingForClone(
            clone_to_namespace_id);
      } else if (base::Contains(metadata_.namespace_origin_map(),
                                clone_from_namespace_id)) {
        DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
        // The namespace exists on disk but is not in-use, so do the appropriate
        // metadata operations to clone the namespace and set up the new object.
        std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>
            save_tasks;
        auto source_namespace_entry =
            metadata_.GetOrCreateNamespaceEntry(clone_from_namespace_id);
        auto namespace_entry =
            metadata_.GetOrCreateNamespaceEntry(clone_to_namespace_id);
        metadata_.RegisterShallowClonedNamespace(source_namespace_entry,
                                                 namespace_entry, &save_tasks);
        if (database_) {
          database_->RunBatchDatabaseTasks(
              std::move(save_tasks),
              base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                             weak_ptr_factory_.GetWeakPtr()));
        }
      }
      // If there is no sign of a source namespace, just run with an empty
      // namespace.
      break;
    default:
      NOTREACHED();
  }
  namespaces_.emplace(
      std::piecewise_construct, std::forward_as_tuple(clone_to_namespace_id),
      std::forward_as_tuple(std::move(clone_to_namespace_impl)));
}

void SessionStorageContextMojo::DeleteSessionNamespace(
    const std::string& namespace_id,
    bool should_persist) {
  auto namespace_it = namespaces_.find(namespace_id);
  // If the namespace has pending clones, do the clone now before destroying it.
  if (namespace_it != namespaces_.end()) {
    SessionStorageNamespaceImplMojo* namespace_ptr = namespace_it->second.get();
    if (namespace_ptr->HasChildNamespacesWaitingForClone()) {
      // Wait until we are connected, as it simplifies our choices.
      if (connection_state_ != CONNECTION_FINISHED) {
        RunWhenConnected(base::BindOnce(
            &SessionStorageContextMojo::DeleteSessionNamespace,
            weak_ptr_factory_.GetWeakPtr(), namespace_id, should_persist));
        return;
      }
      namespace_ptr->CloneAllNamespacesWaitingForClone(database_.get(),
                                                       &metadata_, namespaces_);
    }

    // The object hierarchy uses iterators bound to the metadata object, so
    // make sure to delete the object hierarchy first.
    namespaces_.erase(namespace_it);
  }

  if (!has_scavenged_ && should_persist)
    protected_namespaces_from_scavenge_.insert(namespace_id);

  if (!should_persist) {
    RunWhenConnected(
        base::BindOnce(&SessionStorageContextMojo::DoDatabaseDelete,
                       weak_ptr_factory_.GetWeakPtr(), namespace_id));
  }
}

void SessionStorageContextMojo::Flush() {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageContextMojo::Flush,
                                    weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  for (const auto& it : data_maps_)
    it.second->storage_area()->ScheduleImmediateCommit();
}

void SessionStorageContextMojo::GetStorageUsage(
    GetStorageUsageCallback callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageContextMojo::GetStorageUsage,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback)));
    return;
  }

  const SessionStorageMetadata::NamespaceOriginMap& all_namespaces =
      metadata_.namespace_origin_map();

  std::vector<SessionStorageUsageInfo> result;
  result.reserve(all_namespaces.size());
  for (const auto& pair : all_namespaces) {
    for (const auto& origin_map_pair : pair.second) {
      SessionStorageUsageInfo info = {origin_map_pair.first.GetURL(),
                                      pair.first};
      result.push_back(std::move(info));
    }
  }
  std::move(callback).Run(std::move(result));
}

void SessionStorageContextMojo::DeleteStorage(const url::Origin& origin,
                                              const std::string& namespace_id,
                                              base::OnceClosure callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageContextMojo::DeleteStorage,
                                    weak_ptr_factory_.GetWeakPtr(), origin,
                                    namespace_id, std::move(callback)));
    return;
  }
  auto found = namespaces_.find(namespace_id);
  if (found != namespaces_.end() &&
      found->second->state() !=
          SessionStorageNamespaceImplMojo::State::kNotPopulated) {
    found->second->RemoveOriginData(origin, std::move(callback));
  } else {
    // If we don't have the namespace loaded, then we can delete it all
    // using the metadata.
    std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
    metadata_.DeleteArea(namespace_id, origin, &tasks);
    if (database_) {
      database_->RunBatchDatabaseTasks(
          std::move(tasks),
          base::BindOnce(&SessionStorageContextMojo::OnCommitResultWithCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    } else {
      std::move(callback).Run();
    }
  }
}

void SessionStorageContextMojo::PerformStorageCleanup(
    base::OnceClosure callback) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(
        base::BindOnce(&SessionStorageContextMojo::PerformStorageCleanup,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  if (database_) {
    for (const auto& it : data_maps_)
      it.second->storage_area()->ScheduleImmediateCommit();
    database_->RewriteDB(
        base::BindOnce(&SessionStorageErrorResponse, std::move(callback)));
  } else {
    std::move(callback).Run();
  }
}

void SessionStorageContextMojo::ShutdownAndDelete() {
  DCHECK_NE(connection_state_, CONNECTION_SHUTDOWN);

  // The namespaces will DCHECK if they are destructed with pending clones. It
  // is valid to drop these on shutdown.
  for (auto& namespace_pair : namespaces_) {
    namespace_pair.second->ClearChildNamespacesWaitingForClone();
  }

  // Nothing to do if no connection to the database was ever finished.
  if (connection_state_ != CONNECTION_FINISHED) {
    connection_state_ = CONNECTION_SHUTDOWN;
    OnShutdownComplete(leveldb::Status::OK());
    return;
  }
  connection_state_ = CONNECTION_SHUTDOWN;

  // Flush any uncommitted data.
  for (const auto& it : data_maps_) {
    auto* area = it.second->storage_area();
    LOCAL_HISTOGRAM_BOOLEAN(
        "SessionStorageContext.ShutdownAndDelete.MaybeDroppedChanges",
        area->has_pending_load_tasks());
    area->ScheduleImmediateCommit();
    // TODO(dmurph): Monitor the above histogram, and if dropping changes is
    // common then handle that here.
    area->CancelAllPendingRequests();
  }

  OnShutdownComplete(leveldb::Status::OK());
}

void SessionStorageContextMojo::PurgeMemory() {
  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);

  // Purge all areas that don't have bindings.
  for (const auto& namespace_pair : namespaces_) {
    namespace_pair.second->PurgeUnboundAreas();
  }
  // Purge memory from bound maps.
  for (const auto& data_map_pair : data_maps_) {
    data_map_pair.second->storage_area()->PurgeMemory();
  }

  // Track the size of cache purged.
  size_t final_total_cache_size;
  GetStatistics(&final_total_cache_size, &unused_area_count);
  size_t purged_size_kib = (total_cache_size - final_total_cache_size) / 1024;
  RecordSessionStorageCachePurgedHistogram(
      SessionStorageCachePurgeReason::kAggressivePurgeTriggered,
      purged_size_kib);
}

void SessionStorageContextMojo::PurgeUnusedAreasIfNeeded() {
  size_t total_cache_size, unused_area_count;
  GetStatistics(&total_cache_size, &unused_area_count);

  // Nothing to purge.
  if (!unused_area_count)
    return;

  SessionStorageCachePurgeReason purge_reason =
      SessionStorageCachePurgeReason::kNotNeeded;

  if (total_cache_size > kMaxSessionStorageCacheSize)
    purge_reason = SessionStorageCachePurgeReason::kSizeLimitExceeded;
  else if (data_maps_.size() > kMaxSessionStorageAreaCount)
    purge_reason = SessionStorageCachePurgeReason::kAreaCountLimitExceeded;
  else if (is_low_end_device_)
    purge_reason = SessionStorageCachePurgeReason::kInactiveOnLowEndDevice;

  if (purge_reason == SessionStorageCachePurgeReason::kNotNeeded)
    return;

  // Purge all areas that don't have bindings.
  for (const auto& namespace_pair : namespaces_) {
    namespace_pair.second->PurgeUnboundAreas();
  }

  size_t final_total_cache_size;
  GetStatistics(&final_total_cache_size, &unused_area_count);
  size_t purged_size_kib = (total_cache_size - final_total_cache_size) / 1024;
  RecordSessionStorageCachePurgedHistogram(purge_reason, purged_size_kib);
}

void SessionStorageContextMojo::ScavengeUnusedNamespaces(
    base::OnceClosure done) {
  if (has_scavenged_)
    return;
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(
        base::BindOnce(&SessionStorageContextMojo::ScavengeUnusedNamespaces,
                       weak_ptr_factory_.GetWeakPtr(), std::move(done)));
    return;
  }
  has_scavenged_ = true;
  std::vector<std::string> namespaces_to_delete;
  for (const auto& metadata_namespace : metadata_.namespace_origin_map()) {
    const std::string& namespace_id = metadata_namespace.first;
    if (namespaces_.find(namespace_id) != namespaces_.end() ||
        protected_namespaces_from_scavenge_.find(namespace_id) !=
            protected_namespaces_from_scavenge_.end()) {
      continue;
    }
    namespaces_to_delete.push_back(namespace_id);
  }
  std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
  for (const auto& namespace_id : namespaces_to_delete)
    metadata_.DeleteNamespace(namespace_id, &save_tasks);

  if (database_) {
    database_->RunBatchDatabaseTasks(
        std::move(save_tasks),
        base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  protected_namespaces_from_scavenge_.clear();
  if (done)
    std::move(done).Run();
}

bool SessionStorageContextMojo::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (connection_state_ != CONNECTION_FINISHED)
    return true;

  std::string context_name =
      base::StringPrintf("site_storage/sessionstorage/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this));

  // Account for leveldb memory usage, which actually lives in the file service.
  auto* global_dump = pmd->CreateSharedGlobalAllocatorDump(memory_dump_id_);
  // The size of the leveldb dump will be added by the leveldb service.
  auto* leveldb_mad = pmd->CreateAllocatorDump(context_name + "/leveldb");
  // Specifies that the current context is responsible for keeping memory alive.
  int kImportance = 2;
  pmd->AddOwnershipEdge(leveldb_mad->guid(), global_dump->guid(), kImportance);

  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    size_t total_cache_size, unused_area_count;
    GetStatistics(&total_cache_size, &unused_area_count);
    auto* mad = pmd->CreateAllocatorDump(context_name + "/cache_size");
    mad->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                   base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                   total_cache_size);
    mad->AddScalar("total_areas",
                   base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                   data_maps_.size());
    return true;
  }
  for (const auto& it : data_maps_) {
    // Limit the url length to 50 and strip special characters.
    const auto& origin = it.second->map_data()->origin();
    std::string url = origin.Serialize().substr(0, 50);
    for (size_t index = 0; index < url.size(); ++index) {
      if (!std::isalnum(url[index]))
        url[index] = '_';
    }
    std::string area_dump_name = base::StringPrintf(
        "%s/%s/0x%" PRIXPTR, context_name.c_str(), url.c_str(),
        reinterpret_cast<uintptr_t>(it.second->storage_area()));
    it.second->storage_area()->OnMemoryDump(area_dump_name, pmd);
  }
  return true;
}

void SessionStorageContextMojo::PretendToConnectForTesting() {
  OnDatabaseOpened(leveldb::Status::OK());
}

void SessionStorageContextMojo::FlushAreaForTesting(
    const std::string& namespace_id,
    const url::Origin& origin) {
  if (connection_state_ != CONNECTION_FINISHED)
    return;
  const auto& it = namespaces_.find(namespace_id);
  if (it == namespaces_.end())
    return;
  it->second->FlushOriginForTesting(origin);
}

void SessionStorageContextMojo::SetDatabaseOpenCallbackForTesting(
    base::OnceClosure callback) {
  RunWhenConnected(std::move(callback));
}

scoped_refptr<SessionStorageMetadata::MapData>
SessionStorageContextMojo::RegisterNewAreaMap(
    SessionStorageMetadata::NamespaceEntry namespace_entry,
    const url::Origin& origin) {
  std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
  scoped_refptr<SessionStorageMetadata::MapData> map_entry =
      metadata_.RegisterNewMap(namespace_entry, origin, &save_tasks);

  if (database_) {
    database_->RunBatchDatabaseTasks(
        std::move(save_tasks),
        base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  return map_entry;
}

void SessionStorageContextMojo::OnDataMapCreation(
    const std::vector<uint8_t>& map_prefix,
    SessionStorageDataMap* map) {
  DCHECK(data_maps_.find(map_prefix) == data_maps_.end());
  data_maps_.emplace(std::piecewise_construct,
                     std::forward_as_tuple(map_prefix),
                     std::forward_as_tuple(map));
}

void SessionStorageContextMojo::OnDataMapDestruction(
    const std::vector<uint8_t>& map_prefix) {
  data_maps_.erase(map_prefix);
}

void SessionStorageContextMojo::OnCommitResult(leveldb::Status status) {
  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.CommitResult",
                            leveldb_env::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  if (status.ok()) {
    commit_error_count_ = 0;
    return;
  }
  commit_error_count_++;
  if (commit_error_count_ > kSessionStorageCommitErrorThreshold) {
    if (tried_to_recover_from_commit_errors_) {
      // We already tried to recover from a high commit error rate before, but
      // are still having problems: there isn't really anything left to try, so
      // just ignore errors.
      return;
    }
    tried_to_recover_from_commit_errors_ = true;

    // Deleting StorageAreas in here could cause more commits (and commit
    // errors), but those commits won't reach OnCommitResult because the area
    // will have been deleted before the commit finishes.
    DeleteAndRecreateDatabase(
        "SessionStorageContext.OpenResultAfterCommitErrors");
  }
}

void SessionStorageContextMojo::OnCommitResultWithCallback(
    base::OnceClosure callback,
    leveldb::Status status) {
  OnCommitResult(status);
  std::move(callback).Run();
}

scoped_refptr<SessionStorageDataMap>
SessionStorageContextMojo::MaybeGetExistingDataMapForId(
    const std::vector<uint8_t>& map_number_as_bytes) {
  auto it = data_maps_.find(map_number_as_bytes);
  if (it == data_maps_.end())
    return nullptr;
  return base::WrapRefCounted(it->second);
}

void SessionStorageContextMojo::RegisterShallowClonedNamespace(
    SessionStorageMetadata::NamespaceEntry source_namespace_entry,
    const std::string& new_namespace_id,
    const SessionStorageNamespaceImplMojo::OriginAreas& clone_from_areas) {
  std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;

  bool found = false;
  auto it = namespaces_.find(new_namespace_id);
  if (it != namespaces_.end()) {
    found = true;
    if (it->second->IsPopulated()) {
      // Assumes this method is called on a stack handling a mojo message.
      mojo::ReportBadMessage("Cannot clone to already populated namespace");
      return;
    }
  }

  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  auto namespace_entry = metadata_.GetOrCreateNamespaceEntry(new_namespace_id);
  metadata_.RegisterShallowClonedNamespace(source_namespace_entry,
                                           namespace_entry, &save_tasks);
  if (database_) {
    database_->RunBatchDatabaseTasks(
        std::move(save_tasks),
        base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  if (found) {
    it->second->PopulateAsClone(database_.get(), namespace_entry,
                                clone_from_areas);
    return;
  }

  auto namespace_impl = CreateSessionStorageNamespaceImplMojo(new_namespace_id);
  namespace_impl->PopulateAsClone(database_.get(), namespace_entry,
                                  clone_from_areas);
  namespaces_.emplace(std::piecewise_construct,
                      std::forward_as_tuple(new_namespace_id),
                      std::forward_as_tuple(std::move(namespace_impl)));
}

std::unique_ptr<SessionStorageNamespaceImplMojo>
SessionStorageContextMojo::CreateSessionStorageNamespaceImplMojo(
    std::string namespace_id) {
  SessionStorageAreaImpl::RegisterNewAreaMap map_id_callback =
      base::BindRepeating(&SessionStorageContextMojo::RegisterNewAreaMap,
                          base::Unretained(this));

  return std::make_unique<SessionStorageNamespaceImplMojo>(
      std::move(namespace_id), this, std::move(map_id_callback), this);
}

void SessionStorageContextMojo::DoDatabaseDelete(
    const std::string& namespace_id) {
  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> tasks;
  metadata_.DeleteNamespace(namespace_id, &tasks);
  if (database_) {
    database_->RunBatchDatabaseTasks(
        std::move(tasks),
        base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void SessionStorageContextMojo::RunWhenConnected(base::OnceClosure callback) {
  switch (connection_state_) {
    case NO_CONNECTION:
      // If we don't have a filesystem_connection_, we'll need to establish one.
      connection_state_ = CONNECTION_IN_PROGRESS;
      on_database_opened_callbacks_.push_back(std::move(callback));
      InitiateConnection();
      return;
    case CONNECTION_IN_PROGRESS:
      // Queue this OpenSessionStorage call for when we have a level db pointer.
      on_database_opened_callbacks_.push_back(std::move(callback));
      return;
    case CONNECTION_SHUTDOWN:
      NOTREACHED();
      return;
    case CONNECTION_FINISHED:
      std::move(callback).Run();
      return;
  }
  NOTREACHED();
}

void SessionStorageContextMojo::InitiateConnection(bool in_memory_only) {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);

  if (backing_mode_ != BackingMode::kNoDisk && !in_memory_only &&
      !partition_directory_.empty()) {
    // We were given a subdirectory to write to, so use a disk backed database.
    if (backing_mode_ == BackingMode::kClearDiskStateOnOpen) {
      storage::DomStorageDatabase::Destroy(partition_directory_, leveldb_name_,
                                           leveldb_task_runner_,
                                           base::DoNothing());
    }

    leveldb_env::Options options;
    options.create_if_missing = true;
    options.max_open_files = 0;  // use minimum
    // Default write_buffer_size is 4 MB but that might leave a 3.999
    // memory allocation in RAM from a log file recovery.
    options.write_buffer_size = 64 * 1024;
    options.block_cache = leveldb_chrome::GetSharedWebBlockCache();

    in_memory_ = false;
    database_ = storage::AsyncDomStorageDatabase::OpenDirectory(
        std::move(options), partition_directory_, leveldb_name_,
        memory_dump_id_, leveldb_task_runner_,
        base::BindOnce(&SessionStorageContextMojo::OnDatabaseOpened,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // We were not given a subdirectory. Use a memory backed database.
  in_memory_ = true;
  database_ = storage::AsyncDomStorageDatabase::OpenInMemory(
      memory_dump_id_, "SessionStorageDatabase", leveldb_task_runner_,
      base::BindOnce(&SessionStorageContextMojo::OnDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr()));
}

SessionStorageContextMojo::ValueAndStatus::ValueAndStatus() = default;

SessionStorageContextMojo::ValueAndStatus::ValueAndStatus(ValueAndStatus&&) =
    default;

SessionStorageContextMojo::ValueAndStatus::~ValueAndStatus() = default;

SessionStorageContextMojo::KeyValuePairsAndStatus::KeyValuePairsAndStatus() =
    default;

SessionStorageContextMojo::KeyValuePairsAndStatus::KeyValuePairsAndStatus(
    KeyValuePairsAndStatus&&) = default;

SessionStorageContextMojo::KeyValuePairsAndStatus::~KeyValuePairsAndStatus() =
    default;

void SessionStorageContextMojo::OnDatabaseOpened(leveldb::Status status) {
  if (!status.ok()) {
    UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DatabaseOpenError",
                              leveldb_env::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);
    if (in_memory_) {
      UMA_HISTOGRAM_ENUMERATION(
          "SessionStorageContext.DatabaseOpenError.Memory",
          leveldb_env::GetLevelDBStatusUMAValue(status),
          leveldb_env::LEVELDB_STATUS_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DatabaseOpenError.Disk",
                                leveldb_env::GetLevelDBStatusUMAValue(status),
                                leveldb_env::LEVELDB_STATUS_MAX);
    }
    LogDatabaseOpenResult(OpenResult::kDatabaseOpenFailed);
    // If we failed to open the database, try to delete and recreate the
    // database, or ultimately fallback to an in-memory database.
    DeleteAndRecreateDatabase(
        "SessionStorageContext.OpenResultAfterOpenFailed");
    return;
  }

  if (!database_) {
    // Some tests only simulate database connection without a database being
    // present.
    OnConnectionFinished();
    return;
  }

  database_->RunDatabaseTask(
      base::BindOnce([](const storage::DomStorageDatabase& db) {
        ValueAndStatus version;
        version.status = db.Get(
            base::make_span(SessionStorageMetadata::kDatabaseVersionBytes),
            &version.value);

        KeyValuePairsAndStatus namespaces;
        namespaces.status = db.GetPrefixed(
            base::make_span(SessionStorageMetadata::kNamespacePrefixBytes),
            &namespaces.key_value_pairs);

        ValueAndStatus next_map_id;
        next_map_id.status =
            db.Get(base::make_span(SessionStorageMetadata::kNextMapIdKeyBytes),
                   &next_map_id.value);

        return std::make_tuple(std::move(version), std::move(namespaces),
                               std::move(next_map_id));
      }),
      base::BindOnce(&SessionStorageContextMojo::OnGotDatabaseMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionStorageContextMojo::OnGotDatabaseMetadata(
    ValueAndStatus version,
    KeyValuePairsAndStatus namespaces,
    ValueAndStatus next_map_id) {
  std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>
      migration_tasks;

  MetadataParseResult version_parse =
      ParseDatabaseVersion(std::move(version), &migration_tasks);
  if (version_parse.open_result != OpenResult::kSuccess) {
    LogDatabaseOpenResult(version_parse.open_result);
    DeleteAndRecreateDatabase(version_parse.histogram_name);
    return;
  }

  MetadataParseResult namespaces_parse =
      ParseNamespaces(std::move(namespaces), std::move(migration_tasks));
  if (namespaces_parse.open_result != OpenResult::kSuccess) {
    LogDatabaseOpenResult(namespaces_parse.open_result);
    DeleteAndRecreateDatabase(namespaces_parse.histogram_name);
    return;
  }

  MetadataParseResult next_map_id_parse =
      ParseNextMapId(std::move(next_map_id));
  if (next_map_id_parse.open_result != OpenResult::kSuccess) {
    LogDatabaseOpenResult(next_map_id_parse.open_result);
    DeleteAndRecreateDatabase(next_map_id_parse.histogram_name);
    return;
  }

  OnConnectionFinished();
}

SessionStorageContextMojo::MetadataParseResult
SessionStorageContextMojo::ParseDatabaseVersion(
    ValueAndStatus version,
    std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>*
        migration_tasks) {
  if (version.status.ok()) {
    if (!metadata_.ParseDatabaseVersion(std::move(version.value),
                                        migration_tasks)) {
      return {OpenResult::kInvalidVersion,
              "SessionStorageContext.OpenResultAfterInvalidVersion"};
    }
    database_initialized_ = true;
    return {OpenResult::kSuccess, ""};
  }

  if (version.status.IsNotFound()) {
    // treat as v0 or new database
    metadata_.ParseDatabaseVersion(base::nullopt, migration_tasks);
    return {OpenResult::kSuccess, ""};
  }

  // Other read error, Possibly database corruption
  UMA_HISTOGRAM_ENUMERATION(
      "SessionStorageContext.ReadVersionError",
      leveldb_env::GetLevelDBStatusUMAValue(version.status),
      leveldb_env::LEVELDB_STATUS_MAX);
  return {OpenResult::kVersionReadError,
          "SessionStorageContext.OpenResultAfterReadVersionError"};
}

SessionStorageContextMojo::MetadataParseResult
SessionStorageContextMojo::ParseNamespaces(
    KeyValuePairsAndStatus namespaces,
    std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>
        migration_tasks) {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);

  if (!namespaces.status.ok()) {
    UMA_HISTOGRAM_ENUMERATION(
        "SessionStorageContext.ReadNamespacesError",
        leveldb_env::GetLevelDBStatusUMAValue(namespaces.status),
        leveldb_env::LEVELDB_STATUS_MAX);
    return {OpenResult::kNamespacesReadError,
            "SessionStorageContext.OpenResultAfterReadNamespacesError"};
  }

  bool parsing_success = metadata_.ParseNamespaces(
      std::move(namespaces.key_value_pairs), &migration_tasks);

  if (!parsing_success) {
    UMA_HISTOGRAM_ENUMERATION(
        "SessionStorageContext.ReadNamespacesError",
        leveldb_env::GetLevelDBStatusUMAValue(leveldb::Status::OK()),
        leveldb_env::LEVELDB_STATUS_MAX);
    return {OpenResult::kNamespacesReadError,
            "SessionStorageContext.OpenResultAfterReadNamespacesError"};
  }

  if (!migration_tasks.empty()) {
    // In tests this write may happen synchronously, which is problematic since
    // the OnCommitResult callback can be invoked before the database is fully
    // initialized. There's no harm in deferring in other situations, so we just
    // always defer here.
    database_->RunBatchDatabaseTasks(
        std::move(migration_tasks),
        base::BindOnce(
            [](base::OnceCallback<void(leveldb::Status)> callback,
               scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
               leveldb::Status status) {
              callback_task_runner->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), status));
            },
            base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                           weak_ptr_factory_.GetWeakPtr()),
            base::SequencedTaskRunnerHandle::Get()));
  }

  return {OpenResult::kSuccess, ""};
}

SessionStorageContextMojo::MetadataParseResult
SessionStorageContextMojo::ParseNextMapId(ValueAndStatus next_map_id) {
  if (!next_map_id.status.ok()) {
    if (next_map_id.status.IsNotFound())
      return {OpenResult::kSuccess, ""};

    // Other read error. Possibly database corruption.
    UMA_HISTOGRAM_ENUMERATION(
        "SessionStorageContext.ReadNextMapIdError",
        leveldb_env::GetLevelDBStatusUMAValue(next_map_id.status),
        leveldb_env::LEVELDB_STATUS_MAX);
    return {OpenResult::kNamespacesReadError,
            "SessionStorageContext.OpenResultAfterReadNextMapIdError"};
  }

  metadata_.ParseNextMapId(std::move(next_map_id.value));
  return {OpenResult::kSuccess, ""};
}

void SessionStorageContextMojo::OnConnectionFinished() {
  DCHECK(!database_ || connection_state_ == CONNECTION_IN_PROGRESS);

  // If connection was opened successfully, reset tried_to_recreate_during_open_
  // to enable recreating the database on future errors.
  if (database_)
    tried_to_recreate_during_open_ = false;

  LogDatabaseOpenResult(OpenResult::kSuccess);
  open_result_histogram_ = nullptr;

  // |database_| should be known to either be valid or invalid by now. Run our
  // delayed bindings.
  connection_state_ = CONNECTION_FINISHED;
  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks, on_database_opened_callbacks_);
  for (size_t i = 0; i < callbacks.size(); ++i)
    std::move(callbacks[i]).Run();
}

void SessionStorageContextMojo::DeleteAndRecreateDatabase(
    const char* histogram_name) {
  // We're about to set database_ to null, so delete the StorageAreas
  // that might still be using the old database.
  for (const auto& it : data_maps_)
    it.second->storage_area()->CancelAllPendingRequests();

  for (const auto& namespace_pair : namespaces_) {
    namespace_pair.second->Reset();
  }
  DCHECK(data_maps_.empty());

  // Reset state to be in process of connecting. This will cause requests for
  // StorageAreas to be queued until the connection is complete.
  connection_state_ = CONNECTION_IN_PROGRESS;
  commit_error_count_ = 0;
  database_.reset();
  open_result_histogram_ = histogram_name;

  bool recreate_in_memory = false;

  // If tried to recreate database on disk already, try again but this time
  // in memory.
  if (tried_to_recreate_during_open_ && !in_memory_) {
    recreate_in_memory = true;
  } else if (tried_to_recreate_during_open_) {
    // Give up completely, run without any database.
    OnConnectionFinished();
    return;
  }

  tried_to_recreate_during_open_ = true;

  protected_namespaces_from_scavenge_.clear();

  // Destroy database, and try again.
  if (!in_memory_) {
    storage::DomStorageDatabase::Destroy(
        partition_directory_, leveldb_name_, leveldb_task_runner_,
        base::BindOnce(&SessionStorageContextMojo::OnDBDestroyed,
                       weak_ptr_factory_.GetWeakPtr(), recreate_in_memory));
  } else {
    // No directory, so nothing to destroy. Retrying to recreate will probably
    // fail, but try anyway.
    InitiateConnection(recreate_in_memory);
  }
}

void SessionStorageContextMojo::OnDBDestroyed(bool recreate_in_memory,
                                              leveldb::Status status) {
  UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DestroyDBResult",
                            leveldb_env::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  // We're essentially ignoring the status here. Even if destroying failed we
  // still want to go ahead and try to recreate.
  InitiateConnection(recreate_in_memory);
}

void SessionStorageContextMojo::OnShutdownComplete(leveldb::Status status) {
  delete this;
}

void SessionStorageContextMojo::GetStatistics(size_t* total_cache_size,
                                              size_t* unused_area_count) {
  *total_cache_size = 0;
  *unused_area_count = 0;
  for (const auto& it : data_maps_) {
    *total_cache_size += it.second->storage_area()->memory_used();
    if (it.second->binding_count() == 0)
      (*unused_area_count)++;
  }
}

void SessionStorageContextMojo::LogDatabaseOpenResult(OpenResult result) {
  if (result != OpenResult::kSuccess) {
    UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.OpenError", result);
  }
  if (open_result_histogram_) {
    base::UmaHistogramEnumeration(open_result_histogram_, result);
  }
}

}  // namespace content
