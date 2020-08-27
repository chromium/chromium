// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_sql.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

std::string SerializeOrigin(const url::Origin& origin) {
  // Conversion API is only designed to be used for secure
  // contexts (targets and reporting endpoints). We should have filtered out bad
  // origins at a higher layer.
  DCHECK(!origin.opaque());
  return origin.Serialize();
}

url::Origin DeserializeOrigin(const std::string& origin) {
  return url::Origin::Create(GURL(origin));
}

int64_t SerializeTime(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

base::Time DeserializeTime(int64_t microseconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(microseconds));
}

const base::FilePath::CharType kInMemoryPath[] = FILE_PATH_LITERAL(":memory");

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("Conversions");

}  // namespace

// static
void ConversionStorageSql::RunInMemoryForTesting() {
  g_run_in_memory_ = true;
}

// static
bool ConversionStorageSql::g_run_in_memory_ = false;

ConversionStorageSql::ConversionStorageSql(
    const base::FilePath& path_to_database,
    std::unique_ptr<Delegate> delegate,
    const base::Clock* clock)
    : path_to_database_(g_run_in_memory_
                            ? base::FilePath(kInMemoryPath)
                            : path_to_database.Append(kDatabasePath)),
      clock_(clock),
      delegate_(std::move(delegate)),
      weak_factory_(this) {
  DCHECK(delegate_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ConversionStorageSql::~ConversionStorageSql() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ConversionStorageSql::StoreImpression(
    const StorableImpression& impression) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Force the creation of the database if it doesn't exist, as we need to
  // persist the impression.
  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent))
    return;

  // Cleanup any impression that may be expired by this point. This is done when
  // an impression is added to prevent additional logic for cleaning the table
  // while providing a guarantee that the size of the table is proportional to
  // the number of active impression.
  DeleteExpiredImpressions();

  // TODO(csharrison): Thread this failure to the caller and report a console
  // error.
  const std::string serialized_impression_origin =
      SerializeOrigin(impression.impression_origin());
  if (!HasCapacityForStoringImpression(serialized_impression_origin))
    return;

  // Wrap the deactivation and insertion in the same transaction. If the
  // deactivation fails, we do not want to store the new impression as we may
  // return the wrong set of impressions for a conversion.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  const std::string serialized_conversion_origin =
      SerializeOrigin(impression.conversion_origin());
  const std::string serialized_reporting_origin =
      SerializeOrigin(impression.reporting_origin());

  // In the case where we get a new impression for a given <reporting_origin,
  // conversion_origin> we should mark all active, converted impressions with
  // the matching <reporting_origin, conversion_origin> as not active.
  const char kDeactivateMatchingConvertedImpressionsSql[] =
      "UPDATE impressions SET active = 0 "
      "WHERE conversion_origin = ? AND reporting_origin = ? AND "
      "active = 1 AND num_conversions > 0";
  sql::Statement deactivate_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, kDeactivateMatchingConvertedImpressionsSql));
  deactivate_statement.BindString(0, serialized_conversion_origin);
  deactivate_statement.BindString(1, serialized_reporting_origin);
  deactivate_statement.Run();

  const char kInsertImpressionSql[] =
      "INSERT INTO impressions"
      "(impression_data, impression_origin, conversion_origin, "
      "reporting_origin, impression_time, expiry_time) "
      "VALUES (?,?,?,?,?,?)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertImpressionSql));
  statement.BindString(0, impression.impression_data());
  statement.BindString(1, serialized_impression_origin);
  statement.BindString(2, serialized_conversion_origin);
  statement.BindString(3, serialized_reporting_origin);
  statement.BindInt64(4, SerializeTime(impression.impression_time()));
  statement.BindInt64(5, SerializeTime(impression.expiry_time()));
  statement.Run();

  transaction.Commit();
}

int ConversionStorageSql::MaybeCreateAndStoreConversionReports(
    const StorableConversion& conversion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return 0;

  const url::Origin& conversion_origin = conversion.conversion_origin();
  const std::string serialized_conversion_origin =
      SerializeOrigin(conversion_origin);
  if (!HasCapacityForStoringConversion(serialized_conversion_origin))
    return 0;

  const url::Origin& reporting_origin = conversion.reporting_origin();
  DCHECK(!conversion_origin.opaque());
  DCHECK(!reporting_origin.opaque());

  base::Time current_time = clock_->Now();
  int64_t serialized_current_time = SerializeTime(current_time);

  // Get all impressions that match this <reporting_origin, conversion_origin>
  // pair. Only get impressions that are active and not past their expiry time.
  const char kGetMatchingImpressionsSql[] =
      "SELECT impression_id, impression_data, impression_origin, "
      "impression_time, expiry_time "
      "FROM impressions WHERE conversion_origin = ? AND reporting_origin = ? "
      "AND active = 1 AND expiry_time > ? "
      "ORDER BY impression_time DESC";

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetMatchingImpressionsSql));
  statement.BindString(0, serialized_conversion_origin);
  statement.BindString(1, SerializeOrigin(reporting_origin));
  statement.BindInt64(2, serialized_current_time);

  // Create a set of default reports to add to storage.
  std::vector<ConversionReport> new_reports;
  while (statement.Step()) {
    int64_t impression_id = statement.ColumnInt64(0);
    std::string impression_data = statement.ColumnString(1);
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(2));

    // Skip the report if the impression origin is opaque. This should only
    // happen if there is some sort of database corruption.
    if (impression_origin.opaque())
      continue;
    base::Time impression_time = DeserializeTime(statement.ColumnInt64(3));
    base::Time expiry_time = DeserializeTime(statement.ColumnInt64(4));

    StorableImpression impression(impression_data, impression_origin,
                                  conversion_origin, reporting_origin,
                                  impression_time, expiry_time, impression_id);

    ConversionReport report(std::move(impression), conversion.conversion_data(),
                            current_time, /*conversion_id=*/base::nullopt);
    new_reports.push_back(std::move(report));
  }

  // Exit early if the last statement wasn't valid or if we have no new reports.
  if (!statement.Succeeded() || new_reports.empty())
    return 0;

  // Allow the delegate to make arbitrary changes to the new conversion reports
  // before we add them storage.
  delegate_->ProcessNewConversionReports(&new_reports);

  // |delegate_| may have removed all reports at this point.
  if (new_reports.empty())
    return 0;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return 0;

  const char kStoreConversionSql[] =
      "INSERT INTO conversions "
      "(impression_id, conversion_data, conversion_time, report_time, "
      "attribution_credit) VALUES(?,?,?,?,?)";
  sql::Statement store_conversion_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kStoreConversionSql));

  // Mark impressions inactive if they hit the max conversions allowed limit
  // supplied by the delegate. Because only active impressions log conversions,
  // we do not need to handle cases where active = 0 in this query. Update
  // statements atomically update all values at once. Therefore, for the check
  // |num_conversions < ?|, we used the max number of conversions - 1 as the
  // param. This is not done inside the query to generate better opcodes.
  const char kUpdateImpressionForConversionSql[] =
      "UPDATE impressions SET num_conversions = num_conversions + 1, "
      "active = num_conversions < ? "
      "WHERE impression_id = ?";
  sql::Statement impression_update_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, kUpdateImpressionForConversionSql));

  // Subtract one from the max number of conversions per the query comment
  // above. We need to account for the new conversion in this comparison so we
  // provide the max number of conversions prior to this new conversion being
  // logged.
  int max_prior_conversions_before_inactive =
      delegate_->GetMaxConversionsPerImpression() - 1;

  for (const ConversionReport& report : new_reports) {
    // Insert each report into the conversions table.
    store_conversion_statement.Reset(/*clear_bound_vars=*/true);
    store_conversion_statement.BindInt64(0, *report.impression.impression_id());
    store_conversion_statement.BindString(1, report.conversion_data);
    store_conversion_statement.BindInt64(2, serialized_current_time);
    store_conversion_statement.BindInt64(3, SerializeTime(report.report_time));
    store_conversion_statement.BindInt(4, report.attribution_credit);
    store_conversion_statement.Run();

    // Update each associated impression.
    impression_update_statement.Reset(/*clear_bound_vars=*/true);
    impression_update_statement.BindInt(0,
                                        max_prior_conversions_before_inactive);
    impression_update_statement.BindInt64(1,
                                          *report.impression.impression_id());
    impression_update_statement.Run();
  }

  if (!transaction.Commit())
    return 0;
  return new_reports.size();
}

std::vector<ConversionReport> ConversionStorageSql::GetConversionsToReport(
    base::Time max_report_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  // Get all entries in the conversions table with a |report_time| less than
  // |expired_at| and their matching information from the impression table.
  const char kGetExpiredConversionsSql[] =
      "SELECT C.conversion_data, C.attribution_credit, C.report_time, "
      "C.conversion_id, I.impression_origin, I.conversion_origin, "
      "I.reporting_origin, I.impression_data, I.impression_time, "
      "I.expiry_time, I.impression_id "
      "FROM conversions C JOIN impressions I ON "
      "C.impression_id = I.impression_id WHERE C.report_time <= ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetExpiredConversionsSql));
  statement.BindInt64(0, SerializeTime(max_report_time));

  std::vector<ConversionReport> conversions;
  while (statement.Step()) {
    std::string conversion_data = statement.ColumnString(0);
    int attribution_credit = statement.ColumnInt(1);
    base::Time report_time = DeserializeTime(statement.ColumnInt64(2));
    int64_t conversion_id = statement.ColumnInt64(3);
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(4));
    url::Origin conversion_origin =
        DeserializeOrigin(statement.ColumnString(5));
    url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(6));
    std::string impression_data = statement.ColumnString(7);
    base::Time impression_time = DeserializeTime(statement.ColumnInt64(8));
    base::Time expiry_time = DeserializeTime(statement.ColumnInt64(9));
    int64_t impression_id = statement.ColumnInt64(10);

    // Ensure origins are valid before continuing. This could happen if there is
    // database corruption.
    // TODO(csharrison): This should be an extremely rare occurrence but it
    // would entail that some records will remain in the DB as vestigial if a
    // conversion is never sent. We should delete these entries from the DB.
    if (impression_origin.opaque() || conversion_origin.opaque() ||
        reporting_origin.opaque())
      continue;

    // Create the impression and ConversionReport objects from the retrieved
    // columns.
    StorableImpression impression(impression_data, impression_origin,
                                  conversion_origin, reporting_origin,
                                  impression_time, expiry_time, impression_id);

    ConversionReport report(std::move(impression), conversion_data, report_time,
                            conversion_id);
    report.attribution_credit = attribution_credit;

    conversions.push_back(std::move(report));
  }

  if (!statement.Succeeded())
    return {};
  return conversions;
}

std::vector<StorableImpression> ConversionStorageSql::GetActiveImpressions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  const char kGetImpressionsSql[] =
      "SELECT impression_data, impression_origin, conversion_origin, "
      "reporting_origin, impression_time, expiry_time, impression_id "
      "FROM impressions WHERE active = 1 AND expiry_time > ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetImpressionsSql));
  statement.BindInt64(0, SerializeTime(clock_->Now()));

  std::vector<StorableImpression> impressions;
  while (statement.Step()) {
    std::string impression_data = statement.ColumnString(0);
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(1));
    url::Origin conversion_origin =
        DeserializeOrigin(statement.ColumnString(2));
    url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(3));
    base::Time impression_time = DeserializeTime(statement.ColumnInt64(4));
    base::Time expiry_time = DeserializeTime(statement.ColumnInt64(5));
    int64_t impression_id = statement.ColumnInt64(6);

    StorableImpression impression(impression_data, impression_origin,
                                  conversion_origin, reporting_origin,
                                  impression_time, expiry_time, impression_id);
    impressions.push_back(std::move(impression));
  }
  if (!statement.Succeeded())
    return {};
  return impressions;
}

int ConversionStorageSql::DeleteExpiredImpressions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return 0;

  // Delete all impressions that have no associated conversions and are past
  // their expiry time. Optimized by |kImpressionExpiryIndexSql|.
  const char kDeleteExpiredImpressionsSql[] =
      "DELETE FROM impressions WHERE expiry_time <= ? AND "
      "impression_id NOT IN (SELECT impression_id FROM conversions)";
  sql::Statement delete_expired_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteExpiredImpressionsSql));
  delete_expired_statement.BindInt64(0, SerializeTime(clock_->Now()));
  if (!delete_expired_statement.Run())
    return 0;
  int change_count = db_->GetLastChangeCount();

  // Delete all impressions that have no associated conversions and are
  // inactive. This is done in a separate statement from
  // |kDeleteExpiredImpressionsSql| so that each query is optimized by an index.
  // Optimized by |kConversionUrlIndexSql|.
  const char kDeleteInactiveImpressionsSql[] =
      "DELETE FROM impressions WHERE active = 0 AND "
      "impression_id NOT IN (SELECT impression_id FROM conversions)";
  sql::Statement delete_inactive_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteInactiveImpressionsSql));

  if (!delete_inactive_statement.Run())
    return change_count;
  return change_count + db_->GetLastChangeCount();
}

bool ConversionStorageSql::DeleteConversion(int64_t conversion_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return false;

  // Delete the row identified by |conversion_id|.
  const char kDeleteSentConversionSql[] =
      "DELETE FROM conversions WHERE conversion_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSentConversionSql));
  statement.BindInt64(0, conversion_id);

  if (!statement.Run())
    return false;

  return db_->GetLastChangeCount() > 0;
}

void ConversionStorageSql::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return;

  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.ClearDataTime");
  if (filter.is_null()) {
    ClearAllDataInRange(delete_begin, delete_end);
    return;
  }

  // TODO(csharrison, johnidel): This query can be split up and optimized by
  // adding indexes on the impression_time and conversion_time columns.
  // See this comment for more information:
  // crrev.com/c/2150071/4/content/browser/conversions/conversion_storage_sql.cc#342
  const char kScanCandidateData[] =
      "SELECT C.conversion_id, I.impression_id,"
      "I.impression_origin, I.conversion_origin, I.reporting_origin "
      "FROM impressions I LEFT JOIN conversions C ON "
      "C.impression_id = I.impression_id WHERE"
      "(I.impression_time BETWEEN ?1 AND ?2) OR"
      "(C.conversion_time BETWEEN ?1 AND ?2)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kScanCandidateData));
  statement.BindInt64(0, SerializeTime(delete_begin));
  statement.BindInt64(1, SerializeTime(delete_end));

  std::vector<int64_t> impression_ids_to_delete;
  std::vector<int64_t> conversion_ids_to_delete;
  while (statement.Step()) {
    int64_t conversion_id = statement.ColumnInt64(0);
    int64_t impression_id = statement.ColumnInt64(1);
    if (filter.Run(DeserializeOrigin(statement.ColumnString(2))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(3))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(4)))) {
      impression_ids_to_delete.push_back(impression_id);
      if (conversion_id != 0)
        conversion_ids_to_delete.push_back(conversion_id);
    }
  }

  // Since multiple conversions can be associated with a single impression,
  // |impression_ids_to_delete| may contain duplicates. Remove duplicates by
  // converting the vector into a flat_set. Internally, this sorts the vector
  // and then removes duplicates.
  const base::flat_set<int64_t> unique_impression_ids_to_delete(
      impression_ids_to_delete);

  // TODO(csharrison, johnidel): Should we consider poisoning the DB if some of
  // the delete operations fail?
  if (!statement.Succeeded())
    return;

  // Delete the data in a transaction to avoid cases where the impression part
  // of a conversion is deleted without deleting the associated conversion, or
  // vice versa.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  for (int64_t impression_id : unique_impression_ids_to_delete) {
    const char kDeleteImpressionSql[] =
        "DELETE FROM impressions WHERE impression_id = ?";
    sql::Statement impression_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kDeleteImpressionSql));
    impression_statement.BindInt64(0, impression_id);
    if (!impression_statement.Run())
      return;
  }

  for (int64_t conversion_id : conversion_ids_to_delete) {
    const char kDeleteConversionSql[] =
        "DELETE FROM conversions WHERE conversion_id = ?";
    sql::Statement conversion_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kDeleteConversionSql));
    conversion_statement.BindInt64(0, conversion_id);
    if (!conversion_statement.Run())
      return;
  }

  // Careful! At this point we can still have some vestigial entries in the DB.
  // For example, if an impression has two conversions, and one conversion is
  // deleted, the above logic will delete the impression as well, leaving the
  // second conversion in limbo (it was not in the deletion time range).
  // Delete all unattributed conversions here to ensure everything is cleaned
  // up.
  for (int64_t impression_id : unique_impression_ids_to_delete) {
    const char kDeleteVestigialConversionSql[] =
        "DELETE FROM conversions WHERE impression_id = ?";
    sql::Statement delete_vestigial_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kDeleteVestigialConversionSql));
    delete_vestigial_statement.BindInt64(0, impression_id);
    if (!delete_vestigial_statement.Run())
      return;
  }
  transaction.Commit();
}

void ConversionStorageSql::ClearAllDataInRange(base::Time delete_begin,
                                               base::Time delete_end) {
  // Browsing data remover will call this with null |delete_begin|, but also
  // perform the ClearAllDataAllTime optimization if |delete_begin| is
  // base::Time::Min().
  if ((delete_begin.is_null() || delete_begin.is_min()) &&
      delete_end.is_max()) {
    ClearAllDataAllTime();
    return;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  // Delete all impressions and conversion reports in the given time range.
  // Note: This should follow the same basic logic in ClearData, with the
  // assumption that all origins match the filter. This means we can omit a
  // SELECT statement, and all of the in-memory id management.
  //
  // Optimizing these queries are also tough, see this comment for an idea:
  // http://crrev.com/c/2150071/12/content/browser/conversions/conversion_storage_sql.cc#468
  const char kDeleteImpressionRangeSql[] =
      "DELETE FROM impressions WHERE (impression_time BETWEEN ?1 AND ?2) OR "
      "impression_id in (SELECT impression_id FROM conversions "
      "WHERE conversion_time BETWEEN ?1 AND ?2)";
  sql::Statement delete_impressions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteImpressionRangeSql));
  delete_impressions_statement.BindInt64(0, SerializeTime(delete_begin));
  delete_impressions_statement.BindInt64(1, SerializeTime(delete_end));
  if (!delete_impressions_statement.Run())
    return;

  const char kDeleteConversionRangeSql[] =
      "DELETE FROM conversions WHERE (conversion_time BETWEEN ? AND ?) "
      "OR impression_id NOT IN (SELECT impression_id FROM impressions)";
  sql::Statement delete_conversions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteConversionRangeSql));
  delete_conversions_statement.BindInt64(0, SerializeTime(delete_begin));
  delete_conversions_statement.BindInt64(1, SerializeTime(delete_end));
  if (!delete_conversions_statement.Run())
    return;
  transaction.Commit();
}

void ConversionStorageSql::ClearAllDataAllTime() {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;
  const char kDeleteAllConversionsSql[] = "DELETE FROM conversions";
  const char kDeleteAllImpressionsSql[] = "DELETE FROM impressions";
  sql::Statement delete_all_conversions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllConversionsSql));
  sql::Statement delete_all_impressions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllImpressionsSql));
  if (!delete_all_conversions_statement.Run())
    return;
  if (!delete_all_impressions_statement.Run())
    return;
  transaction.Commit();
}

bool ConversionStorageSql::HasCapacityForStoringImpression(
    const std::string& serialized_origin) {
  // Optimized by impression_origin_idx.
  const char kCountImpressionsSql[] =
      "SELECT COUNT(impression_origin) FROM impressions WHERE "
      "impression_origin = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountImpressionsSql));
  statement.BindString(0, serialized_origin);
  if (!statement.Step())
    return false;
  int64_t count = statement.ColumnInt64(0);
  return count < delegate_->GetMaxImpressionsPerOrigin();
}

bool ConversionStorageSql::HasCapacityForStoringConversion(
    const std::string& serialized_origin) {
  // This query should be reasonably optimized via conversion_origin_idx. The
  // conversion origin is the second column in a multi-column index where the
  // first column is just a boolean. Therefore the second column in the index
  // should be very well-sorted.
  //
  // Note: to take advantage of this, we need to hint to the query planner that
  // |active| is a boolean, so include it in the conditional.
  const char kCountConversionsSql[] =
      "SELECT COUNT(conversion_id) FROM conversions C JOIN impressions I ON"
      " I.impression_id = C.impression_id"
      " WHERE I.conversion_origin = ? AND (active BETWEEN 0 AND 1)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountConversionsSql));
  statement.BindString(0, serialized_origin);
  if (!statement.Step())
    return false;
  int64_t count = statement.ColumnInt64(0);
  return count < delegate_->GetMaxConversionsPerOrigin();
}

bool ConversionStorageSql::LazyInit(DbCreationPolicy creation_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_init_status_) {
    if (g_run_in_memory_) {
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
      if (creation_policy == DbCreationPolicy::kIgnoreIfAbsent)
        return false;
      break;
    case DbStatus::kDeferringOpen:
      break;
    case DbStatus::kClosed:
      return false;
    case DbStatus::kOpen:
      return true;
  }

  db_ = std::make_unique<sql::Database>();
  db_->set_histogram_tag("Conversions");

  // Supply this callback with a weak_ptr to avoid calling the error callback
  // after |this| has been deleted.
  db_->set_error_callback(
      base::BindRepeating(&ConversionStorageSql::DatabaseErrorCallback,
                          weak_factory_.GetWeakPtr()));
  db_->set_page_size(4096);
  db_->set_cache_size(32);
  db_->set_exclusive_locking();

  const base::FilePath& dir = path_to_database_.DirName();
  bool opened = false;
  if (path_to_database_.value() == kInMemoryPath) {
    opened = db_->OpenInMemory();
  } else if (base::DirectoryExists(dir) || base::CreateDirectory(dir)) {
    opened = db_->Open(path_to_database_);
  } else {
    DLOG(ERROR) << "Failed to create directory for Conversion database";
  }

  if (!opened || !InitializeSchema()) {
    db_.reset();
    db_init_status_ = DbStatus::kClosed;
    return false;
  }

  db_init_status_ = DbStatus::kOpen;
  return true;
}

bool ConversionStorageSql::InitializeSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(johnidel, csharrison): Many impressions will share a target origin and
  // a reporting origin, so it makes sense to make a "shared string" table for
  // these to save disk / memory. However, this complicates the schema a lot, so
  // probably best to only do it if there's performance problems here.
  //
  // Origins usually aren't _that_ big compared to a 64 bit integer(8 bytes).
  //
  // All of the columns in this table are designed to be "const" except for
  // |num_conversions| and |active| which are updated when a new conversion is
  // received. |num_conversions| is the number of times a conversion report has
  // been created for a given impression. |delegate_| can choose to enforce a
  // maximum limit on this. |active| indicates whether an impression is able to
  // create new associated conversion reports. |active| can be unset on a number
  // of conditions:
  //   - An impression converted too many times.
  //   - A new impression was stored after an impression converted, making it
  //     ineligible for new impressions due to the attribution model documented
  //     in StoreImpression().
  //   - An impression has expired but still has unsent conversions in the
  //     conversions table meaning it cannot be deleted yet.
  const char kImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS impressions "
      "(impression_id INTEGER PRIMARY KEY,"
      " impression_data TEXT NOT NULL,"
      " impression_origin TEXT NOT NULL,"
      " conversion_origin TEXT NOT NULL,"
      " reporting_origin TEXT NOT NULL,"
      " impression_time INTEGER NOT NULL,"
      " expiry_time INTEGER NOT NULL,"
      " num_conversions INTEGER DEFAULT 0,"
      " active INTEGER DEFAULT 1)";
  if (!db_->Execute(kImpressionTableSql))
    return false;

  // Optimizes impression lookup by conversion/reporting origin during calls to
  // MaybeCreateAndStoreConversionReports(), StoreImpression(),
  // DeleteExpiredImpressions(). Impressions and conversions are considered
  // matching if they share this pair. These calls only look at active
  // conversions, so include |active| in the index.
  const char kConversionUrlIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_origin_idx "
      "ON impressions(active, conversion_origin, reporting_origin)";
  if (!db_->Execute(kConversionUrlIndexSql))
    return false;

  // Optimizes calls to DeleteExpiredImpressions() and
  // MaybeCreateAndStoreConversionReports() by indexing impressions by expiry
  // time. Both calls require only returning impressions that expire after a
  // given time.
  const char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db_->Execute(kImpressionExpiryIndexSql))
    return false;

  // Optimizes counting impressions by impression origin.
  const char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db_->Execute(kImpressionOriginIndexSql))
    return false;

  // All columns in this table are const. |impression_id| is the primary key of
  // a row in the [impressions] table, [impressions.impression_id].
  // |conversion_time| is the time at which the conversion was registered, and
  // should be used for clearing site data. |report_time| is the time a
  // <conversion, impression> pair should be reported, and is specified by
  // |delegate_|. |attribution_credit| is assigned by |delegate_| based on the
  // set of impressions returned from |kGetMatchingImpressionsSql|.
  const char kConversionTableSql[] =
      "CREATE TABLE IF NOT EXISTS conversions "
      "(conversion_id INTEGER PRIMARY KEY,"
      " impression_id INTEGER,"
      " conversion_data TEXT NOT NULL,"
      " conversion_time INTEGER NOT NULL,"
      " report_time INTEGER NOT NULL,"
      " attribution_credit INTEGER NOT NULL)";
  if (!db_->Execute(kConversionTableSql))
    return false;

  // Optimize sorting conversions by report time for calls to
  // GetConversionsToReport(). The reports with the earliest report times are
  // periodically fetched from storage to be sent.
  const char kConversionReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_report_idx "
      "ON conversions(report_time)";
  if (!db_->Execute(kConversionReportTimeIndexSql))
    return false;

  // Want to optimize conversion look up by click id. This allows us to
  // quickly know if an expired impression can be deleted safely if it has no
  // corresponding pending conversions during calls to
  // DeleteExpiredImpressions().
  const char kConversionClickIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_impression_id_idx "
      "ON conversions(impression_id)";
  return db_->Execute(kConversionClickIdIndexSql);
}

void ConversionStorageSql::DatabaseErrorCallback(int extended_error,
                                                 sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Attempt to recover a corrupt database, unless it is setup in memory.
  if (sql::Recovery::ShouldRecover(extended_error) &&
      (path_to_database_.value() != kInMemoryPath)) {
    // Prevent reentrant calls.
    db_->reset_error_callback();

    // After this call, the |db_| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabase(db_.get(), path_to_database_);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    ignore_result(sql::Database::IsExpectedSqliteError(extended_error));
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error) &&
      !ignore_errors_for_testing_)
    DLOG(FATAL) << db_->GetErrorMessage();

  // Consider the  database closed if we did not attempt to recover so we did
  // not produce further errors.
  db_init_status_ = DbStatus::kClosed;
}

}  // namespace content
