// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_database.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/quota_database_migrations.h"
#include "storage/browser/quota/quota_internals.mojom.h"
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
// Version 9 - 2022-05-13 - https://crrev.com/c/3601253
const int kQuotaDatabaseCurrentSchemaVersion = 9;
const int kQuotaDatabaseCompatibleVersion = 9;

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

base::Clock* g_clock_for_testing = nullptr;

void RecordDatabaseResetHistogram(const DatabaseResetReason reason) {
  base::UmaHistogramEnumeration("Quota.QuotaDatabaseReset", reason);
}

// SQL statement fragment for inserting fields into the buckets table.
#define BUCKETS_FIELDS_INSERTER                                               \
  " (storage_key, host, type, name, use_count, last_accessed, last_modified," \
  " expiration, quota, persistent, durability) "                              \
  " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "

void BindBucketInitParamsToInsertStatement(const BucketInitParams& params,
                                           StorageType type,
                                           int use_count,
                                           const base::Time& last_accessed,
                                           const base::Time& last_modified,
                                           sql::Statement& statement) {
  statement.BindString(0, params.storage_key.Serialize());
  statement.BindString(1, params.storage_key.origin().host());
  statement.BindInt(2, static_cast<int>(type));
  statement.BindString(3, params.name);
  statement.BindInt(4, use_count);
  statement.BindTime(5, last_accessed);
  statement.BindTime(6, last_modified);
  statement.BindTime(7, params.expiration);
  statement.BindInt64(8, params.quota);
  statement.BindBool(9, params.persistent.value_or(false));
  int durability = static_cast<int>(
      params.durability.value_or(blink::mojom::BucketDurability::kRelaxed));
  statement.BindInt(10, durability);
}

// Fields to be retrieved from the database and stored in a
// `BucketTableEntryPtr`.
#define BUCKET_TABLE_ENTRY_FIELDS_SELECTOR \
  "id, storage_key, type, name, use_count, last_accessed, last_modified "

mojom::BucketTableEntryPtr BucketTableEntryFromSqlStatement(
    sql::Statement& statement) {
  mojom::BucketTableEntryPtr entry = mojom::BucketTableEntry::New();
  entry->bucket_id = statement.ColumnInt64(0);
  entry->storage_key = statement.ColumnString(1);
  entry->type =
      static_cast<storage::mojom::StorageType>(statement.ColumnInt(2));
  entry->name = statement.ColumnString(3);
  entry->use_count = statement.ColumnInt(4);
  entry->last_accessed = statement.ColumnTime(5);
  entry->last_modified = statement.ColumnTime(6);
  return entry;
}

// Fields to be retrieved from the database and stored in a `BucketInfo`.
#define BUCKET_INFO_FIELDS_SELECTOR \
  " id, storage_key, type, name, expiration, quota, persistent, durability "

QuotaErrorOr<BucketInfo> BucketInfoFromSqlStatement(sql::Statement& statement) {
  if (!statement.Step()) {
    return statement.Succeeded() ? QuotaError::kNotFound
                                 : QuotaError::kDatabaseError;
  }

  absl::optional<StorageKey> storage_key =
      StorageKey::Deserialize(statement.ColumnString(1));
  if (!storage_key.has_value())
    return QuotaError::kNotFound;

  return BucketInfo(
      BucketId(statement.ColumnInt64(0)), storage_key.value(),
      static_cast<StorageType>(statement.ColumnInt(2)),
      statement.ColumnString(3), statement.ColumnTime(4),
      statement.ColumnInt(5), statement.ColumnBool(6),
      static_cast<blink::mojom::BucketDurability>(statement.ColumnInt(7)));
}

std::set<BucketInfo> BucketInfosFromSqlStatement(sql::Statement& statement) {
  std::set<BucketInfo> result;
  QuotaErrorOr<BucketInfo> bucket;
  while ((bucket = BucketInfoFromSqlStatement(statement)).ok()) {
    result.insert(bucket.value());
  }

  return result;
}

}  // anonymous namespace

const QuotaDatabase::TableSchema QuotaDatabase::kTables[] = {
    // TODO(crbug.com/1175113): Cleanup kHostQuotaTable.
    {kHostQuotaTable,
     "(host TEXT NOT NULL,"
     " type INTEGER NOT NULL,"
     " quota INTEGER NOT NULL,"
     " PRIMARY KEY(host, type))"
     " WITHOUT ROWID"},
    {kBucketTable,
     "(id INTEGER PRIMARY KEY AUTOINCREMENT,"
     " storage_key TEXT NOT NULL,"
     " host TEXT NOT NULL,"
     " type INTEGER NOT NULL,"
     " name TEXT NOT NULL,"
     " use_count INTEGER NOT NULL,"
     " last_accessed INTEGER NOT NULL,"
     " last_modified INTEGER NOT NULL,"
     " expiration INTEGER NOT NULL,"
     " quota INTEGER NOT NULL,"
     " persistent INTEGER NOT NULL,"
     " durability INTEGER NOT NULL)"
     " STRICT"}};
const size_t QuotaDatabase::kTableCount = std::size(QuotaDatabase::kTables);

// static
const QuotaDatabase::IndexSchema QuotaDatabase::kIndexes[] = {
    {"buckets_by_storage_key", kBucketTable, "(storage_key, type, name)", true},
    {"buckets_by_host", kBucketTable, "(host, type)", false},
    {"buckets_by_last_accessed", kBucketTable, "(type, last_accessed)", false},
    {"buckets_by_last_modified", kBucketTable, "(type, last_modified)", false},
    {"buckets_by_expiration", kBucketTable, "(expiration)", false},
};
const size_t QuotaDatabase::kIndexCount = std::size(QuotaDatabase::kIndexes);

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

QuotaErrorOr<BucketInfo> QuotaDatabase::UpdateOrCreateBucket(
    const BucketInitParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  QuotaErrorOr<BucketInfo> bucket_result =
      GetBucket(params.storage_key, params.name, StorageType::kTemporary);

  if (!bucket_result.ok()) {
    if (bucket_result.error() == QuotaError::kNotFound)
      return CreateBucketInternal(params, StorageType::kTemporary);

    return bucket_result;
  }

  // Don't bother updating anything if the bucket is expired.
  if (!bucket_result->expiration.is_null() &&
      (bucket_result->expiration <= GetNow())) {
    return bucket_result;
  }

  // Update the parameters that can be changed.
  if (!params.expiration.is_null() &&
      (params.expiration != bucket_result->expiration)) {
    DCHECK(!bucket_result->is_default());
    bucket_result =
        UpdateBucketExpiration(bucket_result->id, params.expiration);
  }
  DCHECK(bucket_result.ok());

  if (params.persistent && (*params.persistent != bucket_result->persistent)) {
    DCHECK(!bucket_result->is_default());
    bucket_result =
        UpdateBucketPersistence(bucket_result->id, *params.persistent);
  }
  DCHECK(bucket_result.ok());

  return bucket_result;
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetOrCreateBucketDeprecated(
    const BucketInitParams& params,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  QuotaErrorOr<BucketInfo> bucket_result =
      GetBucket(params.storage_key, params.name, type);

  if (bucket_result.ok())
    return bucket_result;

  if (bucket_result.error() != QuotaError::kNotFound)
    return bucket_result.error();

  return CreateBucketInternal(params, type);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::CreateBucketForTesting(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    StorageType storage_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BucketInitParams params(storage_key, bucket_name);
  return CreateBucketInternal(params, storage_type);
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
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE storage_key = ? AND type = ? AND name = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindInt(1, static_cast<int>(storage_type));
  statement.BindString(2, bucket_name);

  return BucketInfoFromSqlStatement(statement);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::UpdateBucketExpiration(
    BucketId bucket,
    const base::Time& expiration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "UPDATE buckets "
        "SET expiration = ? "
        "WHERE id = ? "
        "RETURNING " BUCKET_INFO_FIELDS_SELECTOR;
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindTime(0, expiration);
  statement.BindInt64(1, bucket.value());
  ScheduleCommit();

  return BucketInfoFromSqlStatement(statement);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::UpdateBucketPersistence(
    BucketId bucket,
    bool persistent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "UPDATE buckets "
        "SET persistent = ? "
        "WHERE id = ? "
        "RETURNING " BUCKET_INFO_FIELDS_SELECTOR;
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindBool(0, persistent);
  statement.BindInt64(1, bucket.value());
  ScheduleCommit();

  return BucketInfoFromSqlStatement(statement);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetBucketById(BucketId bucket_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE id = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket_id.value());

  return BucketInfoFromSqlStatement(statement);
}

QuotaErrorOr<std::set<BucketInfo>> QuotaDatabase::GetBucketsForType(
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE type = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  return BucketInfosFromSqlStatement(statement);
}

QuotaErrorOr<std::set<BucketInfo>> QuotaDatabase::GetBucketsForHost(
    const std::string& host,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE host = ? AND type = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));

  return BucketInfosFromSqlStatement(statement);
}

QuotaErrorOr<std::set<BucketInfo>> QuotaDatabase::GetBucketsForStorageKey(
    const StorageKey& storage_key,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE storage_key = ? AND type = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindInt(1, static_cast<int>(type));

  return BucketInfosFromSqlStatement(statement);
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
          "INSERT OR IGNORE INTO buckets" BUCKETS_FIELDS_INSERTER;
      sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
      BucketInitParams init_params =
          BucketInitParams::ForDefaultBucket(storage_key);
      BindBucketInitParamsToInsertStatement(
          init_params, storage_type, /*use_count=*/0,
          /*last_accessed=*/base::Time(),
          /*last_modified=*/base::Time(), statement);

      if (!statement.Run())
        return QuotaError::kDatabaseError;
    }
  }
  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaErrorOr<mojom::BucketTableEntryPtr> QuotaDatabase::GetBucketInfoForTest(
    BucketId bucket_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_TABLE_ENTRY_FIELDS_SELECTOR
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
      StorageKey::Deserialize(statement.ColumnString(1));
  if (!storage_key.has_value())
    return QuotaError::kNotFound;

  mojom::BucketTableEntryPtr entry =
      BucketTableEntryFromSqlStatement(statement);
  return entry;
}

QuotaErrorOr<mojom::BucketTableEntryPtr> QuotaDatabase::DeleteBucketData(
    const BucketLocator& bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  // Doom bucket directory first so data is no longer accessible, even if
  // directory deletion fails. `storage_directory_` may be nullptr for
  // in-memory only.
  if (storage_directory_ && !storage_directory_->DoomBucket(bucket))
    return QuotaError::kFileOperationError;

  static constexpr char kSql[] =
      "DELETE FROM buckets WHERE id = ? "
      "RETURNING " BUCKET_TABLE_ENTRY_FIELDS_SELECTOR;
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket.id.value());

  if (!statement.Step()) {
    return QuotaError::kDatabaseError;
  }

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

  return BucketTableEntryFromSqlStatement(statement);
}

QuotaErrorOr<BucketLocator> QuotaDatabase::GetLruEvictableBucket(
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
        "WHERE type = ? AND persistent = 0 "
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

QuotaErrorOr<std::set<BucketInfo>> QuotaDatabase::GetExpiredBuckets() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  // clang-format off
  static constexpr char kSql[] =
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE expiration > 0 AND expiration < ?";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindTime(0, GetNow());
  return BucketInfosFromSqlStatement(statement);
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

// static
base::Time QuotaDatabase::GetNow() {
  return g_clock_for_testing ? g_clock_for_testing->Now() : base::Time::Now();
}

// static
void QuotaDatabase::SetClockForTesting(base::Clock* clock) {
  g_clock_for_testing = clock;
}

void QuotaDatabase::CommitNow() {
  Commit();
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

  db_->set_error_callback(base::BindRepeating(
      [](base::RepeatingClosure full_disk_error_callback, int sqlite_error_code,
         sql::Statement* statement) {
        sql::UmaHistogramSqliteResult("Quota.QuotaDatabaseError",
                                      sqlite_error_code);

        if (!full_disk_error_callback.is_null() &&
            static_cast<sql::SqliteErrorCode>(sqlite_error_code) ==
                sql::SqliteErrorCode::kFullDisk) {
          full_disk_error_callback.Run();
        }
      },
      full_disk_error_callback_));

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
      "SELECT " BUCKET_TABLE_ENTRY_FIELDS_SELECTOR
        "FROM buckets";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step()) {
    absl::optional<StorageKey> storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!storage_key.has_value())
      continue;

    auto entry = BucketTableEntryFromSqlStatement(statement);

    if (!callback.Run(std::move(entry)))
      return QuotaError::kNone;
  }
  return statement.Succeeded() ? QuotaError::kNone : QuotaError::kDatabaseError;
}

QuotaErrorOr<BucketInfo> QuotaDatabase::CreateBucketInternal(
    const BucketInitParams& params,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug/1210259): Add DCHECKs for input validation.
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone)
    return open_error;

  static constexpr char kSql[] =
      // clang-format off
      "INSERT INTO buckets " BUCKETS_FIELDS_INSERTER
        " RETURNING " BUCKET_INFO_FIELDS_SELECTOR;
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  BindBucketInitParamsToInsertStatement(params, type, /*use_count=*/0,
                                        /*last_accessed=*/GetNow(),
                                        /*last_modified=*/GetNow(), statement);
  QuotaErrorOr<BucketInfo> result = BucketInfoFromSqlStatement(statement);
  const bool done = !statement.Step();
  DCHECK(done);

  if (result.ok()) {
    // Commit immediately so that we persist the bucket metadata to disk before
    // we inform other services / web apps (via the Buckets API) that we did so.
    // Once informed, that promise should persist across power failures.
    Commit();
  }

  return result;
}

void QuotaDatabase::SetOnFullDiskErrorCallback(
    const base::RepeatingClosure& callback) {
  full_disk_error_callback_ = callback;
}

}  // namespace storage
