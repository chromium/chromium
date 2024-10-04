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
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/quota_database_migrations.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_internals.mojom.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {
namespace {

static const int kDaysInTenYears = 10 * 365;

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
// Version 5 - 2015-10-19 - https://crrev.com/354932 (unsupported)
// Version 6 - 2021-04-27 - https://crrev.com/c/2757450 (unsupported)
// Version 7 - 2021-05-20 - https://crrev.com/c/2910136
// Version 8 - 2021-09-01 - https://crrev.com/c/3119831
// Version 9 - 2022-05-13 - https://crrev.com/c/3601253
// Version 10 - 2023-04-10 - https://crrev.com/c/4412082
const int kQuotaDatabaseCurrentSchemaVersion = 10;
const int kQuotaDatabaseCompatibleVersion = 10;

// Definitions for database schema.
const char kBucketTable[] = "buckets";

// Flag to ensure that all existing data for storage keys have been
// registered into the buckets table. Introduced 2022-05 (crrev.com/c/3594211).
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
  entry->type = static_cast<blink::mojom::StorageType>(statement.ColumnInt(2));
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
    return base::unexpected(statement.Succeeded() ? QuotaError::kNotFound
                                                  : QuotaError::kDatabaseError);
  }

  std::optional<StorageKey> storage_key =
      StorageKey::Deserialize(statement.ColumnString(1));
  if (!storage_key.has_value()) {
    return base::unexpected(QuotaError::kStorageKeyError);
  }

  BucketInfo bucket_info(
      BucketId(statement.ColumnInt64(0)), storage_key.value(),
      static_cast<StorageType>(statement.ColumnInt(2)),
      statement.ColumnString(3), statement.ColumnTime(4),
      statement.ColumnInt64(5), statement.ColumnBool(6),
      static_cast<blink::mojom::BucketDurability>(statement.ColumnInt(7)));
  // Ignore the durability saved in the database for default buckets, which
  // changed from strict by default to relaxed by default in M124.
  if (bucket_info.is_default()) {
    bucket_info.durability = blink::mojom::BucketDurability::kRelaxed;
  }
  return bucket_info;
}

std::set<BucketInfo> BucketInfosFromSqlStatement(sql::Statement& statement) {
  std::set<BucketInfo> result;
  QuotaErrorOr<BucketInfo> bucket;
  while ((bucket = BucketInfoFromSqlStatement(statement)).has_value()) {
    result.insert(bucket.value());
  }

  return result;
}

}  // anonymous namespace

const QuotaDatabase::TableSchema QuotaDatabase::kTables[] = {
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
    db_->reset_error_callback();
    db_->CommitTransactionDeprecated();
  }
}

constexpr char QuotaDatabase::kDatabaseName[];

QuotaErrorOr<BucketInfo> QuotaDatabase::UpdateOrCreateBucket(
    const BucketInitParams& params,
    int max_bucket_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sqlite_error_code_ = 0;
  QuotaErrorOr<BucketInfo> bucket_result =
      GetBucket(params.storage_key, params.name, StorageType::kTemporary);

  if (!bucket_result.has_value()) {
    if (bucket_result.error() == QuotaError::kNotFound) {
      bucket_result = CreateBucketInternal(params, StorageType::kTemporary,
                                           max_bucket_count);
    }
    if (!bucket_result.has_value()) {
      bucket_result.error().sqlite_error = sqlite_error_code_;
    }
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
    DCHECK(bucket_result.has_value());
  }

  if (params.persistent && (*params.persistent != bucket_result->persistent)) {
    DCHECK(!bucket_result->is_default());
    bucket_result =
        UpdateBucketPersistence(bucket_result->id, *params.persistent);
    DCHECK(bucket_result.has_value());
  }

  return bucket_result;
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetOrCreateBucketDeprecated(
    const BucketInitParams& params,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetBucket(params.storage_key, params.name, type)
      .or_else([&](DetailedQuotaError error) -> QuotaErrorOr<BucketInfo> {
        if (error != QuotaError::kNotFound) {
          return base::unexpected(error);
        }
        return CreateBucketInternal(params, type);
      });
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
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE storage_key = ? AND type = ? AND name = ?";
  // clang-format on
  last_operation_ = "GetBucket";
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
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  static constexpr char kSql[] =
      // clang-format off
      "UPDATE buckets "
        "SET expiration = ? "
        "WHERE id = ? "
        "RETURNING " BUCKET_INFO_FIELDS_SELECTOR;
  // clang-format on
  last_operation_ = "UpdateBucketExpiration";
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
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  static constexpr char kSql[] =
      // clang-format off
      "UPDATE buckets "
        "SET persistent = ? "
        "WHERE id = ? "
        "RETURNING " BUCKET_INFO_FIELDS_SELECTOR;
  // clang-format on
  last_operation_ = "UpdateBucketPersistence";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindBool(0, persistent);
  statement.BindInt64(1, bucket.value());
  ScheduleCommit();

  return BucketInfoFromSqlStatement(statement);
}

QuotaErrorOr<BucketInfo> QuotaDatabase::GetBucketById(BucketId bucket_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE id = ?";
  // clang-format on
  last_operation_ = "GetBucketById";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket_id.value());

  return BucketInfoFromSqlStatement(statement);
}

QuotaErrorOr<std::set<BucketInfo>> QuotaDatabase::GetBucketsForType(
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE type = ?";
  // clang-format on
  last_operation_ = "GetBucketsForType";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  return BucketInfosFromSqlStatement(statement);
}

QuotaErrorOr<std::set<BucketInfo>> QuotaDatabase::GetBucketsForHost(
    const std::string& host,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE host = ? AND type = ?";
  // clang-format on
  last_operation_ = "GetBucketsForHost";
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
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE storage_key = ? AND type = ?";
  // clang-format on
  last_operation_ = "GetBucketsForStorageKey";
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
  if (open_error != QuotaError::kNone) {
    return open_error;
  }

  // clang-format off
  static constexpr char kSqlReadLastAccessed[] =
      "SELECT last_accessed FROM buckets "
        "WHERE storage_key = ? AND type = ? AND name = ?";
  // clang-format on
  last_operation_ = "ReadStorageKeyLastAccessTime";
  sql::Statement statement_read(
      db_->GetCachedStatement(SQL_FROM_HERE, kSqlReadLastAccessed));
  statement_read.BindString(0, storage_key.Serialize());
  statement_read.BindInt(1, static_cast<int>(type));
  statement_read.BindString(2, kDefaultBucketName);

  if (statement_read.Step()) {
    base::Time earlier_last_accessed = statement_read.ColumnTime(0);
    // We want to record the delta in days between the last_accessed field value
    // and the new value so we better understand how often old quota buckets are
    // loaded for new use.
    if (!earlier_last_accessed.is_null() &&
        last_accessed > earlier_last_accessed) {
      int days_since_last_accessed =
          (last_accessed - earlier_last_accessed).InDays();
      if (days_since_last_accessed > 400) {
        base::UmaHistogramCustomCounts("Quota.DaysSinceLastAccessed400DaysGT",
                                       days_since_last_accessed, 401,
                                       kDaysInTenYears, 100);
      } else {
        base::UmaHistogramCustomCounts("Quota.DaysSinceLastAccessed400DaysLTE",
                                       days_since_last_accessed, 1, 400, 100);
      }
    }
  }

  // clang-format off
  static constexpr char kSqlSetLastAccessed[] =
      "UPDATE buckets "
        "SET use_count = use_count + 1, last_accessed = ? "
        "WHERE storage_key = ? AND type = ? AND name = ?";
  // clang-format on
  last_operation_ = "SetStorageKeyLastAccessTime";
  sql::Statement statement_set(
      db_->GetCachedStatement(SQL_FROM_HERE, kSqlSetLastAccessed));
  statement_set.BindTime(0, last_accessed);
  statement_set.BindString(1, storage_key.Serialize());
  statement_set.BindInt(2, static_cast<int>(type));
  statement_set.BindString(3, kDefaultBucketName);

  if (!statement_set.Run()) {
    return QuotaError::kDatabaseError;
  }

  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaError QuotaDatabase::SetBucketLastAccessTime(BucketId bucket_id,
                                                  base::Time last_accessed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return open_error;
  }

  // clang-format off
  static constexpr char kSql[] =
      "UPDATE buckets "
        "SET use_count = use_count + 1, last_accessed = ? "
        "WHERE id = ?";
  // clang-format on
  last_operation_ = "SetBucketLastAccessTime";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindTime(0, last_accessed);
  statement.BindInt64(1, bucket_id.value());

  if (!statement.Run()) {
    return QuotaError::kDatabaseError;
  }

  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaError QuotaDatabase::SetBucketLastModifiedTime(BucketId bucket_id,
                                                    base::Time last_modified) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bucket_id.is_null());
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return open_error;
  }

  static constexpr char kSql[] =
      "UPDATE buckets SET last_modified = ? WHERE id = ?";
  last_operation_ = "SetBucketLastModifiedTime";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindTime(0, last_modified);
  statement.BindInt64(1, bucket_id.value());

  if (!statement.Run()) {
    return QuotaError::kDatabaseError;
  }

  ScheduleCommit();
  return QuotaError::kNone;
}

QuotaError QuotaDatabase::RegisterInitialStorageKeyInfo(
    base::flat_map<StorageType, std::set<StorageKey>> storage_keys_by_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return open_error;
  }

  for (const auto& type_and_storage_keys : storage_keys_by_type) {
    StorageType storage_type = type_and_storage_keys.first;
    for (const auto& storage_key : type_and_storage_keys.second) {
      static constexpr char kSql[] =
          "INSERT OR IGNORE INTO buckets" BUCKETS_FIELDS_INSERTER;
      last_operation_ = "BootstrapDefaultBucket";
      sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
      BucketInitParams init_params =
          BucketInitParams::ForDefaultBucket(storage_key);
      BindBucketInitParamsToInsertStatement(
          init_params, storage_type, /*use_count=*/0,
          /*last_accessed=*/base::Time(),
          /*last_modified=*/base::Time(), statement);

      if (!statement.Run()) {
        return QuotaError::kDatabaseError;
      }
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
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_TABLE_ENTRY_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE id = ?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket_id.value());

  if (!statement.Step()) {
    return base::unexpected(statement.Succeeded() ? QuotaError::kNotFound
                                                  : QuotaError::kDatabaseError);
  }

  std::optional<StorageKey> storage_key =
      StorageKey::Deserialize(statement.ColumnString(1));
  if (!storage_key.has_value()) {
    return base::unexpected(QuotaError::kStorageKeyError);
  }

  mojom::BucketTableEntryPtr entry =
      BucketTableEntryFromSqlStatement(statement);
  return entry;
}

QuotaErrorOr<mojom::BucketTableEntryPtr> QuotaDatabase::DeleteBucketData(
    const BucketLocator& bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  // Doom bucket directory first so data is no longer accessible, even if
  // directory deletion fails. `storage_directory_` may be nullptr for
  // in-memory only.
  if (storage_directory_ && !storage_directory_->DoomBucket(bucket)) {
    return base::unexpected(QuotaError::kFileOperationError);
  }

  static constexpr char kSql[] =
      "DELETE FROM buckets WHERE id = ? "
      "RETURNING " BUCKET_TABLE_ENTRY_FIELDS_SELECTOR;
  last_operation_ = "DeleteBucket";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, bucket.id.value());

  if (!statement.Step()) {
    return base::unexpected(QuotaError::kDatabaseError);
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
  // TODO(crbug.com/40832940): For handling inconsistencies between the db and
  // the file system.
  ScheduleCommit();

  if (storage_directory_) {
    storage_directory_->ClearDoomedBuckets();
  }

  return BucketTableEntryFromSqlStatement(statement);
}

QuotaErrorOr<std::set<BucketLocator>> QuotaDatabase::GetBucketsForEviction(
    StorageType type,
    int64_t target_usage,
    const std::map<BucketLocator, int64_t>& usage_map,
    const std::set<BucketId>& bucket_exceptions,
    SpecialStoragePolicy* special_storage_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  std::set<BucketLocator> buckets_to_evict;

  // clang-format off
  static constexpr char kSql[] =
      "SELECT id, storage_key, name FROM buckets "
        "WHERE type = ? AND persistent = 0 "
        "ORDER BY last_accessed";
  // clang-format on
  last_operation_ = "GetBucketsForEviction";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  // The total space used by all buckets marked for eviction.
  int64_t total_usage = 0;

  while (statement.Step()) {
    std::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!read_storage_key.has_value()) {
      // TODO(estade): this row needs to be deleted.
      continue;
    }

    BucketId read_bucket_id = BucketId(statement.ColumnInt64(0));
    if (base::Contains(bucket_exceptions, read_bucket_id)) {
      continue;
    }

    // Only the default bucket is persisted by `navigator.storage.persist()`.
    const bool is_default = statement.ColumnString(2) == kDefaultBucketName;
    const GURL read_gurl = read_storage_key->origin().GetURL();
    if (is_default && special_storage_policy &&
        (special_storage_policy->IsStorageDurable(read_gurl) ||
         special_storage_policy->IsStorageUnlimited(read_gurl))) {
      continue;
    }

    BucketLocator locator(read_bucket_id, std::move(read_storage_key).value(),
                          type, is_default);
    const auto& bucket_usage = usage_map.find(locator);
    total_usage += (bucket_usage == usage_map.end()) ? 1 : bucket_usage->second;
    buckets_to_evict.insert(locator);
    if (total_usage >= target_usage) {
      break;
    }
  }
  if (buckets_to_evict.empty()) {
    return base::unexpected(QuotaError::kNotFound);
  }
  return buckets_to_evict;
}

QuotaErrorOr<std::set<StorageKey>> QuotaDatabase::GetStorageKeysForType(
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  static constexpr char kSql[] =
      "SELECT DISTINCT storage_key FROM buckets WHERE type = ?";
  last_operation_ = "GetStorageKeys";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  std::set<StorageKey> storage_keys;
  while (statement.Step()) {
    std::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(0));
    if (!read_storage_key.has_value()) {
      continue;
    }
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
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  DCHECK(!begin.is_max());
  DCHECK(end != base::Time());
  // clang-format off
  static constexpr char kSql[] =
      "SELECT id, storage_key, name FROM buckets "
        "WHERE type = ? AND last_modified >= ? AND last_modified < ?";
  // clang-format on
  last_operation_ = "GetBucketsModifiedBetween";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));
  statement.BindTime(1, begin);
  statement.BindTime(2, end);

  std::set<BucketLocator> buckets;
  while (statement.Step()) {
    std::optional<StorageKey> read_storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!read_storage_key.has_value()) {
      continue;
    }
    buckets.emplace(BucketId(statement.ColumnInt64(0)),
                    read_storage_key.value(), type,
                    statement.ColumnString(2) == kDefaultBucketName);
  }
  return buckets;
}

QuotaErrorOr<std::set<BucketInfo>> QuotaDatabase::GetExpiredBuckets(
    SpecialStoragePolicy* special_storage_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  // clang-format off
  static constexpr char kSqlExpired[] =
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE expiration > 0 AND expiration < ?";
  // clang-format on
  last_operation_ = "GetExpired";

  sql::Statement statement_expired(
      db_->GetCachedStatement(SQL_FROM_HERE, kSqlExpired));
  statement_expired.BindTime(0, GetNow());
  std::set<BucketInfo> expired_buckets =
      BucketInfosFromSqlStatement(statement_expired);

  // Return early if we don't need to gather stale buckets as well.
  if (already_evicted_stale_storage_ ||
      !base::FeatureList::IsEnabled(features::kEvictStaleQuotaStorage) ||
      GetNow() < evict_stale_buckets_after_) {
    return expired_buckets;
  }
  already_evicted_stale_storage_ = true;

  // We gather stale buckets in a different fetch round so that we can count
  // the amount found for metrics and filter out persistent buckets. After
  // launch it may be worth merging these queries.
  // clang-format off
  static constexpr char kSqlStale[] =
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE type = ? AND persistent = 0 AND "
          "last_accessed < ? AND last_modified < ?";
  // clang-format on
  last_operation_ = "GetStale";

  sql::Statement statement_stale(
      db_->GetCachedStatement(SQL_FROM_HERE, kSqlStale));
  statement_stale.BindInt(
      0, static_cast<int>(blink::mojom::StorageType::kTemporary));
  base::Time stale_cutoff = GetNow() - base::Days(400);
  statement_stale.BindTime(1, stale_cutoff);
  statement_stale.BindTime(2, stale_cutoff);

  QuotaErrorOr<BucketInfo> bucket;
  uint64_t buckets_found = 0;
  while ((bucket = BucketInfoFromSqlStatement(statement_stale)).has_value()) {
    // Only the default bucket is persisted by `navigator.storage.persist()`.
    const GURL read_gurl = bucket->storage_key.origin().GetURL();
    if (bucket->is_default() && special_storage_policy &&
        (special_storage_policy->IsStorageDurable(read_gurl) ||
         special_storage_policy->IsStorageUnlimited(read_gurl))) {
      continue;
    }
    expired_buckets.insert(*bucket);
    buckets_found++;
  }
  base::UmaHistogramCounts100000("Quota.StaleBucketCount", buckets_found);

  // Return early if we don't need to gather orphan buckets as well.
  if (!base::FeatureList::IsEnabled(features::kEvictOrphanQuotaStorage)) {
    return expired_buckets;
  }

  // We gather orphan buckets in a different fetch round so that we can count
  // the amount found. After launch it may be worth merging these queries.
  // We only need to check for ^1 and ^4 are these are indicators for the
  // presence of a nonce in the storage key.
  // For more on StorageKey encoding see EncodedAttribute in
  // third_party/blink/common/storage_key/storage_key.cc
  // clang-format off
  static constexpr char kSqlOrphan[] =
      "SELECT " BUCKET_INFO_FIELDS_SELECTOR
        "FROM buckets "
        "WHERE storage_key REGEXP '.*\\^(1|4).*' AND "
              "last_accessed < ? AND last_modified < ?";
  // clang-format on
  last_operation_ = "GetOrphan";
  sql::Statement statement_orphan(
      db_->GetCachedStatement(SQL_FROM_HERE, kSqlOrphan));
  base::Time orphan_cutoff = GetNow() - base::Days(1);
  statement_orphan.BindTime(0, orphan_cutoff);
  statement_orphan.BindTime(1, orphan_cutoff);

  buckets_found = 0;
  while ((bucket = BucketInfoFromSqlStatement(statement_orphan)).has_value()) {
    expired_buckets.insert(*bucket);
    buckets_found++;
  }
  base::UmaHistogramCounts100000("Quota.OrphanBucketCount", buckets_found);

  return expired_buckets;
}

bool QuotaDatabase::IsBootstrapped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (EnsureOpened() != QuotaError::kNone) {
    return false;
  }

  int flag = 0;
  return meta_table_->GetValue(kBucketsTableBootstrapped, &flag) && flag;
}

QuotaError QuotaDatabase::SetIsBootstrapped(bool bootstrap_flag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return open_error;
  }

  return meta_table_->SetValue(kBucketsTableBootstrapped, bootstrap_flag)
             ? QuotaError::kNone
             : QuotaError::kDatabaseError;
}

bool QuotaDatabase::RecoverOrRaze(int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::ignore = sql::Recovery::RecoverIfPossible(
      db_.get(), error_code,
      sql::Recovery::Strategy::kRecoverWithMetaVersionOrRaze);

  db_.reset();
  EnsureOpened();
  return db_ && db_->is_open();
}

QuotaError QuotaDatabase::CorruptForTesting(
    base::OnceCallback<void(const base::FilePath&)> corrupter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_) {
    // Commit the long-running transaction.
    db_->CommitTransactionDeprecated();
    db_->Close();
  }

  std::move(corrupter).Run(db_file_path_);

  if (!db_) {
    return QuotaError::kDatabaseError;
  }
  if (!OpenDatabase()) {
    return QuotaError::kDatabaseError;
  }

  // Begin a long-running transaction. This matches EnsureOpen().
  if (!db_->BeginTransactionDeprecated()) {
    return QuotaError::kDatabaseError;
  }
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

void QuotaDatabase::SetAlreadyEvictedStaleStorageForTesting(
    bool already_evicted_stale_storage) {
  already_evicted_stale_storage_ = already_evicted_stale_storage;
}

void QuotaDatabase::CommitNow() {
  Commit();
}

void QuotaDatabase::Commit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    return;
  }

  if (timer_.IsRunning()) {
    timer_.Stop();
  }

  last_operation_ = "Commit";
  DCHECK_EQ(1, db_->transaction_nesting());
  db_->CommitTransactionDeprecated();
  DCHECK_EQ(0, db_->transaction_nesting());
  db_->BeginTransactionDeprecated();
  DCHECK_EQ(1, db_->transaction_nesting());
}

void QuotaDatabase::ScheduleCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_.IsRunning()) {
    return;
  }
  timer_.Start(FROM_HERE, base::Milliseconds(kCommitIntervalMs), this,
               &QuotaDatabase::Commit);
}

QuotaError QuotaDatabase::EnsureOpened() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_) {
    return QuotaError::kNone;
  }

  // If we tried and failed once, don't try again in the same session
  // to avoid creating an incoherent mess on disk.
  if (is_disabled_) {
    return QuotaError::kDatabaseError;
  }

  sql::DatabaseOptions options{
      // The quota database is a critical storage component. If it's corrupted,
      // all client-side storage APIs fail, because they don't know where their
      // data is stored.
      .flush_to_media = true,
      .page_size = 4096,
      .cache_size = 500,
  };

  db_ = std::make_unique<sql::Database>(std::move(options));
  meta_table_ = std::make_unique<sql::MetaTable>();

  db_->set_histogram_tag("Quota");

  db_->set_error_callback(base::BindRepeating(&QuotaDatabase::OnSqliteError,
                                              base::Unretained(this)));

  // Migrate an existing database from the old path.
  if (!db_file_path_.empty() && !MoveLegacyDatabase()) {
    if (ResetStorage()) {
      // ResetStorage() has succeeded and database is already open.
      return QuotaError::kNone;
    }
    is_disabled_ = true;
    db_.reset();
    meta_table_.reset();
    return QuotaError::kDatabaseError;
  }

  if (!OpenDatabase() || !EnsureDatabaseVersion()) {
    LOG(ERROR) << "Could not open the quota database, resetting.";
    if (!db_file_path_.empty() && ResetStorage()) {
      // ResetStorage() has succeeded and database is already open.
      return QuotaError::kNone;
    }
    LOG(ERROR) << "Failed to reset the quota database.";
    is_disabled_ = true;
    db_.reset();
    meta_table_.reset();
    return QuotaError::kDatabaseError;
  }

  // Start a long-running transaction.
  DCHECK_EQ(0, db_->transaction_nesting());
  db_->BeginTransactionDeprecated();

  return QuotaError::kNone;
}

void QuotaDatabase::OnSqliteError(int sqlite_error_code,
                                  sql::Statement* statement) {
  // This check is here to DCHECK the error code in a place that gives a
  // useful stack trace.
  sql::IsErrorCatastrophic(sqlite_error_code);
  sqlite_error_code_ = sqlite_error_code;

  // Don't log UMA twice if the same operation manages to cause more than one
  // error (this can happen in particular when opening a database).
  if (last_operation_) {
    sql::UmaHistogramSqliteResult(
        std::string("Quota.DatabaseSpecificError.") + *last_operation_,
        sqlite_error_code);
    last_operation_.reset();
  }

  if (db_error_callback_) {
    db_error_callback_.Run(sqlite_error_code);
  }
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
  last_operation_ = "Open";

  // Open in memory database.
  if (db_file_path_.empty()) {
    if (db_->OpenInMemory()) {
      return true;
    }
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
    if (CreateSchema()) {
      return true;
    }
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
  for (const TableSchema& table : kTables) {
    DCHECK(db_->DoesTableExist(table.table_name));
  }
#endif

  return true;
}

bool QuotaDatabase::CreateSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(kinuko): Factor out the common code to create databases.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  if (!meta_table_->Init(db_.get(), kQuotaDatabaseCurrentSchemaVersion,
                         kQuotaDatabaseCompatibleVersion)) {
    return false;
  }

  for (const TableSchema& table : kTables) {
    if (!CreateTable(table)) {
      return false;
    }
  }

  for (const IndexSchema& index : kIndexes) {
    if (!CreateIndex(index)) {
      return false;
    }
  }

  return transaction.Commit();
}

bool QuotaDatabase::CreateTable(const TableSchema& table) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  last_operation_ = "CreateTable";
  std::string sql("CREATE TABLE ");
  sql += table.table_name;
  sql += table.columns;
  if (!db_->Execute(sql)) {
    VLOG(1) << "Failed to execute " << sql;
    return false;
  }
  return true;
}

bool QuotaDatabase::CreateIndex(const IndexSchema& index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string sql;
  if (index.unique) {
    sql += "CREATE UNIQUE INDEX ";
  } else {
    sql += "CREATE INDEX ";
  }
  sql += index.index_name;
  sql += " ON ";
  sql += index.table_name;
  sql += index.columns;
  if (!db_->Execute(sql)) {
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

  meta_table_.reset();
  db_.reset();

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
  if (is_recreating_) {
    return false;
  }

  base::AutoReset<bool> auto_reset(&is_recreating_, true);
  return EnsureOpened() == QuotaError::kNone;
}

QuotaError QuotaDatabase::DumpBucketTable(const BucketTableCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return open_error;
  }

  static constexpr char kSql[] =
      // clang-format off
      "SELECT " BUCKET_TABLE_ENTRY_FIELDS_SELECTOR
        "FROM buckets";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step()) {
    std::optional<StorageKey> storage_key =
        StorageKey::Deserialize(statement.ColumnString(1));
    if (!storage_key.has_value()) {
      continue;
    }

    auto entry = BucketTableEntryFromSqlStatement(statement);

    if (!callback.Run(std::move(entry))) {
      return QuotaError::kNone;
    }
  }
  return statement.Succeeded() ? QuotaError::kNone : QuotaError::kDatabaseError;
}

QuotaErrorOr<BucketInfo> QuotaDatabase::CreateBucketInternal(
    const BucketInitParams& params,
    StorageType type,
    int max_bucket_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40182349): Add DCHECKs for input validation.
  QuotaError open_error = EnsureOpened();
  if (open_error != QuotaError::kNone) {
    return base::unexpected(open_error);
  }

  // First verify this won't exceed the max bucket count if one is given.
  if (max_bucket_count > 0) {
    DCHECK_NE(params.name, kDefaultBucketName);
    // Note that technically we should be filtering out default buckets when
    // counting existing buckets so that the max count only applies to
    // non-default buckets. However the precise bucket count is not that
    // important and we don't want to perform a lot of string comparisons.
    static constexpr char kSql[] =
        // clang-format off
        "SELECT count(*) "
          "FROM buckets "
          "WHERE storage_key = ? AND type = ?";
    // clang-format on
    last_operation_ = "CountBuckets";
    sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindString(0, params.storage_key.Serialize());
    statement.BindInt(1, static_cast<int>(type));

    if (!statement.Step()) {
      return base::unexpected(QuotaError::kDatabaseError);
    }

    const int64_t current_bucket_count = statement.ColumnInt64(0);
    if (current_bucket_count >= max_bucket_count) {
      return base::unexpected(QuotaError::kQuotaExceeded);
    }

    base::UmaHistogramCounts100000("Storage.Buckets.BucketCount",
                                   current_bucket_count + 1);
  }

  static constexpr char kSql[] =
      // clang-format off
      "INSERT INTO buckets " BUCKETS_FIELDS_INSERTER
        " RETURNING " BUCKET_INFO_FIELDS_SELECTOR;
  // clang-format on
  last_operation_ = "CreateBucket";

  const base::Time now = GetNow();
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  BindBucketInitParamsToInsertStatement(params, type, /*use_count=*/0,
                                        /*last_accessed=*/now,
                                        /*last_modified=*/now, statement);
  QuotaErrorOr<BucketInfo> result = BucketInfoFromSqlStatement(statement);

  if (result.has_value()) {
    CHECK(!statement.Step());
    // Commit immediately so that we persist the bucket metadata to disk before
    // we inform other services / web apps (via the Buckets API) that we did so.
    // Once informed, that promise should persist across power failures.
    Commit();
  }

  return result;
}

void QuotaDatabase::SetDbErrorCallback(
    const base::RepeatingCallback<void(int)>& db_error_callback) {
  db_error_callback_ = db_error_callback;
}

}  // namespace storage
