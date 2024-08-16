// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/proto/aggregatable_report.pb.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Version number of the database.
//
// Version 1 - https://crrev.com/c/3038364
//             https://crrev.com/c/3462368
// Version 2 - https://crrev.com/c/3733377 (adding report_requests table)
// Version 3 - https://crrev.com/c/3842459 (adding reporting_origin index)
constexpr int AggregationServiceStorageSql::kCurrentVersionNumber = 3;

// Earliest version which can use a `kCurrentVersionNumber` database
// without failing.
constexpr int AggregationServiceStorageSql::kCompatibleVersionNumber = 3;

// Latest version of the database that cannot be upgraded to
// `kCurrentVersionNumber` without razing the database.
constexpr int AggregationServiceStorageSql::kDeprecatedVersionNumber = 0;

namespace {

constexpr base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("AggregationService");

// All columns in this table except `report_time` are designed to be "const".
// `request_id` uses AUTOINCREMENT to ensure that IDs aren't reused over the
// lifetime of the DB.
// `report_time` is when the request should be assembled and sent
// `creation_time` is when the request was stored in the database and will be
// used for data deletion.
// `reporting_origin` should match the corresponding proto field, but is
// maintained separately for data deletion.
// `request_proto` is a serialized AggregatableReportRequest proto.
static constexpr base::cstring_view kReportRequestsCreateTableSql =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS report_requests("
        "request_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
        "report_time INTEGER NOT NULL,"
        "creation_time INTEGER NOT NULL,"
        "reporting_origin TEXT NOT NULL,"
        "request_proto BLOB NOT NULL)";
// clang-format on

// Used to optimize report request lookup by report_time.
static constexpr base::cstring_view kReportTimeIndexSql =
    "CREATE INDEX IF NOT EXISTS report_time_idx ON "
    "report_requests(report_time)";

// Will be used to optimize report request lookup by creation_time for data
// clearing, see crbug.com/1340053.
static constexpr base::cstring_view kCreationTimeIndexSql =
    "CREATE INDEX IF NOT EXISTS creation_time_idx ON "
    "report_requests(creation_time)";

// Used to optimize checking whether there is capacity for the reporting origin.
static constexpr base::cstring_view kReportingOriginIndexSql =
    "CREATE INDEX IF NOT EXISTS reporting_origin_idx ON "
    "report_requests(reporting_origin)";

bool UpgradeAggregationServiceStorageSqlSchema(sql::Database& db,
                                               sql::MetaTable& meta_table) {
  if (meta_table.GetVersionNumber() != 1 && meta_table.GetVersionNumber() != 2)
    return false;  // Migration is not supported.

  sql::Transaction transaction(&db);

  if (!transaction.Begin())
    return false;

  if (meta_table.GetVersionNumber() == 1) {
    // == Migrate from version 1 to 2 ==
    // Create the new empty table.

    if (!db.Execute(kReportRequestsCreateTableSql))
      return false;

    if (!db.Execute(kReportTimeIndexSql))
      return false;

    if (!db.Execute(kCreationTimeIndexSql))
      return false;
  }

  // == Migrate from version 2 to 3 ==
  // Add the new index.
  if (!db.Execute(kReportingOriginIndexSql))
    return false;

  return meta_table.SetVersionNumber(
             AggregationServiceStorageSql::kCurrentVersionNumber) &&
         transaction.Commit();
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
    const base::Clock* clock,
    int max_stored_requests_per_reporting_origin)
    : run_in_memory_(run_in_memory),
      path_to_database_(run_in_memory_
                            ? base::FilePath()
                            : path_to_database.Append(kDatabasePath)),
      clock_(*clock),
      max_stored_requests_per_reporting_origin_(
          max_stored_requests_per_reporting_origin),
      db_(sql::DatabaseOptions{.page_size = 4096, .cache_size = 32}) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  CHECK(clock);

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
  CHECK(network::IsUrlPotentiallyTrustworthy(url));

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return {};

  static constexpr char kGetUrlIdSql[] =
      "SELECT url_id FROM urls WHERE url = ? AND expiry_time > ?";
  sql::Statement get_url_id_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetUrlIdSql));
  get_url_id_statement.BindString(0, url.spec());
  get_url_id_statement.BindTime(1, clock_->Now());
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
  CHECK(network::IsUrlPotentiallyTrustworthy(url));
  CHECK_LE(keyset.keys.size(), PublicKeyset::kMaxNumberKeys);

  // TODO(crbug.com/40190806): Add an allowlist for helper server urls and
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
  CHECK(network::IsUrlPotentiallyTrustworthy(url));

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
  CHECK(!delete_begin.is_null());
  CHECK(!delete_end.is_null());
  CHECK(!delete_begin.is_min() || !delete_end.is_max());

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
  CHECK(!delete_end.is_null());

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
  CHECK(!keyset.fetch_time.is_null());
  CHECK(!keyset.expiry_time.is_null());
  CHECK(db_.HasActiveTransactions());

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
    CHECK_LE(key.id.size(), PublicKey::kMaxIdSize);
    CHECK_EQ(key.key.size(), PublicKey::kKeyByteLength);

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
  CHECK(db_.HasActiveTransactions());

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
  CHECK(db_.HasActiveTransactions());

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

bool AggregationServiceStorageSql::ReportingOriginHasCapacity(
    std::string_view serialized_reporting_origin) {
  static constexpr char kCountRequestSql[] =
      "SELECT COUNT(*)FROM report_requests WHERE reporting_origin = ?";
  sql::Statement count_request_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kCountRequestSql));
  count_request_statement.BindString(0, serialized_reporting_origin);

  if (!count_request_statement.Step())
    return false;

  int64_t count = count_request_statement.ColumnInt64(0);

  // Goes above 1000 to ensure the limit is being applied correctly.
  base::UmaHistogramCustomCounts(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "StoredRequestsPerReportingOrigin",
      count, /*min=*/1, /*exclusive_max=*/2000, /*buckets=*/50);

  return count < max_stored_requests_per_reporting_origin_;
}

void AggregationServiceStorageSql::UpdateReportForSendFailure(
    AggregationServiceStorage::RequestId request_id,
    base::Time new_report_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDatabaseOpen(DbCreationPolicy::kCreateIfAbsent))
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  static constexpr char kGetRequestProtoSql[] =
      "SELECT request_proto FROM report_requests WHERE request_id=?";
  sql::Statement get_request_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetRequestProtoSql));
  get_request_statement.BindInt64(0, request_id.value());

  if (!get_request_statement.Step())
    return;

  base::span<const uint8_t> blob = get_request_statement.ColumnBlob(0);
  proto::AggregatableReportRequest request_proto;
  if (!request_proto.ParseFromArray(blob.data(), blob.size()))
    return;

  if (request_proto.failed_send_attempts() < 0)
    return;

  request_proto.set_failed_send_attempts(request_proto.failed_send_attempts() +
                                         1);

  size_t size = request_proto.ByteSizeLong();
  std::vector<uint8_t> serialized_proto(size);
  if (!request_proto.SerializeToArray(serialized_proto.data(), size))
    return;

  static constexpr char kUpdateRequestSql[] =
      "UPDATE report_requests SET report_time=?,request_proto=? "
      "WHERE request_id=?";

  sql::Statement update_request_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdateRequestSql));

  update_request_statement.BindTime(0, new_report_time);
  update_request_statement.BindBlob(1, serialized_proto);
  update_request_statement.BindInt64(2, request_id.value());

  if (!update_request_statement.Run())
    return;

  transaction.Commit();
}

void AggregationServiceStorageSql::StoreRequest(
    AggregatableReportRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Force the creation of the database if it doesn't exist, as we need to
  // persist the request.
  if (!EnsureDatabaseOpen(DbCreationPolicy::kCreateIfAbsent))
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  const AggregatableReportSharedInfo& shared_info = request.shared_info();
  std::string serialized_reporting_origin =
      shared_info.reporting_origin.Serialize();

  bool reporting_origin_has_capacity =
      ReportingOriginHasCapacity(serialized_reporting_origin);
  base::UmaHistogramBoolean(
      "PrivacySandbox.AggregationService.Storage.Sql.StoreRequestHasCapacity",
      reporting_origin_has_capacity);

  if (!reporting_origin_has_capacity)
    return;

  static constexpr char kStoreRequestSql[] =
      "INSERT INTO report_requests("
      "report_time,creation_time,reporting_origin,request_proto) "
      "VALUES(?,?,?,?)";

  sql::Statement store_request_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kStoreRequestSql));

  store_request_statement.BindTime(0, shared_info.scheduled_report_time);
  store_request_statement.BindTime(1, clock_->Now());
  store_request_statement.BindString(2, serialized_reporting_origin);

  std::vector<uint8_t> serialized_request = request.Serialize();

  // While an empty vector can be a valid proto serialization, report requests
  // should always be non-empty.
  CHECK(!serialized_request.empty());
  store_request_statement.BindBlob(3, serialized_request);

  if (!store_request_statement.Run())
    return;

  transaction.Commit();
}

void AggregationServiceStorageSql::DeleteRequest(
    AggregationServiceStorage::RequestId request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return;

  DeleteRequestImpl(request_id);
}

bool AggregationServiceStorageSql::DeleteRequestImpl(RequestId request_id) {
  static constexpr char kDeleteRequestSql[] =
      "DELETE FROM report_requests WHERE request_id=?";

  sql::Statement delete_request_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteRequestSql));

  delete_request_statement.BindInt64(0, request_id.value());

  return delete_request_statement.Run();
}

std::optional<base::Time> AggregationServiceStorageSql::NextReportTimeAfter(
    base::Time strictly_after_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return std::nullopt;

  return NextReportTimeAfterImpl(strictly_after_time);
}

std::optional<base::Time> AggregationServiceStorageSql::NextReportTimeAfterImpl(
    base::Time strictly_after_time) {
  static constexpr char kGetRequestsSql[] =
      "SELECT MIN(report_time) FROM report_requests WHERE report_time>?";

  sql::Statement get_requests_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetRequestsSql));

  get_requests_statement.BindTime(0, strictly_after_time);

  if (get_requests_statement.Step() &&
      get_requests_statement.GetColumnType(0) != sql::ColumnType::kNull) {
    return get_requests_statement.ColumnTime(0);
  }
  return std::nullopt;
}

std::vector<AggregationServiceStorage::RequestAndId>
AggregationServiceStorageSql::GetRequestsReportingOnOrBefore(
    base::Time not_after_time,
    std::optional<int> limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!limit.has_value() || limit.value() > 0);

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return {};

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return {};
  }

  static constexpr char kGetRequestsSql[] =
      "SELECT request_id,report_time,request_proto FROM report_requests "
      "WHERE report_time<=? ORDER BY report_time LIMIT ?";

  sql::Statement get_requests_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetRequestsSql));
  get_requests_statement.BindTime(0, not_after_time);
  // Negative number indicates no limit.
  // See https://www.sqlite.org/lang_select.html.
  get_requests_statement.BindInt(1, limit.value_or(-1));

  // TODO(crbug.com/40230192): Limit the total number of results that can be
  // returned in one query.
  std::vector<AggregationServiceStorage::RequestAndId> result;
  std::vector<AggregationServiceStorage::RequestId> failures;
  while (get_requests_statement.Step()) {
    AggregationServiceStorage::RequestId request_id{
        get_requests_statement.ColumnInt64(0)};
    std::optional<AggregatableReportRequest> parsed_request =
        AggregatableReportRequest::Deserialize(
            get_requests_statement.ColumnBlob(2));
    if (!parsed_request) {
      failures.push_back(request_id);
      continue;
    }

    // Exclude internals page requests
    if (!not_after_time.is_max()) {
      base::UmaHistogramCustomTimes(
          "PrivacySandbox.AggregationService.Storage.Sql."
          "RequestDelayFromUpdatedReportTime2",
          not_after_time - get_requests_statement.ColumnTime(1),
          /*min=*/base::Milliseconds(1),
          /*max=*/base::Days(24),
          /*buckets=*/50);
    }

    result.push_back(AggregationServiceStorage::RequestAndId{
        .request = std::move(parsed_request.value()), .id = request_id});
  }

  if (!get_requests_statement.Succeeded())
    return {};

  // In case of deserialization failures, remove the request from storage. This
  // could occur if the coordinator chosen is no longer on the allowlist. It is
  // also possible in case of database corruption.
  for (AggregationServiceStorage::RequestId request_id : failures) {
    if (!DeleteRequestImpl(request_id)) {
      return {};
    }
  }

  if (!transaction.Commit()) {
    return {};
  }

  return result;
}

std::vector<AggregationServiceStorage::RequestAndId>
AggregationServiceStorageSql::GetRequests(
    const std::vector<AggregationServiceStorage::RequestId>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return {};

  static constexpr char kGetRequestSql[] =
      "SELECT request_id,request_proto FROM report_requests "
      "WHERE request_id=?";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kGetRequestSql));

  std::vector<AggregationServiceStorage::RequestAndId> result;
  for (AggregationServiceStorage::RequestId id : ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);
    if (!statement.Step())
      continue;
    std::optional<AggregatableReportRequest> parsed_request =
        AggregatableReportRequest::Deserialize(statement.ColumnBlob(1));
    if (!parsed_request)
      continue;
    result.push_back(AggregationServiceStorage::RequestAndId{
        .request = std::move(*parsed_request),
        .id = id,
    });
  }
  return result;
}

std::optional<base::Time>
AggregationServiceStorageSql::AdjustOfflineReportTimes(
    base::Time now,
    base::TimeDelta min_delay,
    base::TimeDelta max_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK_GE(min_delay, base::TimeDelta());
  CHECK_GE(max_delay, base::TimeDelta());
  CHECK_LE(min_delay, max_delay);

  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent))
    return std::nullopt;

  // Set the report time for all reports that should have been sent before `now`
  // to `now` + a random number of microseconds between `min_delay` and
  // `max_delay`, both inclusive. We use RANDOM, instead of a C++ method to
  // avoid having to pull all reports into memory and update them one by one. We
  // use ABS because RANDOM may return a negative integer. We add 1 to the
  // difference between `max_delay` and `min_delay` to ensure that the range of
  // generated values is inclusive. If `max_delay == min_delay`, we take the
  // remainder modulo 1, which is always 0.
  static constexpr char kAdjustOfflineReportTimesSql[] =
      "UPDATE report_requests SET report_time=?+ABS(RANDOM()%?)"
      "WHERE report_time<?";

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kAdjustOfflineReportTimesSql));
  statement.BindTime(0, now + min_delay);
  statement.BindInt64(1, 1 + (max_delay - min_delay).InMicroseconds());
  statement.BindTime(2, now);

  statement.Run();

  return NextReportTimeAfterImpl(base::Time::Min());
}

std::set<url::Origin>
AggregationServiceStorageSql::GetReportRequestReportingOrigins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDatabaseOpen(DbCreationPolicy::kFailIfAbsent)) {
    return {};
  }

  std::set<url::Origin> origins;
  static constexpr char kSelectRequestReportingOrigins[] =
      "SELECT reporting_origin FROM report_requests";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectRequestReportingOrigins));

  while (statement.Step()) {
    url::Origin reporting_origin =
        url::Origin::Create(GURL(statement.ColumnString(0)));
    if (reporting_origin.opaque()) {
      continue;
    }
    origins.insert(std::move(reporting_origin));
  }

  return origins;
}

void AggregationServiceStorageSql::ClearDataBetween(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter) {
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

    if (filter.is_null()) {
      ClearAllRequests();
      return;
    }
  } else {
    ClearPublicKeysFetchedBetween(delete_begin, delete_end);
  }

  ClearRequestsStoredBetween(delete_begin, delete_end, filter);
}

void AggregationServiceStorageSql::ClearRequestsStoredBetween(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter) {
  CHECK(!delete_begin.is_null());
  CHECK(!delete_end.is_null());
  CHECK(!delete_begin.is_min() || !delete_end.is_max() || !filter.is_null());

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  static constexpr char kSelectRequestsToDeleteSql[] =
      "SELECT request_id,reporting_origin FROM report_requests "
      "WHERE creation_time BETWEEN ? AND ?";
  sql::Statement select_requests_to_delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectRequestsToDeleteSql));
  select_requests_to_delete_statement.BindTime(0, delete_begin);
  select_requests_to_delete_statement.BindTime(1, delete_end);

  while (select_requests_to_delete_statement.Step()) {
    url::Origin reporting_origin = url::Origin::Create(
        GURL(select_requests_to_delete_statement.ColumnString(1)));
    if (filter.is_null() ||
        filter.Run(blink::StorageKey::CreateFirstParty(reporting_origin))) {
      if (!DeleteRequestImpl(
              RequestId(select_requests_to_delete_statement.ColumnInt64(0)))) {
        return;
      }
    }
  }

  if (!select_requests_to_delete_statement.Succeeded())
    return;

  transaction.Commit();
}

void AggregationServiceStorageSql::ClearAllRequests() {
  static constexpr char kClearAllRequests[] = "DELETE FROM report_requests";
  sql::Statement select_requests_to_delete_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kClearAllRequests));

  select_requests_to_delete_statement.Run();
}

void AggregationServiceStorageSql::HandleInitializationFailure(
    const InitStatus status) {
  RecordInitializationStatus(status);

  meta_table_.Reset();
  db_.Close();

  // It's possible that `db_status_` was set by `DatabaseErrorCallback()` during
  // a call to `sql::Database::Open()`. Some databases attempt recovery at this
  // point, but we opt to delete the database from disk. Recovery can always
  // result in partial data loss, even when it appears to succeed. SQLite's
  // documentation discusses how some use cases can tolerate partial data loss,
  // while others cannot: <https://www.sqlite.org/recovery.html>.
  if (db_status_ == DbStatus::kClosedDueToCatastrophicError) {
    const bool delete_ok = sql::Database::Delete(path_to_database_);
    LOG_IF(WARNING, !delete_ok)
        << "Failed to delete database after catastrophic SQLite error";
  }

  db_status_ = DbStatus::kClosed;
}

bool AggregationServiceStorageSql::EnsureDatabaseOpen(
    DbCreationPolicy creation_policy) {
  if (!db_status_) {
    if (run_in_memory_) {
      db_status_ = DbStatus::kDeferringCreation;
    } else {
      db_status_ = base::PathExists(path_to_database_)
                       ? DbStatus::kDeferringOpen
                       : DbStatus::kDeferringCreation;
    }
  }

  switch (*db_status_) {
    // If the database file has not been created, we defer creation until
    // storage needs to be used for an operation which needs to operate even on
    // an empty database.
    case DbStatus::kDeferringCreation:
      if (creation_policy == DbCreationPolicy::kFailIfAbsent)
        return false;
      break;
    case DbStatus::kDeferringOpen:
      break;
    case DbStatus::kOpen:
      return true;
    case DbStatus::kClosed:
    case DbStatus::kClosedDueToCatastrophicError:
      return false;
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

  if (!InitializeSchema(db_status_ == DbStatus::kDeferringCreation)) {
    HandleInitializationFailure(InitStatus::kFailedToInitializeSchema);
    return false;
  }

  db_status_ = DbStatus::kOpen;
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

  if (current_version <= kDeprecatedVersionNumber ||
      meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber ||
      !UpgradeAggregationServiceStorageSqlSchema(db_, meta_table_)) {
    // The database version is either deprecated, the version is too new to be
    // used, or the attempt to upgrade failed. In the second case (version too
    // new), the DB will never work until Chrome is re-upgraded. Assume the user
    // will continue using this Chrome version and raze the DB to get
    // aggregation service storage working.
    db_.Raze();
    meta_table_.Reset();
    return CreateSchema();
  }

  return true;  // Upgrade was successful.
}

bool AggregationServiceStorageSql::CreateSchema() {
  base::ElapsedThreadTimer timer;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

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

  // See constant definitions above for documentation.
  if (!db_.Execute(kReportRequestsCreateTableSql))
    return false;

  if (!db_.Execute(kReportTimeIndexSql))
    return false;

  if (!db_.Execute(kCreationTimeIndexSql))
    return false;

  if (!db_.Execute(kReportingOriginIndexSql))
    return false;

  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    return false;
  }

  if (timer.is_supported()) {
    base::UmaHistogramMediumTimes(
        "PrivacySandbox.AggregationService.Storage.Sql.CreationTime2",
        timer.Elapsed());
  }

  return transaction.Commit();
}

// The interaction between this error callback and `sql::Database` is complex.
// Here are just a few of the sharp edges:
//
// 1. This callback would become reentrant if it called a `sql::Database` method
//    that could encounter an error.
//
// 2. This callback may be invoked multiple times by a single call to a
//    `sql::Database` method.
//
// 3. This callback may see phantom errors that do not otherwise bubble up via
//    return values. This can happen because `sql::Database` runs the error
//    callback eagerly despite the fact that some of its methods ignore certain
//    errors.
//
//    A concrete example: opening the database may run the error callback *and*
//    return true if `sql::Database::Open()` encounters a transient error, but
//    opens the database successfully on the second try.
//
// Reducing this complexity will likely require a redesign of `sql::Database`'s
// error handling interface. See <https://crbug.com/40199997>.
void AggregationServiceStorageSql::DatabaseErrorCallback(int extended_error,
                                                         sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Inform the test framework that we encountered this error.
  std::ignore = sql::Database::IsExpectedSqliteError(extended_error);

  if (ignore_errors_for_testing_) {
    return;
  }

  // Consider the database closed to avoid further errors. Note that the value
  // we write to `db_status_` may be subsequently overwritten elsewhere if
  // `sql::Database` ignores the error (see sharp edge #3 above).
  if (sql::IsErrorCatastrophic(extended_error)) {
    db_status_ = DbStatus::kClosedDueToCatastrophicError;
  } else {
    db_status_ = DbStatus::kClosed;
  }

  // Prevent future uses of `db_` from having any effect until we unpoison it
  // with `db_.Close()`.
  if (db_.is_open()) {
    db_.Poison();
  }

  base::UmaHistogramEnumeration(
      "PrivacySandbox.AggregationService.Storage.Sql.Error",
      sql::ToSqliteLoggedResultCode(extended_error));
}

}  // namespace content
