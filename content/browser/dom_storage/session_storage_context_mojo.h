// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_CONTEXT_MOJO_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_CONTEXT_MOJO_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/session_storage_data_map.h"
#include "content/browser/dom_storage/session_storage_metadata.h"
#include "content/browser/dom_storage/session_storage_namespace_impl_mojo.h"
#include "content/common/content_export.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
struct SessionStorageUsageInfo;

// Used for mojo-based SessionStorage implementation.
// Created on the UI thread, but all further methods are called on the task
// runner passed to the constructor. Furthermore since destruction of this class
// can involve asynchronous steps, it can only be deleted by calling
// ShutdownAndDelete (on the correct task runner).
class CONTENT_EXPORT SessionStorageContextMojo
    : public base::trace_event::MemoryDumpProvider,
      public SessionStorageDataMap::Listener,
      public SessionStorageNamespaceImplMojo::Delegate {
 public:
  using GetStorageUsageCallback =
      base::OnceCallback<void(std::vector<SessionStorageUsageInfo>)>;

  enum class CloneType {
    // Expect a clone to come from the SessionStorageNamespace mojo object. This
    // guarantees ordering with any writes from that namespace.
    kWaitForCloneOnNamespace,
    // There will not be a clone coming from the SessionStorageNamespace mojo
    // object, so clone immediately.
    kImmediate
  };

  enum class BackingMode {
    // Use an in-memory leveldb database to store our state.
    kNoDisk,
    // Use disk for the leveldb database, but clear its contents before we open
    // it. This is used for platforms like Android where the session restore
    // code is never used, ScavengeUnusedNamespace is never called, and old
    // session storage data will never be reused.
    kClearDiskStateOnOpen,
    // Use disk for the leveldb database, restore all saved namespaces from
    // disk. This assumes that ScavengeUnusedNamespace will eventually be called
    // to clean up unused namespaces on disk.
    kRestoreDiskState
  };

  SessionStorageContextMojo(
      const base::FilePath& partition_directory,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      scoped_refptr<base::SequencedTaskRunner> memory_dump_task_runner,
      BackingMode backing_option,
      std::string leveldb_name);

  void OpenSessionStorage(
      int process_id,
      const std::string& namespace_id,
      mojo::ReportBadMessageCallback bad_message_callback,
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver);

  void CreateSessionNamespace(const std::string& namespace_id);
  void CloneSessionNamespace(const std::string& namespace_id_to_clone,
                             const std::string& clone_namespace_id,
                             CloneType clone_type);

  // This function is called when the SessionStorageNamespaceImpl is destructed.
  // These generally map 1:1 to each open chrome tab/window, although they are
  // kept alive after the window is closed for restoring purposes.
  void DeleteSessionNamespace(const std::string& namespace_id,
                              bool should_persist);

  void Flush();

  void GetStorageUsage(GetStorageUsageCallback callback);

  void DeleteStorage(const url::Origin& origin,
                     const std::string& namespace_id,
                     base::OnceClosure callback);

  // Ensure that no traces of data are left in the backing storage.
  void PerformStorageCleanup(base::OnceClosure callback);

  // Called when the owning BrowserContext is ending. Schedules the commit of
  // any unsaved changes then deletes this object. All data on disk (where there
  // was no call to |DeleteSessionNamespace| will stay on disk for later
  // restoring.
  void ShutdownAndDelete();

  // Clears any caches, to free up as much memory as possible. Next access to
  // storage for a particular origin will reload the data from the database.
  void PurgeMemory();

  // Clears unused storage areas, when thresholds are reached.
  void PurgeUnusedAreasIfNeeded();

  // Any namespaces that have been loaded from disk and have not had a
  // corresponding CreateSessionNamespace() call will be deleted. Called after
  // startup. The calback is used for unittests, and is called after the
  // scavenging has finished (but not necessarily saved to disk). A null
  // callback is ok.
  void ScavengeUnusedNamespaces(base::OnceClosure done);

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void PretendToConnectForTesting();

  storage::AsyncDomStorageDatabase* DatabaseForTesting() {
    return database_.get();
  }

  void FlushAreaForTesting(const std::string& namespace_id,
                           const url::Origin& origin);

  // Access the underlying DomStorageDatabase. May be null if the database is
  // not yet open.
  const base::SequenceBound<storage::DomStorageDatabase>&
  GetDatabaseForTesting() const {
    return database_->database();
  }

  // Wait for the database to be opened, or for opening to fail. If the database
  // is already opened, |callback| is invoked immediately.
  void SetDatabaseOpenCallbackForTesting(base::OnceClosure callback);

 private:
  friend class DOMStorageBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(SessionStorageContextMojoTest,
                           PurgeMemoryDoesNotCrashOrHang);

  // These values are written to logs.  New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum class OpenResult {
    kDirectoryOpenFailed = 0,
    kDatabaseOpenFailed = 1,
    kInvalidVersion = 2,
    kVersionReadError = 3,
    kNamespacesReadError = 4,
    kSuccess = 6,
    kMaxValue = kSuccess
  };

  // Object deletion is done through |ShutdownAndDelete()|.
  ~SessionStorageContextMojo() override;

  scoped_refptr<SessionStorageMetadata::MapData> RegisterNewAreaMap(
      SessionStorageMetadata::NamespaceEntry namespace_entry,
      const url::Origin& origin);

  // SessionStorageAreaImpl::Listener implementation:
  void OnDataMapCreation(const std::vector<uint8_t>& map_prefix,
                         SessionStorageDataMap* map) override;
  void OnDataMapDestruction(const std::vector<uint8_t>& map_prefix) override;
  void OnCommitResult(leveldb::Status status) override;
  void OnCommitResultWithCallback(base::OnceClosure callback,
                                  leveldb::Status status);

  // SessionStorageNamespaceImplMojo::Delegate implementation:
  scoped_refptr<SessionStorageDataMap> MaybeGetExistingDataMapForId(
      const std::vector<uint8_t>& map_number_as_bytes) override;
  void RegisterShallowClonedNamespace(
      SessionStorageMetadata::NamespaceEntry source_namespace_entry,
      const std::string& new_namespace_id,
      const SessionStorageNamespaceImplMojo::OriginAreas& clone_from_areas)
      override;

  std::unique_ptr<SessionStorageNamespaceImplMojo>
  CreateSessionStorageNamespaceImplMojo(std::string namespace_id);

  void DoDatabaseDelete(const std::string& namespace_id);

  // Runs |callback| immediately if already connected to a database, otherwise
  // delays running |callback| untill after a connection has been established.
  // Initiates connecting to the database if no connection is in progress yet.
  void RunWhenConnected(base::OnceClosure callback);

  // Part of our asynchronous directory opening called from RunWhenConnected().
  void InitiateConnection(bool in_memory_only = false);
  void OnDatabaseOpened(leveldb::Status status);

  struct ValueAndStatus {
    ValueAndStatus();
    ValueAndStatus(ValueAndStatus&&);
    ~ValueAndStatus();
    leveldb::Status status;
    storage::DomStorageDatabase::Value value;
  };

  struct KeyValuePairsAndStatus {
    KeyValuePairsAndStatus();
    KeyValuePairsAndStatus(KeyValuePairsAndStatus&&);
    ~KeyValuePairsAndStatus();
    leveldb::Status status;
    std::vector<storage::DomStorageDatabase::KeyValuePair> key_value_pairs;
  };

  void OnGotDatabaseMetadata(ValueAndStatus version,
                             KeyValuePairsAndStatus namespaces,
                             ValueAndStatus next_map_id);

  struct MetadataParseResult {
    OpenResult open_result;
    const char* histogram_name;
  };
  MetadataParseResult ParseDatabaseVersion(
      ValueAndStatus version,
      std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>*
          migration_tasks);
  MetadataParseResult ParseNamespaces(
      KeyValuePairsAndStatus namespaces,
      std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>
          migration_tasks);
  MetadataParseResult ParseNextMapId(ValueAndStatus next_map_id);

  void OnConnectionFinished();
  void DeleteAndRecreateDatabase(const char* histogram_name);
  void OnDBDestroyed(bool recreate_in_memory, leveldb::Status status);

  void OnShutdownComplete(leveldb::Status status);

  void GetStatistics(size_t* total_cache_size, size_t* unused_areas_count);


  void LogDatabaseOpenResult(OpenResult result);

  // Since the session storage object hierarchy references iterators owned by
  // the metadata, make sure it is destroyed last on destruction.
  SessionStorageMetadata metadata_;

  BackingMode backing_mode_;
  std::string leveldb_name_;

  enum ConnectionState {
    NO_CONNECTION,
    CONNECTION_IN_PROGRESS,
    CONNECTION_FINISHED,
    CONNECTION_SHUTDOWN
  } connection_state_ = NO_CONNECTION;
  bool database_initialized_ = false;

  const base::FilePath partition_directory_;
  const scoped_refptr<base::SequencedTaskRunner> leveldb_task_runner_;

  base::trace_event::MemoryAllocatorDumpGuid memory_dump_id_;

  std::unique_ptr<storage::AsyncDomStorageDatabase> database_;
  bool in_memory_ = false;
  bool tried_to_recreate_during_open_ = false;

  std::vector<base::OnceClosure> on_database_opened_callbacks_;

  // The removal of items from this map is managed by the refcounting in
  // SessionStorageDataMap.
  // Populated after the database is connected.
  std::map<std::vector<uint8_t>, SessionStorageDataMap*> data_maps_;
  // Populated in CreateSessionNamespace, CloneSessionNamespace, and sometimes
  // RegisterShallowClonedNamespace. Items are removed in
  // DeleteSessionNamespace.
  std::map<std::string, std::unique_ptr<SessionStorageNamespaceImplMojo>>
      namespaces_;

  // Scavenging only happens once.
  bool has_scavenged_ = false;
  // When namespaces are destroyed but marked as persistent, a scavenge should
  // not delete them. Cleared after ScavengeUnusedNamespaces is called.
  std::set<std::string> protected_namespaces_from_scavenge_;

  bool is_low_end_device_;
  // Counts consecutive commit errors. If this number reaches a threshold, the
  // whole database is thrown away.
  int commit_error_count_ = 0;
  bool tried_to_recover_from_commit_errors_ = false;

  // Name of an extra histogram to log open results to, if not null.
  const char* open_result_histogram_ = nullptr;

  base::WeakPtrFactory<SessionStorageContextMojo> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_CONTEXT_MOJO_H_
