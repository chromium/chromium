// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store.h"

#include <cstdint>
#include <limits>
#include <optional>

#include "base/containers/flat_set.h"
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
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_persistent_store_queries.h"
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

using disk_cache_sql_queries::GetQuery;
using disk_cache_sql_queries::Query;

using EntryInfo = SqlPersistentStore::EntryInfo;
using Error = SqlPersistentStore::Error;
using EntryInfoOrError = SqlPersistentStore::EntryInfoOrError;
using OptionalEntryInfoOrError = SqlPersistentStore::OptionalEntryInfoOrError;
using EntryInfoWithIdAndKey = SqlPersistentStore::EntryInfoWithIdAndKey;
using OptionalEntryInfoWithIdAndKey =
    SqlPersistentStore::OptionalEntryInfoWithIdAndKey;
using IntOrError = SqlPersistentStore::IntOrError;
using InitResultOrError = base::expected<InitResult, Error>;

// A helper struct to bundle an operation's result with a flag indicating
// whether an eviction check is needed. This allows the background sequence,
// which has direct access to cache size information, to notify the main
// sequence that an eviction might be necessary without requiring an extra
// cross-sequence call to check the cache size.
template <typename ResultType>
struct ResultAndEvictionRequested {
  ResultAndEvictionRequested(ResultType result, bool eviction_requested)
      : result(std::move(result)), eviction_requested(eviction_requested) {}
  ~ResultAndEvictionRequested() = default;
  ResultAndEvictionRequested(ResultAndEvictionRequested&&) = default;

  // The actual result of the operation.
  ResultType result;

  // True if the cache size has exceeded the high watermark, signaling that an
  // eviction task should be considered.
  bool eviction_requested;
};

using ErrorAndEvictionRequested = ResultAndEvictionRequested<Error>;
using EntryInfoOrErrorAndEvictionRequested =
    ResultAndEvictionRequested<EntryInfoOrError>;
using IntOrErrorAndEvictionRequested = ResultAndEvictionRequested<IntOrError>;

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
                                       Error error,
                                       bool corruption_detected) {
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({kHistogramPrefix, method_name,
                    error == Error::kOk ? ".SuccessTime" : ".FailureTime",
                    corruption_detected ? "WithCorruption" : ""}),
      time_delta);
  base::UmaHistogramEnumeration(
      base::StrCat({kHistogramPrefix, method_name,
                    corruption_detected ? ".ResultWithCorruption" : ".Result"}),
      error);
}

// Sets up the database schema and indexes.
[[nodiscard]] bool InitSchema(sql::Database& db) {
  if (!db.Execute(GetQuery(Query::kInitSchema_CreateTableResources)) ||
      !db.Execute(GetQuery(Query::kInitSchema_CreateTableBlobs)) ||
      !db.Execute(GetQuery(Query::kIndex_ResourcesToken)) ||
      !db.Execute(GetQuery(Query::kIndex_ResourcesCacheKeyDoomed)) ||
      !db.Execute(GetQuery(Query::kIndex_ResourcesDoomedLastUsed)) ||
      !db.Execute(GetQuery(Query::kIndex_ResourcesDoomedResId)) ||
      !db.Execute(GetQuery(Query::kIndex_BlobsTokenStart))) {
    return false;
  }
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
        high_watermark_(max_bytes_ -
                        max_bytes_ / kSqlBackendEvictionMarginDivisor),
        low_watermark_(max_bytes_ -
                       2 * (max_bytes_ / kSqlBackendEvictionMarginDivisor)),
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

  EntryInfoOrErrorAndEvictionRequested OpenOrCreateEntry(
      const CacheEntryKey& key);
  OptionalEntryInfoOrError OpenEntry(const CacheEntryKey& key);
  EntryInfoOrErrorAndEvictionRequested CreateEntry(const CacheEntryKey& key);

  ErrorAndEvictionRequested DoomEntry(const CacheEntryKey& key,
                                      const base::UnguessableToken& token);
  ErrorAndEvictionRequested DeleteDoomedEntry(
      const CacheEntryKey& key,
      const base::UnguessableToken& token);
  Error DeleteDoomedEntries(
      base::flat_set<base::UnguessableToken> excluded_tokens);
  ErrorAndEvictionRequested DeleteLiveEntry(const CacheEntryKey& key);

  ErrorAndEvictionRequested DeleteAllEntries();
  ErrorAndEvictionRequested DeleteLiveEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      base::flat_set<CacheEntryKey> excluded_keys);
  Error UpdateEntryLastUsed(const CacheEntryKey& key, base::Time last_used);
  ErrorAndEvictionRequested UpdateEntryHeaderAndLastUsed(
      const CacheEntryKey& key,
      const base::UnguessableToken& token,
      base::Time last_used,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t header_size_delta);
  ErrorAndEvictionRequested WriteEntryData(const CacheEntryKey& key,
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
  ErrorAndEvictionRequested RunEviction(
      base::flat_set<CacheEntryKey> excluded_keys);

  void EnableStrictCorruptionCheckForTesting() {
    strict_corruption_check_enabled_ = true;
  }

 private:
  void DatabaseErrorCallback(int error, sql::Statement* statement);

  Error InitializeInternal(bool& corruption_detected);
  EntryInfoOrError OpenOrCreateEntryInternal(const CacheEntryKey& key,
                                             bool& corruption_detected);
  OptionalEntryInfoOrError OpenEntryInternal(const CacheEntryKey& key,
                                             bool& corruption_detected);
  EntryInfoOrError CreateEntryInternal(const CacheEntryKey& key,
                                       bool run_existance_check,
                                       bool& corruption_detected);
  Error DoomEntryInternal(const CacheEntryKey& key,
                          const base::UnguessableToken& token,
                          bool& corruption_detected);
  Error DeleteDoomedEntryInternal(const CacheEntryKey& key,
                                  const base::UnguessableToken& token,
                                  bool& corruption_detected);
  Error DeleteDoomedEntriesInternal(
      const base::flat_set<base::UnguessableToken>& excluded_tokens,
      size_t& deleted_count,
      bool& corruption_detected);
  Error DeleteLiveEntryInternal(const CacheEntryKey& key,
                                bool& corruption_detected);
  Error DeleteAllEntriesInternal(bool& corruption_detected);
  Error DeleteLiveEntriesBetweenInternal(
      base::Time initial_time,
      base::Time end_time,
      const base::flat_set<CacheEntryKey>& excluded_keys,
      bool& corruption_detected);
  Error UpdateEntryLastUsedInternal(const CacheEntryKey& key,
                                    base::Time last_used);
  Error UpdateEntryHeaderAndLastUsedInternal(
      const CacheEntryKey& key,
      const base::UnguessableToken& token,
      base::Time last_used,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t header_size_delta,
      bool& corruption_detected);
  Error WriteEntryDataInternal(const CacheEntryKey& key,
                               const base::UnguessableToken& token,
                               int64_t old_body_end,
                               int64_t offset,
                               scoped_refptr<net::IOBuffer> buffer,
                               int buf_len,
                               bool truncate,
                               bool& corruption_detected);
  IntOrError ReadEntryDataInternal(const base::UnguessableToken& token,
                                   int64_t offset,
                                   scoped_refptr<net::IOBuffer> buffer,
                                   int buf_len,
                                   int64_t body_end,
                                   bool sparse_reading,
                                   bool& corruption_detected);
  RangeResult GetEntryAvailableRangeInternal(
      const base::UnguessableToken& token,
      int64_t offset,
      int len);
  int64_t CalculateSizeOfEntriesBetweenInternal(base::Time initial_time,
                                                base::Time end_time);
  OptionalEntryInfoWithIdAndKey OpenLatestEntryBeforeResIdInternal(
      int64_t res_id_cursor,
      bool& corruption_detected);
  Error RunEvictionInternal(const base::flat_set<CacheEntryKey>& excluded_keys,
                            bool& corruption_detected);

  // Trims blobs that overlap with the new write range [offset, end), and
  // updates the total size delta.
  Error TrimOverlappingBlobs(
      const base::UnguessableToken& token,
      int64_t offset,
      int64_t end,
      bool truncate,
      base::CheckedNumeric<int64_t>& checked_total_size_delta,
      bool& corruption_detected);
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
  Error DeleteBlobsById(const std::vector<int64_t>& blob_ids_to_be_removed,
                        base::CheckedNumeric<int64_t>& checked_total_size_delta,
                        bool& corruption_detected);
  // Deletes a single blob by its ID, and updates the total size delta.
  Error DeleteBlobById(int64_t blob_id,
                       base::CheckedNumeric<int64_t>& checked_total_size_delta,
                       bool& corruption_detected);
  // Deletes all blobs associated with a given token.
  Error DeleteBlobsByToken(const base::UnguessableToken& token);
  // Deletes all blobs associated with a list of entry tokens.
  Error DeleteBlobsByTokens(const std::vector<base::UnguessableToken>& tokens);
  // Deletes multiple resource entries from the `resources` table by their
  // `res_id`s.
  Error DeleteResourcesByResIds(const std::vector<int64_t>& res_ids);

  // Updates the in-memory `store_status_` by `entry_count_delta` and
  // `total_size_delta`. If the update results in an overflow or a negative
  // value, it recalculates the correct value from the database to recover from
  // potential metadata corruption.
  // It then updates the meta table values and attempts to commit the
  // `transaction`.
  // Returns Error::kOk on success, or an error code on failure.
  Error UpdateStoreStatusAndCommitTransaction(sql::Transaction& transaction,
                                              int64_t entry_count_delta,
                                              int64_t total_size_delta,
                                              bool& corruption_detected);

  // Recalculates the store's status (entry count and total size) directly from
  // the database. This is a recovery mechanism used when metadata might be
  // inconsistent, e.g., after a numerical overflow.
  // Returns Error::kOk on success, or an error code on failure.
  Error RecalculateStoreStatusAndCommitTransaction(
      sql::Transaction& transaction);

  int64_t CalculateResourceEntryCount();
  int64_t CalculateTotalSize();

  // A helper method for checking that the database initialization was
  // successful before proceeding with any database operations.
  void CheckDatabaseInitStatus() {
    CHECK(db_init_status_.has_value());
    CHECK_EQ(*db_init_status_, Error::kOk);
  }

  bool ShouldStartEviction() const {
    return GetSizeOfAllEntries() > high_watermark_;
  }

  void MaybeCrashIfCorrupted(bool corruption_detected) {
    CHECK(!(corruption_detected && strict_corruption_check_enabled_));
  }

  const base::FilePath path_;
  const int64_t max_bytes_;
  const int64_t high_watermark_;
  const int64_t low_watermark_;
  sql::Database db_;
  sql::MetaTable meta_table_;
  std::optional<Error> db_init_status_;
  StoreStatus store_status_;
  bool strict_corruption_check_enabled_ = false;
};

InitResultOrError Backend::Initialize() {
  TRACE_EVENT_BEGIN0("disk_cache", "SqlBackend.Initialize");
  base::ElapsedTimer timer;
  CHECK(!db_init_status_.has_value());
  bool corruption_detected = false;
  db_init_status_ = InitializeInternal(corruption_detected);
  RecordTimeAndErrorResultHistogram("Initialize", timer.Elapsed(),
                                    *db_init_status_, corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.Initialize", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(*db_init_status_, store_status_,
                                          dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return *db_init_status_ == Error::kOk
             ? InitResultOrError(InitResult(max_bytes_))
             : base::unexpected(*db_init_status_);
}

Error Backend::InitializeInternal(bool& corruption_detected) {
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
  if (!GetOrInitializeMetaValue(meta_table_, kSqlBackendMetaTableKeyTotalSize,
                                store_status_.total_size,
                                /*default_value=*/0)) {
    return Error::kFailedToSetTotalSizeMetadata;
  }

  if (tmp_entry_count < 0 ||
      !base::IsValueInRangeForNumericType<int32_t>(tmp_entry_count) ||
      store_status_.total_size < 0) {
    corruption_detected = true;
    return RecalculateStoreStatusAndCommitTransaction(transaction);
  }

  store_status_.entry_count = static_cast<int32_t>(tmp_entry_count);

  return transaction.Commit() ? Error::kOk : Error::kFailedToCommitTransaction;
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

EntryInfoOrErrorAndEvictionRequested Backend::OpenOrCreateEntry(
    const CacheEntryKey& key) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.OpenOrCreateEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = OpenOrCreateEntryInternal(key, corruption_detected);
  RecordTimeAndErrorResultHistogram("OpenOrCreateEntry", timer.Elapsed(),
                                    result.error_or(Error::kOk),
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.OpenOrCreateEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return EntryInfoOrErrorAndEvictionRequested(std::move(result),
                                              ShouldStartEviction());
}

EntryInfoOrError Backend::OpenOrCreateEntryInternal(const CacheEntryKey& key,
                                                    bool& corruption_detected) {
  // Try to open first.
  auto open_result = OpenEntryInternal(key, corruption_detected);
  if (open_result.has_value() && open_result->has_value()) {
    return std::move(*open_result.value());
  }
  // If opening failed with an error, propagate that error.
  if (!open_result.has_value()) {
    return base::unexpected(open_result.error());
  }
  // If the entry was not found, try to create a new one.
  return CreateEntryInternal(key, /*run_existance_check=*/false,
                             corruption_detected);
}

OptionalEntryInfoOrError Backend::OpenEntry(const CacheEntryKey& key) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.OpenEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = OpenEntryInternal(key, corruption_detected);
  RecordTimeAndErrorResultHistogram("OpenEntry", timer.Elapsed(),
                                    result.error_or(Error::kOk),
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.OpenEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return result;
}

OptionalEntryInfoOrError Backend::OpenEntryInternal(const CacheEntryKey& key,
                                                    bool& corruption_detected) {
  CheckDatabaseInitStatus();

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kOpenEntry_SelectLiveResources)));
  statement.BindString(0, key.string());
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
    corruption_detected = true;
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

EntryInfoOrErrorAndEvictionRequested Backend::CreateEntry(
    const CacheEntryKey& key) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.CreateEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = CreateEntryInternal(key, /*run_existance_check=*/true,
                                    corruption_detected);
  RecordTimeAndErrorResultHistogram("CreateEntry", timer.Elapsed(),
                                    result.error_or(Error::kOk),
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.CreateEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return EntryInfoOrErrorAndEvictionRequested(std::move(result),
                                              ShouldStartEviction());
}

EntryInfoOrError Backend::CreateEntryInternal(const CacheEntryKey& key,
                                              bool run_existance_check,
                                              bool& corruption_detected) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }
  if (run_existance_check) {
    auto open_result = OpenEntryInternal(key, corruption_detected);
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
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(
            disk_cache_sql_queries::Query::kCreateEntry_InsertIntoResources)));
    statement.BindInt64(0, TokenHigh(entry_info.token));
    statement.BindInt64(1, TokenLow(entry_info.token));
    statement.BindTime(2, entry_info.last_used);
    statement.BindInt64(3, entry_info.body_end);
    statement.BindInt64(4, bytes_usage);
    statement.BindString(5, key.string());
    if (!statement.Run()) {
      return base::unexpected(Error::kFailedToExecute);
    }
  }

  // Update the store's status and commit the transaction.
  // The entry count is increased by 1, and the total size by `bytes_usage`.
  // This call will also handle updating the on-disk meta table.
  if (const auto error = UpdateStoreStatusAndCommitTransaction(
          transaction,
          /*entry_count_delta=*/1,
          /*total_size_delta=*/bytes_usage, corruption_detected);
      error != Error::kOk) {
    return base::unexpected(error);
  }

  return entry_info;
}

ErrorAndEvictionRequested Backend::DoomEntry(
    const CacheEntryKey& key,
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
  RecordTimeAndErrorResultHistogram("DoomEntry", timer.Elapsed(), result,
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DoomEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                     dict.Add("corruption_detected", corruption_detected);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndEvictionRequested(result, ShouldStartEviction());
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
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kDoomEntry_MarkDoomedResources)));
    statement.BindString(0, key.string());
    statement.BindInt64(1, TokenHigh(token));
    statement.BindInt64(2, TokenLow(token));
    // Iterate through the rows returned by the RETURNING clause.
    while (statement.Step()) {
      // Since we're dooming an entry, its size is subtracted from the total.
      total_size_delta -= statement.ColumnInt64(0);
      // Count how many entries were actually updated.
      ++doomed_count;
    }
  }

  if (doomed_count > 1) {
    // The cache_key and token combination should uniquely identify a single
    // non-doomed entry.
    // TODO(crbug.com/422065015): If this error is observed in UMA, implement
    // recovery logic.
    corruption_detected = true;
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
    return RecalculateStoreStatusAndCommitTransaction(transaction);
  }

  return UpdateStoreStatusAndCommitTransaction(
      transaction,
      /*entry_count_delta=*/-doomed_count,
      /*total_size_delta=*/total_size_delta.ValueOrDie(), corruption_detected);
}

ErrorAndEvictionRequested Backend::DeleteDoomedEntry(
    const CacheEntryKey& key,
    const base::UnguessableToken& token) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteDoomedEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       dict.Add("token", token.ToString());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = DeleteDoomedEntryInternal(key, token, corruption_detected);
  RecordTimeAndErrorResultHistogram("DeleteDoomedEntry", timer.Elapsed(),
                                    result, corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteDoomedEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndEvictionRequested(result, ShouldStartEviction());
}

Error Backend::DeleteDoomedEntryInternal(const CacheEntryKey& key,
                                         const base::UnguessableToken& token,
                                         bool& corruption_detected) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  int64_t deleted_count = 0;
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kDeleteDoomedEntry_DeleteFromResources)));
    statement.BindString(0, key.string());
    statement.BindInt64(1, TokenHigh(token));
    statement.BindInt64(2, TokenLow(token));
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
    deleted_count = db_.GetLastChangeCount();
  }

  if (deleted_count > 1) {
    // The cache_key and token combination should uniquely identify a single
    // non-doomed entry.
    // TODO(crbug.com/422065015): If this error is observed in UMA, implement
    // recovery logic.
    corruption_detected = true;
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

  return transaction.Commit() ? Error::kOk : Error::kFailedToCommitTransaction;
}

Error Backend::DeleteDoomedEntries(
    base::flat_set<base::UnguessableToken> excluded_tokens) {
  TRACE_EVENT_BEGIN0("disk_cache", "SqlBackend.DeleteDoomedEntries");
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  size_t deleted_count = 0;
  auto result = DeleteDoomedEntriesInternal(excluded_tokens, deleted_count,
                                            corruption_detected);
  RecordTimeAndErrorResultHistogram("DeleteDoomedEntries", timer.Elapsed(),
                                    result, corruption_detected);
  base::UmaHistogramCounts100("Net.SqlDiskCache.DeleteDoomedEntriesCount",
                              deleted_count);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteDoomedEntries", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                     dict.Add("deleted_count", deleted_count);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return result;
}

Error Backend::DeleteDoomedEntriesInternal(
    const base::flat_set<base::UnguessableToken>& excluded_tokens,
    size_t& deleted_count,
    bool& corruption_detected) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  std::vector<int64_t> res_ids_to_delete;
  std::vector<base::UnguessableToken> tokens_for_blob_deletion;

  // 1. Select all doomed entries.
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kDeleteDoomedEntries_SelectDoomedResources)));
    // 2. Collect entries to be deleted, skipping excluded ones.
    while (statement.Step()) {
      auto maybe_token = ToUnguessableToken(statement.ColumnInt64(1),
                                            statement.ColumnInt64(2));
      if (!maybe_token) {
        corruption_detected = true;
        continue;
      }
      if (excluded_tokens.contains(*maybe_token)) {
        continue;
      }
      res_ids_to_delete.push_back(statement.ColumnInt64(0));
      tokens_for_blob_deletion.push_back(*maybe_token);
    }
  }

  deleted_count = res_ids_to_delete.size();
  if (deleted_count == 0) {
    // Nothing to delete, abort the transaction and return kOk;
    return Error::kOk;
  }

  // 3. Delete from `resources` table by `res_id`.
  if (auto error = DeleteResourcesByResIds(res_ids_to_delete);
      error != Error::kOk) {
    return error;
  }

  // 4. Delete corresponding blobs by token.
  if (auto error = DeleteBlobsByTokens(tokens_for_blob_deletion);
      error != Error::kOk) {
    return error;
  }

  // 5. Commit the transaction.
  return transaction.Commit() ? Error::kOk : Error::kFailedToCommitTransaction;
}

ErrorAndEvictionRequested Backend::DeleteLiveEntry(const CacheEntryKey& key) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteLiveEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = DeleteLiveEntryInternal(key, corruption_detected);
  RecordTimeAndErrorResultHistogram("DeleteLiveEntry", timer.Elapsed(), result,
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteLiveEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                     dict.Add("corruption_detected", corruption_detected);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndEvictionRequested(result, ShouldStartEviction());
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
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kDeleteLiveEntry_DeleteFromResources)));
    statement.BindString(0, key.string());
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
  if (Error delete_result = DeleteBlobsByTokens(tokens_to_be_deleted);
      delete_result != Error::kOk) {
    return delete_result;
  }

  // If we detected corruption, or if the size update calculation overflowed,
  // our metadata is suspect. We recover by recalculating everything from
  // scratch.
  if (corruption_detected || !total_size_delta.IsValid()) {
    corruption_detected = true;
    return RecalculateStoreStatusAndCommitTransaction(transaction);
  }

  return UpdateStoreStatusAndCommitTransaction(
      transaction,
      /*entry_count_delta=*/
      -static_cast<int64_t>(tokens_to_be_deleted.size()),
      /*total_size_delta=*/total_size_delta.ValueOrDie(), corruption_detected);
}

ErrorAndEvictionRequested Backend::DeleteAllEntries() {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteAllEntries", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  Error result = DeleteAllEntriesInternal(corruption_detected);
  RecordTimeAndErrorResultHistogram("DeleteAllEntries", timer.Elapsed(), result,
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteAllEntries", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndEvictionRequested(result, ShouldStartEviction());
}

Error Backend::DeleteAllEntriesInternal(bool& corruption_detected) {
  CheckDatabaseInitStatus();
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  // Clear the main resources table.
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kDeleteAllEntries_DeleteFromResources)));
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
  }

  // Also clear the blobs table.
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kDeleteAllEntries_DeleteFromBlobs)));
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
  }

  // Update the store's status and commit the transaction.
  // The entry count and the total size will be zero.
  // This call will also handle updating the on-disk meta table.
  return UpdateStoreStatusAndCommitTransaction(
      transaction,
      /*entry_count_delta=*/-store_status_.entry_count,
      /*total_size_delta=*/-store_status_.total_size, corruption_detected);
}

ErrorAndEvictionRequested Backend::DeleteLiveEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    base::flat_set<CacheEntryKey> excluded_keys) {
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
      initial_time, end_time, excluded_keys, corruption_detected);
  RecordTimeAndErrorResultHistogram("DeleteLiveEntriesBetween", timer.Elapsed(),
                                    result, corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteLiveEntriesBetween",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndEvictionRequested(result, ShouldStartEviction());
}

Error Backend::DeleteLiveEntriesBetweenInternal(
    base::Time initial_time,
    base::Time end_time,
    const base::flat_set<CacheEntryKey>& excluded_keys,
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
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kDeleteLiveEntriesBetween_SelectLiveResources)));
    statement.BindTime(0, initial_time);
    statement.BindTime(1, end_time);
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
  if (auto error = DeleteBlobsByTokens(tokens_to_be_deleted);
      error != Error::kOk) {
    return error;
  }

  // Delete the selected entries from the `resources` table.
  if (auto error = DeleteResourcesByResIds(res_ids_to_be_deleted);
      error != Error::kOk) {
    return error;
  }

  // If we detected corruption, or if the size update calculation overflowed,
  // our metadata is suspect. We recover by recalculating everything from
  // scratch.
  if (corruption_detected || !total_size_delta.IsValid()) {
    corruption_detected = true;
    return RecalculateStoreStatusAndCommitTransaction(transaction);
  }

  // Update the in-memory and on-disk store status (entry count and total size)
  // and commit the transaction.
  return UpdateStoreStatusAndCommitTransaction(transaction, entry_count_delta,
                                               total_size_delta.ValueOrDie(),
                                               corruption_detected);
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
                                    result, /*corruption_detected=*/false);
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
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kUpdateEntryLastUsed_UpdateResourceLastUsed)));
    statement.BindTime(0, last_used);
    statement.BindString(1, key.string());
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

ErrorAndEvictionRequested Backend::UpdateEntryHeaderAndLastUsed(
    const CacheEntryKey& key,
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
  bool corruption_detected = false;
  auto result = UpdateEntryHeaderAndLastUsedInternal(
      key, token, last_used, std::move(buffer), header_size_delta,
      corruption_detected);
  RecordTimeAndErrorResultHistogram("UpdateEntryHeaderAndLastUsed",
                                    timer.Elapsed(), result,
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.UpdateEntryHeaderAndLastUsed",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndEvictionRequested(result, ShouldStartEviction());
}

Error Backend::UpdateEntryHeaderAndLastUsedInternal(
    const CacheEntryKey& key,
    const base::UnguessableToken& token,
    base::Time last_used,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t header_size_delta,
    bool& corruption_detected) {
  CHECK(buffer);
  CheckDatabaseInitStatus();

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kUpdateEntryHeaderAndLastUsed_UpdateResource)));
    statement.BindTime(0, last_used);
    statement.BindInt64(1, header_size_delta);
    statement.BindBlob(2, buffer->span());
    statement.BindString(3, key.string());
    statement.BindInt64(4, TokenHigh(token));
    statement.BindInt64(5, TokenLow(token));
    if (statement.Step()) {
      const int64_t bytes_usage = statement.ColumnInt64(0);
      if (bytes_usage < static_cast<int64_t>(buffer->size()) +
                            static_cast<int64_t>(key.string().size())) {
        // This indicates data corruption in the database.
        // TODO(crbug.com/422065015): If this error is observed in UMA,
        // implement recovery logic.
        corruption_detected = true;
        return Error::kInvalidData;
      }
    } else {
      return Error::kNotFound;
    }
  }
  return UpdateStoreStatusAndCommitTransaction(
      transaction,
      /*entry_count_delta=*/0,
      /*total_size_delta=*/header_size_delta, corruption_detected);
}

ErrorAndEvictionRequested Backend::WriteEntryData(
    const CacheEntryKey& key,
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
  bool corruption_detected = false;
  auto result = WriteEntryDataInternal(key, token, old_body_end, offset,
                                       std::move(buffer), buf_len, truncate,
                                       corruption_detected);
  RecordTimeAndErrorResultHistogram("WriteEntryData", timer.Elapsed(), result,
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.WriteEntryData", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndEvictionRequested(result, ShouldStartEviction());
}

Error Backend::WriteEntryDataInternal(const CacheEntryKey& key,
                                      const base::UnguessableToken& token,
                                      int64_t old_body_end,
                                      int64_t offset,
                                      scoped_refptr<net::IOBuffer> buffer,
                                      int buf_len,
                                      bool truncate,
                                      bool& corruption_detected) {
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
    if (Error result =
            TrimOverlappingBlobs(token, offset, write_end, truncate,
                                 checked_total_size_delta, corruption_detected);
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
    corruption_detected = true;
    return Error::kInvalidData;
  }
  int64_t total_size_delta = checked_total_size_delta.ValueOrDie();

  // Update the entry's metadata in the `resources` table if the body size
  // changed or if the total size of blobs changed.
  if (body_end_delta || total_size_delta) {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kWriteEntryData_UpdateResource)));
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
        corruption_detected = true;
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
  return UpdateStoreStatusAndCommitTransaction(
      transaction,
      /*entry_count_delta=*/0,
      /*total_size_delta=*/total_size_delta, corruption_detected);
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
    base::CheckedNumeric<int64_t>& checked_total_size_delta,
    bool& corruption_detected) {
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
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kTrimOverlappingBlobs_DeleteContained)));
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
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kTrimOverlappingBlobs_SelectOverlapping)));
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
        corruption_detected = true;
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
          DeleteBlobsById(blob_ids_to_be_removed, checked_total_size_delta,
                          corruption_detected);
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
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kTruncateBlobsAfter_DeleteAfter)));
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
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kInsertNewBlob_InsertIntoBlobs)));
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
    base::CheckedNumeric<int64_t>& checked_total_size_delta,
    bool& corruption_detected) {
  // Iterate through the provided blob IDs and delete each one.
  for (auto blob_id : blob_ids_to_be_removed) {
    if (Error error = DeleteBlobById(blob_id, checked_total_size_delta,
                                     corruption_detected);
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
    base::CheckedNumeric<int64_t>& checked_total_size_delta,
    bool& corruption_detected) {
  TRACE_EVENT1("disk_cache", "SqlBackend.DeleteBlobById", "data",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("blob_id", blob_id);
               });
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kDeleteBlobById_DeleteFromBlobs)));
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
    corruption_detected = true;
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
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kDeleteBlobsByToken_DeleteFromBlobs)));
  statement.BindInt64(0, TokenHigh(token));
  statement.BindInt64(1, TokenLow(token));
  if (!statement.Run()) {
    return Error::kFailedToExecute;
  }
  return Error::kOk;
}

Error Backend::DeleteBlobsByTokens(
    const std::vector<base::UnguessableToken>& tokens) {
  TRACE_EVENT0("disk_cache", "SqlBackend.DeleteBlobsByTokens");
  for (const auto& token : tokens) {
    if (auto error = DeleteBlobsByToken(token); error != Error::kOk) {
      return error;
    }
  }
  return Error::kOk;
}

Error Backend::DeleteResourcesByResIds(const std::vector<int64_t>& res_ids) {
  TRACE_EVENT0("disk_cache", "SqlBackend.DeleteResourcesByResIds");
  for (int64_t res_id : res_ids) {
    sql::Statement delete_resource_stmt(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kDeleteResourcesByResIds_DeleteFromResources)));
    delete_resource_stmt.BindInt64(0, res_id);
    if (!delete_resource_stmt.Run()) {
      return Error::kFailedToExecute;
    }
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
  bool corruption_detected = false;
  auto result =
      ReadEntryDataInternal(token, offset, std::move(buffer), buf_len, body_end,
                            sparse_reading, corruption_detected);
  RecordTimeAndErrorResultHistogram("ReadEntryData", timer.Elapsed(),
                                    result.error_or(Error::kOk),
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.ReadEntryData", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return result;
}

IntOrError Backend::ReadEntryDataInternal(const base::UnguessableToken& token,
                                          int64_t offset,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          int buf_len,
                                          int64_t body_end,
                                          bool sparse_reading,
                                          bool& corruption_detected) {
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
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kReadEntryData_SelectOverlapping)));
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
      corruption_detected = true;
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
                                    Error::kOk, /*corruption_detected=*/false);
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
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kGetEntryAvailableRange_SelectOverlapping)));
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
                                    timer.Elapsed(), Error::kOk,
                                    /*corruption_detected=*/false);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.CalculateSizeOfEntriesBetween",
                   "result", result);
  return result;
}

int64_t Backend::CalculateSizeOfEntriesBetweenInternal(base::Time initial_time,
                                                       base::Time end_time) {
  // To calculate the total size of all entries whose `last_used` time falls
  // within the range [`initial_time`, `end_time`), sums up the `bytes_usage`
  // from the `resources` table and adds a static overhead for each entry.
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      GetQuery(Query::kCalculateSizeOfEntriesBetween_SelectLiveResources)));
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
  bool corruption_detected = false;
  auto result =
      OpenLatestEntryBeforeResIdInternal(res_id_cursor, corruption_detected);
  RecordTimeAndErrorResultHistogram("OpenLatestEntryBeforeResId",
                                    timer.Elapsed(), Error::kOk,
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.OpenLatestEntryBeforeResId",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return result;
}

OptionalEntryInfoWithIdAndKey Backend::OpenLatestEntryBeforeResIdInternal(
    int64_t res_id_cursor,
    bool& corruption_detected) {
  CheckDatabaseInitStatus();

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      GetQuery(Query::kOpenLatestEntryBeforeResId_SelectLiveResources)));
  statement.BindInt64(0, res_id_cursor);
  while (statement.Step()) {
    auto maybe_token =
        ToUnguessableToken(statement.ColumnInt64(1), statement.ColumnInt64(2));
    if (!maybe_token) {
      // If OpenNextEntry encounters invalid data, it records it in a histogram
      // and ignores the data.
      corruption_detected = true;
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
      corruption_detected = true;
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

ErrorAndEvictionRequested Backend::RunEviction(
    base::flat_set<CacheEntryKey> excluded_keys) {
  TRACE_EVENT0("disk_cache", "SqlBackend.RunEviction");
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result =
      RunEvictionInternal(std::move(excluded_keys), corruption_detected);
  RecordTimeAndErrorResultHistogram("RunEviction", timer.Elapsed(), result,
                                    corruption_detected);
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndEvictionRequested(result, ShouldStartEviction());
}

Error Backend::RunEvictionInternal(
    const base::flat_set<CacheEntryKey>& excluded_keys,
    bool& corruption_detected) {
  int64_t size_to_be_removed = GetSizeOfAllEntries() - low_watermark_;
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToExecute;
  }

  std::vector<base::UnguessableToken> tokens_to_be_deleted;
  int64_t entry_count_delta = 0;
  // Use checked numerics to safely update the total cache size.
  base::CheckedNumeric<int64_t> checked_total_size_delta = 0;
  base::CheckedNumeric<int64_t> checked_removed_total_size = 0;
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kRunEviction_SelectLiveResources)));
    while (size_to_be_removed > checked_removed_total_size.ValueOrDie() &&
           statement.Step()) {
      if (excluded_keys.contains(CacheEntryKey(statement.ColumnString(2)))) {
        continue;
      }
      auto maybe_token = ToUnguessableToken(statement.ColumnInt64(0),
                                            statement.ColumnInt64(1));
      if (!maybe_token) {
        corruption_detected = true;
        continue;
      }
      tokens_to_be_deleted.push_back(*maybe_token);
      --entry_count_delta;
      const int64_t bytes_usage = statement.ColumnInt64(3);
      checked_total_size_delta -= bytes_usage;
      checked_removed_total_size += bytes_usage;
      checked_removed_total_size += kSqlBackendStaticResourceSize;
      if (!checked_total_size_delta.IsValid() ||
          !checked_removed_total_size.IsValid()) {
        corruption_detected = true;
        return Error::kInvalidData;
      }
    }
  }

  for (const auto& token_to_be_deleted : tokens_to_be_deleted) {
    if (Error delete_result = DeleteBlobsByToken(token_to_be_deleted);
        delete_result != Error::kOk) {
      return delete_result;
    }
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kRunEviction_DeleteFromResources)));
    statement.BindInt64(0, TokenHigh(token_to_be_deleted));
    statement.BindInt64(1, TokenLow(token_to_be_deleted));
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
  }
  return UpdateStoreStatusAndCommitTransaction(
      transaction, entry_count_delta, checked_total_size_delta.ValueOrDie(),
      corruption_detected);
}

Error Backend::UpdateStoreStatusAndCommitTransaction(
    sql::Transaction& transaction,
    int64_t entry_count_delta,
    int64_t total_size_delta,
    bool& corruption_detected) {
  const auto old_entry_count = store_status_.entry_count;
  const auto old_total_size = store_status_.total_size;
  if (entry_count_delta != 0) {
    // If the addition overflows or results in a negative count, it implies
    // corrupted metadata. In this case, log an error and recalculate the count
    // directly from the database to recover.
    if (!base::CheckAdd(store_status_.entry_count, entry_count_delta)
             .AssignIfValid(&store_status_.entry_count) ||
        store_status_.entry_count < 0) {
      corruption_detected = true;
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
      corruption_detected = true;
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
    return Error::kFailedToCommitTransaction;
  }
  return Error::kOk;
}

Error Backend::RecalculateStoreStatusAndCommitTransaction(
    sql::Transaction& transaction) {
  store_status_.entry_count = CalculateResourceEntryCount();
  store_status_.total_size = CalculateTotalSize();
  meta_table_.SetValue(kSqlBackendMetaTableKeyEntryCount,
                       store_status_.entry_count);
  meta_table_.SetValue(kSqlBackendMetaTableKeyTotalSize,
                       store_status_.total_size);
  return transaction.Commit() ? Error::kOk : Error::kFailedToCommitTransaction;
}

// Recalculates the number of non-doomed entries in the `resources` table.
int64_t Backend::CalculateResourceEntryCount() {
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      GetQuery(
          Query::kCalculateResourceEntryCount_SelectCountFromLiveResources)));
  int64_t result = 0;
  if (statement.Step()) {
    result = statement.ColumnInt64(0);
  }
  return result;
}

// Recalculates the total size of all non-doomed entries.
int64_t Backend::CalculateTotalSize() {
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      GetQuery(Query::kCalculateTotalSize_SelectTotalSizeFromLiveResources)));
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
        .Then(WrapCallbackWithEvictionRequested(std::move(callback)));
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
        .Then(WrapCallbackWithEvictionRequested(std::move(callback)));
  }
  void DoomEntry(const CacheEntryKey& key,
                 const base::UnguessableToken& token,
                 ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DoomEntry)
        .WithArgs(key, token)
        .Then(WrapCallbackWithEvictionRequested(std::move(callback)));
  }
  void DeleteDoomedEntry(const CacheEntryKey& key,
                         const base::UnguessableToken& token,
                         ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DeleteDoomedEntry)
        .WithArgs(key, token)
        .Then(WrapCallbackWithEvictionRequested(std::move(callback)));
  }
  void DeleteDoomedEntries(
      base::flat_set<base::UnguessableToken> excluded_tokens,
      ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DeleteDoomedEntries)
        .WithArgs(std::move(excluded_tokens))
        .Then(WrapCallback(std::move(callback)));
  }
  void DeleteLiveEntry(const CacheEntryKey& key,
                       ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DeleteLiveEntry)
        .WithArgs(key)
        .Then(WrapCallbackWithEvictionRequested(std::move(callback)));
  }
  void DeleteAllEntries(ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DeleteAllEntries)
        .Then(WrapCallbackWithEvictionRequested(std::move(callback)));
  }
  void DeleteLiveEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                base::flat_set<CacheEntryKey> excluded_keys,
                                ErrorCallback callback) override {
    backend_.AsyncCall(&Backend::DeleteLiveEntriesBetween)
        .WithArgs(initial_time, end_time, std::move(excluded_keys))
        .Then(WrapCallbackWithEvictionRequested(std::move(callback)));
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
        .Then(WrapCallbackWithEvictionRequested(std::move(callback)));
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
        .Then(WrapCallbackWithEvictionRequested(std::move(callback)));
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
  bool ShouldStartEviction() override {
    return !eviction_in_progress_ && eviction_requested_;
  }
  void StartEviction(base::flat_set<CacheEntryKey> excluded_keys,
                     ErrorCallback callback) override {
    CHECK(!eviction_in_progress_);
    eviction_in_progress_ = true;
    backend_.AsyncCall(&Backend::RunEviction)
        .WithArgs(std::move(excluded_keys))
        .Then(base::BindOnce(
            [](base::WeakPtr<SqlPersistentStoreImpl> weak_ptr,
               ErrorCallback callback, ErrorAndEvictionRequested result) {
              if (weak_ptr) {
                weak_ptr->eviction_in_progress_ = false;
                weak_ptr->eviction_requested_ = result.eviction_requested;
                std::move(callback).Run(result.result);
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

  void EnableStrictCorruptionCheckForTesting() override {
    backend_.AsyncCall(&Backend::EnableStrictCorruptionCheckForTesting);
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

  // Like `WrapCallback`, but also updates the `eviction_requested_` flag.
  template <typename ResultType>
  base::OnceCallback<void(ResultAndEvictionRequested<ResultType>)>
  WrapCallbackWithEvictionRequested(
      base::OnceCallback<void(ResultType)> callback) {
    return base::BindOnce(
        [](base::WeakPtr<SqlPersistentStoreImpl> weak_ptr,
           base::OnceCallback<void(ResultType)> callback,
           ResultAndEvictionRequested<ResultType> result) {
          if (weak_ptr) {
            weak_ptr->eviction_requested_ = result.eviction_requested;
            // We should not run the callback when `this` was deleted.
            std::move(callback).Run(std::move(result.result));
          }
        },
        weak_factory_.GetWeakPtr(), std::move(callback));
  }

  base::SequenceBound<Backend> backend_;

  int64_t max_size_ = 0;
  int64_t max_file_size_ = 0;
  bool eviction_in_progress_ = false;
  bool eviction_requested_ = false;

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
