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
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/system/sys_info.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/base/tracing.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code_values.h"
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
  explicit InitResult(int64_t max_bytes) : max_bytes(max_bytes) {}
  ~InitResult() = default;

  int64_t max_bytes = 0;
};

// A helper struct to associate an IOBuffer with a starting offset.
struct BufferWithStart {
  scoped_refptr<net::IOBuffer> buffer;
  int64_t start;
};

using EntryInfo = SqlPersistentStore::EntryInfo;
using Error = SqlPersistentStore::Error;
using EntryInfoOrError = SqlPersistentStore::EntryInfoOrError;
using OptionalEntryInfoOrError = SqlPersistentStore::OptionalEntryInfoOrError;
using EntryInfoWithIdAndKey = SqlPersistentStore::EntryInfoWithIdAndKey;
using OptionalEntryInfoWithIdAndKey =
    SqlPersistentStore::OptionalEntryInfoWithIdAndKey;
using IntOrError = SqlPersistentStore::IntOrError;

using InitResultOrError = base::expected<InitResult, Error>;

std::optional<base::UnguessableToken> ToUnguessableToken(int64_t token_high,
                                                         int64_t token_low) {
  // There is no `sql::Statement::ColumnUint64()` method. So we cast to
  // uint64_t.
  return base::UnguessableToken::Deserialize(static_cast<uint64_t>(token_high),
                                             static_cast<uint64_t>(token_low));
}

int64_t TokenHigh(const base::UnguessableToken& token) {
  return static_cast<int64_t>(token.GetHighForSerialization());
}
int64_t TokenLow(const base::UnguessableToken& token) {
  return static_cast<int64_t>(token.GetLowForSerialization());
}

bool IsBlobSizeValid(int64_t blob_start,
                     int64_t blob_end,
                     const base::span<const uint8_t>& blob) {
  size_t blob_size;
  if (!base::CheckSub(blob_end, blob_start).AssignIfValid(&blob_size)) {
    return false;
  }
  return blob.size() == blob_size;
}

// Calculates the maximum size for a single cache entry's data.
int64_t CalculateMaxFileSize(int64_t max_bytes) {
  return std::max(base::saturated_cast<int64_t>(
                      max_bytes / kSqlBackendMaxFileRatioDenominator),
                  kSqlBackendMinFileSizeLimit);
}

// Helper functions to populate Perfetto trace events with details.
void PopulateTraceDetails(int result, perfetto::TracedDictionary& dict) {
  dict.Add("result", result);
}
void PopulateTraceDetails(Error error, perfetto::TracedDictionary& dict) {
  dict.Add("error", static_cast<int>(error));
}
void PopulateTraceDetails(const StoreStatus& store_status,
                          perfetto::TracedDictionary& dict) {
  dict.Add("entry_count", store_status.entry_count);
  dict.Add("total_size", store_status.total_size);
}
void PopulateTraceDetails(const EntryInfo& entry_info,
                          perfetto::TracedDictionary& dict) {
  dict.Add("token", entry_info.token.ToString());
  dict.Add("last_used", entry_info.last_used);
  dict.Add("body_end", entry_info.body_end);
  dict.Add("head_size", entry_info.head ? entry_info.head->size() : 0);
  dict.Add("opened", entry_info.opened);
}
void PopulateTraceDetails(const std::optional<EntryInfo>& entry_info,
                          perfetto::TracedDictionary& dict) {
  if (entry_info) {
    PopulateTraceDetails(*entry_info, dict);
  } else {
    dict.Add("entry_info", "not found");
  }
}
void PopulateTraceDetails(const RangeResult& range_result,
                          perfetto::TracedDictionary& dict) {
  dict.Add("range_start", range_result.start);
  dict.Add("range_available_len", range_result.available_len);
}
void PopulateTraceDetails(const EntryInfoWithIdAndKey& result,
                          perfetto::TracedDictionary& dict) {
  PopulateTraceDetails(result.info, dict);
  dict.Add("res_id", result.res_id);
  dict.Add("key", result.key.string());
}
void PopulateTraceDetails(
    const std::optional<EntryInfoWithIdAndKey>& entry_info,
    perfetto::TracedDictionary& dict) {
  if (entry_info) {
    PopulateTraceDetails(*entry_info, dict);
  } else {
    dict.Add("entry_info", "not found");
  }
}
void PopulateTraceDetails(Error error,
                          const StoreStatus& store_status,
                          perfetto::TracedDictionary& dict) {
  PopulateTraceDetails(error, dict);
  PopulateTraceDetails(store_status, dict);
}
template <typename ResultType>
void PopulateTraceDetails(const base::expected<ResultType, Error>& result,
                          const StoreStatus& store_status,
                          perfetto::TracedDictionary& dict) {
  if (result.has_value()) {
    PopulateTraceDetails(*result, dict);
  } else {
    PopulateTraceDetails(result.error(), dict);
  }
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
  // TODO(crbug.com/422065015): Re-evaluate kSqlBackendStaticResourceSize after
  // adding indexes, as they increase storage overhead.
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
  int64_t GetSizeOfAllEntries() const {
    base::ClampedNumeric<int64_t> result;
    result = store_status_.entry_count;
    result *= kSqlBackendStaticResourceSize;
    result += store_status_.total_size;
    return result;
  }

  EntryInfoOrError OpenOrCreateEntry(const CacheEntryKey& key);
  OptionalEntryInfoOrError OpenEntry(const CacheEntryKey& key);
  EntryInfoOrError CreateEntry(const CacheEntryKey& key);
  Error DoomEntry(const CacheEntryKey& key,
                  const base::UnguessableToken& token);
  Error DeleteDoomedEntry(const CacheEntryKey& key,
                          const base::UnguessableToken& token);
  Error DeleteLiveEntry(const CacheEntryKey& key);
  Error DeleteAllEntries();
  Error DeleteLiveEntriesBetween(base::Time initial_time,
                                 base::Time end_time,
                                 std::set<CacheEntryKey> excluded_keys);
  Error UpdateEntryLastUsed(const CacheEntryKey& key, base::Time last_used);
  Error UpdateEntryHeaderAndLastUsed(const CacheEntryKey& key,
                                     const base::UnguessableToken& token,
                                     base::Time last_used,
                                     scoped_refptr<net::IOBuffer> buffer,
                                     int64_t header_size_delta);
  Error WriteEntryData(const CacheEntryKey& key,
                       const base::UnguessableToken& token,
                       int64_t old_body_end,
                       int64_t offset,
                       scoped_refptr<net::IOBuffer> buffer,
                       int buf_len,
                       bool truncate);
  IntOrError ReadEntryData(const base::UnguessableToken& token,
                           int64_t offset,
                           scoped_refptr<net::IOBuffer> buffer,
                           int buf_len,
                           int64_t body_end,
                           bool sparse_reading);
  RangeResult GetEntryAvailableRange(const base::UnguessableToken& token,
                                     int64_t offset,
                                     int len);
  int64_t CalculateSizeOfEntriesBetween(base::Time initial_time,
                                        base::Time end_time);
  OptionalEntryInfoWithIdAndKey OpenLatestEntryBeforeResId(
      int64_t res_id_cursor);

 private:
  void DatabaseErrorCallback(int error, sql::Statement* statement);

  Error InitializeInternal();
  EntryInfoOrError OpenOrCreateEntryInternal(const CacheEntryKey& key);
  OptionalEntryInfoOrError OpenEntryInternal(const CacheEntryKey& key);
  EntryInfoOrError CreateEntryInternal(const CacheEntryKey& key,
                                       bool run_existance_check);
  Error DoomEntryInternal(const CacheEntryKey& key,
                          const base::UnguessableToken& token,
                          bool& corruption_detected);
  Error DeleteDoomedEntryInternal(const CacheEntryKey& key,
                                  const base::UnguessableToken& token);
  Error DeleteLiveEntryInternal(const CacheEntryKey& key,
                                bool& corruption_detected);
  Error DeleteAllEntriesInternal();
  Error DeleteLiveEntriesBetweenInternal(base::Time initial_time,
                                         base::Time end_time,
                                         std::set<CacheEntryKey> excluded_keys,
                                         bool& corruption_detected);
  Error UpdateEntryLastUsedInternal(const CacheEntryKey& key,
                                    base::Time last_used);
  Error UpdateEntryHeaderAndLastUsedInternal(
      const CacheEntryKey& key,
      const base::UnguessableToken& token,
      base::Time last_used,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t header_size_delta);
  Error WriteEntryDataInternal(const CacheEntryKey& key,
                               const base::UnguessableToken& token,
                               int64_t old_body_end,
                               int64_t offset,
                               scoped_refptr<net::IOBuffer> buffer,
                               int buf_len,
                               bool truncate);
  IntOrError ReadEntryDataInternal(const base::UnguessableToken& token,
                                   int64_t offset,
                                   scoped_refptr<net::IOBuffer> buffer,
                                   int buf_len,
                                   int64_t body_end,
                                   bool sparse_reading);
  RangeResult GetEntryAvailableRangeInternal(
      const base::UnguessableToken& token,
      int64_t offset,
      int len);
  int64_t CalculateSizeOfEntriesBetweenInternal(base::Time initial_time,
                                                base::Time end_time);
  OptionalEntryInfoWithIdAndKey OpenLatestEntryBeforeResIdInternal(
      int64_t res_id_cursor,
      Error& error_out);

  // Trims blobs that overlap with the new write range [offset, end), and
  // updates the total size delta.
  Error TrimOverlappingBlobs(
      const base::UnguessableToken& token,
      int64_t offset,
      int64_t end,
      bool truncate,
      base::CheckedNumeric<int64_t>& checked_total_size_delta);
  // Truncates data by deleting all blobs that start at or after the given
  // offset.
  Error TruncateBlobsAfter(
      const base::UnguessableToken& token,
      int64_t truncate_offset,
      base::CheckedNumeric<int64_t>& checked_total_size_delta);
  // Inserts a vector of new blobs into the database, and updates the total size
  // delta.
  Error InsertNewBlobs(const base::UnguessableToken& token,
                       const std::vector<BufferWithStart>& new_blobs,
                       base::CheckedNumeric<int64_t>& checked_total_size_delta);
  // Inserts a single new blob into the database, and updates the total size
  // delta.
  Error InsertNewBlob(const base::UnguessableToken& token,
                      int64_t start,
                      const scoped_refptr<net::IOBuffer>& buffer,
                      int buf_len,
                      base::CheckedNumeric<int64_t>& checked_total_size_delta);
  // Deletes blobs by their IDs, and updates the total size delta.
  Error DeleteBlobsById(
      const std::vector<int64_t>& blob_ids_to_be_removed,
      base::CheckedNumeric<int64_t>& checked_total_size_delta);
  // Deletes a single blob by its ID, and updates the total size delta.
  Error DeleteBlobById(int64_t blob_id,
                       base::CheckedNumeric<int64_t>& checked_total_size_delta);
  // Deletes all blobs associated with a given token.
  Error DeleteBlobsByToken(const base::UnguessableToken& token);

  // Updates the in-memory `store_status_` by `entry_count_delta` and
  // `total_size_delta`. If the update results in an overflow or a negative
  // value, it recalculates the correct value from the database to recover from
  // potential metadata corruption.
  // It then updates the meta table values and attempts to commit the
  // `transaction`. Returns true on success, false on failure.
  bool UpdateStoreStatusAndCommitTransaction(sql::Transaction& transaction,
                                             int64_t entry_count_delta,
                                             int64_t total_size_delta);

  // Recalculates the store's status (entry count and total size) directly from
  // the database. This is a recovery mechanism used when metadata might be
  // inconsistent, e.g., after a numerical overflow.
  bool RecalculateStoreStatusAndCommitTransaction(
      sql::Transaction& transaction);

  int64_t CalculateResourceEntryCount();
  int64_t CalculateTotalSize();

  // A helper method for checking that the database initialization was
  // successful before proceeding with any database operations.
  void CheckDatabaseInitStatus() {
    CHECK(db_init_status_.has_value());
    CHECK_EQ(*db_init_status_, Error::kOk);
  }

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
             ? InitResultOrError(InitResult(max_bytes_))
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
      base::IsValueInRangeForNumericType<int32_t>(tmp_entry_count)) {
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

EntryInfoOrError Backend::OpenOrCreateEntry(const CacheEntryKey& key) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.OpenOrCreateEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  auto result = OpenOrCreateEntryInternal(key);
  RecordTimeAndErrorResultHistogram("OpenOrCreateEntry", timer.Elapsed(),
                                    result.error_or(Error::kOk));
  TRACE_EVENT_END1("disk_cache", "SqlBackend.OpenOrCreateEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

EntryInfoOrError Backend::OpenOrCreateEntryInternal(const CacheEntryKey& key) {
  // Try to open first.
  auto open_result = OpenEntryInternal(key);
  if (open_result.has_value() && open_result->has_value()) {
    return std::move(*open_result.value());
  }
  // If opening failed with an error, propagate that error.
  if (!open_result.has_value()) {
    return base::unexpected(open_result.error());
  }
  // If the entry was not found, try to create a new one.
  return CreateEntryInternal(key, /*run_existance_check=*/false);
}

OptionalEntryInfoOrError Backend::OpenEntry(const CacheEntryKey& key) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.OpenEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  auto result = OpenEntryInternal(key);
  RecordTimeAndErrorResultHistogram("OpenEntry", timer.Elapsed(),
                                    result.error_or(Error::kOk));
  TRACE_EVENT_END1("disk_cache", "SqlBackend.OpenEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

OptionalEntryInfoOrError Backend::OpenEntryInternal(const CacheEntryKey& key) {
  CheckDatabaseInitStatus();
  constexpr char kSqlSelectResources[] =
      // clang-format off
      "SELECT "
          "token_high,"  // 0
          "token_low,"   // 1
          "last_used,"   // 2
          "body_end,"    // 3
          "head "        // 4
      "FROM resources "
      "WHERE "
          "cache_key=? AND "  // 0
          "doomed=? "         // 1
      "ORDER BY res_id DESC";
  // clang-format on

  // Intentionally DCHECK() for performance
  DCHECK(db_.IsSQLValid(kSqlSelectResources));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectResources));
  statement.BindString(0, key.string());
  statement.BindBool(1, false);
  if (!statement.Step()) {
    // `Step()` returned false, which means either the query completed with no
    // results, or an error occurred.
    if (db_.GetErrorCode() == static_cast<int>(sql::SqliteResultCode::kDone)) {
      // The query completed successfully but found no matching entry.
      return std::nullopt;
    }
    // An unexpected database error occurred.
    return base::unexpected(Error::kFailedToExecute);
  }
  EntryInfo entry_info;
  // Reconstruct the UnguessableToken from its two 64-bit parts.
  auto maybe_token =
      ToUnguessableToken(statement.ColumnInt64(0), statement.ColumnInt64(1));
  if (!maybe_token) {
    // This indicates data corruption in the database.
    // TODO(crbug.com/422065015): If this error is observed in UMA, implement
    // recovery logic.
    return base::unexpected(Error::kInvalidData);
  }
  entry_info.token = *maybe_token;
  entry_info.last_used = statement.ColumnTime(2);
  entry_info.body_end = statement.ColumnInt64(3);
  base::span<const uint8_t> blob_span = statement.ColumnBlob(4);
  entry_info.head = base::MakeRefCounted<net::GrowableIOBuffer>();
  CHECK(base::IsValueInRangeForNumericType<int>(blob_span.size()));
  entry_info.head->SetCapacity(blob_span.size());
  entry_info.head->span().copy_from_nonoverlapping(blob_span);
  entry_info.opened = true;
  return entry_info;
}

EntryInfoOrError Backend::CreateEntry(const CacheEntryKey& key) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.CreateEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  auto result = CreateEntryInternal(key, /*run_existance_check=*/true);
  RecordTimeAndErrorResultHistogram("CreateEntry", timer.Elapsed(),
                                    result.error_or(Error::kOk));
  TRACE_EVENT_END1("disk_cache", "SqlBackend.CreateEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

EntryInfoOrError Backend::CreateEntryInternal(const CacheEntryKey& key,
                                              bool run_existance_check) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }
  if (run_existance_check) {
    auto open_result = OpenEntryInternal(key);
    if (open_result.has_value() && open_result->has_value()) {
      return base::unexpected(Error::kAlreadyExists);
    }
    // If opening failed with an error, propagate that error.
    if (!open_result.has_value()) {
      return base::unexpected(open_result.error());
    }
  }
  EntryInfo entry_info;
  entry_info.token = base::UnguessableToken::Create();
  entry_info.last_used = base::Time::Now();
  entry_info.body_end = 0;
  entry_info.head = nullptr;
  entry_info.opened = false;
  // The size of an entry is set to the size of its key. This value will be
  // updated as the header and body are written.
  // The static size per entry, `kSqlBackendStaticResourceSize`, is added in
  // `GetSizeOfAllEntries()`.
  const int64_t bytes_usage = key.string().size();
  {
    constexpr char kSqlInsertIntoResources[] =
        // clang-format off
        "INSERT INTO resources("
            "token_high,"   // 0
            "token_low,"    // 1
            "last_used,"    // 2
            "body_end,"     // 3
            "bytes_usage,"  // 4
            "doomed,"       // 5
            "cache_key) "   // 6
        "VALUES(?,?,?,?,?,?,?)";
    // clang-format on

    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlInsertIntoResources));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlInsertIntoResources));
    statement.BindInt64(0, TokenHigh(entry_info.token));
    statement.BindInt64(1, TokenLow(entry_info.token));
    statement.BindTime(2, entry_info.last_used);
    statement.BindInt64(3, entry_info.body_end);
    statement.BindInt64(4, bytes_usage);
    statement.BindBool(5, false);  // doomed
    statement.BindString(6, key.string());
    if (!statement.Run()) {
      return base::unexpected(Error::kFailedToExecute);
    }
  }

  // Update the store's status and commit the transaction.
  // The entry count is increased by 1, and the total size by `bytes_usage`.
  // This call will also handle updating the on-disk meta table.
  if (!UpdateStoreStatusAndCommitTransaction(
          transaction,
          /*entry_count_delta=*/1,
          /*total_size_delta=*/bytes_usage)) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }

  return entry_info;
}

Error Backend::DoomEntry(const CacheEntryKey& key,
                         const base::UnguessableToken& token) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DoomEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       dict.Add("token", token.ToString());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = DoomEntryInternal(key, token, corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "DoomEntry", timer.Elapsed(),
      corruption_detected ? Error::kInvalidData : result);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DoomEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                     dict.Add("corruption_detected", corruption_detected);
                   });
  return result;
}

Error Backend::DoomEntryInternal(const CacheEntryKey& key,
                                 const base::UnguessableToken& token,
                                 bool& corruption_detected) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  int64_t doomed_count = 0;
  // Use checked numerics to safely calculate the change in total size and
  // detect potential metadata corruption from overflows.
  base::CheckedNumeric<int64_t> total_size_delta = 0;
  {
    constexpr char kSqlMarkDoomedResources[] =
        // clang-format off
        "UPDATE resources "
        "SET "
          "doomed=? "          // 0
        "WHERE "
          "cache_key=? AND "   // 1
          "token_high=? AND "  // 2
          "token_low=? AND "   // 3
          "doomed=? "          // 4
        "RETURNING "
          "bytes_usage";       // 0
    // clang-format on

    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlMarkDoomedResources));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlMarkDoomedResources));
    // Set the new value: doomed = true.
    statement.BindBool(0, true);
    statement.BindString(1, key.string());
    statement.BindInt64(2, TokenHigh(token));
    statement.BindInt64(3, TokenLow(token));
    // Set the current value to match: doomed = false.
    statement.BindBool(4, false);
    // Iterate through the rows returned by the RETURNING clause.
    while (statement.Step()) {
      // Since we're dooming an entry, its size is subtracted from the total.
      total_size_delta -= statement.ColumnInt64(0);
      // Count how many entries were actually updated.
      ++doomed_count;
    }
  }

  if (doomed_count > 1) {
    // TODO(crbug.com/422065015): Add histograms to track how often this
    // unexpected case is reached. A cache_key and token combination should
    // uniquely identify a single non-doomed entry.
  }

  // If no rows were updated, it means the entry was not found (or the token
  // was wrong), so we report kNotFound.
  if (doomed_count == 0) {
    return transaction.Commit() ? Error::kNotFound
                                : Error::kFailedToCommitTransaction;
  }

  // If the `total_size_delta` calculation resulted in an overflow, it suggests
  // that the `bytes_usage` value in the database was corrupt. In this case, we
  // trigger a full recalculation of the store's status to recover to a
  // consistent state.
  if (!total_size_delta.IsValid()) {
    corruption_detected = true;
    return RecalculateStoreStatusAndCommitTransaction(transaction)
               ? Error::kOk
               : Error::kFailedToCommitTransaction;
  }

  if (!UpdateStoreStatusAndCommitTransaction(
          transaction,
          /*entry_count_delta=*/-doomed_count,
          /*total_size_delta=*/total_size_delta.ValueOrDie())) {
    return Error::kFailedToCommitTransaction;
  }

  return Error::kOk;
}

Error Backend::DeleteDoomedEntry(const CacheEntryKey& key,
                                 const base::UnguessableToken& token) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteDoomedEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       dict.Add("token", token.ToString());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  auto result = DeleteDoomedEntryInternal(key, token);
  RecordTimeAndErrorResultHistogram("DeleteDoomedEntry", timer.Elapsed(),
                                    result);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteDoomedEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

Error Backend::DeleteDoomedEntryInternal(const CacheEntryKey& key,
                                         const base::UnguessableToken& token) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  int64_t deleted_count = 0;
  {
    constexpr char kSqlDeleteFromResources[] =
        // clang-format off
        "DELETE FROM resources "
        "WHERE "
          "cache_key=? AND "   // 0
          "token_high=? AND "  // 1
          "token_low=? AND "   // 2
          "doomed=?";          // 3
    // clang-format on

    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlDeleteFromResources));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromResources));
    statement.BindString(0, key.string());
    statement.BindInt64(1, TokenHigh(token));
    statement.BindInt64(2, TokenLow(token));
    // Target rows where doomed = true.
    statement.BindBool(3, true);
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
    deleted_count = db_.GetLastChangeCount();
  }

  if (deleted_count > 1) {
    // TODO(crbug.com/422065015): Add histograms to track how often this
    // unexpected case is reached. A cache_key and token combination should
    // uniquely identify a single doomed entry.
  }

  // If we didn't find any doomed entry matching the key and token, report it.
  if (deleted_count == 0) {
    return transaction.Commit() ? Error::kNotFound
                                : Error::kFailedToCommitTransaction;
  }

  // Delete the associated blobs from the `blobs` table.
  if (Error error = DeleteBlobsByToken(token); error != Error::kOk) {
    return error;
  }

  return transaction.Commit() ? Error::kOk : Error::kFailedToExecute;
}

Error Backend::DeleteLiveEntry(const CacheEntryKey& key) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteLiveEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = DeleteLiveEntryInternal(key, corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "DeleteLiveEntry", timer.Elapsed(),
      corruption_detected ? Error::kInvalidData : result);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteLiveEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                     dict.Add("corruption_detected", corruption_detected);
                   });
  return result;
}

Error Backend::DeleteLiveEntryInternal(const CacheEntryKey& key,
                                       bool& corruption_detected) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  // We need to collect the tokens of deleted entries to later remove their
  // corresponding data from the `blobs` table.
  std::vector<base::UnguessableToken> tokens_to_be_deleted;
  // Use checked numerics to safely update the total cache size.
  base::CheckedNumeric<int64_t> total_size_delta = 0;
  int64_t deleted_count = 0;
  {
    constexpr char kSqlDeleteFromResources[] =
        // clang-format off
        "DELETE FROM resources "
        "WHERE "
          "cache_key=? AND "  // 0
          "doomed=? "         // 1
        "RETURNING "
          "token_high,"       // 0
          "token_low,"        // 1
          "bytes_usage";      // 2
    // clang-format on
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlDeleteFromResources));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromResources));
    statement.BindString(0, key.string());
    // Target rows where doomed = false.
    statement.BindBool(1, false);
    while (statement.Step()) {
      ++deleted_count;
      auto maybe_token = ToUnguessableToken(statement.ColumnInt64(0),
                                            statement.ColumnInt64(1));
      // If deserializing the token fails, it's a sign of data corruption.
      if (!maybe_token) {
        corruption_detected = true;
        continue;
      }
      // The size of the deleted entry is subtracted from the total.
      total_size_delta -= statement.ColumnInt64(2);
      tokens_to_be_deleted.emplace_back(*maybe_token);
    }
  }

  // If no entries were deleted, the key wasn't found.
  if (deleted_count == 0) {
    return transaction.Commit() ? Error::kNotFound
                                : Error::kFailedToCommitTransaction;
  }

  // Delete the blobs associated with the deleted entries.
  for (const auto& token_to_be_deleted : tokens_to_be_deleted) {
    if (Error delete_result = DeleteBlobsByToken(token_to_be_deleted);
        delete_result != Error::kOk) {
      return delete_result;
    }
  }

  // If we detected corruption, or if the size update calculation overflowed,
  // our metadata is suspect. We recover by recalculating everything from
  // scratch.
  if (corruption_detected || !total_size_delta.IsValid()) {
    corruption_detected = true;
    return RecalculateStoreStatusAndCommitTransaction(transaction)
               ? Error::kOk
               : Error::kFailedToCommitTransaction;
  }

  if (!UpdateStoreStatusAndCommitTransaction(
          transaction,
          /*entry_count_delta=*/
          -static_cast<int64_t>(tokens_to_be_deleted.size()),
          /*total_size_delta=*/total_size_delta.ValueOrDie())) {
    return Error::kFailedToCommitTransaction;
  }

  return Error::kOk;
}

Error Backend::DeleteAllEntries() {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteAllEntries", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  Error result = DeleteAllEntriesInternal();
  RecordTimeAndErrorResultHistogram("DeleteAllEntries", timer.Elapsed(),
                                    result);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteAllEntries", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

Error Backend::DeleteAllEntriesInternal() {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  // Clear the main resources table.
  {
    constexpr char kSqlDeleteFromResources[] = "DELETE FROM resources";
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlDeleteFromResources));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromResources));
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
  }

  // Also clear the blobs table.
  {
    constexpr char kSqlDeleteFromBlobs[] = "DELETE FROM blobs";
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlDeleteFromBlobs));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromBlobs));
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
  }

  // Update the store's status and commit the transaction.
  // The entry count and the total size will be zero.
  // This call will also handle updating the on-disk meta table.
  if (!UpdateStoreStatusAndCommitTransaction(
          transaction,
          /*entry_count_delta=*/-store_status_.entry_count,
          /*total_size_delta=*/-store_status_.total_size)) {
    return Error::kFailedToCommitTransaction;
  }
  return Error::kOk;
}

Error Backend::DeleteLiveEntriesBetween(base::Time initial_time,
                                        base::Time end_time,
                                        std::set<CacheEntryKey> excluded_keys) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteLiveEntriesBetween",
                     "data", [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("initial_time", initial_time);
                       dict.Add("end_time", end_time);
                       dict.Add("excluded_keys_size", excluded_keys.size());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  // Flag to indicate if we encounter signs of database corruption. In
  // DeleteLiveEntriesBetween, database corruption is ignored.
  bool corruption_detected = false;
  Error result = DeleteLiveEntriesBetweenInternal(
      initial_time, end_time, std::move(excluded_keys), corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "DeleteLiveEntriesBetween", timer.Elapsed(),
      corruption_detected ? Error::kInvalidData : result);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteLiveEntriesBetween",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

Error Backend::DeleteLiveEntriesBetweenInternal(
    base::Time initial_time,
    base::Time end_time,
    std::set<CacheEntryKey> excluded_keys,
    bool& corruption_detected) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  std::vector<int64_t> res_ids_to_be_deleted;
  std::vector<base::UnguessableToken> tokens_to_be_deleted;
  int64_t entry_count_delta = 0;
  base::CheckedNumeric<int64_t> total_size_delta = 0;
  {
    constexpr char kSqlSelectResourcesForEviction[] =
        // clang-format off
        "SELECT "
          "res_id,"       // 0
          "token_high,"   // 1
          "token_low,"    // 2
          "bytes_usage,"  // 3
          "cache_key "    // 4
        "FROM resources "
        "WHERE "
          "last_used>=? AND "  // 0
          "last_used<? AND "   // 1
          "doomed=?";          // 2
    // clang-format on
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlSelectResourcesForEviction));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectResourcesForEviction));
    statement.BindTime(0, initial_time);
    statement.BindTime(1, end_time);
    statement.BindBool(2, false);
    while (statement.Step()) {
      if (excluded_keys.contains(CacheEntryKey(statement.ColumnString(4)))) {
        continue;
      }
      --entry_count_delta;
      res_ids_to_be_deleted.push_back(statement.ColumnInt64(0));
      auto maybe_token = ToUnguessableToken(statement.ColumnInt64(1),
                                            statement.ColumnInt64(2));
      if (maybe_token) {
        tokens_to_be_deleted.push_back(*maybe_token);
        total_size_delta -= statement.ColumnInt64(3);
      } else {
        corruption_detected = true;
      }
    }
  }

  // Delete the blobs associated with the entries to be deleted.
  for (const auto& token_to_be_deleted : tokens_to_be_deleted) {
    Error delete_result = DeleteBlobsByToken(token_to_be_deleted);
    if (delete_result != Error::kOk) {
      return delete_result;
    }
  }

  // Delete the selected entries from the `resources` table.
  for (const auto& res_id : res_ids_to_be_deleted) {
    constexpr char kSqlDeleteFromResources[] =
        "DELETE FROM resources WHERE res_id=?";
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlDeleteFromResources));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromResources));
    statement.BindInt64(0, res_id);
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
  }

  // If we detected corruption, or if the size update calculation overflowed,
  // our metadata is suspect. We recover by recalculating everything from
  // scratch.
  if (corruption_detected || !total_size_delta.IsValid()) {
    corruption_detected = true;
    return RecalculateStoreStatusAndCommitTransaction(transaction)
               ? Error::kOk
               : Error::kFailedToCommitTransaction;
  }

  // Update the in-memory and on-disk store status (entry count and total size)
  // and commit the transaction.
  if (!UpdateStoreStatusAndCommitTransaction(transaction, entry_count_delta,
                                             total_size_delta.ValueOrDie())) {
    return Error::kFailedToCommitTransaction;
  }

  return Error::kOk;
}

Error Backend::UpdateEntryLastUsed(const CacheEntryKey& key,
                                   base::Time last_used) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.UpdateEntryLastUsed", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       dict.Add("last_used", last_used);
                     });
  base::ElapsedTimer timer;
  auto result = UpdateEntryLastUsedInternal(key, last_used);
  RecordTimeAndErrorResultHistogram("UpdateEntryLastUsed", timer.Elapsed(),
                                    result);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.UpdateEntryLastUsed", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, dict);
                   });
  return result;
}

Error Backend::UpdateEntryLastUsedInternal(const CacheEntryKey& key,
                                           base::Time last_used) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }
  int64_t change_count = 0;
  {
    constexpr char kSqlUpdateResourceLastUsed[] =
        // clang-format off
        "UPDATE resources "
        "SET "
          "last_used=? "      // 0
        "WHERE "
          "cache_key=? AND "  // 1
          "doomed=?";         // 2
    // clang-format on
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlUpdateResourceLastUsed));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlUpdateResourceLastUsed));
    statement.BindTime(0, last_used);
    statement.BindString(1, key.string());
    statement.BindBool(2, false);  // doomed
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
    change_count = db_.GetLastChangeCount();
  }
  if (!transaction.Commit()) {
    return Error::kFailedToCommitTransaction;
  }
  return change_count == 0 ? Error::kNotFound : Error::kOk;
}

Error Backend::UpdateEntryHeaderAndLastUsed(const CacheEntryKey& key,
                                            const base::UnguessableToken& token,
                                            base::Time last_used,
                                            scoped_refptr<net::IOBuffer> buffer,
                                            int64_t header_size_delta) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.UpdateEntryHeaderAndLastUsed",
                     "data", [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       dict.Add("token", token.ToString());
                       dict.Add("last_used", last_used);
                       dict.Add("header_size_delta", header_size_delta);
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  auto result = UpdateEntryHeaderAndLastUsedInternal(
      key, token, last_used, std::move(buffer), header_size_delta);
  RecordTimeAndErrorResultHistogram("UpdateEntryHeaderAndLastUsed",
                                    timer.Elapsed(), result);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.UpdateEntryHeaderAndLastUsed",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

Error Backend::UpdateEntryHeaderAndLastUsedInternal(
    const CacheEntryKey& key,
    const base::UnguessableToken& token,
    base::Time last_used,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t header_size_delta) {
  CHECK(buffer);
  CheckDatabaseInitStatus();

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }
  {
    constexpr char kSqlUpdateResourceBodySize[] =
        // clang-format off
        "UPDATE resources "
        "SET "
          "last_used=?, "                // 0
          "bytes_usage=bytes_usage+?, "  // 1
          "head=? "                      // 2
        "WHERE "
          "cache_key=? AND "             // 3
          "token_high=? AND "            // 4
          "token_low=? AND "             // 5
          "doomed=? "                    // 6
        "RETURNING "
          "bytes_usage";                 // 0
    // clang-format on
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlUpdateResourceBodySize));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlUpdateResourceBodySize));
    statement.BindTime(0, last_used);
    statement.BindInt64(1, header_size_delta);
    statement.BindBlob(2, buffer->span());
    statement.BindString(3, key.string());
    statement.BindInt64(4, TokenHigh(token));
    statement.BindInt64(5, TokenLow(token));
    statement.BindBool(6, false);  // doomed
    if (statement.Step()) {
      const int64_t bytes_usage = statement.ColumnInt64(0);
      if (bytes_usage < static_cast<int64_t>(buffer->size()) +
                            static_cast<int64_t>(key.string().size())) {
        // This indicates data corruption in the database.
        // TODO(crbug.com/422065015): If this error is observed in UMA,
        // implement recovery logic.
        return Error::kInvalidData;
      }
    } else {
      return Error::kNotFound;
    }
  }
  if (!UpdateStoreStatusAndCommitTransaction(
          transaction,
          /*entry_count_delta=*/0,
          /*total_size_delta=*/header_size_delta)) {
    return Error::kFailedToCommitTransaction;
  }

  return Error::kOk;
}

Error Backend::WriteEntryData(const CacheEntryKey& key,
                              const base::UnguessableToken& token,
                              int64_t old_body_end,
                              int64_t offset,
                              scoped_refptr<net::IOBuffer> buffer,
                              int buf_len,
                              bool truncate) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.WriteEntryData", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       dict.Add("token", token.ToString());
                       dict.Add("old_body_end", old_body_end);
                       dict.Add("offset", offset);
                       dict.Add("buf_len", buf_len);
                       dict.Add("truncate", truncate);
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  auto result = WriteEntryDataInternal(key, token, old_body_end, offset,
                                       std::move(buffer), buf_len, truncate);
  RecordTimeAndErrorResultHistogram("WriteEntryData", timer.Elapsed(), result);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.WriteEntryData", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

Error Backend::WriteEntryDataInternal(const CacheEntryKey& key,
                                      const base::UnguessableToken& token,
                                      int64_t old_body_end,
                                      int64_t offset,
                                      scoped_refptr<net::IOBuffer> buffer,
                                      int buf_len,
                                      bool truncate) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  int64_t write_end;
  if (old_body_end < 0 || offset < 0 || buf_len < 0 ||
      (!buffer && buf_len > 0) || (buffer && buf_len > buffer->size()) ||
      !base::CheckAdd<int64_t>(offset, buf_len).AssignIfValid(&write_end)) {
    return Error::kInvalidArgument;
  }

  const int64_t new_body_end =
      truncate ? write_end : std::max(write_end, old_body_end);
  // An overflow is not expected here, as both `new_body_end` and `old_body_end`
  // are non-negative int64_t value.
  const int64_t body_end_delta = new_body_end - old_body_end;

  base::CheckedNumeric<int64_t> checked_total_size_delta = 0;

  // If the write starts before the current end of the body, it might overlap
  // with existing data.
  if (offset < old_body_end) {
    if (Error result = TrimOverlappingBlobs(token, offset, write_end, truncate,
                                            checked_total_size_delta);
        result != Error::kOk) {
      return result;
    }
  }

  // If the new body size is smaller, existing blobs beyond the new end must be
  // truncated.
  if (body_end_delta < 0) {
    CHECK(truncate);
    if (Error result =
            TruncateBlobsAfter(token, new_body_end, checked_total_size_delta);
        result != Error::kOk) {
      return result;
    }
  }

  // Insert the new data blob if there is data to write.
  if (buf_len) {
    if (Error result = InsertNewBlob(token, offset, buffer, buf_len,
                                     checked_total_size_delta);
        result != Error::kOk) {
      return result;
    }
  }

  if (!checked_total_size_delta.IsValid()) {
    // If the total size delta calculation resulted in an overflow, it suggests
    // that the size values in the database were corrupt.
    return Error::kInvalidData;
  }
  int64_t total_size_delta = checked_total_size_delta.ValueOrDie();

  // Update the entry's metadata in the `resources` table if the body size
  // changed or if the total size of blobs changed.
  if (body_end_delta || total_size_delta) {
    constexpr char kSqlUpdateResourceBodySize[] =
        // clang-format off
        "UPDATE resources "
        "SET "
            "body_end=body_end+?, "       // 0
            "bytes_usage=bytes_usage+? "  // 1
        "WHERE "
            "cache_key=? AND "            // 2
            "token_high=? AND "           // 3
            "token_low=? "                // 4
        "RETURNING "
            "body_end,"                   // 0
            "doomed";                     // 1
    // clang-format on
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlUpdateResourceBodySize));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlUpdateResourceBodySize));
    statement.BindInt64(0, body_end_delta);
    statement.BindInt64(1, total_size_delta);
    statement.BindString(2, key.string());
    statement.BindInt64(3, TokenHigh(token));
    statement.BindInt64(4, TokenLow(token));
    if (statement.Step()) {
      // Consistency check: The `RETURNING` clause gives us the `body_end` value
      // after the update. If this doesn't match our calculated `new_body_end`,
      // it means the `body_end` in the database was not the `old_body_end` we
      // expected. This indicates data corruption, so we return an error.
      const int64_t returned_new_body_end = statement.ColumnInt64(0);
      if (returned_new_body_end != new_body_end) {
        return Error::kBodyEndMismatch;
      }
      // If the entry is doomed, its size is no longer tracked in the cache's
      // total size, so we don't update the store status.
      const bool doomed = statement.ColumnBool(1);
      if (doomed) {
        total_size_delta = 0;
      }
    } else {
      // If no rows were updated, it means the entry was not found (or the
      // token was wrong).
      return Error::kNotFound;
    }
  }

  // Commit the transaction, which also updates the in-memory and on-disk store
  // status.
  if (!UpdateStoreStatusAndCommitTransaction(
          transaction,
          /*entry_count_delta=*/0,
          /*total_size_delta=*/total_size_delta)) {
    return Error::kFailedToCommitTransaction;
  }
  return Error::kOk;
}

// This function handles writes that overlap with existing data blobs. It finds
// any blobs that intersect with the new write range `[offset, end)`, removes
// them, and recreates any non-overlapping portions as new, smaller blobs. This
// effectively "cuts out" the space for the new data.
Error Backend::TrimOverlappingBlobs(
    const base::UnguessableToken& token,
    int64_t offset,
    int64_t end,
    bool truncate,
    base::CheckedNumeric<int64_t>& checked_total_size_delta) {
  TRACE_EVENT1("disk_cache", "SqlBackend.TrimOverlappingBlobs", "data",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("token", token.ToString());
                 dict.Add("offset", offset);
                 dict.Add("end", end);
               });

  // First, delete all blobs that are fully contained within the new write
  // range.
  // If the write has zero length, no blobs can be fully contained within it, so
  // this can be skipped.
  if (offset != end) {
    constexpr char kSqlDeleteContained[] =
        // clang-format off
        "DELETE FROM blobs "
        "WHERE "
          "token_high=? AND "  // 0
          "token_low=? AND "   // 1
          "start>=? AND "      // 2
          "end<=? "            // 3
        "RETURNING "
          "start,"             // 0
          "end";               // 1
    // clang-format on
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlDeleteContained));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteContained));
    statement.BindInt64(0, TokenHigh(token));
    statement.BindInt64(1, TokenLow(token));
    statement.BindInt64(2, offset);
    statement.BindInt64(3, end);
    while (statement.Step()) {
      const int64_t blob_start = statement.ColumnInt64(0);
      const int64_t blob_end = statement.ColumnInt64(1);
      checked_total_size_delta -= blob_end - blob_start;
    }
  }

  // Now, handle blobs that partially overlap with the write range. There should
  // be at most two such blobs.
  // The SQL condition `blob_start < end AND blob_end > offset` checks for
  // overlap. Example of [offset, end) vs [blob_start, blob_end):
  //   [0, 2) vs [2, 6): Not hit.
  //   [0, 3) vs [2, 6): Hit.
  //   [5, 9) vs [2, 6): Hit.
  //   [6, 9) vs [2, 6): Not hit.
  std::vector<int64_t> blob_ids_to_be_removed;
  std::vector<BufferWithStart> new_blobs;
  // A zero-length, non-truncating write is a no-op. For all other writes, we
  // must handle partially overlapping blobs.
  if (!(offset == end && !truncate)) {
    constexpr char kSqlSelectOverlapping[] =
        // clang-format off
      "SELECT "
          "blob_id,"           // 0
          "start,"             // 1
          "end,"               // 2
          "blob "              // 3
      "FROM blobs "
      "WHERE "
          "token_high=? AND "  // 0
          "token_low=? AND "   // 1
          "start<? AND "       // 2
          "end>?";             // 3
    // clang-format on
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlSelectOverlapping));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectOverlapping));
    statement.BindInt64(0, TokenHigh(token));
    statement.BindInt64(1, TokenLow(token));
    statement.BindInt64(2, end);
    statement.BindInt64(3, offset);
    while (statement.Step()) {
      const int64_t blob_id = statement.ColumnInt64(0);
      const int64_t blob_start = statement.ColumnInt64(1);
      const int64_t blob_end = statement.ColumnInt64(2);
      base::span<const uint8_t> blob = statement.ColumnBlob(3);
      // Consistency check: The blob's size should match its start and end
      // offsets.
      if (!IsBlobSizeValid(blob_start, blob_end, blob)) {
        return Error::kInvalidData;
      }
      // Mark the overlapping blob for removal.
      blob_ids_to_be_removed.push_back(blob_id);
      // If the existing blob starts before the new write, create a new blob
      // for the leading part that doesn't overlap.
      if (blob_start < offset) {
        new_blobs.push_back(BufferWithStart{
            base::MakeRefCounted<net::VectorIOBuffer>(
                blob.first(base::checked_cast<size_t>(offset - blob_start))),
            blob_start});
      }
      // If the existing blob ends after the new write and we are not
      // truncating, create a new blob for the trailing part that doesn't
      // overlap.
      if (!truncate && end < blob_end) {
        new_blobs.push_back(BufferWithStart{
            base::MakeRefCounted<net::VectorIOBuffer>(
                blob.last(base::checked_cast<size_t>(blob_end - end))),
            end});
      }
    }
  }

  // Delete the old blobs.
  if (Error error =
          DeleteBlobsById(blob_ids_to_be_removed, checked_total_size_delta);
      error != Error::kOk) {
    return error;
  }

  // Insert the new, smaller blobs that were preserved from the non-overlapping
  // parts.
  if (Error error = InsertNewBlobs(token, new_blobs, checked_total_size_delta);
      error != Error::kOk) {
    return error;
  }
  return Error::kOk;
}

Error Backend::TruncateBlobsAfter(
    const base::UnguessableToken& token,
    int64_t truncate_offset,
    base::CheckedNumeric<int64_t>& checked_total_size_delta) {
  TRACE_EVENT1("disk_cache", "SqlBackend.TruncateBlobsAfter", "data",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("token", token.ToString());
                 dict.Add("truncate_offset", truncate_offset);
               });
  // Delete all blobs that start at or after the truncation offset.
  {
    constexpr char kSqlDeleteAfter[] =
        // clang-format off
        "DELETE FROM blobs "
        "WHERE "
          "token_high=? AND "  // 0
          "token_low=? AND "   // 1
          "start>=? "          // 2
        "RETURNING "
          "start,"             // 0
          "end";               // 1
    // clang-format on
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlDeleteAfter));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteAfter));
    statement.BindInt64(0, TokenHigh(token));
    statement.BindInt64(1, TokenLow(token));
    statement.BindInt64(2, truncate_offset);
    while (statement.Step()) {
      const int64_t blob_start = statement.ColumnInt64(0);
      const int64_t blob_end = statement.ColumnInt64(1);
      checked_total_size_delta -= blob_end - blob_start;
    }
    if (!statement.Succeeded()) {
      return Error::kFailedToExecute;
    }
  }
  return Error::kOk;
}

// Inserts a vector of new blobs into the database.
Error Backend::InsertNewBlobs(
    const base::UnguessableToken& token,
    const std::vector<BufferWithStart>& new_blobs,
    base::CheckedNumeric<int64_t>& checked_total_size_delta) {
  // Iterate through the provided blobs and insert each one.
  for (const auto& new_blob : new_blobs) {
    if (Error error =
            InsertNewBlob(token, new_blob.start, new_blob.buffer,
                          new_blob.buffer->size(), checked_total_size_delta);
        error != Error::kOk) {
      return error;
    }
  }
  return Error::kOk;
}

// Inserts a single new blob into the database.
Error Backend::InsertNewBlob(
    const base::UnguessableToken& token,
    int64_t start,
    const scoped_refptr<net::IOBuffer>& buffer,
    int buf_len,
    base::CheckedNumeric<int64_t>& checked_total_size_delta) {
  TRACE_EVENT1("disk_cache", "SqlBackend.InsertNewBlob", "data",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("token", token.ToString());
                 dict.Add("start", start);
                 dict.Add("buf_len", buf_len);
               });
  const int64_t end =
      (base::CheckedNumeric<int64_t>(start) + buf_len).ValueOrDie();
  constexpr char kSqlInsertIntoBlobs[] =
      // clang-format off
      "INSERT INTO blobs("
          "token_high,"  // 0
          "token_low,"   // 1
          "start,"       // 2
          "end,"         // 3
          "blob) "       // 4
      "VALUES(?,?,?,?,?)";
  // clang-format on
  // Intentionally DCHECK() for performance
  DCHECK(db_.IsSQLValid(kSqlInsertIntoBlobs));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlInsertIntoBlobs));
  statement.BindInt64(0, TokenHigh(token));
  statement.BindInt64(1, TokenLow(token));
  statement.BindInt64(2, start);
  statement.BindInt64(3, end);
  statement.BindBlob(4,
                     buffer->span().first(base::checked_cast<size_t>(buf_len)));
  if (!statement.Run()) {
    return Error::kFailedToExecute;
  }
  checked_total_size_delta += buf_len;
  return Error::kOk;
}

// A helper function to delete multiple blobs by their IDs.
Error Backend::DeleteBlobsById(
    const std::vector<int64_t>& blob_ids_to_be_removed,
    base::CheckedNumeric<int64_t>& checked_total_size_delta) {
  // Iterate through the provided blob IDs and delete each one.
  for (auto blob_id : blob_ids_to_be_removed) {
    if (Error error = DeleteBlobById(blob_id, checked_total_size_delta);
        error != Error::kOk) {
      return error;
    }
  }
  return Error::kOk;
}

// Deletes a single blob from the `blobs` table given its ID. It uses the
// `RETURNING` clause to get the size of the deleted blob to update the total.
Error Backend::DeleteBlobById(
    int64_t blob_id,
    base::CheckedNumeric<int64_t>& checked_total_size_delta) {
  TRACE_EVENT1("disk_cache", "SqlBackend.DeleteBlobById", "data",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("blob_id", blob_id);
               });
  constexpr char kSqlDeleteFromBlobs[] =
      // clang-format off
      "DELETE FROM blobs "
      "WHERE "
          "blob_id=? "  // 0
      "RETURNING "
          "start,"      // 0
          "end";        // 1
  // clang-format on
  // Intentionally DCHECK() for performance
  DCHECK(db_.IsSQLValid(kSqlDeleteFromBlobs));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromBlobs));
  statement.BindInt64(0, blob_id);
  if (!statement.Step()) {
    // `Step()` returned false, which means either the query completed with no
    // hit, or an error occurred.
    if (db_.GetErrorCode() == static_cast<int>(sql::SqliteResultCode::kDone)) {
      return Error::kNotFound;
    }
    // An unexpected database error occurred.
    return Error::kFailedToExecute;
  }
  const int64_t start = statement.ColumnInt64(0);
  const int64_t end = statement.ColumnInt64(1);
  if (end <= start) {
    return Error::kInvalidData;
  }
  // Subtract the size of the deleted blob from the total size delta.
  checked_total_size_delta -= end - start;
  return Error::kOk;
}

// Deletes all blobs associated with a specific entry token.
Error Backend::DeleteBlobsByToken(const base::UnguessableToken& token) {
  TRACE_EVENT1("disk_cache", "SqlBackend.DeleteBlobsByToken", "token",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("token", token.ToString());
               });
  constexpr char kSqlDeleteFromBlobs[] =
      // clang-format off
      "DELETE FROM blobs "
      "WHERE "
          "token_high=? AND "  // 0
          "token_low=?";       // 1
  // clang-format on
  // Intentionally DCHECK() for performance
  DCHECK(db_.IsSQLValid(kSqlDeleteFromBlobs));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromBlobs));
  statement.BindInt64(0, TokenHigh(token));
  statement.BindInt64(1, TokenLow(token));
  if (!statement.Run()) {
    return Error::kFailedToExecute;
  }
  return Error::kOk;
}

IntOrError Backend::ReadEntryData(const base::UnguessableToken& token,
                                  int64_t offset,
                                  scoped_refptr<net::IOBuffer> buffer,
                                  int buf_len,
                                  int64_t body_end,
                                  bool sparse_reading) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.ReadEntryData", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("token", token.ToString());
                       dict.Add("offset", offset);
                       dict.Add("buf_len", buf_len);
                       dict.Add("body_end", body_end);
                       dict.Add("sparse_reading", sparse_reading);
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  auto result = ReadEntryDataInternal(token, offset, std::move(buffer), buf_len,
                                      body_end, sparse_reading);
  RecordTimeAndErrorResultHistogram("ReadEntryData", timer.Elapsed(),
                                    result.error_or(Error::kOk));
  TRACE_EVENT_END1("disk_cache", "SqlBackend.ReadEntryData", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

IntOrError Backend::ReadEntryDataInternal(const base::UnguessableToken& token,
                                          int64_t offset,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          int buf_len,
                                          int64_t body_end,
                                          bool sparse_reading) {
  CheckDatabaseInitStatus();

  if (offset < 0 || buf_len < 0 || !buffer || buf_len > buffer->size()) {
    return base::unexpected(Error::kInvalidArgument);
  }

  // Truncate `buffer_len` to make sure that `offset + buffer_len` does not
  // overflow.
  int64_t buffer_len = std::min(static_cast<int64_t>(buf_len),
                                std::numeric_limits<int64_t>::max() - offset);
  const int64_t read_end =
      (base::CheckedNumeric<int64_t>(offset) + buffer_len).ValueOrDie();
  // Select all blobs that overlap with the read range [offset, read_end),
  // ordered by their start offset.
  constexpr char kSqlSelectOverrapping[] =
      // clang-format off
      "SELECT "
          "start,"             // 0
          "end,"               // 1
          "blob "              // 2
      "FROM blobs "
      "WHERE "
          "token_high=? AND "  // 0
          "token_low=? AND "   // 1
          "start<? AND "       // 2
          "end>? "             // 3
      "ORDER BY start";
  // clang-format on
  // Intentionally DCHECK() for performance
  DCHECK(db_.IsSQLValid(kSqlSelectOverrapping));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectOverrapping));
  statement.BindInt64(0, TokenHigh(token));
  statement.BindInt64(1, TokenLow(token));
  statement.BindInt64(2, read_end);
  statement.BindInt64(3, offset);

  size_t written_bytes = 0;
  while (statement.Step()) {
    const int64_t blob_start = statement.ColumnInt64(0);
    const int64_t blob_end = statement.ColumnInt64(1);
    base::span<const uint8_t> blob = statement.ColumnBlob(2);
    if (!IsBlobSizeValid(blob_start, blob_end, blob)) {
      return base::unexpected(Error::kInvalidData);
    }
    // Determine the part of the blob that falls within the read request.
    const int64_t copy_start = std::max(offset, blob_start);
    const int64_t copy_end = std::min(read_end, blob_end);
    const size_t copy_size = base::checked_cast<size_t>(copy_end - copy_start);
    const size_t pos_in_buffer =
        base::checked_cast<size_t>(copy_start - offset);
    // If there's a gap between the last written byte and the start of the
    // current blob, handle it based on `sparse_reading`.
    if (written_bytes < pos_in_buffer) {
      if (sparse_reading) {
        // In sparse reading mode, we stop at the first gap.
        // This might be before any data got read.
        return written_bytes;
      }
      // In normal mode, fill the gap with zeros.
      std::ranges::fill(
          buffer->span().subspan(written_bytes, pos_in_buffer - written_bytes),
          0);
    }
    // Copy the relevant part of the blob into the output buffer.
    buffer->span()
        .subspan(pos_in_buffer, copy_size)
        .copy_from_nonoverlapping(blob.subspan(
            base::checked_cast<size_t>(copy_start - blob_start), copy_size));
    written_bytes = copy_end - offset;
  }

  if (sparse_reading) {
    return written_bytes;
  }

  // After processing all blobs, check if we need to zero-fill the rest of the
  // buffer up to the logical end of the entry's body.
  const size_t last_pos_in_buffer =
      std::min(body_end - offset, static_cast<int64_t>(buffer_len));
  if (written_bytes < last_pos_in_buffer) {
    std::ranges::fill(buffer->span().subspan(
                          written_bytes, last_pos_in_buffer - written_bytes),
                      0);
    written_bytes = last_pos_in_buffer;
  }

  return written_bytes;
}

RangeResult Backend::GetEntryAvailableRange(const base::UnguessableToken& token,
                                            int64_t offset,
                                            int len) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.GetEntryAvailableRange", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("token", token.ToString());
                       dict.Add("offset", offset);
                       dict.Add("len", len);
                     });
  base::ElapsedTimer timer;
  auto result = GetEntryAvailableRangeInternal(token, offset, len);
  RecordTimeAndErrorResultHistogram("GetEntryAvailableRange", timer.Elapsed(),
                                    Error::kOk);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.GetEntryAvailableRange", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, dict);
                   });
  return result;
}

RangeResult Backend::GetEntryAvailableRangeInternal(
    const base::UnguessableToken& token,
    int64_t offset,
    int len) {
  CheckDatabaseInitStatus();
  // Truncate `len` to make sure that `offset + len` does not overflow.
  len = std::min(static_cast<int64_t>(len),
                 std::numeric_limits<int64_t>::max() - offset);
  const int64_t end = offset + len;
  std::optional<int64_t> available_start;
  int64_t available_end = 0;

  // To finds the available contiguous range of data for a given entry. queries
  // the `blobs` table for data chunks that overlap with the requested range
  // [offset, end).
  {
    constexpr char kSqlSelectOverrapping[] =
        // clang-format off
        "SELECT "
            "start,"  // 0
            "end "    // 1
        "FROM blobs "
        "WHERE "
          "token_high=? AND "  // 0
          "token_low=? AND "   // 1
          "start<? AND "     // 2
          "end>? "           // 3
        "ORDER BY start";
    // clang-format on
    // Intentionally DCHECK() for performance
    DCHECK(db_.IsSQLValid(kSqlSelectOverrapping));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectOverrapping));
    statement.BindInt64(0, TokenHigh(token));
    statement.BindInt64(1, TokenLow(token));
    statement.BindInt64(2, end);
    statement.BindInt64(3, offset);
    while (statement.Step()) {
      int64_t blob_start = statement.ColumnInt64(0);
      int64_t blob_end = statement.ColumnInt64(1);
      if (!available_start) {
        // This is the first blob we've found in the requested range. Start
        // tracking the contiguous available range from here.
        available_start = std::max(blob_start, offset);
        available_end = std::min(blob_end, end);
      } else {
        // We have already found a blob, check if this one is contiguous.
        if (available_end == blob_start) {
          // The next blob is contiguous with the previous one. Extend the
          // available range.
          available_end = std::min(blob_end, end);
        } else {
          // There's a gap in the data. Return the contiguous range found so
          // far.
          return RangeResult(*available_start,
                             available_end - *available_start);
        }
      }
    }
  }
  // If we found any data, return the total contiguous range.
  if (available_start) {
    return RangeResult(*available_start, available_end - *available_start);
  }
  return RangeResult(offset, 0);
}

int64_t Backend::CalculateSizeOfEntriesBetween(base::Time initial_time,
                                               base::Time end_time) {
  if (initial_time == base::Time::Min() && end_time == base::Time::Max()) {
    return GetSizeOfAllEntries();
  }
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.CalculateSizeOfEntriesBetween",
                     "data", [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("initial_time", initial_time);
                       dict.Add("end_time", end_time);
                     });
  base::ElapsedTimer timer;
  auto result = CalculateSizeOfEntriesBetweenInternal(initial_time, end_time);
  RecordTimeAndErrorResultHistogram("CalculateSizeOfEntriesBetween",
                                    timer.Elapsed(), Error::kOk);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.CalculateSizeOfEntriesBetween",
                   "result", result);
  return result;
}

int64_t Backend::CalculateSizeOfEntriesBetweenInternal(base::Time initial_time,
                                                       base::Time end_time) {
  // To calculate the total size of all entries whose `last_used` time falls
  // within the range [`initial_time`, `end_time`), sums up the `bytes_usage`
  // from the `resources` table and adds a static overhead for each entry.
  constexpr char kSqlSelectFromResources[] =
      // clang-format off
      "SELECT "
          "bytes_usage "  // 0
      "FROM resources "
      "WHERE "
          "last_used>=? AND "  // 0
          "last_used<?";       // 1
  // clang-format on
  // Intentionally DCHECK() for performance
  DCHECK(db_.IsSQLValid(kSqlSelectFromResources));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectFromResources));
  statement.BindTime(0, initial_time);
  statement.BindTime(1, end_time);
  base::ClampedNumeric<int64_t> total_size = 0;
  while (statement.Step()) {
    // `bytes_usage` includes the size of the key, header, and body data.
    total_size += statement.ColumnInt64(0);
    // Add the static overhead for the entry's row in the database.
    total_size += kSqlBackendStaticResourceSize;
  }
  return total_size;
}

OptionalEntryInfoWithIdAndKey Backend::OpenLatestEntryBeforeResId(
    int64_t res_id_cursor) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.OpenLatestEntryBeforeResId",
                     "data", [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("res_id_cursor", res_id_cursor);
                     });
  base::ElapsedTimer timer;
  Error error = Error::kOk;
  auto result = OpenLatestEntryBeforeResIdInternal(res_id_cursor, error);
  RecordTimeAndErrorResultHistogram("OpenLatestEntryBeforeResId",
                                    timer.Elapsed(), error);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.OpenLatestEntryBeforeResId",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, dict);
                     PopulateTraceDetails(error, dict);
                   });
  return result;
}

OptionalEntryInfoWithIdAndKey Backend::OpenLatestEntryBeforeResIdInternal(
    int64_t res_id_cursor,
    Error& error_out) {
  CheckDatabaseInitStatus();

  constexpr char kSqlSelectResources[] =
      // clang-format off
      "SELECT "
        "res_id,"      // 0
        "token_high,"  // 1
        "token_low,"   // 2
        "last_used,"   // 3
        "body_end,"    // 4
        "cache_key,"   // 5
        "head "        // 6
      "FROM resources "
      "WHERE "
        "res_id<? AND "  // 0
        "doomed=? "      // 1
      "ORDER BY res_id DESC";
  // clang-format on
  // Intentionally DCHECK() for performance.
  DCHECK(db_.IsSQLValid(kSqlSelectResources));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectResources));
  statement.BindInt64(0, res_id_cursor);
  statement.BindBool(1, false);
  while (statement.Step()) {
    auto maybe_token =
        ToUnguessableToken(statement.ColumnInt64(1), statement.ColumnInt64(2));
    if (!maybe_token) {
      // If OpenNextEntry encounters invalid data, it records it in a histogram
      // and ignores the data.
      error_out = Error::kInvalidData;
      continue;
    }

    EntryInfoWithIdAndKey result;
    result.res_id = statement.ColumnInt64(0);
    result.key = CacheEntryKey(statement.ColumnString(5));
    auto& entry_info = result.info;
    entry_info.token = *maybe_token;
    entry_info.last_used = statement.ColumnTime(3);
    entry_info.body_end = statement.ColumnInt64(4);
    base::span<const uint8_t> blob_span = statement.ColumnBlob(6);
    if (blob_span.size() > std::numeric_limits<int>::max()) {
      // If OpenNextEntry encounters invalid data, it records it in a histogram
      // and ignores the data.
      error_out = Error::kInvalidData;
      continue;
    }
    entry_info.head = base::MakeRefCounted<net::GrowableIOBuffer>();
    entry_info.head->SetCapacity(blob_span.size());
    entry_info.head->span().copy_from_nonoverlapping(blob_span);
    entry_info.opened = true;
    return result;
  }
  return std::nullopt;
}

bool Backend::UpdateStoreStatusAndCommitTransaction(
    sql::Transaction& transaction,
    int64_t entry_count_delta,
    int64_t total_size_delta) {
  const auto old_entry_count = store_status_.entry_count;
  const auto old_total_size = store_status_.total_size;
  if (entry_count_delta != 0) {
    // If the addition overflows or results in a negative count, it implies
    // corrupted metadata. In this case, log an error and recalculate the count
    // directly from the database to recover.
    if (!base::CheckAdd(store_status_.entry_count, entry_count_delta)
             .AssignIfValid(&store_status_.entry_count) ||
        store_status_.entry_count < 0) {
      store_status_.entry_count = CalculateResourceEntryCount();
    }
    meta_table_.SetValue(kSqlBackendMetaTableKeyEntryCount,
                         store_status_.entry_count);
  }

  if (total_size_delta != 0) {
    // If the addition overflows or results in a negative size, it implies
    // corrupted metadata. In this case, log an error and recalculate the size
    // directly from the database to recover.
    if (!base::CheckAdd(store_status_.total_size, total_size_delta)
             .AssignIfValid(&store_status_.total_size) ||
        store_status_.total_size < 0) {
      store_status_.total_size = CalculateTotalSize();
    }
    meta_table_.SetValue(kSqlBackendMetaTableKeyTotalSize,
                         store_status_.total_size);
  }

  // Intentionally DCHECK for performance.
  // In debug builds, verify consistency by recalculating.
  DCHECK_EQ(store_status_.entry_count, CalculateResourceEntryCount());
  DCHECK_EQ(store_status_.total_size, CalculateTotalSize());

  // Attempt to commit the transaction. If it fails, revert the in-memory
  // store status to its state before the updates.
  // This ensures that the in-memory status always reflects the on-disk state.
  if (!transaction.Commit()) {
    store_status_.entry_count = old_entry_count;
    store_status_.total_size = old_total_size;
    return false;
  }
  return true;
}

bool Backend::RecalculateStoreStatusAndCommitTransaction(
    sql::Transaction& transaction) {
  store_status_.entry_count = CalculateResourceEntryCount();
  store_status_.total_size = CalculateTotalSize();
  meta_table_.SetValue(kSqlBackendMetaTableKeyEntryCount,
                       store_status_.entry_count);
  meta_table_.SetValue(kSqlBackendMetaTableKeyTotalSize,
                       store_status_.total_size);
  return transaction.Commit();
}

// Recalculates the number of non-doomed entries in the `resources` table.
int64_t Backend::CalculateResourceEntryCount() {
  constexpr char kSqlSelectCountFromResources[] =
      "SELECT COUNT(*) FROM resources WHERE doomed=?";
  // Intentionally DCHECK() for performance
  DCHECK(db_.IsSQLValid(kSqlSelectCountFromResources));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectCountFromResources));
  statement.BindBool(0, false);
  int64_t result = 0;
  if (statement.Step()) {
    result = statement.ColumnInt64(0);
  }
  return result;
}

// Recalculates the total size of all non-doomed entries.
int64_t Backend::CalculateTotalSize() {
  constexpr char kSqlSelectTotalSizeFromResources[] =
      "SELECT SUM(bytes_usage) FROM resources WHERE doomed=?";
  // Intentionally DCHECK() for performance
  DCHECK(db_.IsSQLValid(kSqlSelectTotalSizeFromResources));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectTotalSizeFromResources));
  statement.BindBool(0, false);
  int64_t result = 0;
  if (statement.Step()) {
    result = statement.ColumnInt64(0);
  }
  return result;
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
  void OpenOrCreateEntry(const CacheEntryKey& key,
                         EntryInfoOrErrorCallback callback) override {
    backend_.AsyncCall(&Backend::OpenOrCreateEntry)
        .WithArgs(key)
        .Then(WrapCallback(std::move(callback)));
  }
  void OpenEntry(const CacheEntryKey& key,
                 OptionalEntryInfoOrErrorCallback callback) override {
    backend_.AsyncCall(&Backend::OpenEntry)
        .WithArgs(key)
        .Then(WrapCallback(std::move(callback)));
  }
  void CreateEntry(const CacheEntryKey& key,
                   EntryInfoOrErrorCallback callback) override {
    backend_.AsyncCall(&Backend::CreateEntry)
        .WithArgs(key)
        .Then(WrapCallback(std::move(callback)));
  }
  void DoomEntry(const CacheEntryKey& key,
                 const base::UnguessableToken& token,
                 ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DoomEntry)
        .WithArgs(key, token)
        .Then(WrapCallback(std::move(callback)));
  }
  void DeleteDoomedEntry(const CacheEntryKey& key,
                         const base::UnguessableToken& token,
                         ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DeleteDoomedEntry)
        .WithArgs(key, token)
        .Then(WrapCallback(std::move(callback)));
  }
  void DeleteLiveEntry(const CacheEntryKey& key,
                       ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DeleteLiveEntry)
        .WithArgs(key)
        .Then(WrapCallback(std::move(callback)));
  }
  void DeleteAllEntries(ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DeleteAllEntries)
        .Then(WrapCallback(std::move(callback)));
  }
  void DeleteLiveEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                std::set<CacheEntryKey> excluded_keys,
                                ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DeleteLiveEntriesBetween)
        .WithArgs(initial_time, end_time, std::move(excluded_keys))
        .Then(WrapCallback(std::move(callback)));
  }
  void UpdateEntryLastUsed(const CacheEntryKey& key,
                           base::Time last_used,
                           ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::UpdateEntryLastUsed)
        .WithArgs(key, last_used)
        .Then(WrapCallback(std::move(callback)));
  }
  void UpdateEntryHeaderAndLastUsed(const CacheEntryKey& key,
                                    const base::UnguessableToken& token,
                                    base::Time last_used,
                                    scoped_refptr<net::IOBuffer> buffer,
                                    int64_t header_size_delta,
                                    ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::UpdateEntryHeaderAndLastUsed)
        .WithArgs(key, token, last_used, std::move(buffer), header_size_delta)
        .Then(WrapCallback(std::move(callback)));
  }
  void WriteEntryData(const CacheEntryKey& key,
                      const base::UnguessableToken& token,
                      int64_t old_body_end,
                      int64_t offset,
                      scoped_refptr<net::IOBuffer> buffer,
                      int buf_len,
                      bool truncate,
                      ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::WriteEntryData)
        .WithArgs(key, token, old_body_end, offset, std::move(buffer), buf_len,
                  truncate)
        .Then(WrapCallback(std::move(callback)));
  }
  void ReadEntryData(const base::UnguessableToken& token,
                     int64_t offset,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len,
                     int64_t body_end,
                     bool sparse_reading,
                     IntOrErrorCallback callback) override {
    backend_.AsyncCall(&Backend::ReadEntryData)
        .WithArgs(token, offset, std::move(buffer), buf_len, body_end,
                  sparse_reading)
        .Then(WrapCallback(std::move(callback)));
  }
  void GetEntryAvailableRange(const base::UnguessableToken& token,
                              int64_t offset,
                              int len,
                              RangeResultCallback callback) override {
    backend_.AsyncCall(&Backend::GetEntryAvailableRange)
        .WithArgs(token, offset, len)
        .Then(WrapCallback(std::move(callback)));
  }
  void CalculateSizeOfEntriesBetween(base::Time initial_time,
                                     base::Time end_time,
                                     Int64OrErrorCallback callback) override {
    backend_.AsyncCall(&Backend::CalculateSizeOfEntriesBetween)
        .WithArgs(initial_time, end_time)
        .Then(WrapCallback(std::move(callback)));
  }
  void OpenLatestEntryBeforeResId(
      int64_t res_id_cursor,
      OptionalEntryInfoWithIdAndKeyCallback callback) override {
    backend_.AsyncCall(&Backend::OpenLatestEntryBeforeResId)
        .WithArgs(res_id_cursor)
        .Then(WrapCallback(std::move(callback)));
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

  // Wraps a callback to ensure it is only run if the `SqlPersistentStoreImpl`
  // is still alive.
  template <typename ResultType>
  base::OnceCallback<void(ResultType)> WrapCallback(
      base::OnceCallback<void(ResultType)> callback) {
    return base::BindOnce(
        [](base::WeakPtr<SqlPersistentStoreImpl> weak_ptr,
           base::OnceCallback<void(ResultType)> callback, ResultType result) {
          if (weak_ptr) {
            // We should not run the callback when `this` was deleted.
            std::move(callback).Run(std::move(result));
          }
        },
        weak_factory_.GetWeakPtr(), std::move(callback));
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

// Default constructor and move operations for EntryInfo.
SqlPersistentStore::EntryInfo::EntryInfo() = default;
SqlPersistentStore::EntryInfo::~EntryInfo() = default;
SqlPersistentStore::EntryInfo::EntryInfo(EntryInfo&&) = default;
SqlPersistentStore::EntryInfo& SqlPersistentStore::EntryInfo::operator=(
    EntryInfo&&) = default;

SqlPersistentStore::EntryInfoWithIdAndKey::EntryInfoWithIdAndKey() = default;
SqlPersistentStore::EntryInfoWithIdAndKey::~EntryInfoWithIdAndKey() = default;
SqlPersistentStore::EntryInfoWithIdAndKey::EntryInfoWithIdAndKey(
    EntryInfoWithIdAndKey&&) = default;
SqlPersistentStore::EntryInfoWithIdAndKey&
SqlPersistentStore::EntryInfoWithIdAndKey::operator=(EntryInfoWithIdAndKey&&) =
    default;

}  // namespace disk_cache
