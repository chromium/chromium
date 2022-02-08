// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/rate_limit_table.h"

#include "base/check.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace content {

namespace {

using AttributionAllowedStatus =
    ::content::RateLimitTable::AttributionAllowedStatus;

}  // namespace

RateLimitTable::RateLimitTable(const AttributionStorageDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RateLimitTable::~RateLimitTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool RateLimitTable::CreateTable(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // All columns in this table are const.
  // |impression_id| is the primary key of a row in the |impressions| table,
  // though the row may not exist.
  // |impression_site| is the eTLD+1 of the impression.
  // |impression_origin| is the origin of the impression.
  // |conversion_destination| is the destination of the conversion.
  // |conversion_origin| is the origin of the conversion.
  // |reporting_origin| is the reporting origin of the impression/conversion.
  // |conversion_time| is the report's conversion time.
  static constexpr char kRateLimitTableSql[] =
      "CREATE TABLE IF NOT EXISTS rate_limits"
      "(rate_limit_id INTEGER PRIMARY KEY NOT NULL,"
      "impression_id INTEGER NOT NULL,"
      "impression_site TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_destination TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "conversion_time INTEGER NOT NULL)";
  if (!db->Execute(kRateLimitTableSql))
    return false;

  // Optimizes calls to |AttributionAllowed()|.
  static constexpr char kRateLimitReportScopeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_report_scope_idx "
      "ON rate_limits(conversion_destination,impression_site,reporting_origin,"
      "conversion_time)";
  if (!db->Execute(kRateLimitReportScopeIndexSql))
    return false;

  // Optimizes calls to |DeleteExpiredRateLimits()|, |ClearAllDataInRange()|,
  // |ClearDataForOriginsInRange()|.
  static constexpr char kRateLimitAttributionTypeConversionTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS "
      "rate_limit_conversion_time_idx "
      "ON rate_limits(conversion_time)";
  if (!db->Execute(kRateLimitAttributionTypeConversionTimeIndexSql))
    return false;

  // Optimizes calls to |ClearDataForSourceIds()|.
  static constexpr char kRateLimitImpressionIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_id_idx "
      "ON rate_limits(impression_id)";
  return db->Execute(kRateLimitImpressionIndexSql);
}

bool RateLimitTable::AddRateLimit(sql::Database* db,
                                  const AttributionReport& report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const CommonSourceInfo& common_info = report.source().common_info();

  // Only delete expired rate limits periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredRateLimitsFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = base::Time::Now();
  if (now - last_cleared_ >= delete_frequency) {
    if (!DeleteExpiredRateLimits(db))
      return false;
    last_cleared_ = now;
  }

  static constexpr char kStoreRateLimitSql[] =
      "INSERT INTO rate_limits"
      "(impression_id,impression_site,impression_origin,conversion_destination,"
      "conversion_origin,reporting_origin,conversion_time)"
      "VALUES(?,?,?,?,?,?,?)";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kStoreRateLimitSql));
  statement.BindInt64(0, *report.source().source_id());
  statement.BindString(1, common_info.ImpressionSite().Serialize());
  statement.BindString(2, SerializeOrigin(common_info.impression_origin()));
  statement.BindString(3, common_info.ConversionDestination().Serialize());
  statement.BindString(4, SerializeOrigin(common_info.conversion_origin()));
  statement.BindString(5, SerializeOrigin(common_info.reporting_origin()));
  statement.BindTime(6, report.trigger_time());
  return statement.Run();
}

AttributionAllowedStatus RateLimitTable::AttributionAllowed(
    sql::Database* db,
    const AttributionReport& report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const CommonSourceInfo& common_info = report.source().common_info();

  const AttributionStorageDelegate::RateLimitConfig rate_limits =
      delegate_->GetRateLimits();
  DCHECK_GT(rate_limits.time_window, base::TimeDelta());
  DCHECK_GT(rate_limits.max_attributions_per_window, 0);

  base::Time min_timestamp = report.trigger_time() - rate_limits.time_window;

  static constexpr char kAttributionAllowedSql[] =
      "SELECT COUNT(*) FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_report_scope_idx")
      "WHERE impression_site = ? "
      "AND conversion_destination = ? "
      "AND reporting_origin = ? "
      "AND conversion_time > ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kAttributionAllowedSql));
  statement.BindString(0, common_info.ImpressionSite().Serialize());
  statement.BindString(1, common_info.ConversionDestination().Serialize());
  statement.BindString(2, SerializeOrigin(common_info.reporting_origin()));
  statement.BindTime(3, min_timestamp);

  if (!statement.Step())
    return AttributionAllowedStatus::kError;

  int64_t count = statement.ColumnInt64(0);

  return count < rate_limits.max_attributions_per_window
             ? AttributionAllowedStatus::kAllowed
             : AttributionAllowedStatus::kNotAllowed;
}

bool RateLimitTable::ClearAllDataInRange(sql::Database* db,
                                         base::Time delete_begin,
                                         base::Time delete_end) {
  DCHECK(!((delete_begin.is_null() || delete_begin.is_min()) &&
           delete_end.is_max()));

  static constexpr char kDeleteRateLimitRangeSql[] =
      "DELETE FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_conversion_time_idx")
      "WHERE conversion_time BETWEEN ? AND ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteRateLimitRangeSql));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);
  return statement.Run();
}

bool RateLimitTable::ClearAllDataAllTime(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kDeleteAllRateLimitsSql[] = "DELETE FROM rate_limits";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteAllRateLimitsSql));
  return statement.Run();
}

bool RateLimitTable::ClearDataForOriginsInRange(
    sql::Database* db,
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (filter.is_null())
    return ClearAllDataInRange(db, delete_begin, delete_end);

  static constexpr char kDeleteSql[] =
      "DELETE FROM rate_limits WHERE rate_limit_id=?";
  sql::Statement delete_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kSelectSql[] =
      "SELECT rate_limit_id,impression_origin,conversion_origin,"
      "reporting_origin "
      "FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_conversion_time_idx")
      "WHERE conversion_time BETWEEN ? AND ?";
  sql::Statement select_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  select_statement.BindTime(0, delete_begin);
  select_statement.BindTime(1, delete_end);

  while (select_statement.Step()) {
    int64_t rate_limit_id = select_statement.ColumnInt64(0);
    if (filter.Run(DeserializeOrigin(select_statement.ColumnString(1))) ||
        filter.Run(DeserializeOrigin(select_statement.ColumnString(2))) ||
        filter.Run(DeserializeOrigin(select_statement.ColumnString(3)))) {
      // See https://www.sqlite.org/isolation.html for why it's OK for this
      // DELETE to be interleaved in the surrounding SELECT.
      delete_statement.Reset(/*clear_bound_vars=*/false);
      delete_statement.BindInt64(0, rate_limit_id);
      if (!delete_statement.Run())
        return false;
    }
  }

    if (!select_statement.Succeeded())
      return false;

  return transaction.Commit();
}

bool RateLimitTable::DeleteExpiredRateLimits(sql::Database* db) {
  base::Time timestamp =
      base::Time::Now() - delegate_->GetRateLimits().time_window;

  static constexpr char kDeleteExpiredRateLimits[] =
      // clang-format off
      "DELETE FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_conversion_time_idx")
      "WHERE conversion_time <= ?";  // clang-format on
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteExpiredRateLimits));
  statement.BindTime(0, timestamp);
  return statement.Run();
}

bool RateLimitTable::ClearDataForSourceIds(
    sql::Database* db,
    const std::vector<StoredSource::Id>& source_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteRateLimitSql[] =
      "DELETE FROM rate_limits WHERE impression_id = ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteRateLimitSql));

  for (StoredSource::Id id : source_ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);
    if (!statement.Run())
      return false;
  }

  return transaction.Commit();
}

}  // namespace content
