// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_sql.h"

#include <stdint.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_storage_sql_migrations.h"
#include "content/browser/conversions/sql_utils.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

const base::FilePath::CharType kInMemoryPath[] = FILE_PATH_LITERAL(":memory");

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("Conversions");

// Version number of the database.
//
// Version 1 - 2020/01/27 - https://crrev.com/c/1965450
//
// Version 2 - 2020/11/03 - https://crrev.com/c/2194182
//
// Version 2 adds a new impression column "conversion_destination" based on
// registerable domain which is used for attribution instead of
// "conversion_origin".
//
// Version 3 - 2021/03/08 - https://crrev.com/c/2743337
//
// Version 3 adds new impression columns source_type and attributed_truthfully.
//
// Version 4 - 2021/03/16 - https://crrev.com/c/2716913
//
// Version 4 adds a new rate_limits table.
//
// Version 5 - 2021/04/30 - https://crrev.com/c/2860056
//
// Version 6 - 2021/05/06 - https://crrev.com/c/2878235
//
// Version 6 adds the impression.priority column.
const int kCurrentVersionNumber = 6;

// Earliest version which can use a |kCurrentVersionNumber| database
// without failing.
const int kCompatibleVersionNumber = 6;

// Latest version of the database that cannot be upgraded to
// |kCurrentVersionNumber| without razing the database. No versions are
// currently deprecated.
const int kDeprecatedVersionNumber = 0;

void RecordInitializationStatus(const ConversionStorageSql::InitStatus status) {
  base::UmaHistogramEnumeration("Conversions.Storage.Sql.InitStatus", status,
                                ConversionStorageSql::InitStatus::kMaxValue);
}

void RecordImpressionsDeleted(int count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "Conversions.ImpressionsDeletedInDataClearOperation", count);
}

void RecordReportsDeleted(int count) {
  UMA_HISTOGRAM_COUNTS_1000("Conversions.ReportsDeletedInDataClearOperation",
                            count);
}

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
      rate_limit_table_(delegate.get(), clock),
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

  const std::string serialized_conversion_destination =
      impression.ConversionDestination().Serialize();
  const std::string serialized_reporting_origin =
      SerializeOrigin(impression.reporting_origin());

  // In the case where we get a new impression for a given <reporting_origin,
  // conversion_destination> we should mark all active, converted impressions
  // with the matching <reporting_origin, conversion_destination> as not active.
  const char kDeactivateMatchingConvertedImpressionsSql[] =
      "UPDATE impressions SET active = 0 "
      "WHERE conversion_destination = ? AND reporting_origin = ? AND "
      "active = 1 AND num_conversions > 0";
  sql::Statement deactivate_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, kDeactivateMatchingConvertedImpressionsSql));
  deactivate_statement.BindString(0, serialized_conversion_destination);
  deactivate_statement.BindString(1, serialized_reporting_origin);
  deactivate_statement.Run();

  const char kInsertImpressionSql[] =
      "INSERT INTO impressions"
      "(impression_data, impression_origin, conversion_origin, "
      "conversion_destination, "
      "reporting_origin, impression_time, expiry_time, source_type, "
      "attributed_truthfully, priority) "
      "VALUES (?,?,?,?,?,?,?,?,?,?)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertImpressionSql));
  statement.BindString(0, impression.impression_data());
  statement.BindString(1, serialized_impression_origin);
  statement.BindString(2, SerializeOrigin(impression.conversion_origin()));
  statement.BindString(3, serialized_conversion_destination);
  statement.BindString(4, serialized_reporting_origin);
  statement.BindTime(5, impression.impression_time());
  statement.BindTime(6, impression.expiry_time());
  statement.BindInt(7, static_cast<int>(impression.source_type()));
  statement.BindInt(8, 1 /*true*/);
  statement.BindInt64(9, impression.priority());
  statement.Run();

  transaction.Commit();
}

bool ConversionStorageSql::MaybeCreateAndStoreConversionReport(
    const StorableConversion& conversion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return false;

  const net::SchemefulSite& conversion_destination =
      conversion.conversion_destination();
  const std::string serialized_conversion_destination =
      conversion_destination.Serialize();

  int capacity =
      GetCapacityForStoringConversion(serialized_conversion_destination);
  if (capacity == 0)
    return false;

  const url::Origin& reporting_origin = conversion.reporting_origin();
  DCHECK(!conversion_destination.opaque());
  DCHECK(!reporting_origin.opaque());

  base::Time current_time = clock_->Now();

  // TODO(apaseltiner): Support kEvent as well as kNavigation.
  const StorableImpression::SourceType kSourceType =
      StorableImpression::SourceType::kNavigation;

  // Get all impressions that match this <reporting_origin,
  // conversion_destination> pair. Only get impressions that are active and not
  // past their expiry time.
  const char kGetMatchingImpressionsSql[] =
      "SELECT impression_id, impression_data, impression_origin, "
      "conversion_origin, impression_time, expiry_time, priority "
      "FROM impressions "
      "WHERE conversion_destination = ? AND reporting_origin = ? "
      "AND active = 1 AND expiry_time > ? AND source_type = ?"
      "ORDER BY impression_time DESC";

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetMatchingImpressionsSql));
  statement.BindString(0, serialized_conversion_destination);
  statement.BindString(1, SerializeOrigin(reporting_origin));
  statement.BindTime(2, current_time);
  statement.BindInt(3, static_cast<int>(kSourceType));

  std::vector<StorableImpression> impressions;

  while (statement.Step()) {
    int64_t impression_id = statement.ColumnInt64(0);
    std::string impression_data = statement.ColumnString(1);
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(2));
    url::Origin conversion_origin =
        DeserializeOrigin(statement.ColumnString(3));

    // Skip the report if the impression origin is opaque. This should only
    // happen if there is some sort of database corruption.
    if (impression_origin.opaque())
      continue;
    base::Time impression_time = statement.ColumnTime(4);
    base::Time expiry_time = statement.ColumnTime(5);
    int64_t attribution_source_priority = statement.ColumnInt64(6);

    StorableImpression impression(impression_data, impression_origin,
                                  conversion_origin, reporting_origin,
                                  impression_time, expiry_time, kSourceType,
                                  attribution_source_priority, impression_id);
    impressions.push_back(std::move(impression));
  }

  // Exit early if the last statement wasn't valid or if we have no impressions.
  if (!statement.Succeeded() || impressions.empty())
    return false;

  const StorableImpression& impression_to_attribute =
      delegate_->GetImpressionToAttribute(impressions);

  ConversionReport report(impression_to_attribute, conversion.conversion_data(),
                          /*conversion_time=*/current_time,
                          /*report_time=*/current_time,
                          /*conversion_id=*/absl::nullopt);

  // Allow the delegate to make arbitrary changes to the new conversion report
  // before we add it storage.
  delegate_->ProcessNewConversionReport(report);

  if (!rate_limit_table_.IsAttributionAllowed(db_.get(), report, current_time))
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  const char kStoreConversionSql[] =
      "INSERT INTO conversions "
      "(impression_id, conversion_data, conversion_time, report_time) "
      "VALUES(?,?,?,?)";
  sql::Statement store_conversion_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kStoreConversionSql));
  store_conversion_statement.BindInt64(0, *report.impression.impression_id());
  store_conversion_statement.BindString(1, report.conversion_data);
  store_conversion_statement.BindTime(2, current_time);
  store_conversion_statement.BindTime(3, report.report_time);
  if (!store_conversion_statement.Run())
    return false;

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
      delegate_->GetMaxConversionsPerImpression(kSourceType) - 1;

  // Update the attributed impression.
  impression_update_statement.BindInt(0, max_prior_conversions_before_inactive);
  impression_update_statement.BindInt64(1, *report.impression.impression_id());
  if (!impression_update_statement.Run())
    return false;

  // Delete all unattributed impressions.
  const char kDeleteUnattributedImpressionsSql[] =
      "DELETE FROM impressions WHERE impression_id = ?";
  sql::Statement delete_impression_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, kDeleteUnattributedImpressionsSql));

  for (const StorableImpression& impression : impressions) {
    if (impression.impression_id() == *impression_to_attribute.impression_id())
      continue;
    delete_impression_statement.Reset(/*clear_bound_vars=*/true);
    delete_impression_statement.BindInt64(0, *impression.impression_id());
    if (!delete_impression_statement.Run())
      return false;
    // Based on the deletion logic here and the fact that we delete impressions
    // with |num_conversions > 1| when there is a new matching impression in
    // |StoreImpression()|, we should be guaranteed that these impressions all
    // have |num_conversions == 0|, and that they never contributed to a rate
    // limit. Therefore, we don't need to call
    // |RateLimitTable::ClearDataForImpressionIds()| here.
  }

  if (!rate_limit_table_.AddRateLimit(db_.get(), report))
    return false;

  return transaction.Commit();
}

std::vector<ConversionReport> ConversionStorageSql::GetConversionsToReport(
    base::Time max_report_time,
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  // Get at most |limit| entries in the conversions table with a |report_time|
  // less than |max_report_time| and their matching information from the
  // impression table. Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  const char kGetExpiredConversionsSql[] =
      "SELECT C.conversion_data, C.conversion_time, "
      "C.report_time, "
      "C.conversion_id, I.impression_origin, I.conversion_origin, "
      "I.reporting_origin, I.impression_data, I.impression_time, "
      "I.expiry_time, I.impression_id, I.source_type, I.priority "
      "FROM conversions C JOIN impressions I ON "
      "C.impression_id = I.impression_id WHERE C.report_time <= ? "
      "LIMIT ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetExpiredConversionsSql));
  statement.BindTime(0, max_report_time);
  statement.BindInt(1, limit);

  std::vector<ConversionReport> conversions;
  while (statement.Step()) {
    std::string conversion_data = statement.ColumnString(0);
    base::Time conversion_time = statement.ColumnTime(1);
    base::Time report_time = statement.ColumnTime(2);
    int64_t conversion_id = statement.ColumnInt64(3);
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(4));
    url::Origin conversion_origin =
        DeserializeOrigin(statement.ColumnString(5));
    url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(6));
    std::string impression_data = statement.ColumnString(7);
    base::Time impression_time = statement.ColumnTime(8);
    base::Time expiry_time = statement.ColumnTime(9);
    int64_t impression_id = statement.ColumnInt64(10);
    StorableImpression::SourceType source_type =
        static_cast<StorableImpression::SourceType>(statement.ColumnInt(11));
    int64_t attribution_source_priority = statement.ColumnInt64(12);

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
                                  impression_time, expiry_time, source_type,
                                  attribution_source_priority, impression_id);

    ConversionReport report(std::move(impression), conversion_data,
                            conversion_time, report_time, conversion_id);

    conversions.push_back(std::move(report));
  }

  if (!statement.Succeeded())
    return {};
  return conversions;
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
  delete_expired_statement.BindTime(0, clock_->Now());
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

  // Measure the time it takes to perform a clear with a filter separately from
  // the above histogram.
  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.Storage.ClearDataWithFilterDuration");

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
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);

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

  int num_conversions_deleted =
      static_cast<int>(conversion_ids_to_delete.size());

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

    num_conversions_deleted += db_->GetLastChangeCount();
  }

  if (!rate_limit_table_.ClearDataForImpressionIds(
          db_.get(), unique_impression_ids_to_delete))
    return;

  if (!rate_limit_table_.ClearDataForOriginsInRange(db_.get(), delete_begin,
                                                    delete_end, filter))
    return;

  if (!transaction.Commit())
    return;

  RecordImpressionsDeleted(
      static_cast<int>(unique_impression_ids_to_delete.size()));
  RecordReportsDeleted(num_conversions_deleted);
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

  // Select all impressions and conversion reports in the given time range.
  // Note: This should follow the same basic logic in ClearData, with the
  // assumption that all origins match the filter. We cannot use a DELETE
  // statement, because we need the list of |impression_id|s to delete from the
  // |rate_limits| table.
  //
  // Optimizing these queries are also tough, see this comment for an idea:
  // http://crrev.com/c/2150071/12/content/browser/conversions/conversion_storage_sql.cc#468
  const char kSelectImpressionRangeSql[] =
      "SELECT impression_id FROM impressions WHERE (impression_time BETWEEN ?1 "
      "AND ?2) OR "
      "impression_id in (SELECT impression_id FROM conversions "
      "WHERE conversion_time BETWEEN ?1 AND ?2)";
  sql::Statement select_impressions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectImpressionRangeSql));
  select_impressions_statement.BindTime(0, delete_begin);
  select_impressions_statement.BindTime(1, delete_end);

  base::flat_set<int64_t> impression_ids_to_delete;
  while (select_impressions_statement.Step()) {
    int64_t impression_id = select_impressions_statement.ColumnInt64(0);
    impression_ids_to_delete.insert(impression_id);
  }
  if (!select_impressions_statement.Succeeded())
    return;

  const char kDeleteImpressionSql[] =
      "DELETE FROM impressions WHERE impression_id = ?";
  sql::Statement delete_impression_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteImpressionSql));
  for (int64_t impression_id : impression_ids_to_delete) {
    delete_impression_statement.Reset(/*clear_bound_vars=*/true);
    delete_impression_statement.BindInt64(0, impression_id);
    if (!delete_impression_statement.Run())
      return;
  }

  const char kDeleteConversionRangeSql[] =
      "DELETE FROM conversions WHERE (conversion_time BETWEEN ? AND ?) "
      "OR impression_id NOT IN (SELECT impression_id FROM impressions)";
  sql::Statement delete_conversions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteConversionRangeSql));
  delete_conversions_statement.BindTime(0, delete_begin);
  delete_conversions_statement.BindTime(1, delete_end);
  if (!delete_conversions_statement.Run())
    return;

  int num_conversions_deleted = db_->GetLastChangeCount();

  if (!rate_limit_table_.ClearDataForImpressionIds(db_.get(),
                                                   impression_ids_to_delete))
    return;

  if (!rate_limit_table_.ClearAllDataInRange(db_.get(), delete_begin,
                                             delete_end))
    return;

  if (!transaction.Commit())
    return;

  RecordImpressionsDeleted(static_cast<int>(impression_ids_to_delete.size()));
  RecordReportsDeleted(num_conversions_deleted);
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

  int num_conversions_deleted = db_->GetLastChangeCount();

  if (!delete_all_impressions_statement.Run())
    return;

  int num_impressions_deleted = db_->GetLastChangeCount();

  if (!rate_limit_table_.ClearAllDataAllTime(db_.get()))
    return;

  if (!transaction.Commit())
    return;

  RecordImpressionsDeleted(num_impressions_deleted);
  RecordReportsDeleted(num_conversions_deleted);
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

int ConversionStorageSql::GetCapacityForStoringConversion(
    const std::string& serialized_origin) {
  // This query should be reasonably optimized via conversion_destination_idx.
  // The conversion origin is the second column in a multi-column index where
  // the first column is just a boolean. Therefore the second column in the
  // index should be very well-sorted.
  //
  // Note: to take advantage of this, we need to hint to the query planner that
  // |active| is a boolean, so include it in the conditional.
  const char kCountConversionsSql[] =
      "SELECT COUNT(conversion_id) FROM conversions C JOIN impressions I ON"
      " I.impression_id = C.impression_id"
      " WHERE I.conversion_destination = ? AND (active BETWEEN 0 AND 1)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountConversionsSql));
  statement.BindString(0, serialized_origin);
  if (!statement.Step())
    return false;
  int count = static_cast<int>(statement.ColumnInt64(0));
  return std::max(0, delegate_->GetMaxConversionsPerOrigin() - count);
}

std::vector<StorableImpression> ConversionStorageSql::GetActiveImpressions(
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  // Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  const char kGetActiveImpressionsSql[] =
      "SELECT impression_data, impression_origin, conversion_origin, "
      "reporting_origin, impression_time, expiry_time, impression_id, "
      "source_type, priority "
      "FROM impressions "
      "WHERE active = 1 and expiry_time > ? "
      "LIMIT ?";

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetActiveImpressionsSql));
  statement.BindTime(0, clock_->Now());
  statement.BindInt(1, limit);

  std::vector<StorableImpression> impressions;
  while (statement.Step()) {
    std::string impression_data = statement.ColumnString(0);
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(1));
    url::Origin conversion_destination =
        DeserializeOrigin(statement.ColumnString(2));
    url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(3));
    base::Time impression_time = statement.ColumnTime(4);
    base::Time expiry_time = statement.ColumnTime(5);
    int64_t impression_id = statement.ColumnInt64(6);
    StorableImpression::SourceType source_type =
        static_cast<StorableImpression::SourceType>(statement.ColumnInt(7));
    int64_t attribution_source_priority = statement.ColumnInt64(8);

    StorableImpression impression(impression_data, impression_origin,
                                  conversion_destination, reporting_origin,
                                  impression_time, expiry_time, source_type,
                                  attribution_source_priority, impression_id);
    impressions.push_back(std::move(impression));
  }
  if (!statement.Succeeded())
    return {};
  return impressions;
}

void ConversionStorageSql::HandleInitializationFailure(
    const InitStatus status) {
  RecordInitializationStatus(status);
  db_.reset();
  db_init_status_ = DbStatus::kClosed;
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

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = true, .page_size = 4096, .cache_size = 32});
  db_->set_histogram_tag("Conversions");

  // Supply this callback with a weak_ptr to avoid calling the error callback
  // after |this| has been deleted.
  db_->set_error_callback(
      base::BindRepeating(&ConversionStorageSql::DatabaseErrorCallback,
                          weak_factory_.GetWeakPtr()));

  if (path_to_database_.value() == kInMemoryPath) {
    if (!db_->OpenInMemory()) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbInMemory);
      return false;
    }
  } else {
    const base::FilePath& dir = path_to_database_.DirName();
    const bool dir_exists_or_was_created =
        base::DirectoryExists(dir) || base::CreateDirectory(dir);
    if (dir_exists_or_was_created == false) {
      DLOG(ERROR) << "Failed to create directory for Conversion database";
      HandleInitializationFailure(InitStatus::kFailedToCreateDir);
      return false;
    }
    if (db_->Open(path_to_database_) == false) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbFile);
      return false;
    }
  }

  if (InitializeSchema(db_init_status_ == DbStatus::kDeferringCreation) ==
      false) {
    HandleInitializationFailure(InitStatus::kFailedToInitializeSchema);
    return false;
  }

  db_init_status_ = DbStatus::kOpen;
  RecordInitializationStatus(InitStatus::kSuccess);
  return true;
}
bool ConversionStorageSql::InitializeSchema(bool db_empty) {
  if (db_empty)
    return CreateSchema();

  int current_version = kCurrentVersionNumber;
  if (!sql::MetaTable::DoesTableExist(db_.get())) {
    // Version 1 of the schema did not have a metadata table.
    current_version = 1;
    if (!meta_table_.Init(db_.get(), current_version, kCompatibleVersionNumber))
      return false;
  } else {
    if (!meta_table_.Init(db_.get(), current_version, kCompatibleVersionNumber))
      return false;
    current_version = meta_table_.GetVersionNumber();
  }

  if (current_version == kCurrentVersionNumber)
    return true;

  if (current_version <= kDeprecatedVersionNumber) {
    // Note that this also razes the meta table, so it will need to be
    // initialized again.
    db_->Raze();
    return CreateSchema();
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // In this case the database version is too new to be used. The DB will
    // never work until Chrome is re-upgraded. Assume the user will continue
    // using this Chrome version and raze the DB to get conversion measurement
    // working.
    db_->Raze();
    return CreateSchema();
  }

  return ConversionStorageSqlMigrations::UpgradeSchema(this, db_.get(),
                                                       &meta_table_);
}

bool ConversionStorageSql::CreateSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();
  // TODO(https://crbug.com/1163599): Convert impression data and conversion
  // data fields to integers.
  //
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
  // |source_type| is the type of the source of the impression, currently always
  // |kNavigation|.
  // |attributed_truthfully| is whether the impression was noisily attributed:
  // the impression was either marked inactive to start so it would never send a
  // report, or a fake conversion report was generated for the impression.
  const char kImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS impressions"
      "(impression_id INTEGER PRIMARY KEY,"
      "impression_data TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "impression_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
      "num_conversions INTEGER DEFAULT 0,"
      "active INTEGER DEFAULT 1,"
      "conversion_destination TEXT NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "attributed_truthfully INTEGER NOT NULL,"
      "priority INTEGER NOT NULL)";
  if (!db_->Execute(kImpressionTableSql))
    return false;

  // Optimizes impression lookup by conversion destination/reporting origin
  // during calls to `MaybeCreateAndStoreConversionReport()`,
  // `StoreImpression()`, `DeleteExpiredImpressions()`. Impressions and
  // conversions are considered matching if they share this pair. These calls
  // only look at active conversions, so include |active| in the index.
  const char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active, conversion_destination, reporting_origin)";
  if (!db_->Execute(kConversionDestinationIndexSql))
    return false;

  // Optimizes calls to `DeleteExpiredImpressions()` and
  // `MaybeCreateAndStoreConversionReport()` by indexing impressions by expiry
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
  // |delegate_|.
  const char kConversionTableSql[] =
      "CREATE TABLE IF NOT EXISTS conversions "
      "(conversion_id INTEGER PRIMARY KEY,"
      " impression_id INTEGER,"
      " conversion_data TEXT NOT NULL,"
      " conversion_time INTEGER NOT NULL,"
      " report_time INTEGER NOT NULL)";
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
  if (!db_->Execute(kConversionClickIdIndexSql))
    return false;

  if (!rate_limit_table_.CreateTable(db_.get()))
    return false;

  if (!meta_table_.Init(db_.get(), kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    return false;
  }

  base::UmaHistogramMediumTimes("Conversions.Storage.CreationTime",
                                base::ThreadTicks::Now() - start_timestamp);
  return true;
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
    sql::Recovery::RecoverDatabaseWithMetaVersion(db_.get(), path_to_database_);

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
