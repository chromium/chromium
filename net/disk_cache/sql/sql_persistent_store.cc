// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store.h"

#include <cstdint>
#include <limits>
#include <optional>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/system/sys_info.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "net/base/tracing.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace disk_cache {
namespace {

constexpr std::string_view kHistogramPrefix = "Net.SqlDiskCache.Backend.";

// Holds summary statistics about the cache store.
struct StoreStatus {
  int32_t entry_count = 0;
  int64_t total_size = 0;
};

// The result of a successful initialization.
struct InitResult {
  InitResult(const StoreStatus& store_status, int64_t max_bytes)
      : store_status(store_status), max_bytes(max_bytes) {}
  ~InitResult() = default;

  StoreStatus store_status;
  int64_t max_bytes = 0;
};

using Error = SqlPersistentStore::Error;
using InitResultOrError = base::expected<InitResult, Error>;

// Calculates the maximum size for a single cache entry's data.
int64_t CalculateMaxFileSize(int64_t max_bytes) {
  return std::max(base::saturated_cast<int64_t>(
                      max_bytes / kSqlBackendMaxFileRatioDenominator),
                  kSqlBackendMinFileSizeLimit);
}

// Helper functions to populate Perfetto trace events with details.
void PopulateTraceDetails(Error error, perfetto::TracedDictionary& dict) {
  dict.Add("error", static_cast<int>(error));
}
void PopulateTraceDetails(const StoreStatus& store_status,
                          perfetto::TracedDictionary& dict) {
  dict.Add("entry_count", store_status.entry_count);
  dict.Add("total_size", store_status.total_size);
}
void PopulateTraceDetails(Error error,
                          const StoreStatus& store_status,
                          perfetto::TracedDictionary& dict) {
  PopulateTraceDetails(error, dict);
  PopulateTraceDetails(store_status, dict);
}

// Records timing and result histograms for a backend method. This logs the
// method's duration to ".SuccessTime" or ".FailureTime" histograms and the
// `Error` code to a ".Result" histogram.
void RecordTimeAndErrorResultHistogram(std::string_view method_name,
                                       base::TimeDelta time_delta,
                                       Error error) {
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({kHistogramPrefix, method_name,
                    error == Error::kOk ? ".SuccessTime" : ".FailureTime"}),
      time_delta);
  base::UmaHistogramEnumeration(
      base::StrCat({kHistogramPrefix, method_name, ".Result"}), error);
}

// Sets up the database schema.
[[nodiscard]] bool InitSchema(sql::Database& db) {
  // The `resources` table stores the main metadata for each cache entry.
  static constexpr char kSqlCreateTableResources[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS resources("
          // Unique ID for the resource
          "res_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
          // High part of an unguessable token
          "token_high INTEGER NOT NULL,"
          // Low part of an unguessable token
          "token_low INTEGER NOT NULL,"
          // Timestamp for LRU
          "last_used INTEGER NOT NULL,"
          // End offset of the body
          "body_end INTEGER NOT NULL,"
          // Total bytes consumed by the entry
          "bytes_usage INTEGER NOT NULL,"
          // Flag for entries pending deletion
          "doomed INTEGER NOT NULL,"
          // The cache key created by HttpCache::GenerateCacheKeyForRequest()
          "cache_key TEXT NOT NULL,"
          // Serialized response headers
          "head BLOB)";
  // clang-format on

  // The `blobs` table stores the data chunks of the cached body.
  static constexpr char kSqlCreateTableBlobs[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS blobs("
        // Unique ID for the blob
        "blob_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
        // Foreign key to resources.token_high
        "token_high INTEGER NOT NULL,"
        // Foreign key to resources.token_low
        "token_low INTEGER NOT NULL,"
        // Start offset of this blob chunk
        "start INTEGER NOT NULL,"
        // End offset of this blob chunk
        "end INTEGER NOT NULL,"
        // The actual data chunk
        "blob BLOB NOT NULL)";
  // clang-format on

  if (!db.Execute(kSqlCreateTableResources) ||
      !db.Execute(kSqlCreateTableBlobs)) {
    return false;
  }
  // TODO(crbug.com/422065015): Create indexes for performance-critical columns.
  return true;
}

// Retrieves a value from the provided `sql::MetaTable` and initializes it if
// not found.
[[nodiscard]] bool GetOrInitializeMetaValue(sql::MetaTable& meta,
                                            std::string_view key,
                                            int64_t& value,
                                            int64_t default_value) {
  if (meta.GetValue(key, &value)) {
    return true;
  }
  value = default_value;
  return meta.SetValue(key, value);
}

// The `Backend` class encapsulates all direct interaction with the SQLite
// database. It is designed to be owned by a `base::SequenceBound` and run on a
// dedicated background sequence to avoid blocking the network IO thread.
class Backend {
 public:
  Backend(const base::FilePath& path, int64_t max_bytes, net::CacheType type)
      : path_(path),
        max_bytes_(
            // If the specified max_bytes is valid, use it. Otherwise, calculate
            // a preferred size based on available disk space.
            max_bytes > 0
                ? max_bytes
                : PreferredCacheSize(base::SysInfo::AmountOfFreeDiskSpace(path),
                                     type)),
        db_(sql::DatabaseOptions()
                .set_exclusive_locking(true)
#if BUILDFLAG(IS_WIN)
                .set_exclusive_database_file_lock(true)
#endif  // IS_WIN
                .set_preload(true)
                .set_wal_mode(true),
            // Tag for metrics collection.
            sql::Database::Tag("HttpCacheDiskCache")) {
  }

  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;
  ~Backend() = default;

  // Initializes the database, including setting up the schema and reading
  // metadata. Returns the cache status and max size on success.
  InitResultOrError Initialize();

  int32_t GetEntryCount() const { return store_status_.entry_count; }
  int64_t GetSizeOfAllEntries() const { return store_status_.total_size; }

 private:
  void DatabaseErrorCallback(int error, sql::Statement* statement);

  Error InitializeInternal();

  const base::FilePath path_;
  const int64_t max_bytes_;
  sql::Database db_;
  sql::MetaTable meta_table_;
  std::optional<Error> db_init_status_;
  StoreStatus store_status_;
};

InitResultOrError Backend::Initialize() {
  TRACE_EVENT_BEGIN0("disk_cache", "SqlBackend.Initialize");
  base::ElapsedTimer timer;
  CHECK(!db_init_status_.has_value());
  db_init_status_ = InitializeInternal();
  RecordTimeAndErrorResultHistogram("Initialize", timer.Elapsed(),
                                    *db_init_status_);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.Initialize", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(*db_init_status_, store_status_,
                                          dict);
                   });
  return *db_init_status_ == Error::kOk
             ? InitResultOrError(InitResult(store_status_, max_bytes_))
             : base::unexpected(*db_init_status_);
}

Error Backend::InitializeInternal() {
  CHECK(!db_init_status_.has_value());

  db_.set_error_callback(base::BindRepeating(&Backend::DatabaseErrorCallback,
                                             base::Unretained(this)));

  base::FilePath db_file_path = path_.Append(kSqlBackendDatabaseFileName);
  DVLOG(1) << "Backend::InitializeInternal db_file_path: " << db_file_path;

  base::FilePath directory = db_file_path.DirName();
  if (!base::DirectoryExists(directory) && !base::CreateDirectory(directory)) {
    return Error::kFailedToCreateDirectory;
  }

  if (!db_.Open(db_file_path)) {
    return Error::kFailedToOpenDatabase;
  }

  // Raze old incompatible databases.
  if (sql::MetaTable::RazeIfIncompatible(
          &db_, kSqlBackendLowestSupportedDatabaseVersion,
          kSqlBackendCurrentDatabaseVersion) ==
      sql::RazeIfIncompatibleResult::kFailed) {
    return Error::kFailedToRazeIncompatibleDatabase;
  }

  // Ensures atomicity of initialization: either all schema setup and metadata
  // writes succeed, or all are rolled back, preventing an inconsistent state.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  // Initialize the meta table, which stores version info and other metadata.
  if (!meta_table_.Init(&db_, kSqlBackendCurrentDatabaseVersion,
                        kSqlBackendCompatibleDatabaseVersion)) {
    return Error::kFailedToInitializeMetaTable;
  }

  // Initialize the database schema.
  if (!InitSchema(db_)) {
    return Error::kFailedToInitializeSchema;
  }

  int64_t tmp_entry_count = 0;
  if (!GetOrInitializeMetaValue(meta_table_, kSqlBackendMetaTableKeyEntryCount,
                                tmp_entry_count,
                                /*default_value=*/0)) {
    return Error::kFailedToSetEntryCountMetadata;
  }
  if (tmp_entry_count >= 0 &&
      tmp_entry_count <= std::numeric_limits<int32_t>::max()) {
    store_status_.entry_count = base::checked_cast<int32_t>(tmp_entry_count);
  } else {
    // TODO(crbug.com/422065015): Recalculate the entry count. And store into
    // the metadata table.
  }

  if (!GetOrInitializeMetaValue(meta_table_, kSqlBackendMetaTableKeyTotalSize,
                                store_status_.total_size,
                                /*default_value=*/0)) {
    return Error::kFailedToSetEntryCountMetadata;
  }
  if (store_status_.total_size < 0) {
    store_status_.total_size = 0;
    // TODO(crbug.com/422065015): Recalculate the total size. And store into the
    // metadata table.
  }

  if (!transaction.Commit()) {
    return Error::kFailedToCommitTransaction;
  }
  return Error::kOk;
}

void Backend::DatabaseErrorCallback(int error, sql::Statement* statement) {
  TRACE_EVENT("disk_cache", "SqlBackend.Error", "error", error);
  sql::UmaHistogramSqliteResult(base::StrCat({kHistogramPrefix, "SqliteError"}),
                                error);
  if (sql::IsErrorCatastrophic(error) && db_.is_open()) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndPoison() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    db_.RazeAndPoison();
  }
}

// `SqlPersistentStoreImpl` is the concrete implementation of the
// `SqlPersistentStore` interface. It serves as the bridge between the caller
// (on the main sequence = network IO thread) and the `Backend` (on the
// background sequence). It uses `base::SequenceBound` to safely manage the
// thread-hopping.
class SqlPersistentStoreImpl : public SqlPersistentStore {
 public:
  SqlPersistentStoreImpl(
      const base::FilePath& path,
      int64_t max_bytes,
      net::CacheType type,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
      : backend_(background_task_runner, path, max_bytes, type) {}
  ~SqlPersistentStoreImpl() override = default;

  // Kicks off the asynchronous initialization of the backend.
  void Initialize(ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::Initialize)
        .Then(base::BindOnce(
            [](base::WeakPtr<SqlPersistentStoreImpl> weak_ptr,
               ErrorCallback callback, InitResultOrError result) {
              if (weak_ptr) {
                if (result.has_value()) {
                  weak_ptr->SetMaxSize(result->max_bytes);
                }
                std::move(callback).Run(result.has_value() ? Error::kOk
                                                           : result.error());
              }
            },
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  int64_t MaxFileSize() const override { return max_file_size_; }
  int64_t MaxSize() const override { return max_size_; }
  void GetEntryCount(Int32Callback callback) const override {
    backend_.AsyncCall(&Backend::GetEntryCount).Then(std::move(callback));
  }
  void GetSizeOfAllEntries(Int64Callback callback) const override {
    backend_.AsyncCall(&Backend::GetSizeOfAllEntries).Then(std::move(callback));
  }

 private:
  void SetMaxSize(int64_t max_bytes) {
    max_size_ = max_bytes;
    max_file_size_ = CalculateMaxFileSize(max_bytes);
  }

  base::SequenceBound<Backend> backend_;

  int64_t max_size_ = 0;
  int64_t max_file_size_ = 0;

  base::WeakPtrFactory<SqlPersistentStoreImpl> weak_factory_{this};
};

}  // namespace

// Factory function for creating a `SqlPersistentStore` instance.
std::unique_ptr<SqlPersistentStore> SqlPersistentStore::Create(
    const base::FilePath& path,
    int64_t max_bytes,
    net::CacheType type,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner) {
  return std::make_unique<SqlPersistentStoreImpl>(path, max_bytes, type,
                                                  background_task_runner);
}
}  // namespace disk_cache
