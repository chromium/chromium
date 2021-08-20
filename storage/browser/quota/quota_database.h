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
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/id_type.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
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
// constructor, must called on the DB thread.
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

  enum class LazyOpenMode { kCreateIfNotFound, kFailIfNotFound };

  // If 'path' is empty, an in memory database will be used.
  explicit QuotaDatabase(const base::FilePath& path);
  ~QuotaDatabase();

  // Returns whether the record could be found.
  bool GetHostQuota(const std::string& host,
                    blink::mojom::StorageType type,
                    int64_t* quota);

  // Returns whether the operation succeeded.
  bool SetHostQuota(const std::string& host,
                    blink::mojom::StorageType type,
                    int64_t quota);
  bool DeleteHostQuota(const std::string& host, blink::mojom::StorageType type);

  // Gets the bucket with `bucket_name` for the `storage_key` for StorageType
  // kTemporary and returns the BucketInfo. If one doesn't exist, it creates
  // a new bucket with the specified policies. Returns a QuotaError if the
  // operation has failed.
  // TODO(crbug/1203467): Include more policies when supported.
  QuotaErrorOr<BucketInfo> GetOrCreateBucket(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name);

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

  // TODO(crbug.com/1202167): Remove once all usages have updated to use
  // SetBucketLastAccessTime.
  bool SetStorageKeyLastAccessTime(const blink::StorageKey& storage_key,
                                   blink::mojom::StorageType type,
                                   base::Time last_accessed);

  // Called by QuotaClient implementers to update when the bucket was last
  // accessed.  If `bucket_id` refers to a bucket with an opaque StorageKey, the
  // bucket's last access time will not be updated and the function will return
  // false.
  bool SetBucketLastAccessTime(BucketId bucket_id, base::Time last_accessed);

  // TODO(crbug.com/1202167): Remove once all usages have updated to use
  // SetBucketLastModifiedTime.
  bool SetStorageKeyLastModifiedTime(const blink::StorageKey& storage_key,
                                     blink::mojom::StorageType type,
                                     base::Time last_modified);

  // Called by QuotaClient implementers to update when the bucket was last
  // modified.
  bool SetBucketLastModifiedTime(BucketId bucket_id, base::Time last_modified);

  // Register initial `storage_keys` info `type` to the database.
  // This method is assumed to be called only after the installation or
  // the database schema reset.
  bool RegisterInitialStorageKeyInfo(
      const std::set<blink::StorageKey>& storage_keys,
      blink::mojom::StorageType type);

  // TODO(crbug.com/1202167): Remove once all usages have been updated to use
  // GetBucketInfo. Gets the BucketTableEntry for `storage_key`. Returns whether
  // the record for a storage key's default bucket could be found.
  bool GetStorageKeyInfo(const blink::StorageKey& storage_key,
                         blink::mojom::StorageType type,
                         BucketTableEntry* entry);

  // Gets the table entry for `bucket`. Returns whether the record for an
  // origin bucket can be found.
  bool GetBucketInfo(BucketId bucket_id, BucketTableEntry* entry);

  // Removes all buckets for `storage_key` with `type`.
  bool DeleteStorageKeyInfo(const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type);

  // Deletes the specified bucket.
  bool DeleteBucketInfo(BucketId bucket_id);

  // Returns the BucketInfo for the least recently used bucket. Will exclude
  // buckets with ids in `bucket_exceptions` and origins that have the special
  // unlimited storage policy. Returns a QuotaError if the operation has failed.
  QuotaErrorOr<BucketInfo> GetLRUBucket(
      blink::mojom::StorageType type,
      const std::set<BucketId>& bucket_exceptions,
      SpecialStoragePolicy* special_storage_policy);

  // Returns all storage keys for `type` in the bucket database.
  QuotaErrorOr<std::set<blink::StorageKey>> GetStorageKeysForType(
      blink::mojom::StorageType type);

  // Returns a set of buckets that have been modified since the `begin` and
  // until the `end`. Returns a QuotaError if the operations has failed.
  QuotaErrorOr<std::set<BucketInfo>> GetBucketsModifiedBetween(
      blink::mojom::StorageType type,
      base::Time begin,
      base::Time end);

  // Returns false if SetBootstrappedForEviction() has never
  // been called before, which means existing storage keys may not have been
  // registered.
  bool IsBootstrappedForEviction();
  bool SetBootstrappedForEviction(bool bootstrap_flag);

  // Manually disable database to test database error scenarios for testing.
  void SetDisabledForTesting(bool disable) { is_disabled_ = disable; }

 private:
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

  QuotaError LazyOpen(LazyOpenMode mode);
  bool OpenDatabase();
  bool EnsureDatabaseVersion();
  bool ResetSchema();
  bool UpgradeSchema(int current_version);
  bool InsertOrReplaceHostQuota(const std::string& host,
                                blink::mojom::StorageType type,
                                int64_t quota);

  bool CreateSchema();
  bool CreateTable(const TableSchema& table);
  bool CreateIndex(const IndexSchema& index);

  // `callback` may return false to stop reading data.
  bool DumpQuotaTable(const QuotaTableCallback& callback);
  bool DumpBucketTable(const BucketTableCallback& callback);

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
  bool is_recreating_;
  bool is_disabled_;

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
  DISALLOW_COPY_AND_ASSIGN(QuotaDatabase);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_DATABASE_H_
