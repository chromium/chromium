// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_sql.h"

#include <stdint.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
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
// Version 5 drops the conversions.attribution_credit column.
//
// Version 6 - 2021/05/06 - https://crrev.com/c/2878235
//
// Version 6 adds the impressions.priority column.
//
// Version 7 - 2021/06/03 - https://crrev.com/c/2904386
//
// Version 7 adds the impressions.impression_site column.
//
// Version 8 - 2021/06/30 - https://crrev.com/c/2888906
//
// Version 8 changes the conversions.conversion_data and
// impressions.impression_data columns from TEXT to INTEGER and makes
// conversions.impression_id NOT NULL.
//
// Version 9 - 2021/06/30 - https://crrev.com/c/2951620
//
// Version 9 adds the conversions.priority column.
const int kCurrentVersionNumber = 9;

// Earliest version which can use a |kCurrentVersionNumber| database
// without failing.
const int kCompatibleVersionNumber = 9;

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

WARN_UNUSED_RESULT bool ShouldReplaceImpressionToAttribute(
    const absl::optional<StorableImpression>& impression_to_attribute,
    int64_t candidate_priority,
    base::Time candidate_impression_time) {
  if (!impression_to_attribute.has_value())
    return true;

  // Chooses the impression with the largest priority value. In the case of
  // ties, most recent impression_time is used to tie break.
  //
  // Note that impressions which do not get a priority get defaulted to 0,
  // meaning they can be attributed over impressions which set a negative
  // priority.
  if (impression_to_attribute->priority() < candidate_priority)
    return true;
  if (impression_to_attribute->priority() > candidate_priority)
    return false;
  return impression_to_attribute->impression_time() < candidate_impression_time;
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

  if (!EnsureCapacityForPendingDestinationLimit(impression))
    return;

  const std::string serialized_conversion_destination =
      impression.ConversionDestination().Serialize();
  const std::string serialized_reporting_origin =
      SerializeOrigin(impression.reporting_origin());

  // In the case where we get a new impression for a given <reporting_origin,
  // conversion_destination> we should mark all active, converted impressions
  // with the matching <reporting_origin, conversion_destination> as not active.
  static constexpr char kDeactivateMatchingConvertedImpressionsSql[] =
      "UPDATE impressions SET active = 0 "
      "WHERE conversion_destination = ? AND reporting_origin = ? AND "
      "active = 1 AND num_conversions > 0";
  sql::Statement deactivate_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, kDeactivateMatchingConvertedImpressionsSql));
  deactivate_statement.BindString(0, serialized_conversion_destination);
  deactivate_statement.BindString(1, serialized_reporting_origin);
  if (!deactivate_statement.Run())
    return;

  const StorableImpression::AttributionLogic attribution_logic =
      delegate_->SelectAttributionLogic(impression);

  static constexpr char kInsertImpressionSql[] =
      "INSERT INTO impressions"
      "(impression_data,impression_origin,conversion_origin,"
      "conversion_destination,"
      "reporting_origin,impression_time,expiry_time,source_type,"
      "attributed_truthfully,priority,impression_site,"
      "num_conversions,active)"
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertImpressionSql));
  statement.BindInt64(
      0, SerializeImpressionOrConversionData(impression.impression_data()));
  statement.BindString(1, serialized_impression_origin);
  statement.BindString(2, SerializeOrigin(impression.conversion_origin()));
  statement.BindString(3, serialized_conversion_destination);
  statement.BindString(4, serialized_reporting_origin);
  statement.BindTime(5, impression.impression_time());
  statement.BindTime(6, impression.expiry_time());
  statement.BindInt(7, static_cast<int>(impression.source_type()));
  statement.BindInt(8, static_cast<int>(attribution_logic));
  statement.BindInt64(9, impression.priority());
  statement.BindString(10, impression.ImpressionSite().Serialize());

  if (attribution_logic == StorableImpression::AttributionLogic::kFalsely) {
    // Falsely attributed impressions are immediately stored with
    // `num_conversions == 1` and `active == 0`, as they will be attributed via
    // the below call to `StoreConversionReport()` in the same transaction.
    statement.BindInt(11, 1);  // num_conversions
    statement.BindInt(12, 0);  // active
  } else {
    statement.BindInt(11, 0);  // num_conversions
    statement.BindInt(12, 1);  // active
  }

  if (!statement.Run())
    return;

  if (attribution_logic == StorableImpression::AttributionLogic::kFalsely) {
    DCHECK_EQ(StorableImpression::SourceType::kEvent, impression.source_type());

    int64_t impression_id = db_->GetLastInsertRowId();
    uint64_t event_source_trigger_data =
        delegate_->GetFakeEventSourceTriggerData();

    ConversionReport report(impression, event_source_trigger_data,
                            /*conversion_time=*/impression.impression_time(),
                            /*report_time=*/impression.impression_time(),
                            /*conversion_id=*/absl::nullopt);
    report.report_time = delegate_->GetReportTime(report);

    if (!StoreConversionReport(report, impression_id, /*priority=*/0))
      return;
  }

  transaction.Commit();
}

// Checks whether a new report is allowed to be stored for the given impression
// based on `GetMaxConversionsPerImpression()`. If there's sufficient capacity,
// the new report should be stored. Otherwise, if all existing reports were from
// an earlier window, the corresponding impression is deactivated and the new
// report should be dropped. Otherwise, If there's insufficient capacity, checks
// the new report's priority against all existing ones for the same impression.
// If all existing ones have greater priority, the new report should be dropped;
// otherwise, the existing one with the lowest priority is deleted and the new
// one should be stored.
ConversionStorageSql::MaybeReplaceLowerPriorityReportResult
ConversionStorageSql::MaybeReplaceLowerPriorityReport(
    const StorableImpression& impression,
    int num_conversions,
    int64_t conversion_priority,
    base::Time report_time) {
  DCHECK(impression.impression_id().has_value());
  DCHECK_GE(num_conversions, 0);

  // If there's already capacity for the new report, there's nothing to do.
  if (num_conversions <
      delegate_->GetMaxConversionsPerImpression(impression.source_type())) {
    return ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::
        kAddNewReport;
  }

  // Prioritization is scoped within report windows.
  // This is reasonably optimized as is, because we only store a ~small number
  // of reports per impression_id.
  static constexpr char kMinPrioritySql[] =
      "SELECT MIN(priority),conversion_id "
      "FROM conversions "
      "WHERE impression_id = ? AND report_time = ?";
  sql::Statement min_priority_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kMinPrioritySql));
  min_priority_statement.BindInt64(0, *impression.impression_id());
  min_priority_statement.BindTime(1, report_time);
  if (!min_priority_statement.Step()) {
    return ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::kError;
  }

  // Deactivate the impression as a new report will never be generated in the
  // future.
  if (min_priority_statement.GetColumnType(0) == sql::ColumnType::kNull) {
    static constexpr char kDeactivateSql[] =
        "UPDATE impressions SET active = 0 WHERE impression_id = ?";
    sql::Statement deactivate_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kDeactivateSql));
    deactivate_statement.BindInt64(0, *impression.impression_id());
    return deactivate_statement.Run()
               ? ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::
                     kDropNewReport
               : ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::
                     kError;
  }

  int64_t min_priority = min_priority_statement.ColumnInt64(0);
  int64_t conversion_id_with_min_priority =
      min_priority_statement.ColumnInt64(1);

  // If the new report's priority is less than or equal to all existing ones,
  // drop it.
  if (conversion_priority <= min_priority) {
    return ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::
        kDropNewReport;
  }

  // Otherwise, delete the existing report with the lowest priority.
  return DeleteConversionInternal(conversion_id_with_min_priority)
             ? ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::
                   kReplaceOldReport
             : ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::
                   kError;
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

  // Get all impressions that match this <reporting_origin,
  // conversion_destination> pair. Only get impressions that are active and not
  // past their expiry time.
  static constexpr char kGetMatchingImpressionsSql[] =
      "SELECT impression_origin,impression_id,impression_time,priority,"
      "impression_data,conversion_origin,expiry_time,"
      "attributed_truthfully,source_type,num_conversions "
      "FROM impressions "
      "WHERE conversion_destination = ? AND reporting_origin = ? "
      "AND active = 1 AND expiry_time > ? "
      "ORDER BY impression_time DESC";

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetMatchingImpressionsSql));
  statement.BindString(0, serialized_conversion_destination);
  statement.BindString(1, SerializeOrigin(reporting_origin));
  statement.BindTime(2, current_time);

  absl::optional<StorableImpression> impression_to_attribute;
  StorableImpression::AttributionLogic attribution_logic =
      StorableImpression::AttributionLogic::kNever;
  int num_conversions = 0;
  std::vector<int64_t> impression_ids_to_delete;

  while (statement.Step()) {
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(0));

    // Skip the report if the impression origin is opaque. This should only
    // happen if there is some sort of database corruption.
    if (impression_origin.opaque())
      continue;

    int64_t impression_id = statement.ColumnInt64(1);
    base::Time impression_time = statement.ColumnTime(2);
    int64_t attribution_source_priority = statement.ColumnInt64(3);

    // Select the row to attribute to the conversion. All other matching rows
    // will be deleted by the attribution logic.
    if (ShouldReplaceImpressionToAttribute(impression_to_attribute,
                                           attribution_source_priority,
                                           impression_time)) {
      if (impression_to_attribute.has_value()) {
        impression_ids_to_delete.push_back(
            *impression_to_attribute->impression_id());
      }

      uint64_t impression_data =
          DeserializeImpressionOrConversionData(statement.ColumnInt64(4));
      url::Origin conversion_origin =
          DeserializeOrigin(statement.ColumnString(5));
      base::Time expiry_time = statement.ColumnTime(6);
      attribution_logic = static_cast<StorableImpression::AttributionLogic>(
          statement.ColumnInt(7));
      StorableImpression::SourceType source_type =
          static_cast<StorableImpression::SourceType>(statement.ColumnInt(8));
      num_conversions = statement.ColumnInt(9);

      // There should never be an unattributed impression with `kFalsely`.
      DCHECK_NE(attribution_logic,
                StorableImpression::AttributionLogic::kFalsely);

      impression_to_attribute = StorableImpression(
          impression_data, std::move(impression_origin),
          std::move(conversion_origin), reporting_origin, impression_time,
          expiry_time, source_type, attribution_source_priority, impression_id);
    } else
      impression_ids_to_delete.push_back(impression_id);
  }

  // Exit early if the last statement wasn't valid or if we have no impressions.
  if (!statement.Succeeded() || !impression_to_attribute.has_value())
    return false;

  const uint64_t conversion_data =
      impression_to_attribute->source_type() ==
              StorableImpression::SourceType::kEvent
          ? conversion.event_source_trigger_data()
          : conversion.conversion_data();

  ConversionReport report(std::move(*impression_to_attribute), conversion_data,
                          /*conversion_time=*/current_time,
                          /*report_time=*/current_time,
                          /*conversion_id=*/absl::nullopt);

  report.report_time = delegate_->GetReportTime(report);

  if (!rate_limit_table_.IsAttributionAllowed(db_.get(), report, current_time))
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  const auto maybe_replace_lower_priority_report_result =
      MaybeReplaceLowerPriorityReport(report.impression, num_conversions,
                                      conversion.priority(),
                                      report.report_time);
  if (maybe_replace_lower_priority_report_result ==
      ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::kError) {
    return false;
  }

  if (maybe_replace_lower_priority_report_result ==
      ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::
          kDropNewReport) {
    transaction.Commit();
    return false;
  }

  // Reports with `AttributionLogic::kNever` should be included in all
  // attribution operations and matching, but only `kTruthfully` should generate
  // reports that get sent.
  const bool create_report =
      attribution_logic == StorableImpression::AttributionLogic::kTruthfully;

  if (create_report) {
    DCHECK(report.impression.impression_id().has_value());
    if (!StoreConversionReport(report, *report.impression.impression_id(),
                               conversion.priority())) {
      return false;
    }
  }

  // Only increment the number of conversions associated with the impression if
  // we are adding a new one, rather than replacing a dropped one.
  if (maybe_replace_lower_priority_report_result ==
      ConversionStorageSql::MaybeReplaceLowerPriorityReportResult::
          kAddNewReport) {
    static constexpr char kUpdateImpressionForConversionSql[] =
        "UPDATE impressions SET num_conversions = num_conversions + 1 "
        "WHERE impression_id = ?";
    sql::Statement impression_update_statement(db_->GetCachedStatement(
        SQL_FROM_HERE, kUpdateImpressionForConversionSql));

    // Update the attributed impression.
    impression_update_statement.BindInt64(0,
                                          *report.impression.impression_id());
    if (!impression_update_statement.Run())
      return false;
  }

  // Delete all unattributed impressions.
  if (!DeleteImpressions(impression_ids_to_delete))
    return false;

  // Based on the deletion logic here and the fact that we delete impressions
  // with |num_conversions > 1| when there is a new matching impression in
  // |StoreImpression()|, we should be guaranteed that these impressions all
  // have |num_conversions == 0|, and that they never contributed to a rate
  // limit. Therefore, we don't need to call
  // |RateLimitTable::ClearDataForImpressionIds()| here.

  if (create_report && !rate_limit_table_.AddRateLimit(db_.get(), report))
    return false;

  if (!transaction.Commit())
    return false;

  return create_report;
}

bool ConversionStorageSql::StoreConversionReport(const ConversionReport& report,
                                                 int64_t impression_id,
                                                 int64_t priority) {
  static constexpr char kStoreConversionSql[] =
      "INSERT INTO conversions"
      "(impression_id,conversion_data,conversion_time,report_time,"
      "priority)VALUES(?,?,?,?,?)";
  sql::Statement store_conversion_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kStoreConversionSql));
  store_conversion_statement.BindInt64(0, impression_id);
  store_conversion_statement.BindInt64(
      1, SerializeImpressionOrConversionData(report.conversion_data));
  store_conversion_statement.BindTime(2, report.conversion_time);
  store_conversion_statement.BindTime(3, report.report_time);
  store_conversion_statement.BindInt64(4, priority);
  return store_conversion_statement.Run();
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
  static constexpr char kGetExpiredConversionsSql[] =
      "SELECT C.conversion_data,C.conversion_time,"
      "C.report_time,"
      "C.conversion_id,I.impression_origin,I.conversion_origin,"
      "I.reporting_origin,I.impression_data,I.impression_time,"
      "I.expiry_time,I.impression_id,I.source_type,I.priority "
      "FROM conversions C JOIN impressions I ON "
      "C.impression_id = I.impression_id WHERE C.report_time <= ? "
      "LIMIT ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetExpiredConversionsSql));
  statement.BindTime(0, max_report_time);
  statement.BindInt(1, limit);

  std::vector<ConversionReport> conversions;
  while (statement.Step()) {
    uint64_t conversion_data =
        DeserializeImpressionOrConversionData(statement.ColumnInt64(0));
    base::Time conversion_time = statement.ColumnTime(1);
    base::Time report_time = statement.ColumnTime(2);
    int64_t conversion_id = statement.ColumnInt64(3);
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(4));
    url::Origin conversion_origin =
        DeserializeOrigin(statement.ColumnString(5));
    url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(6));
    uint64_t impression_data =
        DeserializeImpressionOrConversionData(statement.ColumnInt64(7));
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
    StorableImpression impression(impression_data, std::move(impression_origin),
                                  std::move(conversion_origin),
                                  std::move(reporting_origin), impression_time,
                                  expiry_time, source_type,
                                  attribution_source_priority, impression_id);

    ConversionReport report(std::move(impression), conversion_data,
                            conversion_time, report_time, conversion_id);

    conversions.push_back(std::move(report));
  }

  if (!statement.Succeeded())
    return {};
  return conversions;
}

void ConversionStorageSql::DeleteExpiredImpressions() {
  // Delete all impressions that have no associated conversions and are past
  // their expiry time. Optimized by |kImpressionExpiryIndexSql|.
  static constexpr char kDeleteExpiredImpressionsSql[] =
      "DELETE FROM impressions WHERE expiry_time <= ? AND "
      "impression_id NOT IN(SELECT impression_id FROM conversions)";
  sql::Statement delete_expired_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteExpiredImpressionsSql));
  delete_expired_statement.BindTime(0, clock_->Now());
  if (!delete_expired_statement.Run())
    return;

  // Delete all impressions that have no associated conversions and are
  // inactive. This is done in a separate statement from
  // |kDeleteExpiredImpressionsSql| so that each query is optimized by an index.
  // Optimized by |kConversionUrlIndexSql|.
  static constexpr char kDeleteInactiveImpressionsSql[] =
      "DELETE FROM impressions WHERE active = 0 AND "
      "impression_id NOT IN(SELECT impression_id FROM conversions)";
  sql::Statement delete_inactive_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteInactiveImpressionsSql));
  delete_inactive_statement.Run();
}

bool ConversionStorageSql::DeleteConversion(int64_t conversion_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return false;
  if (!DeleteConversionInternal(conversion_id))
    return false;
  return db_->GetLastChangeCount() > 0;
}

bool ConversionStorageSql::DeleteConversionInternal(int64_t conversion_id) {
  // Delete the row identified by |conversion_id|.
  static constexpr char kDeleteSentConversionSql[] =
      "DELETE FROM conversions WHERE conversion_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSentConversionSql));
  statement.BindInt64(0, conversion_id);
  return statement.Run();
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
  static constexpr char kScanCandidateData[] =
      "SELECT C.conversion_id,I.impression_id,"
      "I.impression_origin,I.conversion_origin,I.reporting_origin "
      "FROM impressions I LEFT JOIN conversions C ON "
      "C.impression_id = I.impression_id WHERE"
      "(I.impression_time BETWEEN ?1 AND ?2)OR"
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

  // TODO(csharrison, johnidel): Should we consider poisoning the DB if some of
  // the delete operations fail?
  if (!statement.Succeeded())
    return;

  // Since multiple conversions can be associated with a single impression,
  // deduplicate impression IDs using a set to avoid redundant DB operations
  // below.
  impression_ids_to_delete =
      base::flat_set<int64_t>(std::move(impression_ids_to_delete)).extract();

  // Delete the data in a transaction to avoid cases where the impression part
  // of a conversion is deleted without deleting the associated conversion, or
  // vice versa.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  if (!DeleteImpressions(impression_ids_to_delete))
    return;

  for (int64_t conversion_id : conversion_ids_to_delete) {
    if (!DeleteConversionInternal(conversion_id))
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
  static constexpr char kDeleteVestigialConversionSql[] =
      "DELETE FROM conversions WHERE impression_id = ?";
  sql::Statement delete_vestigial_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteVestigialConversionSql));
  for (int64_t impression_id : impression_ids_to_delete) {
    delete_vestigial_statement.Reset(/*clear_bound_vars=*/true);
    delete_vestigial_statement.BindInt64(0, impression_id);
    if (!delete_vestigial_statement.Run())
      return;

    num_conversions_deleted += db_->GetLastChangeCount();
  }

  if (!rate_limit_table_.ClearDataForImpressionIds(db_.get(),
                                                   impression_ids_to_delete)) {
    return;
  }

  if (!rate_limit_table_.ClearDataForOriginsInRange(db_.get(), delete_begin,
                                                    delete_end, filter)) {
    return;
  }

  if (!transaction.Commit())
    return;

  RecordImpressionsDeleted(static_cast<int>(impression_ids_to_delete.size()));
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
  static constexpr char kSelectImpressionRangeSql[] =
      "SELECT impression_id FROM impressions WHERE(impression_time BETWEEN ?1 "
      "AND ?2)OR "
      "impression_id IN(SELECT impression_id FROM conversions "
      "WHERE conversion_time BETWEEN ?1 AND ?2)";
  sql::Statement select_impressions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectImpressionRangeSql));
  select_impressions_statement.BindTime(0, delete_begin);
  select_impressions_statement.BindTime(1, delete_end);

  std::vector<int64_t> impression_ids_to_delete;
  while (select_impressions_statement.Step()) {
    int64_t impression_id = select_impressions_statement.ColumnInt64(0);
    impression_ids_to_delete.push_back(impression_id);
  }
  if (!select_impressions_statement.Succeeded())
    return;

  if (!DeleteImpressions(impression_ids_to_delete))
    return;

  static constexpr char kDeleteConversionRangeSql[] =
      "DELETE FROM conversions WHERE(conversion_time BETWEEN ? AND ?)"
      "OR impression_id NOT IN(SELECT impression_id FROM impressions)";
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

  static constexpr char kDeleteAllConversionsSql[] = "DELETE FROM conversions";
  sql::Statement delete_all_conversions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllConversionsSql));
  if (!delete_all_conversions_statement.Run())
    return;
  int num_conversions_deleted = db_->GetLastChangeCount();

  static constexpr char kDeleteAllImpressionsSql[] = "DELETE FROM impressions";
  sql::Statement delete_all_impressions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllImpressionsSql));
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
  static constexpr char kCountImpressionsSql[] =
      "SELECT COUNT(impression_origin)FROM impressions WHERE "
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
  static constexpr char kCountConversionsSql[] =
      "SELECT COUNT(conversion_id)FROM conversions C JOIN impressions I ON"
      " I.impression_id = C.impression_id"
      " WHERE I.conversion_destination = ? AND(active BETWEEN 0 AND 1)";
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
  static constexpr char kGetActiveImpressionsSql[] =
      "SELECT impression_data,impression_origin,conversion_origin,"
      "reporting_origin,impression_time,expiry_time,impression_id,"
      "source_type,priority "
      "FROM impressions "
      "WHERE active = 1 and expiry_time > ? "
      "LIMIT ?";

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetActiveImpressionsSql));
  statement.BindTime(0, clock_->Now());
  statement.BindInt(1, limit);

  std::vector<StorableImpression> impressions;
  while (statement.Step()) {
    uint64_t impression_data =
        DeserializeImpressionOrConversionData(statement.ColumnInt64(0));
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(1));
    url::Origin conversion_origin =
        DeserializeOrigin(statement.ColumnString(2));
    url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(3));
    base::Time impression_time = statement.ColumnTime(4);
    base::Time expiry_time = statement.ColumnTime(5);
    int64_t impression_id = statement.ColumnInt64(6);
    StorableImpression::SourceType source_type =
        static_cast<StorableImpression::SourceType>(statement.ColumnInt(7));
    int64_t attribution_source_priority = statement.ColumnInt64(8);

    StorableImpression impression(impression_data, std::move(impression_origin),
                                  std::move(conversion_origin),
                                  std::move(reporting_origin), impression_time,
                                  expiry_time, source_type,
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

  return UpgradeConversionStorageSqlSchema(db_.get(), &meta_table_);
}

bool ConversionStorageSql::CreateSchema() {
  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();
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
  // |attributed_truthfully| corresponds to the
  // |StorableImpression::AttributionLogic| enum.
  // |impression_site| is used to optimize the lookup of impressions;
  // |StorableImpression::ImpressionSite| is always derived from the origin.
  static constexpr char kImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS impressions"
      "(impression_id INTEGER PRIMARY KEY,"
      "impression_data INTEGER NOT NULL,"
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
      "priority INTEGER NOT NULL,"
      "impression_site TEXT NOT NULL)";
  if (!db_->Execute(kImpressionTableSql))
    return false;

  // Optimizes impression lookup by conversion destination/reporting origin
  // during calls to `MaybeCreateAndStoreConversionReport()`,
  // `StoreImpression()`, `DeleteExpiredImpressions()`. Impressions and
  // conversions are considered matching if they share this pair. These calls
  // only look at active conversions, so include |active| in the index.
  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active,conversion_destination,reporting_origin)";
  if (!db_->Execute(kConversionDestinationIndexSql))
    return false;

  // Optimizes calls to `DeleteExpiredImpressions()` and
  // `MaybeCreateAndStoreConversionReport()` by indexing impressions by expiry
  // time. Both calls require only returning impressions that expire after a
  // given time.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db_->Execute(kImpressionExpiryIndexSql))
    return false;

  // Optimizes counting impressions by impression origin.
  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db_->Execute(kImpressionOriginIndexSql))
    return false;

  // Optimizes `EnsureCapacityForPendingDestinationLimit()`.
  static constexpr char kImpressionSiteIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_site_idx "
      "ON impressions(active,impression_site,source_type)";
  if (!db_->Execute(kImpressionSiteIndexSql))
    return false;

  // All columns in this table are const. |impression_id| is the primary key of
  // a row in the [impressions] table, [impressions.impression_id].
  // |conversion_time| is the time at which the conversion was registered, and
  // should be used for clearing site data. |report_time| is the time a
  // <conversion, impression> pair should be reported, and is specified by
  // |delegate_|.
  static constexpr char kConversionTableSql[] =
      "CREATE TABLE IF NOT EXISTS conversions"
      "(conversion_id INTEGER PRIMARY KEY,"
      "impression_id INTEGER NOT NULL,"
      "conversion_data INTEGER NOT NULL,"
      "conversion_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "priority INTEGER NOT NULL)";
  if (!db_->Execute(kConversionTableSql))
    return false;

  // Optimize sorting conversions by report time for calls to
  // GetConversionsToReport(). The reports with the earliest report times are
  // periodically fetched from storage to be sent.
  static constexpr char kConversionReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_report_idx "
      "ON conversions(report_time)";
  if (!db_->Execute(kConversionReportTimeIndexSql))
    return false;

  // Want to optimize conversion look up by click id. This allows us to
  // quickly know if an expired impression can be deleted safely if it has no
  // corresponding pending conversions during calls to
  // DeleteExpiredImpressions().
  static constexpr char kConversionClickIdIndexSql[] =
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

bool ConversionStorageSql::EnsureCapacityForPendingDestinationLimit(
    const StorableImpression& impression) {
  // TODO(apaseltiner): Add metrics for how this behaves so we can see how often
  // sites are hitting the limit.

  if (impression.source_type() != StorableImpression::SourceType::kEvent)
    return true;

  const std::string serialized_conversion_destination =
      impression.ConversionDestination().Serialize();

  static constexpr char kSelectImpressionsSql[] =
      "SELECT impression_id,conversion_destination "
      "FROM impressions "
      "WHERE impression_site = ? AND source_type = ? "
      "AND active = 1 AND num_conversions = 0 "
      "ORDER BY impression_time ASC";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectImpressionsSql));
  statement.BindString(0, impression.ImpressionSite().Serialize());
  statement.BindInt(1,
                    static_cast<int>(StorableImpression::SourceType::kEvent));

  base::flat_map<std::string, size_t> conversion_destinations;

  struct ImpressionData {
    int64_t impression_id;
    std::string conversion_destination;
  };
  std::vector<ImpressionData> impressions_by_impression_time;

  while (statement.Step()) {
    ImpressionData impression_data = {
        .impression_id = statement.ColumnInt64(0),
        .conversion_destination = statement.ColumnString(1),
    };

    // If there's already an impression matching the to-be-stored
    // `impression_site` and `conversion_destination`, then the unique count
    // won't be changed, so there's nothing else to do.
    if (impression_data.conversion_destination ==
        serialized_conversion_destination) {
      return true;
    }

    conversion_destinations[impression_data.conversion_destination]++;
    impressions_by_impression_time.push_back(std::move(impression_data));
  }

  if (!statement.Succeeded())
    return false;

  const int max = delegate_->GetMaxAttributionDestinationsPerEventSource();
  // TODO(apaseltiner): We could just make
  // `GetMaxAttributionDestinationsPerEventSource()` return `size_t`, but it
  // would be inconsistent with the other `ConversionStorage::Delegate` methods.
  DCHECK_GT(max, 0);

  // Otherwise, if there's capacity for the new `conversion_destination` to be
  // stored for the `impression_site`, there's nothing else to do.
  if (conversion_destinations.size() < static_cast<size_t>(max))
    return true;

  // Otherwise, delete impressions in order by `impression_time` until the
  // number of distinct `conversion_destination`s is under `max`.
  std::vector<int64_t> impression_ids_to_delete;
  for (const auto& impression_data : impressions_by_impression_time) {
    auto it =
        conversion_destinations.find(impression_data.conversion_destination);
    DCHECK(it != conversion_destinations.end());
    it->second--;
    if (it->second == 0)
      conversion_destinations.erase(it);
    impression_ids_to_delete.push_back(impression_data.impression_id);
    if (conversion_destinations.size() < static_cast<size_t>(max))
      break;
  }

  return DeleteImpressions(impression_ids_to_delete);

  // Because this is limited to active impressions with `num_conversions = 0`,
  // we should be guaranteed that there is not any corresponding data in the
  // rate limit table or the report table.
}

bool ConversionStorageSql::DeleteImpressions(
    const std::vector<int64_t>& impression_ids) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteImpressionSql[] =
      "DELETE FROM impressions WHERE impression_id = ?";
  sql::Statement delete_impression_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteImpressionSql));

  for (int64_t impression_id : impression_ids) {
    delete_impression_statement.Reset(/*clear_bound_vars=*/true);
    delete_impression_statement.BindInt64(0, impression_id);
    if (!delete_impression_statement.Run())
      return false;
  }

  return transaction.Commit();
}

}  // namespace content
