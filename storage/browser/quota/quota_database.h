// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/id_type.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "storage/browser/quota/quota_internals.mojom-forward.h"
#include "storage/browser/quota/storage_directory.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

namespace base {
class Clock;
}

namespace sql {
class Database;
class MetaTable;
class Statement;
}  // namespace sql

namespace storage {

class SpecialStoragePolicy;

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with "DatabaseResetReason"
// in tools/metrics/histograms/enums.xml.
enum class DatabaseResetReason {
  kOpenDatabase = 0,
  kOpenInMemoryDatabase = 1,
  kCreateSchema = 2,
  kDatabaseMigration = 3,
  kDatabaseVersionTooNew = 4,
  kInitMetaTable = 5,
  kCreateDirectory = 6,
  kMaxValue = kCreateDirectory
};

// Stores all quota managed origin bucket data and metadata.
//
// Instances are owned by QuotaManagerImpl. There is one instance per
// QuotaManagerImpl instance. All the methods of this class, except the
// constructor, must called on the DB thread.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaDatabase {
 public:
  static constexpr char kDatabaseName[] = "QuotaManager";

  // If `profile_path` is empty, an in-memory database will be used.
  explicit QuotaDatabase(const base::FilePath& profile_path);

  QuotaDatabase(const QuotaDatabase&) = delete;
  QuotaDatabase& operator=(const QuotaDatabase&) = delete;

  ~QuotaDatabase();

  // Gets the bucket described by `params.storage_key` and `params.name`
  // for StorageType kTemporary and returns the BucketInfo. If a bucket fitting
  // the params doesn't exist, it creates a new bucket with the policies in
  // `params`. If the bucket exists but policies don't match what's provided in
  // `params`, the existing bucket will be updated and returned (for those
  // policies that are possible to modify --- expiration and persistence).
  // Returns a QuotaError if the operation has failed. If `max_bucket_count` is
  // greater than zero, and this operation would create a new bucket, then fail
  // to create the new bucket if the total bucket count for this storage key is
  // already at or above the max.
  QuotaErrorOr<BucketInfo> UpdateOrCreateBucket(const BucketInitParams& params,
                                                int max_bucket_count);

  // Same as UpdateOrCreateBucket but takes in StorageType. This should only
  // be used by FileSystem, and is expected to be removed when
  // StorageType::kSyncable and StorageType::kPersistent are deprecated.
  // (crbug.com/1233525, crbug.com/1286964).
  QuotaErrorOr<BucketInfo> GetOrCreateBucketDeprecated(
      const BucketInitParams& params,
      blink::mojom::StorageType type);

  // TODO(crbug.com/40181609): Remove `storage_type` when the only supported
  // StorageType is kTemporary.
  QuotaErrorOr<BucketInfo> CreateBucketForTesting(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType storage_type);

  // Retrieves BucketInfo of the bucket with `bucket_name` for `storage_key`.
  // Returns a QuotaError::kEntryNotFound if the bucket does not exist, or
  // a QuotaError::kDatabaseError if the operation has failed.
  QuotaErrorOr<BucketInfo> GetBucket(const blink::StorageKey& storage_key,
                                     const std::string& bucket_name,
                                     blink::mojom::StorageType storage_type);

  // Retrieves BucketInfo of the bucket with `bucket_id`.
  // Returns a QuotaError::kEntryNotFound if the bucket does not exist, or
  // a QuotaError::kDatabaseError if the operation has failed.
  QuotaErrorOr<BucketInfo> GetBucketById(BucketId bucket_id);

  // Returns all buckets for `type` in the buckets table. Returns a QuotaError
  // if the operation has failed.
  QuotaErrorOr<std::set<BucketInfo>> GetBucketsForType(
      blink::mojom::StorageType type);

  // Retrieves all buckets for `host` and `type`. Returns a QuotaError if the
  // operation has failed.
  QuotaErrorOr<std::set<BucketInfo>> GetBucketsForHost(
      const std::string& host,
      blink::mojom::StorageType type);

  // Returns all buckets for `storage_key` in the buckets table. Returns a
  // QuotaError if the operation has failed.
  QuotaErrorOr<std::set<BucketInfo>> GetBucketsForStorageKey(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type);

  // Updates the expiration for the designated bucket.
  QuotaErrorOr<BucketInfo> UpdateBucketExpiration(BucketId bucket,
                                                  const base::Time& expiration);
  // Updates the persistence bit for the designated bucket.
  QuotaErrorOr<BucketInfo> UpdateBucketPersistence(BucketId bucket,
                                                   bool persistent);

  // TODO(crbug.com/40179024): Remove once all usages have updated to use
  // SetBucketLastAccessTime.
  [[nodiscard]] QuotaError SetStorageKeyLastAccessTime(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      base::Time last_accessed);

  // Called by QuotaClient implementers to update when the bucket was last
  // accessed.  If `bucket_id` refers to a bucket with an opaque StorageKey, the
  // bucket's last access time will not be updated and the function will return
  // QuotaError::kNotFound. Returns QuotaError::kNone on a successful update.
  [[nodiscard]] QuotaError SetBucketLastAccessTime(BucketId bucket_id,
                                                   base::Time last_accessed);

  // Called by QuotaClient implementers to update when the bucket was last
  // modified. Returns QuotaError::kNone on a successful update.
  [[nodiscard]] QuotaError SetBucketLastModifiedTime(BucketId bucket_id,
                                                     base::Time last_modified);

  // Register initial `storage_keys_by_type` into the database.
  // This method is assumed to be called only after the installation or
  // the database schema reset.
  QuotaError RegisterInitialStorageKeyInfo(
      base::flat_map<blink::mojom::StorageType, std::set<blink::StorageKey>>
          storage_keys_by_type);

  // Returns the BucketTableEntry for `bucket` if one exists. Returns a
  // QuotaError if not found or the operation has failed.
  QuotaErrorOr<mojom::BucketTableEntryPtr> GetBucketInfoForTest(
      BucketId bucket_id);

  // Deletes the bucket from the database as well as the bucket directory in the
  // storage directory. Returns the bucket data that was deleted.
  QuotaErrorOr<mojom::BucketTableEntryPtr> DeleteBucketData(
      const BucketLocator& bucket);

  // Returns BucketLocators for the least recently used buckets, to clear at
  // least `target_usage` space. Will exclude buckets with ids in
  // `bucket_exceptions`, buckets marked persistent, and origins that have the
  // special unlimited storage policy. Returns a QuotaError if the operation has
  // failed. `usage_map` describes the amount of space used by each bucket; any
  // bucket missing from this map will be considered to use only 1b.
  QuotaErrorOr<std::set<BucketLocator>> GetBucketsForEviction(
      blink::mojom::StorageType type,
      int64_t target_usage,
      const std::map<BucketLocator, int64_t>& usage_map,
      const std::set<BucketId>& bucket_exceptions,
      SpecialStoragePolicy* special_storage_policy);

  // Returns all storage keys for `type` in the buckets table.
  QuotaErrorOr<std::set<blink::StorageKey>> GetStorageKeysForType(
      blink::mojom::StorageType type);

  // Returns a set of buckets that have been modified since the `begin` and
  // until the `end`. Returns a QuotaError if the operations has failed.
  QuotaErrorOr<std::set<BucketLocator>> GetBucketsModifiedBetween(
      blink::mojom::StorageType type,
      base::Time begin,
      base::Time end);

  // Returns a set of all expired or stale (not accessed or modified in 400 days
  // and not persisted) buckets.
  // See crbug.com/40281870 for more info.
  QuotaErrorOr<std::set<BucketInfo>> GetExpiredBuckets(
      SpecialStoragePolicy* special_storage_policy);

  base::FilePath GetStoragePath() const { return storage_directory_->path(); }

  // Returns false if SetIsBootstrapped() has never been called before, which
  // means existing storage keys may not have been registered. Bootstrapping
  // ensures that there is a bucket entry in the buckets table for all storage
  // keys that have stored data by quota managed Storage APIs.
  bool IsBootstrapped();
  QuotaError SetIsBootstrapped(bool bootstrap_flag);

  // If the database has failed to open, this will attempt to reopen it.
  // Otherwise, it will attempt to recover the database. If recovery is
  // attempted but fails, the database will be razed. In all cases, this will
  // attempt to reopen the database and return true on success.
  bool RecoverOrRaze(int error_code);

  // Flushes previously scheduled commits.
  void CommitNow();

  // The given callback will be invoked whenever the database encounters an
  // error.
  void SetDbErrorCallback(
      const base::RepeatingCallback<void(int)>& db_error_callback);

  // Testing support for database corruption handling.
  //
  // Runs `corrupter` on the same sequence used to do database I/O,
  // guaranteeing that no other database operation is performed at the same
  // time. `corrupter` receives the path to the underlying SQLite database
  // as an argument. The underlying SQLite database is closed while
  // `corrupter` runs, and reopened afterwards.

  // Returns QuotaError::kNone if the database was successfully reopened
  // after `corrupter` was run, or QuotaError::kDatabaseError otherwise.
  QuotaError CorruptForTesting(
      base::OnceCallback<void(const base::FilePath&)> corrupter);

  // Manually disable database to test database error scenarios for testing.
  void SetDisabledForTesting(bool disable);

  static base::Time GetNow();
  static void SetClockForTesting(base::Clock* clock);

  void SetAlreadyEvictedStaleStorageForTesting(
      bool already_evicted_stale_storage);

 private:
  // Structures used for CreateSchema.
  struct TableSchema {
    const char* table_name;
    const char* columns;
  };
  struct IndexSchema {
    const char* index_name;
    const char* table_name;
    const char* columns;
    bool unique;
  };

  using BucketTableCallback =
      base::RepeatingCallback<bool(mojom::BucketTableEntryPtr)>;

  // For long-running transactions support.  We always keep a transaction open
  // so that multiple transactions can be batched.  They are flushed
  // with a delay after a modification has been made.  We support neither
  // nested transactions nor rollback (as we don't need them for now).
  void Commit();
  void ScheduleCommit();

  QuotaError EnsureOpened();
  void OnSqliteError(int sqlite_error_code, sql::Statement* statement);
  bool MoveLegacyDatabase();
  bool OpenDatabase();
  bool EnsureDatabaseVersion();
  bool ResetStorage();

  bool CreateSchema();
  bool CreateTable(const TableSchema& table);
  bool CreateIndex(const IndexSchema& index);

  // Dumps table entries for chrome://quota-internals page.
  // `callback` may return false to stop reading data.
  QuotaError DumpBucketTable(const BucketTableCallback& callback);

  // Adds a new bucket entry in the buckets table. Will return a
  // QuotaError::kDatabaseError if the query fails. Will fail if adding the new
  // bucket would cause the count of buckets for that storage key and type to
  // exceed `max_bucket_count`, if `max_bucket_count` is greater than zero.
  QuotaErrorOr<BucketInfo> CreateBucketInternal(const BucketInitParams& params,
                                                blink::mojom::StorageType type,
                                                int max_bucket_count = 0);

  SEQUENCE_CHECKER(sequence_checker_);

  const std::unique_ptr<StorageDirectory> storage_directory_;
  const base::FilePath db_file_path_;
  const base::FilePath legacy_db_file_path_;

  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<sql::MetaTable> meta_table_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool is_recreating_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_disabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  int sqlite_error_code_ = 0;

  base::OneShotTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  friend class QuotaDatabaseTest;
  friend class QuotaDatabaseMigrations;
  friend class QuotaDatabaseMigrationsTest;
  friend class QuotaManagerImpl;

  static const TableSchema kTables[];
  static const size_t kTableCount;
  static const IndexSchema kIndexes[];
  static const size_t kIndexCount;

  // A descriptor of the last SQL statement that was executed, used for metrics.
  std::optional<std::string> last_operation_;

  base::RepeatingCallback<void(int)> db_error_callback_;

  // We need to delay evicting stale buckets until after any session
  // restore has taken place, otherwise we might fail to record current usage.
  // See crbug.com/40281870 for more info.
  base::Time evict_stale_buckets_after_{GetNow() + base::Minutes(1)};

  // We only need to evict stale storage once per profile load. Unlike
  // expired storage, there is no contract with the site to evict storage
  // on a specific timeline. Saving on latency here is more important than
  // cleaning ASAP in long-lived browsing sessions. Also, where possible we
  // should parallel the stale storage clearing efforts in local storage,
  // and once per profile load is the standard there.
  bool already_evicted_stale_storage_{false};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_
