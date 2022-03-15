// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/id_type.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

namespace sql {
class Database;
class MetaTable;
}

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
// constructor, must called on the DB thread. QuotaDatabase should only be
// subclassed in tests.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaDatabase {
 public:
  struct COMPONENT_EXPORT(STORAGE_BROWSER) BucketTableEntry {
    BucketTableEntry();
    BucketTableEntry(BucketId bucket_id,
                     blink::StorageKey storage_key,
                     blink::mojom::StorageType type,
                     std::string name,
                     int use_count,
                     const base::Time& last_accessed,
                     const base::Time& last_modified);
    ~BucketTableEntry();

    BucketTableEntry(const BucketTableEntry&);
    BucketTableEntry& operator=(const BucketTableEntry&);

    BucketId bucket_id;
    blink::StorageKey storage_key;
    blink::mojom::StorageType type = blink::mojom::StorageType::kUnknown;
    std::string name;
    int use_count = 0;
    base::Time last_accessed;
    base::Time last_modified;
  };

  // If 'path' is empty, an in memory database will be used.
  explicit QuotaDatabase(const base::FilePath& path);

  QuotaDatabase(const QuotaDatabase&) = delete;
  QuotaDatabase& operator=(const QuotaDatabase&) = delete;

  virtual ~QuotaDatabase();

  // Returns quota if entry is found. Returns QuotaError::kNotFound no entry if
  // found.
  QuotaErrorOr<int64_t> GetHostQuota(const std::string& host,
                                     blink::mojom::StorageType type);

  // Returns whether the operation succeeded.
  QuotaError SetHostQuota(const std::string& host,
                          blink::mojom::StorageType type,
                          int64_t quota);
  QuotaError DeleteHostQuota(const std::string& host,
                             blink::mojom::StorageType type);

  // Gets the bucket with `bucket_name` for the `storage_key` for StorageType
  // kTemporary and returns the BucketInfo. If one doesn't exist, it creates
  // a new bucket with the specified policies. Returns a QuotaError if the
  // operation has failed.
  // TODO(crbug/1203467): Include more policies when supported.
  QuotaErrorOr<BucketInfo> GetOrCreateBucket(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name);

  // Same as GetOrCreateBucket but takes in StorageType. This should only be
  // used by FileSystem, and is expected to be removed when
  // StorageType::kSyncable and StorageType::kPersistent are deprecated.
  // (crbug.com/1233525, crbug.com/1286964).
  QuotaErrorOr<BucketInfo> GetOrCreateBucketDeprecated(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType type);

  // TODO(crbug.com/1208141): Remove `storage_type` when the only supported
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
  QuotaErrorOr<std::set<BucketLocator>> GetBucketsForType(
      blink::mojom::StorageType type);

  // Retrieves all buckets for `host` and `type`. Returns a QuotaError if the
  // operation has failed.
  QuotaErrorOr<std::set<BucketLocator>> GetBucketsForHost(
      const std::string& host,
      blink::mojom::StorageType type);

  // Returns all buckets for `storage_key` in the buckets table. Returns a
  // QuotaError if the operation has failed.
  QuotaErrorOr<std::set<BucketLocator>> GetBucketsForStorageKey(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type);

  // TODO(crbug.com/1202167): Remove once all usages have updated to use
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
  QuotaErrorOr<BucketTableEntry> GetBucketInfo(BucketId bucket_id);

  // Deletes the specified bucket. This method is virtual for testing.
  virtual QuotaError DeleteBucketInfo(BucketId bucket_id);

  // Returns the BucketLocator for the least recently used bucket. Will exclude
  // buckets with ids in `bucket_exceptions` and origins that have the special
  // unlimited storage policy. Returns a QuotaError if the operation has failed.
  QuotaErrorOr<BucketLocator> GetLRUBucket(
      blink::mojom::StorageType type,
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

  // Returns false if SetIsBootstrapped() has never been called before, which
  // means existing storage keys may not have been registered. Bootstrapping
  // ensures that there is a bucket entry in the buckets table for all storage
  // keys that have stored data by quota managed Storage APIs.
  bool IsBootstrapped();
  QuotaError SetIsBootstrapped(bool bootstrap_flag);

  // Razes and re-opens the database. Should only be called if the database is
  // actually open.
  QuotaError RazeAndReopen();

  // Testing support for database corruption handling.
  //
  // Runs `corrupter` on the same sequence used to do database I/O,
  // guaranteeing that no other database operation is performed at the same
  // time. `corrupter` receives the path to the underlying SQLite database as an
  // argument. The underlying SQLite database is closed while `corrupter` runs,
  // and reopened afterwards.

  // Returns QuotaError::kNone if the database was successfully reopened after
  // `corrupter` was run, or QuotaError::kDatabaseError otherwise.
  QuotaError CorruptForTesting(
      base::OnceCallback<void(const base::FilePath&)> corrupter);

  // Manually disable database to test database error scenarios for testing.
  void SetDisabledForTesting(bool disable) { is_disabled_ = disable; }

 private:
  enum class EnsureOpenedMode { kCreateIfNotFound, kFailIfNotFound };

  struct COMPONENT_EXPORT(STORAGE_BROWSER) QuotaTableEntry {
    std::string host;
    blink::mojom::StorageType type = blink::mojom::StorageType::kUnknown;
    int64_t quota = 0;
  };
  friend COMPONENT_EXPORT(STORAGE_BROWSER) bool operator==(
      const QuotaTableEntry& lhs,
      const QuotaTableEntry& rhs);
  friend COMPONENT_EXPORT(STORAGE_BROWSER) bool operator<(
      const QuotaTableEntry& lhs,
      const QuotaTableEntry& rhs);
  friend COMPONENT_EXPORT(STORAGE_BROWSER) bool operator<(
      const BucketTableEntry& lhs,
      const BucketTableEntry& rhs);

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

  using QuotaTableCallback =
      base::RepeatingCallback<bool(const QuotaTableEntry&)>;
  using BucketTableCallback =
      base::RepeatingCallback<bool(const BucketTableEntry&)>;

  // For long-running transactions support.  We always keep a transaction open
  // so that multiple transactions can be batched.  They are flushed
  // with a delay after a modification has been made.  We support neither
  // nested transactions nor rollback (as we don't need them for now).
  void Commit();
  void ScheduleCommit();

  QuotaError EnsureOpened(EnsureOpenedMode mode);
  bool OpenDatabase();
  bool EnsureDatabaseVersion();
  bool ResetSchema();
  bool UpgradeSchema(int current_version);

  bool CreateSchema();
  bool CreateTable(const TableSchema& table);
  bool CreateIndex(const IndexSchema& index);

  // Dumps table entries for chrome://quota-internals page.
  // `callback` may return false to stop reading data.
  QuotaError DumpQuotaTable(const QuotaTableCallback& callback);
  QuotaError DumpBucketTable(const BucketTableCallback& callback);

  // Adds a new bucket entry in the buckets table. Will return a
  // QuotaError::kDatabaseError if the query fails.
  QuotaErrorOr<BucketInfo> CreateBucketInternal(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      const std::string& bucket_name,
      int use_count,
      base::Time last_accessed,
      base::Time last_modified);

  const base::FilePath db_file_path_;

  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<sql::MetaTable> meta_table_;
  bool is_recreating_ = false;
  bool is_disabled_ = false;

  base::OneShotTimer timer_;

  friend class QuotaDatabaseTest;
  friend class QuotaDatabaseMigrations;
  friend class QuotaDatabaseMigrationsTest;
  friend class QuotaManagerImpl;

  static const TableSchema kTables[];
  static const size_t kTableCount;
  static const IndexSchema kIndexes[];
  static const size_t kIndexCount;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_
