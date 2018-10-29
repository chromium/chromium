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
#include "base/debug/stack_trace.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "components/services/leveldb/public/interfaces/leveldb.mojom.h"
#include "content/browser/dom_storage/session_storage_area_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl_mojo.h"
#include "content/browser/dom_storage/storage_area_impl.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "services/file/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
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
}  // namespace

SessionStorageContextMojo::SessionStorageContextMojo(
    scoped_refptr<base::SequencedTaskRunner> memory_dump_task_runner,
    service_manager::Connector* connector,
    BackingMode backing_mode,
    base::FilePath local_partition_directory,
    std::string leveldb_name)
    : connector_(connector ? connector->Clone() : nullptr),
      backing_mode_(backing_mode),
      partition_directory_path_(std::move(local_partition_directory)),
      leveldb_name_(std::move(leveldb_name)),
      memory_dump_id_(base::StringPrintf("SessionStorage/0x%" PRIXPTR,
                                         reinterpret_cast<uintptr_t>(this))),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      weak_ptr_factory_(this) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kOnionSoupDOMStorage));
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
    blink::mojom::SessionStorageNamespaceRequest request) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(
        base::BindOnce(&SessionStorageContextMojo::OpenSessionStorage,
                       weak_ptr_factory_.GetWeakPtr(), process_id, namespace_id,
                       std::move(bad_message_callback), std::move(request)));
    return;
  }
  auto found = namespaces_.find(namespace_id);
  if (found == namespaces_.end()) {
    std::move(bad_message_callback).Run("Namespace not found: " + namespace_id);
    return;
  }

  if (!found->second->IsPopulated() &&
      !found->second->waiting_on_clone_population()) {
    found->second->PopulateFromMetadata(
        database_.get(), metadata_.GetOrCreateNamespaceEntry(namespace_id));
  }

  PurgeUnusedAreasIfNeeded();
  found->second->Bind(std::move(request), process_id);

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
    const std::string& namespace_id_to_clone,
    const std::string& clone_namespace_id,
    CloneType clone_type) {
  if (namespaces_.find(clone_namespace_id) != namespaces_.end()) {
    // Non-immediate commits expect to be paired with a |Clone| from the mojo
    // namespace object. If that clone has already happened, then we don't need
    // to do anything here.
    // However, immediate commits happen without a |Clone| from the mojo
    // namespace object, so there should never be a namespace already populated
    // for an immediate clone.
    DCHECK_NE(clone_type, CloneType::kImmediate);
    return;
  }

  std::unique_ptr<SessionStorageNamespaceImplMojo> namespace_impl =
      CreateSessionStorageNamespaceImplMojo(clone_namespace_id);
  switch (clone_type) {
    case CloneType::kImmediate: {
      auto clone_from_ns = namespaces_.find(namespace_id_to_clone);
      // If the namespace doesn't exist or it's not populated yet, just create
      // an empty session storage.
      if (clone_from_ns == namespaces_.end() ||
          !clone_from_ns->second->IsPopulated()) {
        break;
      }
      clone_from_ns->second->Clone(clone_namespace_id);
      return;
    }
    case CloneType::kWaitForCloneOnNamespace:
      namespace_impl->SetWaitingForClonePopulation();
      break;
    default:
      NOTREACHED();
  }
  namespaces_.emplace(std::piecewise_construct,
                      std::forward_as_tuple(clone_namespace_id),
                      std::forward_as_tuple(std::move(namespace_impl)));
}

void SessionStorageContextMojo::DeleteSessionNamespace(
    const std::string& namespace_id,
    bool should_persist) {
  // The object hierarchy uses iterators bound to the metadata object, so make
  // sure to delete the object hierarchy first.
  namespaces_.erase(namespace_id);

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
                                              const std::string& namespace_id) {
  if (connection_state_ != CONNECTION_FINISHED) {
    RunWhenConnected(base::BindOnce(&SessionStorageContextMojo::DeleteStorage,
                                    weak_ptr_factory_.GetWeakPtr(), origin,
                                    namespace_id));
    return;
  }
  auto found = namespaces_.find(namespace_id);
  if (found != namespaces_.end() &&
      (found->second->IsPopulated() ||
       found->second->waiting_on_clone_population())) {
    found->second->RemoveOriginData(origin);
  } else {
    // If we don't have the namespace loaded, then we can delete it all
    // using the metadata.
    std::vector<leveldb::mojom::BatchedOperationPtr> delete_operations;
    metadata_.DeleteArea(namespace_id, origin, &delete_operations);
    database_->Write(std::move(delete_operations),
                     base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                                    base::Unretained(this)));
  }
}

void SessionStorageContextMojo::ShutdownAndDelete() {
  DCHECK_NE(connection_state_, CONNECTION_SHUTDOWN);

  // Nothing to do if no connection to the database was ever finished.
  if (connection_state_ != CONNECTION_FINISHED) {
    connection_state_ = CONNECTION_SHUTDOWN;
    OnShutdownComplete(leveldb::mojom::DatabaseError::OK);
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

  OnShutdownComplete(leveldb::mojom::DatabaseError::OK);
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
  std::vector<leveldb::mojom::BatchedOperationPtr> delete_operations;
  for (const auto& namespace_id : namespaces_to_delete) {
    metadata_.DeleteNamespace(namespace_id, &delete_operations);
  }

  if (!delete_operations.empty()) {
    database_->Write(std::move(delete_operations),
                     base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                                    base::Unretained(this)));
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

void SessionStorageContextMojo::SetDatabaseForTesting(
    leveldb::mojom::LevelDBDatabaseAssociatedPtr database) {
  DCHECK_EQ(connection_state_, NO_CONNECTION);
  connection_state_ = CONNECTION_IN_PROGRESS;
  database_ = std::move(database);
  OnDatabaseOpened(true, leveldb::mojom::DatabaseError::OK);
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

scoped_refptr<SessionStorageMetadata::MapData>
SessionStorageContextMojo::RegisterNewAreaMap(
    SessionStorageMetadata::NamespaceEntry namespace_entry,
    const url::Origin& origin) {
  std::vector<leveldb::mojom::BatchedOperationPtr> save_operations;
  scoped_refptr<SessionStorageMetadata::MapData> map_entry =
      metadata_.RegisterNewMap(namespace_entry, origin, &save_operations);

  database_->Write(std::move(save_operations),
                   base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                                  base::Unretained(this)));
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

void SessionStorageContextMojo::OnCommitResult(
    leveldb::mojom::DatabaseError error) {
  DCHECK_EQ(connection_state_, CONNECTION_FINISHED);
  UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.CommitResult",
                            leveldb::GetLevelDBStatusUMAValue(error),
                            leveldb_env::LEVELDB_STATUS_MAX);
  if (error == leveldb::mojom::DatabaseError::OK) {
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
  std::vector<leveldb::mojom::BatchedOperationPtr> save_operations;

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

  auto namespace_entry = metadata_.GetOrCreateNamespaceEntry(new_namespace_id);
  metadata_.RegisterShallowClonedNamespace(source_namespace_entry,
                                           namespace_entry, &save_operations);
  database_->Write(std::move(save_operations),
                   base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                                  base::Unretained(this)));

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
  std::vector<leveldb::mojom::BatchedOperationPtr> delete_operations;
  metadata_.DeleteNamespace(namespace_id, &delete_operations);
  database_->Write(std::move(delete_operations),
                   base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                                  base::Unretained(this)));
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
  // Unit tests might not always have a Connector, use in-memory only if that
  // happens.
  if (!connector_) {
    OnDatabaseOpened(false, leveldb::mojom::DatabaseError::OK);
    return;
  }

  if (backing_mode_ != BackingMode::kNoDisk && !in_memory_only) {
    // We were given a subdirectory to write to. Get it and use a disk backed
    // database.
    connector_->BindInterface(file::mojom::kServiceName, &file_system_);
    file_system_->GetSubDirectory(
        partition_directory_path_.AsUTF8Unsafe(),
        MakeRequest(&partition_directory_),
        base::BindOnce(&SessionStorageContextMojo::OnDirectoryOpened,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // We were not given a subdirectory. Use a memory backed database.
    connector_->BindInterface(file::mojom::kServiceName, &leveldb_service_);
    leveldb_service_->OpenInMemory(
        memory_dump_id_, "SessionStorageDatabase", MakeRequest(&database_),
        base::BindOnce(&SessionStorageContextMojo::OnDatabaseOpened,
                       weak_ptr_factory_.GetWeakPtr(), true));
  }
}

void SessionStorageContextMojo::OnDirectoryOpened(base::File::Error err) {
  if (err != base::File::FILE_OK) {
    // We failed to open the directory; continue with startup so that we create
    // the data maps.
    UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DirectoryOpenError", -err,
                              -base::File::FILE_ERROR_MAX);
    LogDatabaseOpenResult(OpenResult::kDirectoryOpenFailed);
    OnDatabaseOpened(false, leveldb::mojom::DatabaseError::OK);
    return;
  }

  // Now that we have a directory, connect to the LevelDB service and get our
  // database.
  connector_->BindInterface(file::mojom::kServiceName, &leveldb_service_);

  // We might still need to use the directory, so create a clone.
  filesystem::mojom::DirectoryPtr partition_directory_clone;
  partition_directory_->Clone(MakeRequest(&partition_directory_clone));

  if (backing_mode_ == BackingMode::kClearDiskStateOnOpen) {
    filesystem::mojom::DirectoryPtr partition_directory_clone_for_deletion;
    partition_directory_->Clone(
        MakeRequest(&partition_directory_clone_for_deletion));
    leveldb_service_->Destroy(std::move(partition_directory_clone_for_deletion),
                              leveldb_name_, base::DoNothing());
  }

  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // use minimum
  // Default write_buffer_size is 4 MB but that might leave a 3.999
  // memory allocation in RAM from a log file recovery.
  options.write_buffer_size = 64 * 1024;
  options.block_cache = leveldb_chrome::GetSharedWebBlockCache();
  leveldb_service_->OpenWithOptions(
      std::move(options), std::move(partition_directory_clone), leveldb_name_,
      memory_dump_id_, MakeRequest(&database_),
      base::BindOnce(&SessionStorageContextMojo::OnDatabaseOpened,
                     weak_ptr_factory_.GetWeakPtr(), false));
}

void SessionStorageContextMojo::OnDatabaseOpened(
    bool in_memory,
    leveldb::mojom::DatabaseError status) {
  if (status != leveldb::mojom::DatabaseError::OK) {
    UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DatabaseOpenError",
                              leveldb::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);
    if (in_memory) {
      UMA_HISTOGRAM_ENUMERATION(
          "SessionStorageContext.DatabaseOpenError.Memory",
          leveldb::GetLevelDBStatusUMAValue(status),
          leveldb_env::LEVELDB_STATUS_MAX);
    } else {
      UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DatabaseOpenError.Disk",
                                leveldb::GetLevelDBStatusUMAValue(status),
                                leveldb_env::LEVELDB_STATUS_MAX);
    }
    LogDatabaseOpenResult(OpenResult::kDatabaseOpenFailed);
    // If we failed to open the database, try to delete and recreate the
    // database, or ultimately fallback to an in-memory database.
    DeleteAndRecreateDatabase(
        "SessionStorageContext.OpenResultAfterOpenFailed");
    return;
  }

  // Verify DB schema version.
  if (database_) {
    database_->Get(
        std::vector<uint8_t>(
            SessionStorageMetadata::kDatabaseVersionBytes,
            std::end(SessionStorageMetadata::kDatabaseVersionBytes)),
        base::BindOnce(&SessionStorageContextMojo::OnGotDatabaseVersion,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  OnConnectionFinished();
}

void SessionStorageContextMojo::OnGotDatabaseVersion(
    leveldb::mojom::DatabaseError status,
    const std::vector<uint8_t>& value) {
  std::vector<leveldb::mojom::BatchedOperationPtr> migration_operations;
  if (status == leveldb::mojom::DatabaseError::NOT_FOUND) {
    // New database, or schema v0. We must treat this as a schema v0 database.
    metadata_.ParseDatabaseVersion(base::nullopt, &migration_operations);
  } else if (status == leveldb::mojom::DatabaseError::OK) {
    if (!metadata_.ParseDatabaseVersion(value, &migration_operations)) {
      LogDatabaseOpenResult(OpenResult::kInvalidVersion);
      DeleteAndRecreateDatabase(
          "SessionStorageContext.OpenResultAfterInvalidVersion");
      return;
    }
    database_initialized_ = true;
  } else {
    // Other read error. Possibly database corruption.
    UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.ReadVersionError",
                              leveldb::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);
    LogDatabaseOpenResult(OpenResult::kVersionReadError);
    DeleteAndRecreateDatabase(
        "SessionStorageContext.OpenResultAfterReadVersionError");
    return;
  }

  base::RepeatingClosure barrier = base::BarrierClosure(
      2, base::BindOnce(&SessionStorageContextMojo::OnConnectionFinished,
                        weak_ptr_factory_.GetWeakPtr()));

  std::vector<uint8_t> namespace_prefix(
      SessionStorageMetadata::kNamespacePrefixBytes,
      std::end(SessionStorageMetadata::kNamespacePrefixBytes));
  std::vector<uint8_t> next_map_id_key(
      SessionStorageMetadata::kNextMapIdKeyBytes,
      std::end(SessionStorageMetadata::kNextMapIdKeyBytes));
  database_->GetPrefixed(
      namespace_prefix,
      base::BindOnce(&SessionStorageContextMojo::OnGotNamespaces,
                     weak_ptr_factory_.GetWeakPtr(), barrier,
                     std::move(migration_operations)));
  database_->Get(next_map_id_key,
                 base::BindOnce(&SessionStorageContextMojo::OnGotNextMapId,
                                weak_ptr_factory_.GetWeakPtr(), barrier));
}

void SessionStorageContextMojo::OnGotNamespaces(
    base::OnceClosure done,
    std::vector<leveldb::mojom::BatchedOperationPtr> migration_operations,
    leveldb::mojom::DatabaseError status,
    std::vector<leveldb::mojom::KeyValuePtr> values) {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);
  bool parsing_failure =
      status == leveldb::mojom::DatabaseError::OK &&
      !metadata_.ParseNamespaces(std::move(values), &migration_operations);
  if (status != leveldb::mojom::DatabaseError::OK || parsing_failure) {
    UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.ReadNamespacesError",
                              leveldb::GetLevelDBStatusUMAValue(status),
                              leveldb_env::LEVELDB_STATUS_MAX);
    LogDatabaseOpenResult(OpenResult::kNamespacesReadError);
    DeleteAndRecreateDatabase(
        "SessionStorageContext.OpenResultAfterReadNamespacesError");
    return;
  }

  // Write all of our migration operations if we have any.
  if (!migration_operations.empty()) {
    database_->Write(std::move(migration_operations),
                     base::BindOnce(&SessionStorageContextMojo::OnCommitResult,
                                    base::Unretained(this)));
  }
  std::move(done).Run();
}

void SessionStorageContextMojo::OnGotNextMapId(
    base::OnceClosure done,
    leveldb::mojom::DatabaseError status,
    const std::vector<uint8_t>& map_id) {
  DCHECK_EQ(connection_state_, CONNECTION_IN_PROGRESS);
  if (status == leveldb::mojom::DatabaseError::NOT_FOUND) {
    std::move(done).Run();
    return;
  }
  if (status == leveldb::mojom::DatabaseError::OK) {
    metadata_.ParseNextMapId(map_id);
    std::move(done).Run();
    return;
  }

  // Other read error. Possibly database corruption.
  UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.ReadNextMapIdError",
                            leveldb::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  LogDatabaseOpenResult(OpenResult::kNamespacesReadError);
  DeleteAndRecreateDatabase(
      "SessionStorageContext.OpenResultAfterReadNextMapIdError");
}

void SessionStorageContextMojo::OnConnectionFinished() {
  DCHECK(!database_ || connection_state_ == CONNECTION_IN_PROGRESS);
  if (!database_) {
    partition_directory_.reset();
    file_system_.reset();
    leveldb_service_.reset();
  }

  // If connection was opened successfully, reset tried_to_recreate_during_open_
  // to enable recreating the database on future errors.
  if (database_)
    tried_to_recreate_during_open_ = false;

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
  database_ = nullptr;
  open_result_histogram_ = histogram_name;

  bool recreate_in_memory = false;

  // If tried to recreate database on disk already, try again but this time
  // in memory.
  if (tried_to_recreate_during_open_ && backing_mode_ != BackingMode::kNoDisk) {
    recreate_in_memory = true;
  } else if (tried_to_recreate_during_open_) {
    // Give up completely, run without any database.
    OnConnectionFinished();
    return;
  }

  tried_to_recreate_during_open_ = true;

  // Unit tests might not have a bound file_service_, in which case there is
  // nothing to retry.
  if (!file_system_.is_bound()) {
    OnConnectionFinished();
    return;
  }

  protected_namespaces_from_scavenge_.clear();

  // Destroy database, and try again.
  if (partition_directory_.is_bound()) {
    leveldb_service_->Destroy(
        std::move(partition_directory_), leveldb_name_,
        base::BindOnce(&SessionStorageContextMojo::OnDBDestroyed,
                       weak_ptr_factory_.GetWeakPtr(), recreate_in_memory));
  } else {
    // No directory, so nothing to destroy. Retrying to recreate will probably
    // fail, but try anyway.
    InitiateConnection(recreate_in_memory);
  }
}

void SessionStorageContextMojo::OnDBDestroyed(
    bool recreate_in_memory,
    leveldb::mojom::DatabaseError status) {
  UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.DestroyDBResult",
                            leveldb::GetLevelDBStatusUMAValue(status),
                            leveldb_env::LEVELDB_STATUS_MAX);
  // We're essentially ignoring the status here. Even if destroying failed we
  // still want to go ahead and try to recreate.
  InitiateConnection(recreate_in_memory);
}

void SessionStorageContextMojo::OnShutdownComplete(
    leveldb::mojom::DatabaseError error) {
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
    LOG(ERROR) << "Got error when openning: " << static_cast<int>(result);
    UMA_HISTOGRAM_ENUMERATION("SessionStorageContext.OpenError", result);
  }
  if (open_result_histogram_) {
    base::UmaHistogramEnumeration(open_result_histogram_, result);
  }
}

}  // namespace content
