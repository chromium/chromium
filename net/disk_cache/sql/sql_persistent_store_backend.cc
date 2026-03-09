// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_backend.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/memory_entry_data_hints.h"
#include "net/disk_cache/simple/simple_util.h"
#include "net/disk_cache/sql/eviction_candidate_aggregator.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_persistent_store_in_memory_index.h"
#include "net/disk_cache/sql/sql_persistent_store_queries.h"
#include "net/disk_cache/sql/sql_read_cache_memory_monitor.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace disk_cache {

using disk_cache_sql_queries::GetQuery;
using disk_cache_sql_queries::Query;

using Error = SqlPersistentStore::Error;
using EntryInfo = SqlPersistentStore::EntryInfo;
using ResId = SqlPersistentStore::ResId;
using ResIdAndShardId = SqlPersistentStore::ResIdAndShardId;
using StoreStatus = SqlPersistentStore::StoreStatus;
using EntryInfoWithKeyAndIterator =
    SqlPersistentStore::EntryInfoWithKeyAndIterator;
using ResIdList = SqlPersistentStore::ResIdList;
using EntryInfoOrError = SqlPersistentStore::EntryInfoOrError;
using EntryInfoOrErrorAndStoreStatus =
    SqlPersistentStore::EntryInfoOrErrorAndStoreStatus;
using OptionalEntryInfoOrError = SqlPersistentStore::OptionalEntryInfoOrError;
using ErrorAndStoreStatus = SqlPersistentStore::ErrorAndStoreStatus;
using ResIdListOrErrorAndStoreStatus =
    SqlPersistentStore::ResIdListOrErrorAndStoreStatus;
using ResIdListOrError = SqlPersistentStore::ResIdListOrError;
using ResIdOrError = SqlPersistentStore::ResIdOrError;
using ResIdOrErrorAndStoreStatus =
    SqlPersistentStore::ResIdOrErrorAndStoreStatus;
using ReadResult = SqlPersistentStore::ReadResult;
using ReadResultOrError = SqlPersistentStore::ReadResultOrError;
using Int64OrError = SqlPersistentStore::Int64OrError;
using OptionalEntryInfoWithKeyAndIterator =
    SqlPersistentStore::OptionalEntryInfoWithKeyAndIterator;

using InMemoryIndexAndDoomedResIds =
    SqlPersistentStore::InMemoryIndexAndDoomedResIds;
using EvictionResultOrError = SqlPersistentStore::EvictionResultOrError;
using EvictionResultOrErrorAndStoreStatus =
    SqlPersistentStore::EvictionResultOrErrorAndStoreStatus;
namespace {

bool IsBlobSizeValid(int64_t blob_start,
                     int64_t blob_end,
                     const base::span<const uint8_t>& blob) {
  size_t blob_size;
  if (!base::CheckSub(blob_end, blob_start).AssignIfValid(&blob_size)) {
    return false;
  }
  return blob.size() == blob_size;
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
  dict.Add("res_id", entry_info.res_id.value());
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
void PopulateTraceDetails(const ReadResult& read_result,
                          perfetto::TracedDictionary& dict) {
  dict.Add("read_bytes", read_result.read_bytes);
  dict.Add("cache_buffer_size",
           read_result.cache_buffer ? read_result.cache_buffer->size() : 0);
  dict.Add("cache_buffer_offset", read_result.cache_buffer_offset);
}
void PopulateTraceDetails(const EntryInfoWithKeyAndIterator& result,
                          perfetto::TracedDictionary& dict) {
  PopulateTraceDetails(result.info, dict);
  dict.Add("iterator_res_id", result.iterator.value().res_id);
  dict.Add("key", result.key.string());
}
void PopulateTraceDetails(
    const std::optional<EntryInfoWithKeyAndIterator>& entry_info,
    perfetto::TracedDictionary& dict) {
  if (entry_info) {
    PopulateTraceDetails(*entry_info, dict);
  } else {
    dict.Add("entry_info", "not found");
  }
}
void PopulateTraceDetails(const ResIdList& result,
                          perfetto::TracedDictionary& dict) {
  dict.Add("doomed_entry_count", result.size());
}
void PopulateTraceDetails(const InMemoryIndexAndDoomedResIds& result,
                          perfetto::TracedDictionary& dict) {
  dict.Add("index_size", result.index.size());
  dict.Add("doomed_entry_count", result.doomed_entry_res_ids.size());
}
void PopulateTraceDetails(const SqlPersistentStore::EvictionResult& result,
                          perfetto::TracedDictionary& dict) {
  dict.Add("deleted", result.deleted_res_ids.size());
  dict.Add("pending", result.pending_eviction_targets.size());
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

// A helper function to record the time delay from posting a task to its
// execution.
void RecordPostingDelay(std::string_view method_name,
                        base::TimeDelta posting_delay) {
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat(
          {kSqlDiskCacheBackendHistogramPrefix, method_name, ".PostingDelay"}),
      posting_delay);
}

// Records timing and result histograms for a backend method. This logs the
// method's duration to ".SuccessTime" or ".FailureTime" histograms and the
// `Error` code to a ".Result" histogram.
void RecordTimeAndErrorResultHistogram(std::string_view method_name,
                                       base::TimeDelta posting_delay,
                                       base::TimeDelta time_delta,
                                       Error error,
                                       bool corruption_detected) {
  RecordPostingDelay(method_name, posting_delay);
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, method_name,
                    error == Error::kOk ? ".SuccessTime" : ".FailureTime",
                    corruption_detected ? "WithCorruption" : ""}),
      time_delta);
  base::UmaHistogramEnumeration(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, method_name,
                    corruption_detected ? ".ResultWithCorruption" : ".Result"}),
      error);
}

int32_t CalculateCheckSum(base::span<const uint8_t> data,
                          CacheEntryKey::Hash key_hash) {
  // Add key_hash in network order to the CRC calculation to ensure it can be
  // read correctly on CPUs with different endianness.
  uint32_t hash_value_net_order =
      base::HostToNet32(static_cast<uint32_t>(key_hash.value()));
  uint32_t crc32_value = simple_util::IncrementalCrc32(
      simple_util::Crc32(data), base::byte_span_from_ref(hash_value_net_order));
  return static_cast<int32_t>(crc32_value);
}

// Sets up the database schema and indexes.
[[nodiscard]] bool InitSchema(sql::Database& db) {
  if (!db.Execute(GetQuery(Query::kInitSchema_CreateTableResources)) ||
      !db.Execute(GetQuery(Query::kInitSchema_CreateTableBlobs)) ||
      !db.Execute(GetQuery(Query::kIndex_ResourcesCacheKeyHashDoomed)) ||
      !db.Execute(GetQuery(Query::kIndex_LiveResourcesLastUsed)) ||
      !db.Execute(GetQuery(Query::kIndex_LiveResourcesHints)) ||
      !db.Execute(GetQuery(Query::kIndex_BlobsResIdStart))) {
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

bool IsBrowserIdle() {
  return performance_scenarios::CurrentScenariosMatch(
      performance_scenarios::ScenarioScope::kGlobal,
      performance_scenarios::kDefaultIdleScenarios);
}

class MonitoredVectorIOBuffer : public net::IOBuffer {
 public:
  MonitoredVectorIOBuffer(base::span<const uint8_t> data,
                          scoped_refptr<SqlReadCacheMemoryMonitor> monitor)
      : monitor_(std::move(monitor)), vector_(data.begin(), data.end()) {
    SetSpan(vector_);
  }

 private:
  ~MonitoredVectorIOBuffer() override {
    ClearSpan();
    if (monitor_) {
      monitor_->ReleaseBytes(vector_.size());
    }
  }

  scoped_refptr<SqlReadCacheMemoryMonitor> monitor_;
  std::vector<uint8_t> vector_;
};

}  // namespace

SqlPersistentStore::Backend::Backend(
    ShardId shard_id,
    const base::FilePath& path,
    net::CacheType type,
    scoped_refptr<SqlReadCacheMemoryMonitor> read_cache_memory_monitor)
    : shard_id_(shard_id),
      path_(path),
      type_(type),
      read_cache_memory_monitor_(std::move(read_cache_memory_monitor)),
      db_(sql::DatabaseOptions()
#if BUILDFLAG(IS_WIN)
              .set_exclusive_database_file_lock(true)
#endif  // IS_WIN
              .set_preload(net::features::kSqlDiskCachePreloadDatabase.Get())
              .set_wal_mode(true)
              .set_no_sync_on_wal_mode(
                  net::features::kSqlDiskCacheSynchronousOff.Get())
              .set_wal_commit_callback(base::BindRepeating(
                  &Backend::OnCommitCallback,
                  // This callback is only called while the `db_` instance
                  // is alive, and never during destructor, so it's safe
                  // to use base::Unretained.
                  base::Unretained(this)))
              .set_release_memory_after_writes(
                  net::features::kSqlDiskCacheReleaseMemoryAfterWrites.Get())
              .set_cache_size(net::features::kSqlDiskCacheCacheSize.Get()),
          // Tag for metrics collection.
          sql::Database::Tag("HttpCacheDiskCache")) {
}

SqlPersistentStore::Backend::~Backend() = default;

Error SqlPersistentStore::Backend::CheckDatabaseStatus() {
  if (simulate_db_failure_for_testing_) {
    return Error::kFailedForTesting;
  }
  if (!db_init_status_.has_value() || *db_init_status_ != Error::kOk) {
    return Error::kNotInitialized;
  }
  if (!db_.is_open()) {
    // The database have been closed when a catastrophic error occurred and
    // RazeAndPoison() was called.
    return Error::kDatabaseClosed;
  }
  return Error::kOk;
}

SqlPersistentStore::InitResultOrError SqlPersistentStore::Backend::Initialize(
    int64_t user_max_bytes,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN0("disk_cache", "SqlBackend.Initialize");
  base::ElapsedTimer timer;
  CHECK(!db_init_status_.has_value());
  bool corruption_detected = false;
  db_init_status_ = InitializeInternal(corruption_detected);

  std::optional<int64_t> result_max_bytes;
  // `max_bytes` of InitResult is set only for the first shard.
  if (shard_id_ == ShardId(0)) {
    // If the specified max_bytes is valid, use it. Otherwise, calculate a
    // preferred size based on available disk space.
    result_max_bytes =
        user_max_bytes > 0
            ? user_max_bytes
            : PreferredCacheSize(
                  base::SysInfo::AmountOfFreeDiskSpace(path_).value_or(-1),
                  type_);
  }
  std::optional<InMemoryIndexAndDoomedResIds> in_memory_data;
  if (net::features::kSqlDiskCacheLoadIndexOnInit.Get()) {
    if (auto in_memory_index_result = LoadInMemoryIndex();
        in_memory_index_result.has_value()) {
      in_memory_data = std::move(in_memory_index_result.value());
    }
  }
  RecordTimeAndErrorResultHistogram("Initialize", posting_delay,
                                    timer.Elapsed(), *db_init_status_,
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.Initialize", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(*db_init_status_, store_status_,
                                          dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return *db_init_status_ == Error::kOk
             ? InitResultOrError(InitResult(
                   result_max_bytes, store_status_,
                   base::GetFileSize(GetDatabaseFilePath()).value_or(0),
                   std::move(in_memory_data)))
             : base::unexpected(*db_init_status_);
}

Error SqlPersistentStore::Backend::InitializeInternal(
    bool& corruption_detected) {
  if (simulate_db_failure_for_testing_) {
    return Error::kFailedForTesting;
  }
  CHECK(!db_init_status_.has_value());

  db_.set_error_callback(base::BindRepeating(&Backend::DatabaseErrorCallback,
                                             base::Unretained(this)));

  base::FilePath db_file_path = GetDatabaseFilePath();
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

  if (!sql::MetaTable::DoesTableExist(&db_)) {
    // Initialize the database schema.
    if (!InitSchema(db_)) {
      return Error::kFailedToInitializeSchema;
    }
  }

  // Initialize the meta table, which stores version info and other metadata.
  if (!meta_table_.Init(&db_, kSqlBackendCurrentDatabaseVersion,
                        kSqlBackendCompatibleDatabaseVersion)) {
    return Error::kFailedToInitializeMetaTable;
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

void SqlPersistentStore::Backend::DatabaseErrorCallback(
    int error,
    sql::Statement* statement) {
  TRACE_EVENT("disk_cache", "SqlBackend.Error", "error", error);
  sql::UmaHistogramSqliteResult(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, "SqliteError"}),
      error);
  // For the HTTP Cache, a kFullDisk error is not recoverable and freeing up
  // disk space is the best course of action. So, we treat it as a catastrophic
  // error to raze the database.
  if ((sql::IsErrorCatastrophic(error) ||
       error == static_cast<int>(sql::SqliteErrorCode::kFullDisk)) &&
      db_.is_open()) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndPoison() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    db_.RazeAndPoison();
    store_status_ = StoreStatus();
  }
}

int32_t SqlPersistentStore::Backend::GetEntryCount() const {
  return store_status_.entry_count;
}

EntryInfoOrErrorAndStoreStatus SqlPersistentStore::Backend::OpenOrCreateEntry(
    const CacheEntryKey& key,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.OpenOrCreateEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = OpenOrCreateEntryInternal(key, corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "OpenOrCreateEntry", posting_delay, timer.Elapsed(),
      result.error_or(Error::kOk), corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.OpenOrCreateEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return EntryInfoOrErrorAndStoreStatus(std::move(result), store_status_);
}

EntryInfoOrError SqlPersistentStore::Backend::OpenOrCreateEntryInternal(
    const CacheEntryKey& key,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
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
  return CreateEntryInternal(key, base::Time::Now(),
                             /*run_existance_check=*/false,
                             corruption_detected);
}

OptionalEntryInfoOrError SqlPersistentStore::Backend::OpenEntry(
    const CacheEntryKey& key,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.OpenEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  auto result = OpenEntryInternal(key);
  RecordTimeAndErrorResultHistogram("OpenEntry", posting_delay, timer.Elapsed(),
                                    result.error_or(Error::kOk),
                                    /*corruption_detected=*/false);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.OpenEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

OptionalEntryInfoOrError SqlPersistentStore::Backend::OpenEntryInternal(
    const CacheEntryKey& key) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kOpenEntry_SelectLiveResources)));
  statement.BindInt(0, key.hash().value());
  statement.BindString(1, key.string());
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
  entry_info.res_id = ResId(statement.ColumnInt64(0));
  entry_info.last_used = statement.ColumnTime(1);
  entry_info.body_end = statement.ColumnInt64(2);
  int32_t check_sum = statement.ColumnInt(3);
  base::span<const uint8_t> blob_span = statement.ColumnBlob(4);
  if (CalculateCheckSum(blob_span, key.hash()) != check_sum) {
    return base::unexpected(Error::kCheckSumError);
  }
  entry_info.head = base::MakeRefCounted<net::GrowableIOBuffer>();
  CHECK(base::IsValueInRangeForNumericType<int>(blob_span.size()));
  entry_info.head->SetCapacity(blob_span.size());
  entry_info.head->span().copy_from_nonoverlapping(blob_span);
  entry_info.opened = true;
  return entry_info;
}

EntryInfoOrErrorAndStoreStatus SqlPersistentStore::Backend::CreateEntry(
    const CacheEntryKey& key,
    base::Time creation_time,
    bool run_existance_check,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.CreateEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = CreateEntryInternal(key, creation_time, run_existance_check,
                                    corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "CreateEntry", posting_delay, timer.Elapsed(),
      result.error_or(Error::kOk), corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.CreateEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return EntryInfoOrErrorAndStoreStatus(std::move(result), store_status_);
}

EntryInfoOrError SqlPersistentStore::Backend::CreateEntryInternal(
    const CacheEntryKey& key,
    base::Time creation_time,
    bool run_existance_check,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
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
  entry_info.last_used = creation_time;
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
    statement.BindTime(0, entry_info.last_used);
    statement.BindInt64(1, entry_info.body_end);
    statement.BindInt64(2, bytes_usage);
    statement.BindBool(3, false);  // doomed
    statement.BindInt(4, CalculateCheckSum({}, key.hash()));
    statement.BindInt(5, key.hash().value());
    statement.BindString(6, key.string());
    if (!statement.Step()) {
      return base::unexpected(Error::kFailedToExecute);
    }
    entry_info.res_id = ResId(statement.ColumnInt64(0));
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

ErrorAndStoreStatus SqlPersistentStore::Backend::DoomEntry(
    const CacheEntryKey& key,
    ResId res_id,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DoomEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       dict.Add("res_id", res_id.value());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = DoomEntryInternal(res_id, corruption_detected);
  RecordTimeAndErrorResultHistogram("DoomEntry", posting_delay, timer.Elapsed(),
                                    result, corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DoomEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                     dict.Add("corruption_detected", corruption_detected);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndStoreStatus(result, store_status_);
}

Error SqlPersistentStore::Backend::DoomEntryInternal(
    ResId res_id,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return db_error;
  }
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
    statement.BindInt64(0, res_id.value());
    // Iterate through the rows returned by the RETURNING clause.
    while (statement.Step()) {
      // Since we're dooming an entry, its size is subtracted from the total.
      total_size_delta -= statement.ColumnInt64(0);
      // Count how many entries were actually updated.
      ++doomed_count;
    }
  }
  // The res_id should uniquely identify a single non-doomed entry.
  CHECK_LE(doomed_count, 1);

  // If no rows were updated, it means the entry was not found, so we report
  // kNotFound.
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

ErrorAndStoreStatus SqlPersistentStore::Backend::DeleteDoomedEntry(
    const CacheEntryKey& key,
    ResId res_id,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteDoomedEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       dict.Add("res_id", res_id.value());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  auto result = DeleteDoomedEntryInternal(res_id);
  RecordTimeAndErrorResultHistogram("DeleteDoomedEntry", posting_delay,
                                    timer.Elapsed(), result,
                                    /*corruption_detected=*/false);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteDoomedEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return ErrorAndStoreStatus(result, store_status_);
}

Error SqlPersistentStore::Backend::DeleteDoomedEntryInternal(ResId res_id) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return db_error;
  }
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  int64_t deleted_count = 0;
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kDeleteDoomedEntry_DeleteFromResources)));
    statement.BindInt64(0, res_id.value());
    if (!statement.Run()) {
      return Error::kFailedToExecute;
    }
    deleted_count = db_.GetLastChangeCount();
  }
  // The res_id should uniquely identify a single doomed entry.
  CHECK_LE(deleted_count, 1);

  // If we didn't find any doomed entry matching the res_id, report it.
  if (deleted_count == 0) {
    return transaction.Commit() ? Error::kNotFound
                                : Error::kFailedToCommitTransaction;
  }

  // Delete the associated blobs from the `blobs` table.
  if (Error error = DeleteBlobsByResId(res_id); error != Error::kOk) {
    return error;
  }

  return transaction.Commit() ? Error::kOk : Error::kFailedToCommitTransaction;
}

Error SqlPersistentStore::Backend::DeleteDoomedEntries(
    ResIdList res_ids_to_delete,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN0("disk_cache", "SqlBackend.DeleteDoomedEntries");
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result =
      DeleteDoomedEntriesInternal(res_ids_to_delete, corruption_detected);
  RecordTimeAndErrorResultHistogram("DeleteDoomedEntries", posting_delay,
                                    timer.Elapsed(), result,
                                    corruption_detected);
  base::UmaHistogramCounts100("Net.SqlDiskCache.DeleteDoomedEntriesCount",
                              res_ids_to_delete.size());
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteDoomedEntries", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                     dict.Add("deleted_count", res_ids_to_delete.size());
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return result;
}

Error SqlPersistentStore::Backend::DeleteDoomedEntriesInternal(
    const ResIdList& res_ids_to_delete,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return db_error;
  }
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }

  // 1. Delete from `resources` table by `res_id`.
  if (auto error = DeleteResourcesByResIds(res_ids_to_delete);
      error != Error::kOk) {
    return error;
  }

  // 2. Delete corresponding blobs by res_id.
  if (auto error = DeleteBlobsByResIds(res_ids_to_delete);
      error != Error::kOk) {
    return error;
  }

  // 3. Commit the transaction.
  // Note: The entries for the res IDs passed to this method are assumed to be
  // doomed, so store_status_'s entry_count and total_size are not updated.
  return transaction.Commit() ? Error::kOk : Error::kFailedToCommitTransaction;
}

ResIdListOrErrorAndStoreStatus SqlPersistentStore::Backend::DeleteLiveEntry(
    const CacheEntryKey& key,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
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
      "DeleteLiveEntry", posting_delay, timer.Elapsed(),
      result.error_or(Error::kOk), corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteLiveEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                     dict.Add("corruption_detected", corruption_detected);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ResIdListOrErrorAndStoreStatus(std::move(result), store_status_);
}

ResIdListOrError SqlPersistentStore::Backend::DeleteLiveEntryInternal(
    const CacheEntryKey& key,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }

  // We need to collect the res_ids of deleted entries to later remove their
  // corresponding data from the `blobs` table.
  ResIdList res_ids_to_be_deleted;
  // Use checked numerics to safely update the total cache size.
  base::CheckedNumeric<int64_t> total_size_delta = 0;
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kDeleteLiveEntry_DeleteFromResources)));
    statement.BindInt(0, key.hash().value());
    statement.BindString(1, key.string());
    while (statement.Step()) {
      const auto res_id = ResId(statement.ColumnInt64(0));
      res_ids_to_be_deleted.emplace_back(res_id);
      // The size of the deleted entry is subtracted from the total.
      total_size_delta -= statement.ColumnInt64(1);
    }
  }

  // If no entries were deleted, the key wasn't found.
  if (res_ids_to_be_deleted.empty()) {
    return transaction.Commit()
               ? base::unexpected(Error::kNotFound)
               : base::unexpected(Error::kFailedToCommitTransaction);
  }

  // Delete the blobs associated with the deleted entries.
  if (Error delete_result = DeleteBlobsByResIds(res_ids_to_be_deleted);
      delete_result != Error::kOk) {
    // If blob deletion fails, returns the error. The transaction will be
    // rolled back. So no need to return `deleted_enties`.
    return base::unexpected(delete_result);
  }

  // If we detected corruption, or if the size update calculation overflowed,
  // our metadata is suspect. We recover by recalculating everything from
  // scratch.
  if (corruption_detected || !total_size_delta.IsValid()) {
    corruption_detected = true;
    auto error = RecalculateStoreStatusAndCommitTransaction(transaction);
    return error == Error::kOk
               ? ResIdListOrError(std::move(res_ids_to_be_deleted))
               : base::unexpected(error);
  }

  auto error = UpdateStoreStatusAndCommitTransaction(
      transaction,
      /*entry_count_delta=*/
      -static_cast<int64_t>(res_ids_to_be_deleted.size()),
      /*total_size_delta=*/total_size_delta.ValueOrDie(), corruption_detected);
  return error == Error::kOk
             ? ResIdListOrError(std::move(res_ids_to_be_deleted))
             : base::unexpected(error);
}

ErrorAndStoreStatus SqlPersistentStore::Backend::DeleteAllEntries(
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteAllEntries", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  Error result = DeleteAllEntriesInternal(corruption_detected);
  RecordTimeAndErrorResultHistogram("DeleteAllEntries", posting_delay,
                                    timer.Elapsed(), result,
                                    corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteAllEntries", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ErrorAndStoreStatus(result, store_status_);
}

Error SqlPersistentStore::Backend::DeleteAllEntriesInternal(
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return db_error;
  }
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

ResIdListOrErrorAndStoreStatus
SqlPersistentStore::Backend::DeleteLiveEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    base::flat_set<ResId> excluded_res_ids,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.DeleteLiveEntriesBetween",
                     "data", [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("initial_time", initial_time);
                       dict.Add("end_time", end_time);
                       dict.Add("excluded_res_ids_size",
                                excluded_res_ids.size());
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  // Flag to indicate if we encounter signs of database corruption. In
  // DeleteLiveEntriesBetween, database corruption is ignored.
  bool corruption_detected = false;
  auto result = DeleteLiveEntriesBetweenInternal(
      initial_time, end_time, excluded_res_ids, corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "DeleteLiveEntriesBetween", posting_delay, timer.Elapsed(),
      result.error_or(Error::kOk), corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.DeleteLiveEntriesBetween",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ResIdListOrErrorAndStoreStatus(std::move(result), store_status_);
}

ResIdListOrError SqlPersistentStore::Backend::DeleteLiveEntriesBetweenInternal(
    base::Time initial_time,
    base::Time end_time,
    const base::flat_set<ResId>& excluded_res_ids,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }

  ResIdList res_ids_to_be_deleted;
  base::CheckedNumeric<int64_t> total_size_delta = 0;
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kDeleteLiveEntriesBetween_SelectLiveResources)));
    statement.BindTime(0, initial_time);
    statement.BindTime(1, end_time);
    while (statement.Step()) {
      const auto res_id = ResId(statement.ColumnInt64(0));
      if (excluded_res_ids.contains(res_id)) {
        continue;
      }
      res_ids_to_be_deleted.emplace_back(res_id);
      total_size_delta -= statement.ColumnInt64(1);
    }
  }

  // Delete the blobs associated with the entries to be deleted.
  if (auto error = DeleteBlobsByResIds(res_ids_to_be_deleted);
      error != Error::kOk) {
    return base::unexpected(error);
  }

  // Delete the selected entries from the `resources` table.
  if (auto error = DeleteResourcesByResIds(res_ids_to_be_deleted);
      error != Error::kOk) {
    return base::unexpected(error);
  }

  // If we detected corruption, or if the size update calculation overflowed,
  // our metadata is suspect. We recover by recalculating everything from
  // scratch.
  if (corruption_detected || !total_size_delta.IsValid()) {
    corruption_detected = true;
    auto error = RecalculateStoreStatusAndCommitTransaction(transaction);
    return error == Error::kOk
               ? ResIdListOrError(std::move(res_ids_to_be_deleted))
               : base::unexpected(error);
  }

  // Update the in-memory and on-disk store status (entry count and total size)
  // and commit the transaction.
  auto error = UpdateStoreStatusAndCommitTransaction(
      transaction, -static_cast<int64_t>(res_ids_to_be_deleted.size()),
      total_size_delta.ValueOrDie(), corruption_detected);
  return error == Error::kOk
             ? ResIdListOrError(std::move(res_ids_to_be_deleted))
             : base::unexpected(error);
}

Error SqlPersistentStore::Backend::UpdateEntryLastUsedByKey(
    const CacheEntryKey& key,
    base::Time last_used,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.UpdateEntryLastUsedByKey",
                     "data", [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       dict.Add("last_used", last_used);
                     });
  base::ElapsedTimer timer;
  auto result = UpdateEntryLastUsedByKeyInternal(key, last_used);
  RecordTimeAndErrorResultHistogram("UpdateEntryLastUsedByKey", posting_delay,
                                    timer.Elapsed(), result,
                                    /*corruption_detected=*/false);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.UpdateEntryLastUsedByKey",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, dict);
                   });
  return result;
}

Error SqlPersistentStore::Backend::UpdateEntryLastUsedByKeyInternal(
    const CacheEntryKey& key,
    base::Time last_used) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return db_error;
  }
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return Error::kFailedToStartTransaction;
  }
  int64_t change_count = 0;
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kUpdateEntryLastUsedByKey_UpdateResourceLastUsed)));
    statement.BindTime(0, last_used);
    statement.BindInt(1, key.hash().value());
    statement.BindString(2, key.string());
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

ResIdOrErrorAndStoreStatus
SqlPersistentStore::Backend::WriteEntryDataAndMetadata(
    const CacheEntryKey& key,
    std::optional<ResId> res_id,
    std::optional<int64_t> old_body_end,
    EntryWriteBuffer buffer,
    base::Time last_used,
    const std::optional<MemoryEntryDataHints>& new_hints,
    scoped_refptr<net::IOBuffer> head_buffer,
    int64_t header_size_delta,
    bool doomed_new_entry,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.WriteEntryDataAndMetadata",
                     "data", [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       if (res_id) {
                         dict.Add("res_id", res_id->value());
                       }
                       dict.Add("has_body_write", old_body_end.has_value());
                       dict.Add("last_used", last_used);
                       dict.Add("doomed_new_entry", doomed_new_entry);
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = WriteEntryDataAndMetadataInternal(
      key, res_id, old_body_end, std::move(buffer), last_used, new_hints,
      std::move(head_buffer), header_size_delta, doomed_new_entry,
      corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "WriteEntryDataAndMetadata", posting_delay, timer.Elapsed(),
      result.has_value() ? Error::kOk : result.error(), corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.WriteEntryDataAndMetadata",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(
                         result.has_value() ? Error::kOk : result.error(),
                         store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ResIdOrErrorAndStoreStatus(result, store_status_);
}

Error SqlPersistentStore::Backend::WriteEntryBodyDataHelper(
    const CacheEntryKey& key,
    ResId res_id,
    int64_t old_body_end,
    EntryWriteBuffer buffer,
    bool truncate,
    int64_t& body_end_delta,
    base::CheckedNumeric<int64_t>& checked_total_size_delta,
    int64_t& new_body_end,
    bool& corruption_detected) {
  const int64_t offset = buffer.offset;
  const int buf_len = buffer.size;

  scoped_refptr<net::IOBuffer> combined_buffer;
  if (buffer.buffers.size() == 1) {
    combined_buffer = std::move(buffer.buffers[0]);
  } else if (!buffer.buffers.empty()) {
    auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(buf_len);
    size_t current_offset = 0;
    for (const auto& chunk : buffer.buffers) {
      base::as_writable_bytes(io_buffer->span())
          .subspan(current_offset)
          .copy_prefix_from(base::as_bytes(chunk->span()));
      current_offset += chunk->size();
    }
    combined_buffer = std::move(io_buffer);
  }

  int64_t write_end;
  if (old_body_end < 0 || offset < 0 || buf_len < 0 ||
      (!combined_buffer && buf_len > 0) ||
      (combined_buffer && buf_len > combined_buffer->size()) ||
      !base::CheckAdd<int64_t>(offset, buf_len).AssignIfValid(&write_end)) {
    return Error::kInvalidArgument;
  }

  new_body_end = truncate ? write_end : std::max(write_end, old_body_end);
  // An overflow is not expected here, as both `new_body_end` and `old_body_end`
  // are non-negative int64_t value.
  body_end_delta = new_body_end - old_body_end;

  if (offset < old_body_end) {
    if (Error result =
            TrimOverlappingBlobs(key, res_id, offset, write_end, truncate,
                                 checked_total_size_delta, corruption_detected);
        result != Error::kOk) {
      return result;
    }
  }

  if (body_end_delta < 0) {
    CHECK(truncate);
    if (Error result =
            TruncateBlobsAfter(res_id, new_body_end, checked_total_size_delta);
        result != Error::kOk) {
      return result;
    }
  }

  if (buf_len) {
    if (Error result = InsertNewBlob(key, res_id, offset, combined_buffer,
                                     buf_len, checked_total_size_delta);
        result != Error::kOk) {
      return result;
    }
  }
  return Error::kOk;
}

ResIdOrError SqlPersistentStore::Backend::WriteEntryDataAndMetadataInternal(
    const CacheEntryKey& key,
    std::optional<ResId> res_id,
    std::optional<int64_t> old_body_end,
    EntryWriteBuffer buffer,
    base::Time last_used,
    const std::optional<MemoryEntryDataHints>& new_hints,
    scoped_refptr<net::IOBuffer> head_buffer,
    int64_t header_size_delta,
    bool doomed_new_entry,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }

  base::CheckedNumeric<int64_t> checked_total_size_delta = 0;

  const bool is_new_entry = !res_id.has_value();
  int64_t body_end_for_new_entry = 0;
  int64_t bytes_usage_for_new_entry = 0;

  // If `res_id` is nullopt, create a new entry in the resources table.
  if (is_new_entry) {
    CHECK_EQ(old_body_end.value_or(0), 0);
    CHECK_EQ(old_body_end.has_value(), buffer.size != 0);
    checked_total_size_delta += key.string().size();
    body_end_for_new_entry = buffer.offset + buffer.size;
    bytes_usage_for_new_entry = key.string().size() + buffer.size +
                                (head_buffer ? head_buffer->size() : 0);
    auto blob_span =
        head_buffer ? head_buffer->span() : base::span<const uint8_t>();
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(disk_cache_sql_queries::Query::kInsertIntoResources)));
    statement.BindTime(0, last_used);
    statement.BindInt(1, new_hints.value_or(MemoryEntryDataHints(0)).value());
    statement.BindInt64(2, body_end_for_new_entry);
    statement.BindInt64(3, bytes_usage_for_new_entry);
    statement.BindBool(4, doomed_new_entry);
    statement.BindInt(5, CalculateCheckSum(blob_span, key.hash()));
    statement.BindInt(6, key.hash().value());
    statement.BindString(7, key.string());
    // SAFETY: The memory referenced by `blob_span` must outlive `statement`.
    statement.BindBlob(
        8, base::MakeRefCounted<base::RefCountedStaticMemory>(blob_span));

    if (!statement.Step()) {
      return base::unexpected(Error::kFailedToExecute);
    }
    res_id = ResId(statement.ColumnInt64(0));
  }

  int64_t body_end_delta = 0;
  int64_t new_body_end = 0;

  // Write the body to the blobs table.
  if (old_body_end.has_value()) {
    if (Error result = WriteEntryBodyDataHelper(
            key, *res_id, *old_body_end, std::move(buffer), /*truncate=*/false,
            body_end_delta, checked_total_size_delta, new_body_end,
            corruption_detected);
        result != Error::kOk) {
      return base::unexpected(result);
    }
  }

  if (head_buffer) {
    checked_total_size_delta += header_size_delta;
  }

  if (!checked_total_size_delta.IsValid()) {
    corruption_detected = true;
    return base::unexpected(Error::kInvalidData);
  }
  int64_t total_size_delta = checked_total_size_delta.ValueOrDie();

  if (is_new_entry) {
    CHECK_EQ(body_end_delta, body_end_for_new_entry);
    CHECK_EQ(total_size_delta, bytes_usage_for_new_entry);
    if (doomed_new_entry) {
      total_size_delta = 0;
    }
  } else {
    // Update the entry in the resources table.
    const bool has_hints = new_hints.has_value();
    const bool has_body = old_body_end.has_value();
    const bool has_head = head_buffer != nullptr;
    sql::Statement statement(
        has_head
            ? (has_body
                   ? (has_hints
                          ? db_.GetCachedStatement(
                                SQL_FROM_HERE,
                                GetQuery(Query::kUpdateLastUsedBodyHeaderHints))
                          : db_.GetCachedStatement(
                                SQL_FROM_HERE,
                                GetQuery(Query::kUpdateLastUsedBodyHeader)))
                   : (has_hints
                          ? db_.GetCachedStatement(
                                SQL_FROM_HERE,
                                GetQuery(Query::kUpdateLastUsedHeaderHints))
                          : db_.GetCachedStatement(
                                SQL_FROM_HERE,
                                GetQuery(Query::kUpdateLastUsedHeader))))
            : (has_body
                   ? (has_hints ? db_.GetCachedStatement(
                                      SQL_FROM_HERE,
                                      GetQuery(Query::kUpdateLastUsedBodyHints))
                                : db_.GetCachedStatement(
                                      SQL_FROM_HERE,
                                      GetQuery(Query::kUpdateLastUsedBody)))
                   : (has_hints ? db_.GetCachedStatement(
                                      SQL_FROM_HERE,
                                      GetQuery(Query::kUpdateLastUsedHints))
                                : db_.GetCachedStatement(
                                      SQL_FROM_HERE,
                                      GetQuery(Query::kUpdateLastUsed)))));

    int param_index = 0;
    statement.BindTime(param_index++, last_used);
    if (has_hints) {
      statement.BindInt(param_index++, new_hints->value());
    }
    if (has_body) {
      statement.BindInt64(param_index++, body_end_delta);
    }
    if (has_body || has_head) {
      statement.BindInt64(param_index++, total_size_delta);
    }
    if (has_head) {
      statement.BindInt(param_index++,
                        CalculateCheckSum(head_buffer->span(), key.hash()));
      // SAFETY: The memory referenced by `head_buffer` must outlive
      // `statement`.
      statement.BindBlob(param_index++,
                         base::MakeRefCounted<base::RefCountedStaticMemory>(
                             head_buffer->span()));
    }
    statement.BindInt64(param_index++, res_id->value());

    if (statement.Step()) {
      int col = 0;
      if (has_head || has_body) {
        const int64_t bytes_usage = statement.ColumnInt64(col++);
        if (bytes_usage <
            static_cast<int64_t>(head_buffer ? head_buffer->size() : 0) +
                static_cast<int64_t>(key.string().size())) {
          // This indicates data corruption in the database.
          corruption_detected = true;
          return base::unexpected(Error::kInvalidData);
        }
      }
      if (has_body) {
        if (statement.ColumnInt64(col++) != new_body_end) {
          // This indicates data corruption in the database.
          corruption_detected = true;
          return base::unexpected(Error::kBodyEndMismatch);
        }
      }
    } else {
      return base::unexpected(Error::kNotFound);
    }
  }

  if (auto error = UpdateStoreStatusAndCommitTransaction(
          transaction,
          /*entry_count_delta=*/is_new_entry && !doomed_new_entry ? 1 : 0,
          /*total_size_delta=*/total_size_delta, corruption_detected);
      error != Error::kOk) {
    return base::unexpected(error);
  }
  return *res_id;
}

ResIdOrErrorAndStoreStatus SqlPersistentStore::Backend::WriteEntryData(
    const CacheEntryKey& key,
    const ResIdOrTime& res_id_or_last_used_time,
    int64_t old_body_end,
    EntryWriteBuffer buffer,
    bool truncate,
    bool doomed_new_entry,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.WriteEntryData", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("key", key.string());
                       if (std::holds_alternative<ResId>(
                               res_id_or_last_used_time)) {
                         dict.Add(
                             "res_id",
                             std::get<ResId>(res_id_or_last_used_time).value());
                       } else {
                         dict.Add("last_used", std::get<base::Time>(
                                                   res_id_or_last_used_time));
                       }
                       dict.Add("old_body_end", old_body_end);
                       dict.Add("offset", buffer.offset);
                       dict.Add("buf_len", buffer.size);
                       dict.Add("truncate", truncate);
                       dict.Add("doomed_new_entry", doomed_new_entry);
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = WriteEntryDataInternal(
      key, res_id_or_last_used_time, old_body_end, std::move(buffer), truncate,
      doomed_new_entry, corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "WriteEntryData", posting_delay, timer.Elapsed(),
      result.has_value() ? Error::kOk : result.error(), corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.WriteEntryData", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(
                         result.has_value() ? Error::kOk : result.error(),
                         store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return ResIdOrErrorAndStoreStatus(result, store_status_);
}

ResIdOrError SqlPersistentStore::Backend::WriteEntryDataInternal(
    const CacheEntryKey& key,
    const ResIdOrTime& res_id_or_last_used_time,
    int64_t old_body_end,
    EntryWriteBuffer buffer,
    bool truncate,
    bool doomed_new_entry,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }

  base::CheckedNumeric<int64_t> checked_total_size_delta = 0;

  ResId res_id = ResId(0);
  int64_t body_end_for_new_entry = 0;
  int64_t bytes_usage_for_new_entry = 0;

  const bool is_new_entry =
      std::holds_alternative<base::Time>(res_id_or_last_used_time);

  // If `res_id_or_last_used_time` holds `base::Time`, create a new entry in the
  // resources table.
  if (is_new_entry) {
    CHECK_EQ(old_body_end, 0);
    checked_total_size_delta += key.string().size();
    body_end_for_new_entry = buffer.offset + buffer.size;
    bytes_usage_for_new_entry = key.string().size() + buffer.size;
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(
            disk_cache_sql_queries::Query::kCreateEntry_InsertIntoResources)));
    statement.BindTime(0, std::get<base::Time>(res_id_or_last_used_time));
    statement.BindInt64(1, body_end_for_new_entry);
    statement.BindInt64(2, bytes_usage_for_new_entry);
    statement.BindBool(3, doomed_new_entry);
    statement.BindInt(4, CalculateCheckSum({}, key.hash()));
    statement.BindInt(5, key.hash().value());
    statement.BindString(6, key.string());
    if (!statement.Step()) {
      return base::unexpected(Error::kFailedToExecute);
    }
    res_id = ResId(statement.ColumnInt64(0));
  } else {
    res_id = std::get<ResId>(res_id_or_last_used_time);
  }

  int64_t body_end_delta = 0;
  int64_t new_body_end = 0;

  // Write the body to the blobs table.
  if (Error result = WriteEntryBodyDataHelper(
          key, res_id, old_body_end, std::move(buffer), truncate,
          body_end_delta, checked_total_size_delta, new_body_end,
          corruption_detected);
      result != Error::kOk) {
    return base::unexpected(result);
  }

  if (!checked_total_size_delta.IsValid()) {
    // If the total size delta calculation resulted in an overflow, it suggests
    // that the size values in the database were corrupt.
    corruption_detected = true;
    return base::unexpected(Error::kInvalidData);
  }
  int64_t total_size_delta = checked_total_size_delta.ValueOrDie();

  if (is_new_entry) {
    CHECK_EQ(body_end_delta, body_end_for_new_entry);
    CHECK_EQ(total_size_delta, bytes_usage_for_new_entry);
    if (doomed_new_entry) {
      total_size_delta = 0;
    }
  } else if (body_end_delta || total_size_delta) {
    // Update the entry's metadata in the `resources` table if the body size
    // changed or if the total size of blobs changed.
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kWriteEntryData_UpdateResource)));
    statement.BindInt64(0, body_end_delta);
    statement.BindInt64(1, total_size_delta);
    statement.BindInt64(2, res_id.value());
    if (statement.Step()) {
      // Consistency check: The `RETURNING` clause gives us the `body_end` value
      // after the update. If this doesn't match our calculated `new_body_end`,
      // it means the `body_end` in the database was not the `old_body_end` we
      // expected. This indicates data corruption, so we return an error.
      const int64_t returned_new_body_end = statement.ColumnInt64(0);
      if (returned_new_body_end != new_body_end) {
        corruption_detected = true;
        return base::unexpected(Error::kBodyEndMismatch);
      }
      // If the entry is doomed, its size is no longer tracked in the cache's
      // total size, so we don't update the store status.
      const bool doomed = statement.ColumnBool(1);
      if (doomed) {
        total_size_delta = 0;
      }
    } else {
      // If no rows were updated, it means the entry was not found.
      return base::unexpected(Error::kNotFound);
    }
  }

  // Commit the transaction, which also updates the in-memory and on-disk store
  // status.
  if (auto error = UpdateStoreStatusAndCommitTransaction(
          transaction,
          /*entry_count_delta=*/is_new_entry && !doomed_new_entry ? 1 : 0,
          /*total_size_delta=*/total_size_delta, corruption_detected);
      error != Error::kOk) {
    return base::unexpected(error);
  }
  return res_id;
}

// This function handles writes that overlap with existing data blobs. It finds
// any blobs that intersect with the new write range `[offset, end)`, removes
// them, and recreates any non-overlapping portions as new, smaller blobs. This
// effectively "cuts out" the space for the new data.
Error SqlPersistentStore::Backend::TrimOverlappingBlobs(
    const CacheEntryKey& key,
    ResId res_id,
    int64_t offset,
    int64_t end,
    bool truncate,
    base::CheckedNumeric<int64_t>& checked_total_size_delta,
    bool& corruption_detected) {
  TRACE_EVENT1("disk_cache", "SqlBackend.TrimOverlappingBlobs", "data",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("res_id", res_id.value());
                 dict.Add("offset", offset);
                 dict.Add("end", end);
               });

  const bool zero_length_write = offset == end;
  if (zero_length_write) {
    if (!truncate) {
      // A zero-length, non-truncating write is a no-op.
      return Error::kOk;
    }
    if (end == 0) {
      // If the end is zero, there are no blobs to overlap with.
      return Error::kOk;
    }
  }

  // First, delete all blobs that are fully contained within the new write
  // range.
  // If the write has zero length, no blobs can be fully contained within it, so
  // this can be skipped.
  if (!zero_length_write) {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kTrimOverlappingBlobs_DeleteContained)));
    statement.BindInt64(0, res_id.value());
    statement.BindInt64(1, offset);
    statement.BindInt64(2, end);
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
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kTrimOverlappingBlobs_SelectOverlapping)));
    statement.BindInt64(0, res_id.value());
    statement.BindInt64(1, end);
    statement.BindInt64(2, offset);
    while (statement.Step()) {
      const int64_t blob_id = statement.ColumnInt64(0);
      const int64_t blob_start = statement.ColumnInt64(1);
      const int64_t blob_end = statement.ColumnInt64(2);
      const int32_t check_sum = statement.ColumnInt(3);
      base::span<const uint8_t> blob = statement.ColumnBlob(4);
      // Consistency check: The blob's size should match its start and end
      // offsets.
      if (!IsBlobSizeValid(blob_start, blob_end, blob)) {
        corruption_detected = true;
        return Error::kInvalidData;
      }
      if (CalculateCheckSum(blob, key.hash()) != check_sum) {
        corruption_detected = true;
        return Error::kCheckSumError;
      }
      // Mark the overlapping blob for removal.
      blob_ids_to_be_removed.push_back(blob_id);
      // If the existing blob starts before the new write, create a new blob
      // for the leading part that doesn't overlap.
      if (blob_start < offset) {
        new_blobs.emplace_back(
            base::MakeRefCounted<net::VectorIOBuffer>(
                blob.first(base::checked_cast<size_t>(offset - blob_start))),
            blob_start);
      }
      // If the existing blob ends after the new write and we are not
      // truncating, create a new blob for the trailing part that doesn't
      // overlap.
      if (!truncate && end < blob_end) {
        new_blobs.emplace_back(
            base::MakeRefCounted<net::VectorIOBuffer>(
                blob.last(base::checked_cast<size_t>(blob_end - end))),
            end);
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
  if (Error error =
          InsertNewBlobs(key, res_id, new_blobs, checked_total_size_delta);
      error != Error::kOk) {
    return error;
  }
  return Error::kOk;
}

Error SqlPersistentStore::Backend::TruncateBlobsAfter(
    ResId res_id,
    int64_t truncate_offset,
    base::CheckedNumeric<int64_t>& checked_total_size_delta) {
  TRACE_EVENT1("disk_cache", "SqlBackend.TruncateBlobsAfter", "data",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("res_id", res_id.value());
                 dict.Add("truncate_offset", truncate_offset);
               });
  // Delete all blobs that start at or after the truncation offset.
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kTruncateBlobsAfter_DeleteAfter)));
    statement.BindInt64(0, res_id.value());
    statement.BindInt64(1, truncate_offset);
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
Error SqlPersistentStore::Backend::InsertNewBlobs(
    const CacheEntryKey& key,
    ResId res_id,
    const std::vector<BufferWithStart>& new_blobs,
    base::CheckedNumeric<int64_t>& checked_total_size_delta) {
  // Iterate through the provided blobs and insert each one.
  for (const auto& new_blob : new_blobs) {
    if (Error error =
            InsertNewBlob(key, res_id, new_blob.start, new_blob.buffer,
                          new_blob.buffer->size(), checked_total_size_delta);
        error != Error::kOk) {
      return error;
    }
  }
  return Error::kOk;
}

// Inserts a single new blob into the database.
Error SqlPersistentStore::Backend::InsertNewBlob(
    const CacheEntryKey& key,
    ResId res_id,
    int64_t start,
    const scoped_refptr<net::IOBuffer>& buffer,
    int buf_len,
    base::CheckedNumeric<int64_t>& checked_total_size_delta) {
  TRACE_EVENT1("disk_cache", "SqlBackend.InsertNewBlob", "data",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("res_id", res_id.value());
                 dict.Add("start", start);
                 dict.Add("buf_len", buf_len);
               });
  const int64_t end =
      (base::CheckedNumeric<int64_t>(start) + buf_len).ValueOrDie();
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kInsertNewBlob_InsertIntoBlobs)));
  statement.BindInt64(0, res_id.value());
  statement.BindInt64(1, start);
  statement.BindInt64(2, end);
  const auto new_blob = buffer->first(base::checked_cast<size_t>(buf_len));
  statement.BindInt(3, CalculateCheckSum(new_blob, key.hash()));
  // SAFETY: The memory referenced by `new_blob` must outlive `statement`.
  statement.BindBlob(
      4, base::MakeRefCounted<base::RefCountedStaticMemory>(new_blob));
  if (!statement.Run()) {
    return Error::kFailedToExecute;
  }
  checked_total_size_delta += buf_len;
  return Error::kOk;
}

// A helper function to delete multiple blobs by their IDs.
Error SqlPersistentStore::Backend::DeleteBlobsById(
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
Error SqlPersistentStore::Backend::DeleteBlobById(
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

// Deletes all blobs associated with a specific entry res_id.
Error SqlPersistentStore::Backend::DeleteBlobsByResId(ResId res_id) {
  TRACE_EVENT1("disk_cache", "SqlBackend.DeleteBlobsByResId", "res_id",
               [&](perfetto::TracedValue trace_context) {
                 auto dict = std::move(trace_context).WriteDictionary();
                 dict.Add("res_id", res_id.value());
               });
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kDeleteBlobsByResId_DeleteFromBlobs)));
  statement.BindInt64(0, res_id.value());
  if (!statement.Run()) {
    return Error::kFailedToExecute;
  }
  return Error::kOk;
}

Error SqlPersistentStore::Backend::DeleteBlobsByResIds(
    const std::vector<ResId>& res_ids) {
  TRACE_EVENT0("disk_cache", "SqlBackend.DeleteBlobsByResIds");
  for (const auto& res_id : res_ids) {
    if (auto error = DeleteBlobsByResId(res_id); error != Error::kOk) {
      return error;
    }
  }
  return Error::kOk;
}

Error SqlPersistentStore::Backend::DeleteResourceByResId(ResId res_id) {
  TRACE_EVENT0("disk_cache", "SqlBackend.DeleteResourceByResId");
  sql::Statement delete_resource_stmt(db_.GetCachedStatement(
      SQL_FROM_HERE,
      GetQuery(Query::kDeleteResourceByResIds_DeleteFromResources)));
  delete_resource_stmt.BindInt64(0, res_id.value());
  if (!delete_resource_stmt.Run()) {
    return Error::kFailedToExecute;
  }
  return Error::kOk;
}

Int64OrError SqlPersistentStore::Backend::DeleteLiveResourceByResIdReturnUsage(
    ResId res_id) {
  TRACE_EVENT0("disk_cache", "SqlBackend.DeleteLiveResourceByResIdReturnUsage");
  sql::Statement delete_resource_stmt(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kDeleteLiveResourceByResIdReturnUsage)));
  delete_resource_stmt.BindInt64(0, res_id.value());
  if (delete_resource_stmt.Step()) {
    return delete_resource_stmt.ColumnInt64(0);
  }
  return base::unexpected(Error::kNotFound);
}

Error SqlPersistentStore::Backend::DeleteResourcesByResIds(
    const std::vector<ResId>& res_ids) {
  TRACE_EVENT0("disk_cache", "SqlBackend.DeleteResourcesByResIds");
  for (const auto& res_id : res_ids) {
    if (auto error = DeleteResourceByResId(res_id); error != Error::kOk) {
      return error;
    }
  }
  return Error::kOk;
}

ReadResultOrError SqlPersistentStore::Backend::ReadEntryData(
    const CacheEntryKey& key,
    ResId res_id,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    int64_t body_end,
    bool sparse_reading,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.ReadEntryData", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("res_id", res_id.value());
                       dict.Add("offset", offset);
                       dict.Add("buf_len", buf_len);
                       dict.Add("body_end", body_end);
                       dict.Add("sparse_reading", sparse_reading);
                       PopulateTraceDetails(store_status_, dict);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result =
      ReadEntryDataInternal(key, res_id, offset, std::move(buffer), buf_len,
                            body_end, sparse_reading, corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "ReadEntryData", posting_delay, timer.Elapsed(),
      result.error_or(Error::kOk), corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.ReadEntryData", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  return result;
}

ReadResultOrError SqlPersistentStore::Backend::ReadEntryDataInternal(
    const CacheEntryKey& key,
    ResId res_id,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    int64_t body_end,
    bool sparse_reading,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }

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
  statement.BindInt64(0, res_id.value());
  statement.BindInt64(1, read_end);
  statement.BindInt64(2, offset);

  ReadResult read_result;
  size_t written_bytes = 0;
  while (statement.Step()) {
    const int64_t blob_start = statement.ColumnInt64(0);
    const int64_t blob_end = statement.ColumnInt64(1);
    int32_t check_sum = statement.ColumnInt(2);
    base::span<const uint8_t> blob = statement.ColumnBlob(3);
    if (!IsBlobSizeValid(blob_start, blob_end, blob)) {
      corruption_detected = true;
      return base::unexpected(Error::kInvalidData);
    }
    if (CalculateCheckSum(blob, key.hash()) != check_sum) {
      corruption_detected = true;
      return base::unexpected(Error::kCheckSumError);
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
        read_result.read_bytes = static_cast<int>(written_bytes);
        return read_result;
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

    // If the blob extends beyond the read request, cache the remaining part.
    if (copy_end == read_end && blob_end > read_end) {
      const int64_t remaining_bytes = blob_end - read_end;
      // IOBuffer size is limited to int.
      const int cache_size = base::saturated_cast<int>(remaining_bytes);

      // We only cache the part from the *last* blob that overlaps with
      // read_end. If multiple blobs overlap (unlikely due to
      // TrimOverlappingBlobs), the last one wins.
      if (read_cache_memory_monitor_->Allocate(cache_size)) {
        read_result.cache_buffer =
            base::MakeRefCounted<MonitoredVectorIOBuffer>(
                blob.subspan(base::checked_cast<size_t>(read_end - blob_start),
                             base::checked_cast<size_t>(cache_size)),
                read_cache_memory_monitor_);
        read_result.cache_buffer_offset = read_end;
      }
    }
  }

  if (sparse_reading) {
    read_result.read_bytes = static_cast<int>(written_bytes);
    return read_result;
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

  read_result.read_bytes = static_cast<int>(written_bytes);
  return read_result;
}

RangeResult SqlPersistentStore::Backend::GetEntryAvailableRange(
    ResId res_id,
    int64_t offset,
    int len,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.GetEntryAvailableRange", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("res_id", res_id.value());
                       dict.Add("offset", offset);
                       dict.Add("len", len);
                     });
  base::ElapsedTimer timer;
  auto result = GetEntryAvailableRangeInternal(res_id, offset, len);
  RecordTimeAndErrorResultHistogram("GetEntryAvailableRange", posting_delay,
                                    timer.Elapsed(),
                                    result.error_or(Error::kOk),
                                    /*corruption_detected=*/false);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.GetEntryAvailableRange", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result.value_or(RangeResult(net::Error::ERR_FAILED));
}

SqlPersistentStore::Backend::RangeResultOrError
SqlPersistentStore::Backend::GetEntryAvailableRangeInternal(ResId res_id,
                                                            int64_t offset,
                                                            int len) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
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
    statement.BindInt64(0, res_id.value());
    statement.BindInt64(1, end);
    statement.BindInt64(2, offset);
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

Int64OrError SqlPersistentStore::Backend::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    base::TimeTicks start_time) {
  if (initial_time == base::Time::Min() && end_time == base::Time::Max()) {
    return store_status_.GetEstimatedDiskUsage();
  }
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.CalculateSizeOfEntriesBetween",
                     "data", [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("initial_time", initial_time);
                       dict.Add("end_time", end_time);
                     });
  base::ElapsedTimer timer;
  auto result = CalculateSizeOfEntriesBetweenInternal(initial_time, end_time);
  RecordTimeAndErrorResultHistogram("CalculateSizeOfEntriesBetween",
                                    posting_delay, timer.Elapsed(),
                                    result.error_or(Error::kOk),
                                    /*corruption_detected=*/false);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.CalculateSizeOfEntriesBetween",
                   "result", [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

Int64OrError SqlPersistentStore::Backend::CalculateSizeOfEntriesBetweenInternal(
    base::Time initial_time,
    base::Time end_time) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
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
  return Int64OrError(total_size);
}

OptionalEntryInfoWithKeyAndIterator SqlPersistentStore::Backend::OpenNextEntry(
    const EntryIterator& iterator,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.OpenNextEntry", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("res_id_iterator", iterator.value().res_id);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = OpenNextEntryInternal(iterator, corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "OpenNextEntry", posting_delay, timer.Elapsed(),
      result.error_or(Error::kOk), corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.OpenNextEntry", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  if (!result.has_value()) {
    return std::nullopt;
  }
  return std::move(*result);
}

SqlPersistentStore::Backend::OptionalEntryInfoWithKeyAndIteratorOrError
SqlPersistentStore::Backend::OpenNextEntryInternal(
    const EntryIterator& iterator,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetQuery(Query::kOpenNextEntry_SelectLiveResources)));
  statement.BindInt64(0, iterator.value().res_id.value());
  while (statement.Step()) {
    const ResId res_id = ResId(statement.ColumnInt64(0));
    EntryInfoWithKeyAndIterator result;
    result.iterator.value().res_id = res_id;
    result.iterator.value().shard_id = shard_id_;
    auto& entry_info = result.info;
    entry_info.res_id = res_id;
    entry_info.last_used = statement.ColumnTime(1);
    entry_info.body_end = statement.ColumnInt64(2);
    int32_t check_sum = statement.ColumnInt(3);
    result.key = CacheEntryKey(statement.ColumnString(4));
    base::span<const uint8_t> blob_span = statement.ColumnBlob(5);
    if (CalculateCheckSum(blob_span, result.key.hash()) != check_sum ||
        blob_span.size() > std::numeric_limits<int>::max()) {
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

void SqlPersistentStore::Backend::StartEviction(
    int64_t size_to_be_removed,
    base::flat_set<ResId> excluded_res_ids,
    std::vector<ResId> high_priority_res_ids,
    bool is_idle_time_eviction,
    scoped_refptr<EvictionCandidateAggregator> aggregator,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
        remaining_mandatory_size,
    EvictionResultOrErrorAndStoreStatusCallback callback) {
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.StartEviction", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("size_to_be_removed", size_to_be_removed);
                       dict.Add("is_idle_time_eviction", is_idle_time_eviction);
                       dict.Add("excluded_res_ids", excluded_res_ids.size());
                       dict.Add("high_priority_res_ids",
                                high_priority_res_ids.size());
                     });
  auto candidates = SelectEvictionCandidates(
      size_to_be_removed, std::move(excluded_res_ids),
      std::move(high_priority_res_ids), is_idle_time_eviction);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.StartEviction", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     dict.Add("candidates_size", candidates.size());
                   });
  aggregator->OnCandidate(
      shard_id_, std::move(candidates),
      base::BindOnce(&Backend::EvictEntries, weak_factory_.GetWeakPtr(),
                     std::move(callback), is_idle_time_eviction,
                     std::move(abort_flag),
                     std::move(remaining_mandatory_size)));
}

SqlPersistentStore::Backend::EvictionCandidateList
SqlPersistentStore::Backend::SelectEvictionCandidates(
    int64_t size_to_be_removed,
    base::flat_set<ResId> excluded_res_ids,
    std::vector<ResId> high_priority_res_ids,
    bool is_idle_time_eviction) {
  if (is_idle_time_eviction && !IsBrowserIdle()) {
    return {};
  }
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return {};
  }

  base::ElapsedTimer timer;

  const bool size_and_priority_aware_eviction =
      net::features::kSqlDiskCacheSizeAndPriorityAwareEviction.Get();
  const bool prioritized_caching_enabled = base::FeatureList::IsEnabled(
      net::features::kSimpleCachePrioritizedCaching);
  const int caching_prioritization_factor =
      net::features::kSimpleCachePrioritizedCachingPrioritizationFactor.Get();
  const uint64_t caching_prioritization_period_in_seconds =
      static_cast<uint64_t>(
          net::features::kSimpleCachePrioritizedCachingPrioritizationPeriod
              .Get()
              .InSeconds());

  base::flat_set<ResId> high_priority_res_ids_set;
  if (size_and_priority_aware_eviction) {
    std::sort(high_priority_res_ids.begin(), high_priority_res_ids.end());
    high_priority_res_ids_set = base::flat_set<ResId>(
        base::sorted_unique, std::move(high_priority_res_ids));
  }

  EvictionCandidateList candidates;
  const base::Time now = base::Time::Now();
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE, GetQuery(Query::kStartEviction_SelectLiveResources)));
    base::ClampedNumeric<int64_t> candidates_total_size = 0;
    while (statement.Step()) {
      if (is_idle_time_eviction && !IsBrowserIdle()) {
        return {};
      }
      const ResId res_id = ResId(statement.ColumnInt64(0));
      if (excluded_res_ids.contains(res_id)) {
        continue;
      }
      const int64_t bytes_usage = statement.ColumnInt64(1);
      const base::Time last_used = statement.ColumnTime(2);
      const uint64_t time_since_last_used = (now - last_used).InSeconds();
      uint64_t sort_value = time_since_last_used;
      if (size_and_priority_aware_eviction) {
        sort_value *= bytes_usage + kSqlBackendStaticResourceSize;
        if (prioritized_caching_enabled &&
            time_since_last_used < caching_prioritization_period_in_seconds &&
            high_priority_res_ids_set.contains(res_id)) {
          sort_value /= caching_prioritization_factor;
        }
      }
      candidates.emplace_back(res_id, shard_id_,
                              bytes_usage + kSqlBackendStaticResourceSize,
                              sort_value);
      if (!size_and_priority_aware_eviction) {
        candidates_total_size += bytes_usage;
        candidates_total_size += kSqlBackendStaticResourceSize;
        if (size_to_be_removed <= candidates_total_size) {
          // Since "ORDER BY last_used" is specified in the
          // kStartEviction_SelectLiveResources query, for LRU eviction that is
          // not size and priority aware, there is no need to read more once
          // the total size exceeds `size_to_be_removed`.
          break;
        }
      }
    }
  }

  // For size and priority aware eviction, all entry information is included in
  // `candidates` at this point. Since we don't need more than
  // `size_to_be_removed`, we remove unnecessary candidates before passing them
  // to the aggregator.
  if (size_and_priority_aware_eviction) {
    // Sort candidates by sort_value
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) {
                return a.sort_value > b.sort_value;
              });
    base::ClampedNumeric<int64_t> candidates_total_size = 0;
    auto it = candidates.begin();
    while (it != candidates.end() &&
           size_to_be_removed > candidates_total_size) {
      candidates_total_size += it->entry_size_with_overhead;
      ++it;
    }
    candidates.erase(it, candidates.end());
  }

  base::UmaHistogramMicrosecondsTimes(
      base::StrCat(
          {kSqlDiskCacheBackendHistogramPrefix,
           !is_idle_time_eviction ? "RunEviction" : "RunEvictionOnIdleTime",
           ".TimeToSelectEntries"}),
      timer.Elapsed());
  return candidates;
}

void SqlPersistentStore::Backend::EvictEntries(
    EvictionResultOrErrorAndStoreStatusCallback callback,
    bool is_idle_time_eviction,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
        remaining_mandatory_size,
    EvictionTargetQueue eviction_targets,
    base::TimeTicks post_task_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - post_task_time;
  // Checks that this method is called on the expected sequence when invoked via
  // EvictionCandidateAggregator.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.EvictEntries", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("target_size", eviction_targets.size());
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  // Entries in excluded_res_ids are excluded from candidates at the
  // SelectEvictionCandidates stage, so pass an empty excluded_res_ids here.
  auto result =
      EvictEntriesHelper(std::move(eviction_targets), /*excluded_res_ids=*/{},
                         is_idle_time_eviction, std::move(abort_flag),
                         std::move(remaining_mandatory_size),
                         /*trust_target_size=*/true, corruption_detected);
  RecordTimeAndErrorResultHistogram(
      !is_idle_time_eviction ? "EvictEntries" : "EvictEntriesOnIdleTime",
      posting_delay, timer.Elapsed(), result.error_or(Error::kOk),
      corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.EvictEntries", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  MaybeCrashIfCorrupted(corruption_detected);
  std::move(callback).Run(
      EvictionResultOrErrorAndStoreStatus(std::move(result), store_status_));
}

EvictionResultOrErrorAndStoreStatus
SqlPersistentStore::Backend::ResumePendingEviction(
    EvictionTargetQueue eviction_targets,
    base::flat_set<ResId> excluded_res_ids,
    bool is_idle_time_eviction,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
        remaining_mandatory_size,
    base::TimeTicks start_time) {
  const base::TimeDelta posting_delay = base::TimeTicks::Now() - start_time;
  TRACE_EVENT_BEGIN1("disk_cache", "SqlBackend.ResumePendingEviction", "data",
                     [&](perfetto::TracedValue trace_context) {
                       auto dict = std::move(trace_context).WriteDictionary();
                       dict.Add("eviction_target_size",
                                eviction_targets.size());
                       dict.Add("is_idle_time_eviction", is_idle_time_eviction);
                     });
  base::ElapsedTimer timer;
  bool corruption_detected = false;
  auto result = EvictEntriesHelper(
      std::move(eviction_targets), excluded_res_ids, is_idle_time_eviction,
      std::move(abort_flag), std::move(remaining_mandatory_size),
      /*trust_target_size=*/false, corruption_detected);
  RecordTimeAndErrorResultHistogram(
      "CalculateSizeOfEntriesBetween", posting_delay, timer.Elapsed(),
      result.error_or(Error::kOk), corruption_detected);
  TRACE_EVENT_END1("disk_cache", "SqlBackend.ResumePendingEviction", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return EvictionResultOrErrorAndStoreStatus(std::move(result), store_status_);
}

EvictionResultOrError SqlPersistentStore::Backend::EvictEntriesHelper(
    EvictionTargetQueue eviction_targets,
    const base::flat_set<ResId>& excluded_res_ids,
    bool is_idle_time_eviction,
    scoped_refptr<base::RefCountedData<std::atomic_bool>> abort_flag,
    scoped_refptr<base::RefCountedData<std::atomic_int64_t>>
        remaining_mandatory_size,
    bool trust_target_size,
    bool& corruption_detected) {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
  ResIdList deleted_res_ids;
  while (!eviction_targets.empty()) {
    const auto res_id = eviction_targets.front().res_id;
    const auto entry_size_with_overhead =
        eviction_targets.front().entry_size_with_overhead;
    if ((is_idle_time_eviction && !IsBrowserIdle()) ||
        (abort_flag->data.load(std::memory_order_relaxed) &&
         remaining_mandatory_size->data.load(std::memory_order_relaxed) <= 0)) {
      break;
    }
    if (excluded_res_ids.contains(res_id)) {
      eviction_targets.pop();
      continue;
    }
    sql::Transaction transaction(&db_);
    if (!transaction.Begin()) {
      return base::unexpected(Error::kFailedToExecute);
    }
    if (eviction_hook_) {
      eviction_hook_.Run();
    }

    int64_t deleted_byte = 0;
    if (trust_target_size) {
      if (auto error = DeleteResourceByResId(res_id); error != Error::kOk) {
        return base::unexpected(error);
      }
      // store_status_.total_size tracks payload only, so subtract overhead.
      deleted_byte = entry_size_with_overhead - kSqlBackendStaticResourceSize;
    } else {
      auto usage_or_error = DeleteLiveResourceByResIdReturnUsage(res_id);
      if (!usage_or_error.has_value()) {
        // DeleteLiveResourceByResIdReturnUsage() only returns kNotFound as an
        // error. In that case, continue eviction by ignoring the entry instead
        // of aborting.
        CHECK_EQ(usage_or_error.error(), Error::kNotFound);
        eviction_targets.pop();
        continue;
      }
      deleted_byte = *usage_or_error;
    }

    if (auto error = DeleteBlobsByResId(res_id); error != Error::kOk) {
      return base::unexpected(error);
    }

    deleted_res_ids.emplace_back(res_id);
    if (const auto error = UpdateStoreStatusAndCommitTransaction(
            transaction, -1, -deleted_byte, corruption_detected);
        error != Error::kOk) {
      return base::unexpected(error);
    }
    remaining_mandatory_size->data.fetch_sub(
        deleted_byte + kSqlBackendStaticResourceSize,
        std::memory_order_relaxed);
    eviction_targets.pop();
  }
  return EvictionResult(std::move(deleted_res_ids),
                        std::move(eviction_targets));
}

Error SqlPersistentStore::Backend::UpdateStoreStatusAndCommitTransaction(
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

#if DCHECK_IS_ON()
  // In debug builds, verify consistency by recalculating.
  const int64_t actual_entry_count = CalculateResourceEntryCount();
  const int64_t actual_total_size = CalculateTotalSize();
  if (store_status_.entry_count != actual_entry_count ||
      store_status_.total_size != actual_total_size) {
    // For debugging crbug.com/488877236.
    SCOPED_CRASH_KEY_NUMBER("DiskCache", "entry_count_delta",
                            entry_count_delta);
    SCOPED_CRASH_KEY_NUMBER("DiskCache", "entry_count",
                            store_status_.entry_count);
    SCOPED_CRASH_KEY_NUMBER("DiskCache", "actual_entry_count",
                            actual_entry_count);
    SCOPED_CRASH_KEY_NUMBER("DiskCache", "total_size_delta", total_size_delta);
    SCOPED_CRASH_KEY_NUMBER("DiskCache", "total_size",
                            store_status_.total_size);
    SCOPED_CRASH_KEY_NUMBER("DiskCache", "actual_total_size",
                            actual_total_size);
    base::debug::DumpWithoutCrashing();
    store_status_.entry_count = actual_entry_count;
    meta_table_.SetValue(kSqlBackendMetaTableKeyEntryCount,
                         store_status_.entry_count);
    store_status_.total_size = actual_total_size;
    meta_table_.SetValue(kSqlBackendMetaTableKeyTotalSize,
                         store_status_.total_size);
  }
#endif  // DCHECK_IS_ON()

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

Error SqlPersistentStore::Backend::RecalculateStoreStatusAndCommitTransaction(
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
int64_t SqlPersistentStore::Backend::CalculateResourceEntryCount() {
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
int64_t SqlPersistentStore::Backend::CalculateTotalSize() {
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      GetQuery(Query::kCalculateTotalSize_SelectTotalSizeFromLiveResources)));
  int64_t result = 0;
  if (statement.Step()) {
    result = statement.ColumnInt64(0);
  }
  return result;
}

SqlPersistentStore::InMemoryIndexAndDoomedResIdsOrError
SqlPersistentStore::Backend::LoadInMemoryIndex() {
  TRACE_EVENT_BEGIN("disk_cache", "SqlBackend.LoadInMemoryIndex");
  auto result = LoadInMemoryIndexInternal();
  TRACE_EVENT_END1("disk_cache", "SqlBackend.LoadInMemoryIndex", "result",
                   [&](perfetto::TracedValue trace_context) {
                     auto dict = std::move(trace_context).WriteDictionary();
                     PopulateTraceDetails(result, store_status_, dict);
                   });
  return result;
}

SqlPersistentStore::InMemoryIndexAndDoomedResIdsOrError
SqlPersistentStore::Backend::LoadInMemoryIndexInternal() {
  if (auto db_error = CheckDatabaseStatus(); db_error != Error::kOk) {
    return base::unexpected(db_error);
  }
  SqlPersistentStoreInMemoryIndex index;
  ResIdList doomed_entry_res_ids;
  base::ElapsedTimer timer;
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(
            Query::kLoadInMemoryIndex_SelectCacheKeyHashFromLiveResources)));
    while (statement.Step()) {
      const auto res_id = ResId(statement.ColumnInt64(0));
      const auto key_hash = CacheEntryKey::Hash(statement.ColumnInt(1));
      const bool doomed = statement.ColumnBool(2);
      if (doomed) {
        doomed_entry_res_ids.emplace_back(res_id);
      } else {
        index.Insert(key_hash, res_id);
      }
    }
  }
  {
    sql::Statement statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetQuery(Query::kLoadInMemoryIndex_SelectHintsFromLiveResources)));
    while (statement.Step()) {
      const auto res_id = ResId(statement.ColumnInt64(0));
      const auto hints = MemoryEntryDataHints(statement.ColumnInt(1));
      index.SetEntryDataHints(res_id, hints);
    }
  }
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat(
          {kSqlDiskCacheBackendHistogramPrefix, "LoadInMemoryIndexTime"}),
      timer.Elapsed());
  return InMemoryIndexAndDoomedResIds(std::move(index),
                                      std::move(doomed_entry_res_ids));
}

bool SqlPersistentStore::Backend::MaybeRunCheckpoint() {
  TRACE_EVENT("disk_cache", "SqlBackend.MaybeRunCheckpoint");
  if (!db_.is_open()) {
    // The database might have been closed if a catastrophic error occurred and
    // RazeAndPoison() was called.
    return false;
  }
  if (!IsBrowserIdle()) {
    // Between the time when idle was detected in the browser process and the
    // time when this backend was notified, the browser became non-idle.
    return false;
  }
  if (wal_pages_ < net::features::kSqlDiskCacheIdleCheckpointThreshold.Get()) {
    return false;
  }
  TRACE_EVENT("disk_cache", "SqlBackend.CheckpointDatabase", "pages",
              wal_pages_);
  base::ElapsedTimer timer;
  bool checkpoint_result = db_.CheckpointDatabase();
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, "IdleEventCheckpoint.",
                    checkpoint_result ? "Success" : "Failure", "Time"}),
      timer.Elapsed());
  base::UmaHistogramCounts100000(
      base::StrCat({kSqlDiskCacheBackendHistogramPrefix, "IdleEventCheckpoint.",
                    checkpoint_result ? "Success" : "Failure", "Pages"}),
      wal_pages_);
  wal_pages_ = 0;
  return checkpoint_result;
}

void SqlPersistentStore::Backend::MaybeCrashIfCorrupted(
    bool corruption_detected) {
  CHECK(!(corruption_detected && strict_corruption_check_enabled_));
}

void SqlPersistentStore::Backend::OnCommitCallback(int pages) {
  TRACE_EVENT("disk_cache", "SqlBackend.OnCommitCallback");
  const bool is_idle = IsBrowserIdle();
  if (pages >= net::features::kSqlDiskCacheForceCheckpointThreshold.Get() ||
      (pages >= net::features::kSqlDiskCacheIdleCheckpointThreshold.Get() &&
       is_idle)) {
    TRACE_EVENT("disk_cache", "SqlBackend.CheckpointDatabase", "pages", pages);
    base::ElapsedTimer timer;
    bool checkpoint_result = db_.CheckpointDatabase();
    base::UmaHistogramMicrosecondsTimes(
        base::StrCat({kSqlDiskCacheBackendHistogramPrefix,
                      is_idle ? "Idle" : "Force", "Checkpoint.",
                      checkpoint_result ? "Success" : "Failure", "Time"}),
        timer.Elapsed());
    base::UmaHistogramCounts100000(
        base::StrCat({kSqlDiskCacheBackendHistogramPrefix,
                      is_idle ? "Idle" : "Force", "Checkpoint.",
                      checkpoint_result ? "Success" : "Failure", "Pages"}),
        pages);
    wal_pages_ = 0;
    return;
  }
  wal_pages_ = pages;
}

base::FilePath SqlPersistentStore::Backend::GetDatabaseFilePath() const {
  return path_.AppendASCII(
      base::StrCat({kSqlBackendDatabaseFileNamePrefix,
                    base::NumberToString(shard_id_.value())}));
}

SqlPersistentStore::Backend::BufferWithStart::BufferWithStart(
    scoped_refptr<net::IOBuffer> buffer,
    int64_t start)
    : buffer(std::move(buffer)), start(start) {}
SqlPersistentStore::Backend::BufferWithStart::~BufferWithStart() = default;
SqlPersistentStore::Backend::BufferWithStart::BufferWithStart(
    BufferWithStart&& other) = default;
SqlPersistentStore::Backend::BufferWithStart&
SqlPersistentStore::Backend::BufferWithStart::operator=(
    BufferWithStart&& other) = default;

}  // namespace disk_cache
