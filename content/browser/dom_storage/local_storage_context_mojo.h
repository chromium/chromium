// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_LOCAL_STORAGE_CONTEXT_MOJO_H_
#define CONTENT_BROWSER_DOM_STORAGE_LOCAL_STORAGE_CONTEXT_MOJO_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/leveldb/public/interfaces/leveldb.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "services/file/public/mojom/file_system.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

namespace service_manager {
class Connector;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace content {

class DOMStorageTaskRunner;
struct LocalStorageUsageInfo;

// Used for mojo-based LocalStorage implementation (can be disabled with
// --disable-mojo-local-storage for now).
// Created on the UI thread, but all further methods are called on the task
// runner passed to the constructor. Furthermore since destruction of this class
// can involve asynchronous steps, it can only be deleted by calling
// ShutdownAndDelete (on the correct task runner).
class CONTENT_EXPORT LocalStorageContextMojo
    : public base::trace_event::MemoryDumpProvider {
 public:
  using GetStorageUsageCallback =
      base::OnceCallback<void(std::vector<LocalStorageUsageInfo>)>;

  LocalStorageContextMojo(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      service_manager::Connector* connector,
      scoped_refptr<DOMStorageTaskRunner> legacy_task_runner,
      const base::FilePath& old_localstorage_path,
      const base::FilePath& subdirectory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);

  void OpenLocalStorage(const url::Origin& origin,
                        blink::mojom::StorageAreaRequest request);
  void GetStorageUsage(GetStorageUsageCallback callback);
  // |callback| is called when the deletion is sent to the database and
  // GetStorageUsage() will not return entries for |origin| anymore.
  void DeleteStorage(const url::Origin& origin, base::OnceClosure callback);
  // Ensure that no traces of deleted data are left in the backing storage.
  void PerformCleanup(base::OnceClosure callback);
  void Flush();
  void FlushOriginForTesting(const url::Origin& origin);

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState() { force_keep_session_state_ = true; }

  // Called when the owning BrowserContext is ending.
  // Schedules the commit of any unsaved changes and will delete
  // and keep data on disk per the content settings and special storage
  // policies.
  void ShutdownAndDelete();

  // Clears any caches, to free up as much memory as possible. Next access to
  // storage for a particular origin will reload the data from the database.
  void PurgeMemory();

  // Clears unused storage areas, when thresholds are reached.
  void PurgeUnusedAreasIfNeeded();

  void SetDatabaseForTesting(
      leveldb::mojom::LevelDBDatabaseAssociatedPtr database);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Converts a string from the old storage format to the new storage format.
  static std::vector<uint8_t> MigrateString(const base::string16& input);

 private:
  friend class DOMStorageBrowserTest;

  class StorageAreaHolder;

  ~LocalStorageContextMojo() override;

  // Runs |callback| immediately if already connected to a database, otherwise
  // delays running |callback| untill after a connection has been established.
  // Initiates connecting to the database if no connection is in progres yet.
  void RunWhenConnected(base::OnceClosure callback);

  // Part of our asynchronous directory opening called from RunWhenConnected().
  void InitiateConnection(bool in_memory_only = false);
  void OnDirectoryOpened(base::File::Error err);
  void OnDatabaseOpened(bool in_memory, leveldb::mojom::DatabaseError status);
  void OnGotDatabaseVersion(leveldb::mojom::DatabaseError status,
                            const std::vector<uint8_t>& value);
  void OnConnectionFinished();
  void DeleteAndRecreateDatabase(const char* histogram_name);
  void OnDBDestroyed(bool recreate_in_memory,
                     leveldb::mojom::DatabaseError status);
  void OnMojoConnectionDestroyed();

  // The (possibly delayed) implementation of OpenLocalStorage(). Can be called
  // directly from that function, or through |on_database_open_callbacks_|.
  void BindLocalStorage(const url::Origin& origin,
                        blink::mojom::StorageAreaRequest request);
  StorageAreaHolder* GetOrCreateStorageArea(const url::Origin& origin);

  // The (possibly delayed) implementation of GetStorageUsage(). Can be called
  // directly from that function, or through |on_database_open_callbacks_|.
  void RetrieveStorageUsage(GetStorageUsageCallback callback);
  void OnGotMetaData(GetStorageUsageCallback callback,
                     leveldb::mojom::DatabaseError status,
                     std::vector<leveldb::mojom::KeyValuePtr> data);

  void OnGotStorageUsageForShutdown(std::vector<LocalStorageUsageInfo> usage);
  void OnShutdownComplete(leveldb::mojom::DatabaseError error);

  void GetStatistics(size_t* total_cache_size, size_t* unused_area_count);
  void OnCommitResult(leveldb::mojom::DatabaseError error);

  // These values are written to logs.  New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum class OpenResult {
    DIRECTORY_OPEN_FAILED = 0,
    DATABASE_OPEN_FAILED = 1,
    INVALID_VERSION = 2,
    VERSION_READ_ERROR = 3,
    SUCCESS = 4,
    MAX
  };

  void LogDatabaseOpenResult(OpenResult result);

  std::unique_ptr<service_manager::Connector> connector_;
  const base::FilePath subdirectory_;

  enum ConnectionState {
    NO_CONNECTION,
    CONNECTION_IN_PROGRESS,
    CONNECTION_FINISHED,
    CONNECTION_SHUTDOWN
  } connection_state_ = NO_CONNECTION;
  bool database_initialized_ = false;

  bool force_keep_session_state_ = false;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  file::mojom::FileSystemPtr file_system_;
  filesystem::mojom::DirectoryPtr directory_;

  base::trace_event::MemoryAllocatorDumpGuid memory_dump_id_;

  leveldb::mojom::LevelDBServicePtr leveldb_service_;
  leveldb::mojom::LevelDBDatabaseAssociatedPtr database_;
  bool tried_to_recreate_during_open_ = false;

  std::vector<base::OnceClosure> on_database_opened_callbacks_;

  // Maps between an origin and its prefixed LevelDB view.
  std::map<url::Origin, std::unique_ptr<StorageAreaHolder>> areas_;

  // Used to access old data for migration.
  scoped_refptr<DOMStorageTaskRunner> task_runner_;
  base::FilePath old_localstorage_path_;

  bool is_low_end_device_;
  // Counts consecutive commit errors. If this number reaches a threshold, the
  // whole database is thrown away.
  int commit_error_count_ = 0;
  bool tried_to_recover_from_commit_errors_ = false;

  // Name of an extra histogram to log open results to, if not null.
  const char* open_result_histogram_ = nullptr;

  base::WeakPtrFactory<LocalStorageContextMojo> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_LOCAL_STORAGE_CONTEXT_MOJO_H_
