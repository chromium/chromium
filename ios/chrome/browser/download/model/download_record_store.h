// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_STORE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_STORE_H_

#import <map>
#import <memory>
#import <optional>
#import <string>
#import <string_view>
#import <vector>

#import "base/containers/flat_map.h"
#import "base/files/file_path.h"
#import "base/sequence_checker.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_query.h"
#import "ios/web/public/download/download_task.h"

class DownloadRecordDatabase;

// Sequence-bound owner of the SQLite-backed download record database and the
// in-memory record cache. All public methods MUST be called on the database
// sequence (the `SequencedTaskRunner` that the owning service binds at
// construction). Thread safety comes from sequence affinity, not locks.
//
// Sync/async contract: every public method below is SYNCHRONOUS — it runs
// inline on the bound database sequence and returns its result by value.
// The owning service (`DownloadRecordServiceImpl`) is the only intended
// caller and invokes them off the main thread via
// `base::SequenceBound<DownloadRecordStore>::AsyncCall(...)`, which makes
// calls appear asynchronous from the service's perspective. Return values
// flow back to the main thread via `.Then(callback)`; void-returning
// methods are fire-and-forget from the service's POV but still execute
// inline (and FIFO-ordered with all other calls) on the database sequence.
//
// Lifetime: `DownloadRecordServiceImpl` owns a
// `base::SequenceBound<DownloadRecordStore>`. `SequenceBound` posts both
// construction and destruction onto the bound sequence, so any in-flight
// DB-sequence task observes a still-live `DownloadRecordStore` even if the
// owning service has already been torn down on the main thread.
//
// See crbug.com/524030520 for the original use-after-free risk this design
// addresses.
class DownloadRecordStore {
 public:
  // `pagination_enabled` is the snapshot of
  // `IsDownloadListPaginationEnabled()` taken by the owning service at its
  // construction. The store stores the value verbatim and uses it to gate
  // every behavior that differs between the legacy (flag-OFF) and pagination
  // (flag-ON) paths. The flag is sampled once and never re-read from Finch,
  // so any in-process flip cannot change the path chosen at startup.
  explicit DownloadRecordStore(bool pagination_enabled);

  DownloadRecordStore(const DownloadRecordStore&) = delete;
  DownloadRecordStore& operator=(const DownloadRecordStore&) = delete;

  ~DownloadRecordStore();

  // Initializes the SQLite database under
  // `<profile_path>/download_record_db/DownloadRecord`. No-op (and the store
  // remains in an "uninitialized" state) if the directory cannot be created
  // or if database initialization fails.
  void InitializeDatabase(const base::FilePath& profile_path);

  // ---- Startup paths -----------------------------------------------------

  // Legacy startup loader (flag-OFF path). Loads every persisted row into
  // the in-memory cache and flips any `kInProgress`/`kNotStarted` row to
  // `kFailed`. Used only when `IsDownloadListPaginationEnabled()` is false.
  // TODO(crbug.com/524790428): Remove this legacy startup path (and the
  // cache-based readers) once the pagination flag has shipped to stable.
  void LoadHistoricalRecords();

  // Pagination-aware startup cleanup (flag-ON path). Issues a single
  // `UPDATE ... WHERE state IN (kInProgress, kNotStarted)` against the DB
  // (no full-table load).
  void MarkUnfinishedDownloadsAsFailed();

  // ---- Database CRUD operations ------------------------------------------

  // Inserts `record`. For incognito records (`is_incognito == true`) the
  // record is held only in the in-memory cache and never persisted.
  // Returns true on success.
  bool InsertRecord(const DownloadRecord& record);

  // Updates an existing record identified by `updated_record.download_id`.
  // Preserves `created_time` from the cached record. Returns the merged
  // record on success — whether or not the DB was actually written
  // (incognito / progress-only updates skip the DB but still refresh the
  // cache). Returns `std::nullopt` if no cached record exists or a required
  // DB write failed.
  std::optional<DownloadRecord> UpdateRecord(
      const DownloadRecord& updated_record);

  // Bulk-update legacy helper (flag-OFF only). Updates `download_ids` to
  // `new_state` in a single DB transaction and refreshes cache entries.
  // TODO(crbug.com/524790428): Remove once the pagination flag has shipped
  // to stable.
  bool UpdateRecordsState(const std::vector<std::string>& download_ids,
                          web::DownloadTask::State new_state);

  // Looks up `download_id` in the cache, applies `file_path` to a copy of
  // it, and routes through `UpdateRecord`. Returns the merged record on
  // success, `std::nullopt` otherwise.
  std::optional<DownloadRecord> UpdateFilePathInRecord(
      const std::string& download_id,
      const base::FilePath& file_path);

  // Deletes the record identified by `id`. For non-incognito records,
  // attempts a DB delete first; on DB success the cache entry is also
  // erased. Returns true on success (or if the record never existed).
  bool DeleteRecord(std::string_view id);

  // Posted by the service from `OnDownloadDestroyed` to evict `download_id`
  // from the in-memory hot caches once its `web::DownloadTask` is gone
  // (flag-ON only). Runs on the database sequence so it is FIFO-ordered
  // with every other CRUD task — a pending `UpdateRecord` for the same id
  // finishes first and cannot resurrect the entry. The erase calls are
  // idempotent: a concurrent `RemoveDownloadByIdAsync` may have already
  // evicted this id from the DB sequence. No-op when `pagination_enabled_`
  // is false (legacy path keeps records in `record_cache_` for the full
  // session and never evicts on task destruction).
  void EvictOnDestroy(const std::string& download_id);

  // ---- Hot-cache + single-row reads --------------------------------------

  // Returns a snapshot of all records in the in-memory cache. On the
  // legacy flag-OFF path this snapshots `record_cache_`; on the flag-ON
  // path the in-memory layer holds only hot rows so the snapshot covers
  // `active_records_cache_` + `incognito_records_` and is NOT a full
  // history snapshot. Paginated readers must use `GetDownloadsPage` /
  // `GetDownloadsCount` instead.
  std::vector<DownloadRecord> GetAllFromCache();

  // Returns the record for `download_id`, or `std::nullopt` if not found.
  // On the legacy flag-OFF path this is a pure cache lookup against
  // `record_cache_`. On the flag-ON path it probes the active hot cache
  // and the incognito cache first, then falls back to a single-row DB
  // lookup so a persisted-but-not-cached row (e.g. a cold DB row from a
  // previous session) is still reachable.
  std::optional<DownloadRecord> GetById(std::string_view download_id);

  // ---- Database paginated reads ------------------------------------------

  // Cold-DB readers for the paginated UI. Both bypass the in-memory cache
  // by design — they let the flag-ON path serve very large download
  // histories without paying the O(N) load cost of `LoadHistoricalRecords`.
  // Returns an empty vector / 0 if the DB is not initialized.
  //
  // Requires `kDownloadListPagination` to be enabled.
  // Legacy flag-OFF UI must read from the cache via `GetAllFromCache`.
  std::vector<DownloadRecord> GetDownloadsPage(
      const DownloadRecordQuery& query) const;
  size_t GetDownloadsCount(const DownloadRecordQuery& query) const;

 private:
  // DRY helper for the repeated `database_ && database_->IsInitialized()`
  // check used by every mutator below.
  bool IsDatabaseReady() const;

  // Flag-ON only. Inserts or overwrites `record` in the appropriate hot
  // cache: `incognito_records_` if `record.is_incognito`, otherwise
  // `active_records_cache_`. No lock is taken — the store is sequence-
  // bound and the maps are mutated only on `sequence_checker_`.
  void WriteThroughToActiveCache(const DownloadRecord& record);

  // Snapshot of `IsDownloadListPaginationEnabled()` taken by the owning
  // service at its construction. Stored as a const member so an in-process
  // Finch flip cannot change the path chosen at startup. Every code path
  // that branches on the pagination mode reads this member, not the live
  // function.
  const bool pagination_enabled_;

  // Legacy in-memory cache of download records (flag-OFF only). Mirrors
  // every persisted row plus any incognito-only records. On the flag-ON
  // path this map stays empty by design — hot rows live in
  // `active_records_cache_` / `incognito_records_` instead and paginated
  // readers go cold-DB. Accessed only on `sequence_checker_`.
  // `std::less<>` enables heterogeneous lookup (e.g.
  // `find(std::string_view)`) without constructing a temporary
  // `std::string`.
  // TODO(crbug.com/524790428): Remove this map and its legacy population
  // path once the pagination flag has shipped to stable.
  std::map<std::string, DownloadRecord, std::less<>> record_cache_;

  // Hot path for persisted records (flag-ON only). Mirrors live
  // `web::DownloadTask` values whose volatile fields (`received_bytes`,
  // `progress_percent`) are not always flushed to disk on every update —
  // the cache holds the freshest copy so the UI sees fresh byte progress
  // even when nothing was written to disk.
  //
  // By design this cache has NO hard cap. It is correctness-bearing:
  // silently evicting an entry (size cap, LRU, or any other policy) would
  // freeze the corresponding row's UI byte progress at the last persisted
  // value until the next forced flush. The true bound is the live
  // `web::DownloadTask` count, itself bounded by process memory. Eviction
  // happens only when its `DownloadTask` is destroyed (`EvictOnDestroy`)
  // or the row is deleted via `DeleteRecord`. Empty when
  // `pagination_enabled_` is false.
  base::flat_map<std::string, DownloadRecord> active_records_cache_;

  // Incognito records (flag-ON only). Never persisted; cleared on OTR
  // profile destruction or explicit `DeleteRecord`. Same no-hard-cap
  // rationale as `active_records_cache_`. Empty when
  // `pagination_enabled_` is false; in that case incognito records live
  // in `record_cache_` instead.
  base::flat_map<std::string, DownloadRecord> incognito_records_;

  // SQLite-backed database. Owned and accessed only on
  // `sequence_checker_`. Held by `unique_ptr` so an `Init()` failure
  // leaves this null and callers short-circuit via `IsDatabaseReady()`.
  std::unique_ptr<DownloadRecordDatabase> database_;

  // Pins all public and private methods to a single sequence. The store
  // is constructed on the database sequence by `base::SequenceBound`;
  // the checker is detached at construction and rebinds on the first
  // method call (which always happens on the bound sequence), so all
  // subsequent calls happen on that same sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_STORE_H_
