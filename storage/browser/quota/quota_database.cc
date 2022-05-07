// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_database.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/quota_database_migrations.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {
namespace {

// Version number of the database schema.
//
// We support migrating the database schema from versions that are at most 2
// years old. Older versions are unsupported, and will cause the database to get
// razed.
//
// Version 1 - 2011-03-17 - http://crrev.com/78521 (unsupported)
// Version 2 - 2011-04-25 - http://crrev.com/82847 (unsupported)
// Version 3 - 2011-07-08 - http://crrev.com/91835 (unsupported)
// Version 4 - 2011-10-17 - http://crrev.com/105822 (unsupported)
// Version 5 - 2015-10-19 - https://crrev.com/354932
// Version 6 - 2021-04-27 - https://crrev.com/c/2757450
// Version 7 - 2021-05-20 - https://crrev.com/c/2910136
// Version 8 - 2021-09-01 - https://crrev.com/c/3119831
const int kQuotaDatabaseCurrentSchemaVersion = 8;
const int kQuotaDatabaseCompatibleVersion = 8;

// Definitions for database schema.
const char kHostQuotaTable[] = "quota";
const char kBucketTable[] = "buckets";

// Deprecated flag that ensured that the buckets table was bootstrapped
// with existing storage key data for eviction logic.
// TODO(crbug.com/1254535): Remove once enough time has passed to ensure that
// this flag is no longer stored and supported in the QuotaDatabase.
const char kIsOriginTableBootstrapped[] = "IsOriginTableBootstrapped";
// Deprecated bootstrap flag, invalidated in 03/2022 as part of crbug/1306279.
const char kDeprecatedBucketsTableBootstrapped[] = "IsBucketsTableBootstrapped";
// Flag to ensure that all existing data for storage keys have been
// registered into the buckets table.
const char kBucketsTableBootstrapped[] = "IsBucketsBootstrapped";

const int kCommitIntervalMs = 30000;

void RecordDatabaseResetHistogram(const DatabaseResetReason reason) {
  base::UmaHistogramEnumeration("Quota.QuotaDatabaseReset", reason);
}

}  // anonymous namespace

const QuotaDatabase::TableSchema QuotaDatabase::kTables[] = {
    {kHostQuotaTable,
     "(host TEXT NOT NULL,"
     " type INTEGER NOT NULL,"
     " quota INTEGER NOT NULL,"
     " PRIMARY KEY(host, type))"
     " WITHOUT ROWID"},
    {kBucketTable,
     "(id INTEGER PRIMARY KEY,"
     " storage_key TEXT NOT NULL,"
     " host TEXT NOT NULL,"
     " type INTEGER NOT NULL,"
     " name TEXT NOT NULL,"
     " use_count INTEGER NOT NULL,"
     " last_accessed INTEGER NOT NULL,"
     " last_modified INTEGER NOT NULL,"
     " expiration INTEGER NOT NULL,"
     " quota INTEGER NOT NULL)"}};
const size_t QuotaDatabase::kTableCount = std::size(QuotaDatabase::kTables);

// static
const QuotaDatabase::IndexSchema QuotaDatabase::kIndexes[] = {
    {"buckets_by_storage_key", kBucketTable, "(storage_key, type, name)", true},
    {"buckets_by_host", kBucketTable, "(type, host)", false},
    {"buckets_by_last_accessed", kBucketTable, "(type, last_accessed)", false},
    {"buckets_by_last_modified", kBucketTable, "(type, last_modified)", false},
    {"buckets_by_expiration", kBucketTable, "(expiration)", false},
};
const size_t QuotaDatabase::kIndexCount = std::size(QuotaDatabase::kIndexes);

QuotaDatabase::BucketTableEntry::BucketTableEntry() = default;

QuotaDatabase::BucketTableEntry::~BucketTableEntry() = default;

QuotaDatabase::BucketTableEntry::BucketTableEntry(const BucketTableEntry&) =
    default;
QuotaDatabase::BucketTableEntry& QuotaDatabase::BucketTableEntry::operator=(
    const QuotaDatabase::BucketTableEntry&) = default;

QuotaDatabase::BucketTableEntry::BucketTableEntry(
    BucketId bucket_id,
    StorageKey storage_key,
    StorageType type,
    std::string name,
    int use_count,
    const base::Time& last_accessed,
    const base::Time& last_modified)
    : bucket_id(std::move(bucket_id)),
      storage_key(std::move(storage_key)),
      type(type),
      name(std::move(name)),
      use_count(use_count),
      last_accessed(last_accessed),
      last_modified(last_modified) {}

// QuotaDatabase ------------------------------------------------------------
QuotaDatabase::QuotaDatabase(const base::FilePath& profile_path)
    : storage_directory_(
          profile_path.empty()
              ? nullptr
              : std::make_unique<StorageDirectory>(profile_path)),
      db_file_path_(
          profile_path.empty()
              ? base::FilePath()
              : storage_directory_->path().AppendASCII(kDatabaseName)),
      legacy_db_file_path_(profile_path.empty()
                               ? base::FilePath()
                               : profile_path.AppendASCII(kDatabaseName)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

QuotaDatabase::~QuotaDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_) {
    db_->CommitTransaction();
  }
}

constexpr char QuotaDatabase::kDatabaseName[];

QuotaErrorOr<int64_t> QuotaDatabase::GetHostQuota(const std::string& host,
                                                  StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      "SELECT quota FROM quota WHERE host = ? AND type = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Step()) {
    return statement.Succeeded() ? QuotaError::kNotFound
                                 : QuotaError::kDatabaseError;
  }
  return statement.ColumnInt64(0);
}

QuotaError QuotaDatabase::SetHostQuota(const std::string& host,
                                       StorageType type,
                                       int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(quota, 0);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  if (quota == 0)
    return DeleteHostQuota(host, type);

  static constexpr char kSql[] =
      "INSERT OR REPLACE INTO quota(quota, host, type) VALUES (?, ?, ?)";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, quota);
  statement.BindString(1, host);
  statement.BindInt(2, static_cast<int>(type));
  if (!statement.Run())
    return QuotaError::kDatabaseError;

  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetOrCreateBucket(
    const BucketInitParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  QuotaErrorOr<BucketInfo> bucket_result =
      GetBucket(params.storage_key, params.name, StorageType::kTemporary);

  if (bucket_result.ok())
    return bucket_result;

  if (bucket_result.error() != QuotaError::kNotFound)
    return bucket_result.error();

  base::Time now = base::Time::Now();
  return CreateBucketInternal(
      params.storage_key, StorageType::kTemporary, params.name,
      /*use_count=*/0, now, now, params.expiration, params.quota);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetOrCreateBucketDeprecated(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  QuotaErrorOr<BucketInfo> bucket_result =
      GetBucket(storage_key, bucket_name, type);

  if (bucket_result.ok())
    return bucket_result;

  if (bucket_result.error() != QuotaError::kNotFound)
    return bucket_result.error();

  base::Time now = base::Time::Now();
  return CreateBucketInternal(storage_key, type, bucket_name, /*use_count=*/0,
                              now, now, absl::nullopt, 0);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::CreateBucketForTesting(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time now = base::Time::Now();
  return CreateBucketInternal(storage_key, storage_type, bucket_name,
                              /*use_count=*/0, now, now, absl::nullopt, 0);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT id, expiration, quota "
        "FROM buckets "
        "WHERE storage_key = ? AND type = ? AND name = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindInt(1, static_cast<int>(storage_type));
  statement.BindString(2, bucket_name);

  if (!statement.Step()) {
    return statement.Succeeded() ? QuotaError::kNotFound
                                 : QuotaError::kDatabaseError;
  }

  return BucketInfo(BucketId(statement.ColumnInt64(0)), storage_key,
                    storage_type, bucket_name, statement.ColumnTime(1),
                    statement.ColumnInt(2));
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetBucketById(BucketId bucket_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT storage_key, type, name, expiration, quota "
        "FROM buckets "
        "WHERE id = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket_id.value());

  if (!statement.Step()) {
    return statement.Succeeded() ? QuotaError::kNotFound
                                 : QuotaError::kDatabaseError;
  }

  absl::optional<StorageKey> storage_key =
      StorageKey::Deserialize(statement.ColumnString(0));
  if (!storage_key.has_value())
    return QuotaError::kNotFound;

  return BucketInfo(bucket_id, storage_key.value(),
                    static_cast<StorageType>(statement.ColumnInt(1)),
                    statement.ColumnString(2), statement.ColumnTime(3),
                    statement.ColumnInt(4));
}

QuotaErrorOr<std::set<BucketLocator>> QuotaDatabase::GetBucketsForType(
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      "SELECT id, storage_key, name FROM buckets WHERE type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  std::set<BucketLocator> buckets;
  while (statement.Step()) {
    absl::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!read_storage_key.has_value())
      continue;
    buckets.emplace(BucketId(statement.ColumnInt64(0)),
                    read_storage_key.value(), type,
                    statement.ColumnString(2) == kDefaultBucketName);
  }
  return buckets;
}

QuotaErrorOr<std::set<BucketLocator>> QuotaDatabase::GetBucketsForHost(
    const std::string& host,
    blink::mojom::StorageType storage_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      "SELECT id, storage_key, name FROM buckets WHERE host = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(storage_type));

  std::set<BucketLocator> buckets;
  while (statement.Step()) {
    absl::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!read_storage_key.has_value())
      continue;
    buckets.emplace(BucketId(statement.ColumnInt64(0)),
                    read_storage_key.value(), storage_type,
                    statement.ColumnString(2) == kDefaultBucketName);
  }
  return buckets;
}

QuotaErrorOr<std::set<BucketLocator>> QuotaDatabase::GetBucketsForStorageKey(
    const StorageKey& storage_key,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      "SELECT id, name FROM buckets WHERE storage_key = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindInt(1, static_cast<int>(type));

  std::set<BucketLocator> buckets;
  while (statement.Step()) {
    buckets.emplace(BucketId(statement.ColumnInt64(0)), storage_key, type,
                    statement.ColumnString(1) == kDefaultBucketName);
  }
  return buckets;
}

QuotaError QuotaDatabase::SetStorageKeyLastAccessTime(
    const StorageKey& storage_key,
    StorageType type,
    base::Time last_accessed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  // clang-format off
  static constexpr char kSql[] =
      "UPDATE buckets "
        "SET use_count = use_count + 1, last_accessed = ? "
        "WHERE storage_key = ? AND type = ? AND name = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindTime(0, last_accessed);
  statement.BindString(1, storage_key.Serialize());
  statement.BindInt(2, static_cast<int>(type));
  statement.BindString(3, kDefaultBucketName);

  if (!statement.Run())
    return QuotaError::kDatabaseError;

  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaError QuotaDatabase::SetBucketLastAccessTime(BucketId bucket_id,
                                                  base::Time last_accessed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  // clang-format off
  static constexpr char kSql[] =
      "UPDATE buckets "
        "SET use_count = use_count + 1, last_accessed = ? "
        "WHERE id = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindTime(0, last_accessed);
  statement.BindInt64(1, bucket_id.value());

  if (!statement.Run())
    return QuotaError::kDatabaseError;

  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaError QuotaDatabase::SetBucketLastModifiedTime(BucketId bucket_id,
                                                    base::Time last_modified) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      "UPDATE buckets SET last_modified = ? WHERE id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindTime(0, last_modified);
  statement.BindInt64(1, bucket_id.value());

  if (!statement.Run())
    return QuotaError::kDatabaseError;

  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaError QuotaDatabase::RegisterInitialStorageKeyInfo(
    base::flat_map<StorageType, std::set<StorageKey>> storage_keys_by_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  for (const auto& type_and_storage_keys : storage_keys_by_type) {
    StorageType storage_type = type_and_storage_keys.first;
    for (const auto& storage_key : type_and_storage_keys.second) {
      static constexpr char kSql[] =
          // clang-format off
          "INSERT OR IGNORE INTO buckets("
              "storage_key,"
              "host,"
              "type,"
              "name,"
              "use_count,"
              "last_accessed,"
              "last_modified,"
              "expiration,"
              "quota) "
            "VALUES (?, ?, ?, ?, 0, 0, 0, ?, 0)";
      // clang-format on
      sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
      statement.BindString(0, storage_key.Serialize());
      statement.BindString(1, storage_key.origin().host());
      statement.BindInt(2, static_cast<int>(storage_type));
      statement.BindString(3, kDefaultBucketName);
      statement.BindTime(4, base::Time::Max());

      if (!statement.Run())
        return QuotaError::kDatabaseError;
    }
  }
  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaErrorOr<QuotaDatabase::BucketTableEntry> QuotaDatabase::GetBucketInfo(
    BucketId bucket_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT "
          "storage_key,"
          "type,"
          "name,"
          "use_count,"
          "last_accessed,"
          "last_modified "
        "FROM buckets "
        "WHERE id = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket_id.value());

  if (!statement.Step()) {
    return statement.Succeeded() ? QuotaError::kNotFound
                                 : QuotaError::kDatabaseError;
  }

  absl::optional<StorageKey> storage_key =
      StorageKey::Deserialize(statement.ColumnString(0));
  if (!storage_key.has_value())
    return QuotaError::kNotFound;

  return BucketTableEntry(bucket_id, std::move(storage_key).value(),
                          static_cast<StorageType>(statement.ColumnInt(1)),
                          statement.ColumnString(2), statement.ColumnInt(3),
                          statement.ColumnTime(4), statement.ColumnTime(5));
}

QuotaError QuotaDatabase::DeleteHostQuota(const std::string& host,
                                          StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      "DELETE FROM quota WHERE host = ? AND type = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Run())
    return QuotaError::kDatabaseError;

  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaError QuotaDatabase::DeleteBucketData(const BucketLocator& bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  // Doom bucket directory first so data is no longer accessible, even if
  // directory deletion fails. `storage_directory_` may be nullptr for
  // in-memory only.
  if (storage_directory_ && !storage_directory_->DoomBucket(bucket))
    return QuotaError::kFileOperationError;

  static constexpr char kSql[] = "DELETE FROM buckets WHERE id = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket.id.value());

  if (!statement.Run())
    return QuotaError::kDatabaseError;

  // Scheduling this commit introduces the chance of inconsistencies
  // between the buckets table and data stored on disk in the file system.
  // If there is a crash or a battery failure before the transaction is
  // committed, the bucket directory may be deleted from the file system,
  // while an entry still may exist in the database.
  //
  // While this is not ideal, this does not introduce any new edge case.
  // We should check that bucket IDs have existing associated directories,
  // because database corruption could result in invalid bucket IDs.
  // TODO(crbug.com/1314567): For handling inconsistencies between the db and
  // the file system.
  ScheduleCommit();

  if (storage_directory_)
    storage_directory_->ClearDoomedBuckets();

  return QuotaError::kNone;
}

QuotaErrorOr<BucketLocator> QuotaDatabase::GetLRUBucket(
    StorageType type,
    const std::set<BucketId>& bucket_exceptions,
    SpecialStoragePolicy* special_storage_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  // clang-format off
  static constexpr char kSql[] =
      "SELECT id, storage_key, name FROM buckets "
        "WHERE type = ? "
        "ORDER BY last_accessed";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  while (statement.Step()) {
    absl::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!read_storage_key.has_value())
      continue;

    BucketId read_bucket_id = BucketId(statement.ColumnInt64(0));
    if (base::Contains(bucket_exceptions, read_bucket_id))
      continue;

    // TODO(crbug/1176774): Once BucketTable holds bucket durability info,
    // add logic to allow durable buckets to also bypass eviction.
    GURL read_gurl = read_storage_key->origin().GetURL();
    if (special_storage_policy &&
        (special_storage_policy->IsStorageDurable(read_gurl) ||
         special_storage_policy->IsStorageUnlimited(read_gurl))) {
      continue;
    }
    return BucketLocator(read_bucket_id, std::move(read_storage_key).value(),
                         type, statement.ColumnString(2) == kDefaultBucketName);
  }
  return QuotaError::kNotFound;
}

QuotaErrorOr<std::set<StorageKey>> QuotaDatabase::GetStorageKeysForType(
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      "SELECT DISTINCT storage_key FROM buckets WHERE type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  std::set<StorageKey> storage_keys;
  while (statement.Step()) {
    absl::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(0));
    if (!read_storage_key.has_value())
      continue;
    storage_keys.insert(read_storage_key.value());
  }
  return storage_keys;
}

QuotaErrorOr<std::set<BucketLocator>> QuotaDatabase::GetBucketsModifiedBetween(
    StorageType type,
    base::Time begin,
    base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  DCHECK(!begin.is_max());
  DCHECK(end != base::Time());
  // clang-format off
  static constexpr char kSql[] =
      "SELECT id, storage_key, name FROM buckets "
        "WHERE type = ? AND last_modified >= ? AND last_modified < ?";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));
  statement.BindTime(1, begin);
  statement.BindTime(2, end);

  std::set<BucketLocator> buckets;
  while (statement.Step()) {
    absl::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!read_storage_key.has_value())
      continue;
    buckets.emplace(BucketId(statement.ColumnInt64(0)),
                    read_storage_key.value(), type,
                    statement.ColumnString(2) == kDefaultBucketName);
  }
  return buckets;
}

bool QuotaDatabase::IsBootstrapped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (EnsureOpened() != QuotaError::kNone)
    return false;

  int flag = 0;
  return meta_table_->GetValue(kBucketsTableBootstrapped, &flag) && flag;
}

QuotaError QuotaDatabase::SetIsBootstrapped(bool bootstrap_flag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  // Delete deprecated bootstrap flag if it still exists.
  // TODO(crbug.com/1254535): Remove once enough time has passed to ensure that
  // this flag is no longer stored and supported in the QuotaDatabase.
  meta_table_->DeleteKey(kIsOriginTableBootstrapped);
  meta_table_->DeleteKey(kDeprecatedBucketsTableBootstrapped);

  return meta_table_->SetValue(kBucketsTableBootstrapped, bootstrap_flag)
             ? QuotaError::kNone
             : QuotaError::kDatabaseError;
}

QuotaError QuotaDatabase::RazeAndReopen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Try creating a database one last time if there isn't one.
  if (!db_) {
    if (!db_file_path_.empty()) {
      DCHECK(!legacy_db_file_path_.empty());
      sql::Database::Delete(db_file_path_);
      sql::Database::Delete(legacy_db_file_path_);
    }
    return EnsureOpened();
  }

  // Abort the long-running transaction.
  db_->RollbackTransaction();

  // Raze and close the database. Reset `db_` to nullptr so EnsureOpened will
  // recreate the database.
  if (!db_->Raze())
    return QuotaError::kDatabaseError;
  db_ = nullptr;

  return EnsureOpened();
}

QuotaError QuotaDatabase::CorruptForTesting(
    base::OnceCallback<void(const base::FilePath&)> corrupter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_) {
    // Commit the long-running transaction.
    db_->CommitTransaction();
    db_->Close();
  }

  std::move(corrupter).Run(db_file_path_);

  if (!db_)
    return QuotaError::kDatabaseError;
  if (!OpenDatabase())
    return QuotaError::kDatabaseError;

  // Begin a long-running transaction. This matches EnsureOpen().
  if (!db_->BeginTransaction())
    return QuotaError::kDatabaseError;
  return QuotaError::kNone;
}

void QuotaDatabase::SetDisabledForTesting(bool disable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_disabled_ = disable;
}

void QuotaDatabase::Commit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return;

  if (timer_.IsRunning())
    timer_.Stop();

  DCHECK_EQ(1, db_->transaction_nesting());
  db_->CommitTransaction();
  DCHECK_EQ(0, db_->transaction_nesting());
  db_->BeginTransaction();
  DCHECK_EQ(1, db_->transaction_nesting());
}

void QuotaDatabase::ScheduleCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, base::Milliseconds(kCommitIntervalMs), this,
               &QuotaDatabase::Commit);
}

QuotaError QuotaDatabase::EnsureOpened() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_)
    return QuotaError::kNone;

  // If we tried and failed once, don't try again in the same session
  // to avoid creating an incoherent mess on disk.
  if (is_disabled_)
    return QuotaError::kDatabaseError;

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = true,
      // The quota database is a critical storage component. If it's corrupted,
      // all client-side storage APIs fail, because they don't know where their
      // data is stored.
      .flush_to_media = true,
      .page_size = 4096,
      .cache_size = 500,
  });
  meta_table_ = std::make_unique<sql::MetaTable>();

  db_->set_histogram_tag("Quota");

  // UMA logging and don't crash on database errors in DCHECK builds.
  db_->set_error_callback(
      base::BindRepeating([](int sqlite_error_code, sql::Statement* statement) {
        sql::UmaHistogramSqliteResult("Quota.QuotaDatabaseError",
                                      sqlite_error_code);
      }));

  // Migrate an existing database from the old path.
  if (!db_file_path_.empty() && !MoveLegacyDatabase()) {
    if (!ResetStorage()) {
      is_disabled_ = true;
      db_.reset();
      meta_table_.reset();
      return QuotaError::kDatabaseError;
    }
    // ResetStorage() has succeeded and database is already open.
    return QuotaError::kNone;
  }

  if (!OpenDatabase() || !EnsureDatabaseVersion()) {
    LOG(ERROR) << "Could not open the quota database, resetting.";
    if (db_file_path_.empty() || !ResetStorage()) {
      LOG(ERROR) << "Failed to reset the quota database.";
      is_disabled_ = true;
      db_.reset();
      meta_table_.reset();
      return QuotaError::kDatabaseError;
    }
  }

  // Start a long-running transaction.
  db_->BeginTransaction();

  return QuotaError::kNone;
}

bool QuotaDatabase::MoveLegacyDatabase() {
  // Migration was added on 04/2022 (https://crrev.com/c/3513545).
  // Cleanup after enough time has passed.
  if (base::PathExists(db_file_path_) ||
      !base::PathExists(legacy_db_file_path_)) {
    return true;
  }

  if (!base::CreateDirectory(db_file_path_.DirName()) ||
      !base::CopyFile(legacy_db_file_path_, db_file_path_)) {
    sql::Database::Delete(db_file_path_);
    return false;
  }

  base::FilePath legacy_journal_path =
      sql::Database::JournalPath(legacy_db_file_path_);
  if (base::PathExists(legacy_journal_path) &&
      !base::CopyFile(legacy_journal_path,
                      sql::Database::JournalPath(db_file_path_))) {
    sql::Database::Delete(db_file_path_);
    return false;
  }

  sql::Database::Delete(legacy_db_file_path_);
  return true;
}

bool QuotaDatabase::OpenDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Open in memory database.
  if (db_file_path_.empty()) {
    if (db_->OpenInMemory())
      return true;
    RecordDatabaseResetHistogram(DatabaseResetReason::kOpenInMemoryDatabase);
    return false;
  }

  if (!base::CreateDirectory(db_file_path_.DirName())) {
    RecordDatabaseResetHistogram(DatabaseResetReason::kCreateDirectory);
    return false;
  }

  if (!db_->Open(db_file_path_)) {
    RecordDatabaseResetHistogram(DatabaseResetReason::kOpenDatabase);
    return false;
  }

  db_->Preload();
  return true;
}

bool QuotaDatabase::EnsureDatabaseVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sql::MetaTable::DoesTableExist(db_.get())) {
    if (CreateSchema())
      return true;
    RecordDatabaseResetHistogram(DatabaseResetReason::kCreateSchema);
    return false;
  }

  if (!meta_table_->Init(db_.get(), kQuotaDatabaseCurrentSchemaVersion,
                         kQuotaDatabaseCompatibleVersion)) {
    RecordDatabaseResetHistogram(DatabaseResetReason::kInitMetaTable);
    return false;
  }

  if (meta_table_->GetCompatibleVersionNumber() >
      kQuotaDatabaseCurrentSchemaVersion) {
    RecordDatabaseResetHistogram(DatabaseResetReason::kDatabaseVersionTooNew);
    LOG(WARNING) << "Quota database is too new.";
    return false;
  }

  if (meta_table_->GetVersionNumber() < kQuotaDatabaseCurrentSchemaVersion) {
    if (!QuotaDatabaseMigrations::UpgradeSchema(*this)) {
      RecordDatabaseResetHistogram(DatabaseResetReason::kDatabaseMigration);
      return false;
    }
  }

#if DCHECK_IS_ON()
  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  for (const TableSchema& table : kTables)
    DCHECK(db_->DoesTableExist(table.table_name));
#endif

  return true;
}

bool QuotaDatabase::CreateSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(kinuko): Factor out the common code to create databases.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  if (!meta_table_->Init(db_.get(), kQuotaDatabaseCurrentSchemaVersion,
                         kQuotaDatabaseCompatibleVersion)) {
    return false;
  }

  for (const TableSchema& table : kTables) {
    if (!CreateTable(table))
      return false;
  }

  for (const IndexSchema& index : kIndexes) {
    if (!CreateIndex(index))
      return false;
  }

  return transaction.Commit();
}

bool QuotaDatabase::CreateTable(const TableSchema& table) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string sql("CREATE TABLE ");
  sql += table.table_name;
  sql += table.columns;
  if (!db_->Execute(sql.c_str())) {
    VLOG(1) << "Failed to execute " << sql;
    return false;
  }
  return true;
}

bool QuotaDatabase::CreateIndex(const IndexSchema& index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string sql;
  if (index.unique)
    sql += "CREATE UNIQUE INDEX ";
  else
    sql += "CREATE INDEX ";
  sql += index.index_name;
  sql += " ON ";
  sql += index.table_name;
  sql += index.columns;
  if (!db_->Execute(sql.c_str())) {
    VLOG(1) << "Failed to execute " << sql;
    return false;
  }
  return true;
}

bool QuotaDatabase::ResetStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_file_path_.empty());
  DCHECK(storage_directory_);
  DCHECK(!db_ || !db_->transaction_nesting());
  VLOG(1) << "Deleting existing quota data and starting over.";

  db_.reset();
  meta_table_.reset();

  sql::Database::Delete(legacy_db_file_path_);
  sql::Database::Delete(db_file_path_);

  // Explicit file deletion to try and get consistent deletion across platforms.
  base::DeleteFile(legacy_db_file_path_);
  base::DeleteFile(db_file_path_);
  base::DeleteFile(sql::Database::JournalPath(legacy_db_file_path_));
  base::DeleteFile(sql::Database::JournalPath(db_file_path_));

  storage_directory_->Doom();
  storage_directory_->ClearDoomed();

  // So we can't go recursive.
  if (is_recreating_)
    return false;

  base::AutoReset<bool> auto_reset(&is_recreating_, true);
  return EnsureOpened() == QuotaError::kNone;
}

QuotaError QuotaDatabase::DumpBucketTable(const BucketTableCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT "
          "id,"
          "storage_key,"
          "type,"
          "name,"
          "use_count,"
          "last_accessed,"
          "last_modified "
        "FROM buckets";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step()) {
    BucketId bucket_id = BucketId(statement.ColumnInt64(0));
    absl::optional<StorageKey> storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!storage_key.has_value())
      continue;
    BucketTableEntry entry(std::move(bucket_id), std::move(storage_key).value(),
                           static_cast<StorageType>(statement.ColumnInt(2)),
                           statement.ColumnString(3), statement.ColumnInt(4),
                           statement.ColumnTime(5), statement.ColumnTime(6));

    if (!callback.Run(entry))
      return QuotaError::kNone;
  }
  return statement.Succeeded() ? QuotaError::kNone : QuotaError::kDatabaseError;
}

QuotaErrorOr<BucketInfo> QuotaDatabase::CreateBucketInternal(
    const blink::StorageKey& storage_key,
    StorageType type,
    const std::string& bucket_name,
    int use_count,
    base::Time last_accessed,
    base::Time last_modified,
    absl::optional<base::Time> expiration,
    int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug/1210259): Add DCHECKs for input validation.
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "INSERT INTO buckets("
        "storage_key,"
        "host,"
        "type,"
        "name,"
        "use_count,"
        "last_accessed,"
        "last_modified,"
        "expiration,"
        "quota) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindString(1, storage_key.origin().host());
  statement.BindInt(2, static_cast<int>(type));
  statement.BindString(3, bucket_name);
  statement.BindInt(4, use_count);
  statement.BindTime(5, last_accessed);
  statement.BindTime(6, last_modified);
  const base::Time expires = expiration.value_or(base::Time::Max());
  statement.BindTime(7, expires);
  statement.BindInt64(8, quota);

  if (!statement.Run())
    return QuotaError::kDatabaseError;

  int64_t bucket_id = db_->GetLastInsertRowId();
  DCHECK_GT(bucket_id, 0);

  // Commit immediately so that we persist the bucket metadata to disk before we
  // inform other services / web apps (via the Buckets API) that we did so.
  // Once informed, that promise should persist across power failures.
  Commit();

  return BucketInfo(BucketId(bucket_id), storage_key, type, bucket_name,
                    expires, quota);
}

bool operator<(const QuotaDatabase::BucketTableEntry& lhs,
               const QuotaDatabase::BucketTableEntry& rhs) {
  return std::tie(lhs.storage_key, lhs.type, lhs.use_count, lhs.last_accessed) <
         std::tie(rhs.storage_key, rhs.type, rhs.use_count, rhs.last_accessed);
}

}  // namespace storage
