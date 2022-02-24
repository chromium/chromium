// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/aggregation_service/public_key.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("AggregationService");

// Version number of the database.
//
// Version 1 - https://crrev.com/c/3038364
//             https://crrev.com/c/3462368
constexpr int kCurrentVersionNumber = 1;

// Earliest version which can use a `kCurrentVersionNumber` database
// without failing.
constexpr int kCompatibleVersionNumber = 1;

// Latest version of the database that cannot be upgraded to
// `kCurrentVersionNumber` without razing the database.
constexpr int kDeprecatedVersionNumber = 0;

bool UpgradeAggregationServiceStorageSqlSchema(sql::Database& db,
                                               sql::MetaTable& meta_table) {
  // Placeholder for database migration logic.
  NOTREACHED();

  return true;
}

void RecordInitializationStatus(
    const AggregationServiceStorageSql::InitStatus status) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus", status);
}

}  // namespace

AggregationServiceStorageSql::AggregationServiceStorageSql(
    bool run_in_memory,
    const base::FilePath& path_to_database,
    const base::Clock* clock)
    : run_in_memory_(run_in_memory),
      path_to_database_(run_in_memory_
                            ? base::FilePath()
                            : path_to_database.Append(kDatabasePath)),
      clock_(*clock),
      db_(sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 4096,
                               .cache_size = 32}) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(clock);

  db_.set_histogram_tag("AggregationService");

  // base::Unretained is safe here because the callback will only be called
  // while the sql::Database in `db_` is alive, and this instance owns `db_`.
  db_.set_error_callback(
      base::BindRepeating(&AggregationServiceStorageSql::DatabaseErrorCallback,
                          base::Unretained(this)));
}

AggregationServiceStorageSql::~AggregationServiceStorageSql() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::vector<PublicKey> AggregationServiceStorageSql::GetPublicKeys(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network::IsUrlPotentiallyTrustworthy(url));

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return {};

  static constexpr char kGetUrlIdSql[] =
      "SELECT url_id FROM urls WHERE url = ? AND expiry_time > ?";
  sql::Statement get_url_id_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetUrlIdSql));
  get_url_id_statement.BindString(0, url.spec());
  get_url_id_statement.BindTime(1, clock_.Now());
  if (!get_url_id_statement.Step())
    return {};

  int64_t url_id = get_url_id_statement.ColumnInt64(0);

  static constexpr char kGetKeysSql[] =
      "SELECT key_id, key FROM keys WHERE url_id = ? ORDER BY url_id";

  sql::Statement get_keys_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetKeysSql));
  get_keys_statement.BindInt64(0, url_id);

  // Partial results are not returned in case of any error.
  std::vector<PublicKey> result;
  while (get_keys_statement.Step()) {
    if (result.size() >= PublicKeyset::kMaxNumberKeys)
      return {};

    std::string id = get_keys_statement.ColumnString(0);

    std::vector<uint8_t> key;
    get_keys_statement.ColumnBlobAsVector(1, &key);

    if (id.size() > PublicKey::kMaxIdSize ||
        key.size() != PublicKey::kKeyByteLength) {
      return {};
    }

    result.emplace_back(std::move(id), std::move(key));
  }

  if (!get_keys_statement.Succeeded())
    return {};

  return result;
}

void AggregationServiceStorageSql::SetPublicKeys(const GURL& url,
                                                 const PublicKeyset& keyset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network::IsUrlPotentiallyTrustworthy(url));
  DCHECK_LE(keyset.keys.size(), PublicKeyset::kMaxNumberKeys);

  // TODO(crbug.com/1231703): Add an allowlist for helper server urls and
  // validate the url.

  // Force the creation of the database if it doesn't exist, as we need to
  // persist the public keys.
  if (!EnsureDatabaseOpen(DbCreationPolicy::kCreateIfAbsent))
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  // Replace the public keys for the url. Deleting the existing rows and
  // inserting new ones to reduce the complexity.
  if (!ClearPublicKeysImpl(url))
    return;

  if (!InsertPublicKeysImpl(url, keyset))
    return;

  transaction.Commit();
}

void AggregationServiceStorageSql::ClearPublicKeys(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network::IsUrlPotentiallyTrustworthy(url));

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  ClearPublicKeysImpl(url);

  transaction.Commit();
}

void AggregationServiceStorageSql::ClearPublicKeysFetchedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return;

  // Treat null times as unbounded lower or upper range. This is used by
  // browsing data remover.
  if (delete_begin.is_null())
    delete_begin = base::Time::Min();

  if (delete_end.is_null())
    delete_end = base::Time::Max();

  if (delete_begin.is_min() && delete_end.is_max()) {
    ClearAllPublicKeys();
    return;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  static constexpr char kDeleteCandidateData[] =
      "DELETE FROM urls WHERE fetch_time BETWEEN ? AND ? "
      "RETURNING url_id";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteCandidateData));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);

  while (statement.Step()) {
    if (!ClearPublicKeysByUrlId(/*url_id=*/statement.ColumnInt64(0))) {
      return;
    }
  }

  if (!statement.Succeeded())
    return;

  transaction.Commit();
}

void AggregationServiceStorageSql::ClearPublicKeysExpiredBy(
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!delete_end.is_null());

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  static constexpr char kDeleteUrlRangeSql[] =
      "DELETE FROM urls WHERE expiry_time <= ? "
      "RETURNING url_id";
  sql::Statement delete_urls_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteUrlRangeSql));
  delete_urls_statement.BindTime(0, delete_end);

  while (delete_urls_statement.Step()) {
    if (!ClearPublicKeysByUrlId(
            /*url_id=*/delete_urls_statement.ColumnInt64(0))) {
      return;
    }
  }

  if (!delete_urls_statement.Succeeded())
    return;

  transaction.Commit();
}

bool AggregationServiceStorageSql::InsertPublicKeysImpl(
    const GURL& url,
    const PublicKeyset& keyset) {
  DCHECK(!keyset.fetch_time.is_null());
  DCHECK(!keyset.expiry_time.is_null());
  DCHECK(db_.HasActiveTransactions());

  static constexpr char kInsertUrlSql[] =
      "INSERT INTO urls(url, fetch_time, expiry_time) VALUES (?,?,?)";

  sql::Statement insert_url_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertUrlSql));
  insert_url_statement.BindString(0, url.spec());
  insert_url_statement.BindTime(1, keyset.fetch_time);
  insert_url_statement.BindTime(2, keyset.expiry_time);

  if (!insert_url_statement.Run())
    return false;

  int64_t url_id = db_.GetLastInsertRowId();

  static constexpr char kInsertKeySql[] =
      "INSERT INTO keys(url_id, key_id, key) VALUES (?,?,?)";
  sql::Statement insert_key_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertKeySql));

  for (const PublicKey& key : keyset.keys) {
    DCHECK_LE(key.id.size(), PublicKey::kMaxIdSize);
    DCHECK_EQ(key.key.size(), PublicKey::kKeyByteLength);

    insert_key_statement.Reset(/*clear_bound_vars=*/true);
    insert_key_statement.BindInt64(0, url_id);
    insert_key_statement.BindString(1, key.id);
    insert_key_statement.BindBlob(2, key.key);

    if (!insert_key_statement.Run())
      return false;
  }

  return true;
}

bool AggregationServiceStorageSql::ClearPublicKeysImpl(const GURL& url) {
  DCHECK(db_.HasActiveTransactions());

  static constexpr char kDeleteUrlSql[] =
      "DELETE FROM urls WHERE url = ? "
      "RETURNING url_id";
  sql::Statement delete_url_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteUrlSql));
  delete_url_statement.BindString(0, url.spec());

  bool has_matched_url = delete_url_statement.Step();

  if (!delete_url_statement.Succeeded())
    return false;

  if (!has_matched_url)
    return true;

  return ClearPublicKeysByUrlId(
      /*url_id=*/delete_url_statement.ColumnInt64(0));
}

bool AggregationServiceStorageSql::ClearPublicKeysByUrlId(int64_t url_id) {
  DCHECK(db_.HasActiveTransactions());

  static constexpr char kDeleteKeysSql[] = "DELETE FROM keys WHERE url_id = ?";
  sql::Statement delete_keys_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteKeysSql));
  delete_keys_statement.BindInt64(0, url_id);
  return delete_keys_statement.Run();
}

void AggregationServiceStorageSql::ClearAllPublicKeys() {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  static constexpr char kDeleteAllUrlsSql[] = "DELETE FROM urls";
  sql::Statement delete_all_urls_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllUrlsSql));
  if (!delete_all_urls_statement.Run())
    return;

  static constexpr char kDeleteAllKeysSql[] = "DELETE FROM keys";
  sql::Statement delete_all_keys_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllKeysSql));
  if (!delete_all_keys_statement.Run())
    return;

  transaction.Commit();
}

void AggregationServiceStorageSql::HandleInitializationFailure(
    const InitStatus status) {
  RecordInitializationStatus(status);
  db_init_status_ = DbStatus::kClosed;
}

bool AggregationServiceStorageSql::EnsureDatabaseOpen(
    DbCreationPolicy creation_policy) {
  if (!db_init_status_) {
    if (run_in_memory_) {
      db_init_status_ = DbStatus::kDeferringCreation;
    } else {
      db_init_status_ = base::PathExists(path_to_database_)
                            ? DbStatus::kDeferringOpen
                            : DbStatus::kDeferringCreation;
    }
  }

  switch (*db_init_status_) {
    // If the database file has not been created, we defer creation until
    // storage needs to be used for an operation which needs to operate even on
    // an empty database.
    case DbStatus::kDeferringCreation:
      if (creation_policy == DbCreationPolicy::kFailIfAbsent)
        return false;
      break;
    case DbStatus::kDeferringOpen:
      break;
    case DbStatus::kClosed:
      return false;
    case DbStatus::kOpen:
      return true;
  }

  if (run_in_memory_) {
    if (!db_.OpenInMemory()) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbInMemory);
      return false;
    }
  } else {
    const base::FilePath& dir = path_to_database_.DirName();
    const bool dir_exists_or_was_created =
        base::DirectoryExists(dir) || base::CreateDirectory(dir);
    if (!dir_exists_or_was_created) {
      DLOG(ERROR)
          << "Failed to create directory for AggregationService database";
      HandleInitializationFailure(InitStatus::kFailedToCreateDir);
      return false;
    }
    if (!db_.Open(path_to_database_)) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbFile);
      return false;
    }
  }

  if (!InitializeSchema(db_init_status_ == DbStatus::kDeferringCreation)) {
    HandleInitializationFailure(InitStatus::kFailedToInitializeSchema);
    return false;
  }

  db_init_status_ = DbStatus::kOpen;
  RecordInitializationStatus(InitStatus::kSuccess);
  return true;
}

bool AggregationServiceStorageSql::InitializeSchema(bool db_empty) {
  if (db_empty)
    return CreateSchema();

  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber))
    return false;

  int current_version = meta_table_.GetVersionNumber();
  if (current_version == kCurrentVersionNumber)
    return true;

  if (current_version <= kDeprecatedVersionNumber) {
    // Note that this also razes the meta table, so it will need to be
    // initialized again.
    db_.Raze();
    return CreateSchema();
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // In this case the database version is too new to be used. The DB will
    // never work until Chrome is re-upgraded. Assume the user will continue
    // using this Chrome version and raze the DB to get aggregation service
    // storage working.
    db_.Raze();
    return CreateSchema();
  }

  return UpgradeAggregationServiceStorageSqlSchema(db_, meta_table_);
}

bool AggregationServiceStorageSql::CreateSchema() {
  base::ElapsedThreadTimer timer;

  // All of the columns in this table are designed to be "const".
  // `url` is the helper server url.
  // `fetch_time` is when the key is fetched and inserted into database, and
  // will be used for data deletion.
  // `expiry_time` is when the key becomes invalid and will be used for data
  // pruning.
  static constexpr char kUrlsTableSql[] =
      "CREATE TABLE IF NOT EXISTS urls("
      "    url_id INTEGER PRIMARY KEY NOT NULL,"
      "    url TEXT NOT NULL,"
      "    fetch_time INTEGER NOT NULL,"
      "    expiry_time INTEGER NOT NULL)";
  if (!db_.Execute(kUrlsTableSql))
    return false;

  static constexpr char kUrlsByUrlIndexSql[] =
      "CREATE UNIQUE INDEX IF NOT EXISTS urls_by_url_idx "
      "    ON urls(url)";
  if (!db_.Execute(kUrlsByUrlIndexSql))
    return false;

  // Will be used to optimize key lookup by fetch time for data clearing (see
  // crbug.com/1231689).
  static constexpr char kFetchTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS fetch_time_idx ON urls(fetch_time)";
  if (!db_.Execute(kFetchTimeIndexSql))
    return false;

  // Will be used to optimize key lookup by expiry time for data pruning (see
  // crbug.com/1231696).
  static constexpr char kExpiryTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS expiry_time_idx ON urls(expiry_time)";
  if (!db_.Execute(kExpiryTimeIndexSql))
    return false;

  // All of the columns in this table are designed to be "const".
  // `url_id` is the primary key of a row in the `urls` table.
  // `key_id` is an arbitrary string identifying the key which is set by helper
  // servers and not required to be unique, but is required to be unique per
  // url.
  // `key` is the public key as a sequence of bytes.
  static constexpr char kKeysTableSql[] =
      "CREATE TABLE IF NOT EXISTS keys("
      "    url_id INTEGER NOT NULL,"
      "    key_id TEXT NOT NULL,"
      "    key BLOB NOT NULL,"
      "    PRIMARY KEY(url_id, key_id)) WITHOUT ROWID";
  if (!db_.Execute(kKeysTableSql))
    return false;

  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    return false;
  }

  base::UmaHistogramMediumTimes(
      "PrivacySandbox.AggregationService.Storage.Sql.CreationTime",
      timer.Elapsed());

  return true;
}

void AggregationServiceStorageSql::DatabaseErrorCallback(int extended_error,
                                                         sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error) &&
      !ignore_errors_for_testing_)
    DLOG(FATAL) << db_.GetErrorMessage();

  // Consider the database closed to avoid further errors.
  db_init_status_ = DbStatus::kClosed;
}

}  // namespace content
