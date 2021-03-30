// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/rate_limit_table.h"

#include "base/check.h"
#include "base/time/clock.h"
#include "content/browser/conversions/sql_utils.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

RateLimitTable::RateLimitTable(const ConversionStorage::Delegate* delegate,
                               const base::Clock* clock)
    : delegate_(delegate), clock_(clock) {
  DCHECK(delegate_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RateLimitTable::~RateLimitTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool RateLimitTable::CreateTable(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // All columns in this table are const.
  // |attribution_type| is the type of the report, currently always kNavigation.
  // |impression_id| is the primary key of a row in the |impressions| table,
  // though the row may not exist.
  // |impression_site| is the eTLD+1 of the impression.
  // |impression_origin| is the origin of the impression.
  // |conversion_destination| is the destination of the conversion.
  // |conversion_origin| is the origin of the conversion.
  // |conversion_time| is the report's conversion time.
  const char kRateLimitTableSql[] =
      "CREATE TABLE IF NOT EXISTS rate_limits"
      "(rate_limit_id INTEGER PRIMARY KEY,"
      "attribution_type INTEGER NOT NULL,"
      "impression_id INTEGER NOT NULL,"
      "impression_site TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_destination TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "conversion_time INTEGER NOT NULL)";
  if (!db->Execute(kRateLimitTableSql))
    return false;

  // Optimizes calls to |IsAttributionAllowed()|.
  const char kRateLimitImpressionSiteTypeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_site_type_idx "
      "ON rate_limits(attribution_type, conversion_destination, "
      "impression_site, conversion_time)";
  if (!db->Execute(kRateLimitImpressionSiteTypeIndexSql))
    return false;

  // Optimizes calls to |DeleteExpiredRateLimits()| and |ClearAllDataInRange()|.
  const char kRateLimitConversionTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_conversion_time_idx "
      "ON rate_limits(conversion_time)";
  if (!db->Execute(kRateLimitConversionTimeIndexSql))
    return false;

  // Optimizes calls to |ClearDataForImpressionIds()|.
  const char kRateLimitImpressionIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_id_idx "
      "ON rate_limits(impression_id)";
  return db->Execute(kRateLimitImpressionIndexSql);
}

bool RateLimitTable::AddRateLimit(sql::Database* db,
                                  const ConversionReport& report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(report.impression.impression_id().has_value());

  // TODO(apaseltiner): Call this at most every X minutes.
  DeleteExpiredRateLimits(db);

  const char kStoreRateLimitSql[] =
      "INSERT INTO rate_limits "
      "(attribution_type, impression_id, impression_site, impression_origin, "
      "conversion_destination, conversion_origin, conversion_time) "
      "VALUES(?,?,?,?,?,?,?)";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kStoreRateLimitSql));
  statement.BindInt(0, static_cast<int>(AttributionType::kNavigation));
  statement.BindInt64(1, *report.impression.impression_id());
  statement.BindString(
      2, net::SchemefulSite(report.impression.impression_origin()).Serialize());
  statement.BindString(3, report.impression.impression_origin().Serialize());
  statement.BindString(4,
                       report.impression.ConversionDestination().Serialize());
  statement.BindString(5, report.impression.conversion_origin().Serialize());
  statement.BindInt64(6, SerializeTime(report.conversion_time));
  return statement.Run();
}

bool RateLimitTable::IsAttributionAllowed(sql::Database* db,
                                          const ConversionReport& report,
                                          base::Time now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ConversionStorage::Delegate::RateLimitConfig rate_limits =
      delegate_->GetRateLimits();
  base::Time min_timestamp = now - rate_limits.time_window;

  const char kAttributionAllowedSql[] =
      "SELECT COUNT(*) FROM rate_limits "
      "WHERE attribution_type = ? "
      "AND impression_site = ? "
      "AND conversion_destination = ? "
      "AND conversion_time > ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kAttributionAllowedSql));
  statement.BindInt(0, static_cast<int>(AttributionType::kNavigation));
  statement.BindString(
      1, net::SchemefulSite(report.impression.impression_origin()).Serialize());
  statement.BindString(2,
                       report.impression.ConversionDestination().Serialize());
  statement.BindInt64(3, SerializeTime(min_timestamp));
  if (!statement.Step())
    return false;

  int64_t count = statement.ColumnInt64(0);
  return count < rate_limits.max_attributions_per_window;
}

bool RateLimitTable::ClearAllDataInRange(sql::Database* db,
                                         base::Time delete_begin,
                                         base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!((delete_begin.is_null() || delete_begin.is_min()) &&
           delete_end.is_max()));

  const char kDeleteRateLimitRangeSql[] =
      "DELETE FROM rate_limits WHERE conversion_time BETWEEN ? AND "
      "?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteRateLimitRangeSql));
  statement.BindInt64(0, SerializeTime(delete_begin));
  statement.BindInt64(1, SerializeTime(delete_end));
  return statement.Run();
}

bool RateLimitTable::ClearAllDataAllTime(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const char kDeleteAllRateLimitsSql[] = "DELETE FROM rate_limits";
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
  DCHECK(!filter.is_null());

  std::vector<int64_t> rate_limit_ids_to_delete;
  {
    const char kScanCandidateData[] =
        "SELECT rate_limit_id, impression_site, impression_origin, "
        "conversion_destination, conversion_origin FROM rate_limits "
        "WHERE conversion_time BETWEEN ? AND ?";
    sql::Statement statement(
        db->GetCachedStatement(SQL_FROM_HERE, kScanCandidateData));
    statement.BindInt64(0, SerializeTime(delete_begin));
    statement.BindInt64(1, SerializeTime(delete_end));

    while (statement.Step()) {
      int64_t rate_limit_id = statement.ColumnInt64(0);
      if (filter.Run(DeserializeOrigin(statement.ColumnString(1))) ||
          filter.Run(DeserializeOrigin(statement.ColumnString(2))) ||
          filter.Run(DeserializeOrigin(statement.ColumnString(3))) ||
          filter.Run(DeserializeOrigin(statement.ColumnString(4)))) {
        rate_limit_ids_to_delete.push_back(rate_limit_id);
      }
    }

    if (!statement.Succeeded())
      return false;
  }

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  for (int64_t rate_limit_id : rate_limit_ids_to_delete) {
    const char kDeleteRateLimitSql[] =
        "DELETE FROM rate_limits WHERE rate_limit_id = ?";
    sql::Statement statement(
        db->GetCachedStatement(SQL_FROM_HERE, kDeleteRateLimitSql));
    statement.BindInt64(0, rate_limit_id);
    if (!statement.Run())
      return false;
  }

  return transaction.Commit();
}

int RateLimitTable::DeleteExpiredRateLimits(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time timestamp = clock_->Now() - delegate_->GetRateLimits().time_window;

  const char kDeleteExpiredRateLimits[] =
      "DELETE FROM rate_limits WHERE conversion_time <= ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteExpiredRateLimits));
  statement.BindInt64(0, SerializeTime(timestamp));
  if (!statement.Run())
    return 0;
  return db->GetLastChangeCount();
}

bool RateLimitTable::ClearDataForImpressionIds(
    sql::Database* db,
    const base::flat_set<int64_t>& impression_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  for (int64_t id : impression_ids) {
    const char kDeleteRateLimitSql[] =
        "DELETE FROM rate_limits WHERE impression_id = ?";
    sql::Statement statement(
        db->GetCachedStatement(SQL_FROM_HERE, kDeleteRateLimitSql));
    statement.BindInt64(0, id);
    if (!statement.Run())
      return false;
  }

  return transaction.Commit();
}

}  // namespace content
