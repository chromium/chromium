// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/rate_limit_table.h"

#include <string>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace content {

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
  // |source_id| is the primary key of a row in the |impressions| table,
  // though the row may not exist.
  // |scope| is a serialized `RateLimitTable::Scope`.
  // |source_site| is the eTLD+1 of the impression.
  // |source_origin| is the origin of the impression.
  // |destination_site| is the destination of the conversion.
  // |destination_origin| is the origin of the conversion.
  // |reporting_origin| is the reporting origin of the impression/conversion.
  // |time| is the time of either the source registration or the attribution
  // trigger, depending on |scope|.
  static constexpr char kRateLimitTableSql[] =
      "CREATE TABLE IF NOT EXISTS rate_limits"
      "(id INTEGER PRIMARY KEY NOT NULL,"
      "scope INTEGER NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "source_site TEXT NOT NULL,"
      "source_origin TEXT NOT NULL,"
      "destination_site TEXT NOT NULL,"
      "destination_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "time INTEGER NOT NULL)";
  if (!db->Execute(kRateLimitTableSql))
    return false;

  static_assert(static_cast<int>(Scope::kAttribution) == 1,
                "update `scope=1` clause below");

  // Optimizes calls to |AttributionAllowedForAttributionLimit()|.
  static constexpr char kRateLimitAttributionIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_attribution_idx ON rate_limits"
      "(destination_site,source_site,reporting_origin,time)"
      "WHERE scope=1";
  if (!db->Execute(kRateLimitAttributionIndexSql))
    return false;

  // Optimizes calls to |AllowedForReportingOriginLimit()|.
  static constexpr char kRateLimitReportingOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_reporting_origin_idx "
      "ON rate_limits(scope,destination_site,source_site,time)";
  if (!db->Execute(kRateLimitReportingOriginIndexSql))
    return false;

  // Optimizes calls to |DeleteExpiredRateLimits()|, |ClearAllDataInRange()|,
  // |ClearDataForOriginsInRange()|.
  static constexpr char kRateLimitTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_time_idx ON rate_limits(time)";
  if (!db->Execute(kRateLimitTimeIndexSql))
    return false;

  // Optimizes calls to |ClearDataForSourceIds()|.
  static constexpr char kRateLimitImpressionIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_source_id_idx "
      "ON rate_limits(source_id)";
  return db->Execute(kRateLimitImpressionIdIndexSql);
}

bool RateLimitTable::AddRateLimitForSource(sql::Database* db,
                                           const StoredSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddRateLimit(db, Scope::kSource, source,
                      source.common_info().impression_time());
}

bool RateLimitTable::AddRateLimitForAttribution(
    sql::Database* db,
    const AttributionInfo& attribution_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddRateLimit(db, Scope::kAttribution, attribution_info.source,
                      attribution_info.time);
}

bool RateLimitTable::AddRateLimit(sql::Database* db,
                                  Scope scope,
                                  const StoredSource& source,
                                  base::Time time) {
  const CommonSourceInfo& common_info = source.common_info();

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
      "(scope,source_id,source_site,source_origin,"
      "destination_site,destination_origin,reporting_origin,time)"
      "VALUES(?,?,?,?,?,?,?,?)";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kStoreRateLimitSql));
  statement.BindInt(0, static_cast<int>(scope));
  statement.BindInt64(1, *source.source_id());
  statement.BindString(2, common_info.ImpressionSite().Serialize());
  statement.BindString(3, SerializeOrigin(common_info.impression_origin()));
  statement.BindString(4, common_info.ConversionDestination().Serialize());
  statement.BindString(5, SerializeOrigin(common_info.conversion_origin()));
  statement.BindString(6, SerializeOrigin(common_info.reporting_origin()));
  statement.BindTime(7, time);
  return statement.Run();
}

RateLimitResult RateLimitTable::AttributionAllowedForAttributionLimit(
    sql::Database* db,
    const AttributionInfo& attribution_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const CommonSourceInfo& common_info = attribution_info.source.common_info();

  const AttributionStorageDelegate::RateLimitConfig rate_limits =
      delegate_->GetRateLimits();
  DCHECK_GT(rate_limits.time_window, base::TimeDelta());
  DCHECK_GT(rate_limits.max_attributions, 0);

  base::Time min_timestamp = attribution_info.time - rate_limits.time_window;

  static_assert(static_cast<int>(Scope::kAttribution) == 1,
                "update `scope=1` clause below");

  static constexpr char kAttributionAllowedSql[] =
      "SELECT COUNT(*)FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_attribution_idx")
      "WHERE scope=1 "
      "AND source_site=? "
      "AND destination_site=? "
      "AND reporting_origin=? "
      "AND time>?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kAttributionAllowedSql));
  statement.BindString(0, common_info.ImpressionSite().Serialize());
  statement.BindString(1, common_info.ConversionDestination().Serialize());
  statement.BindString(2, SerializeOrigin(common_info.reporting_origin()));
  statement.BindTime(3, min_timestamp);

  if (!statement.Step())
    return RateLimitResult::kError;

  int64_t count = statement.ColumnInt64(0);

  return count < rate_limits.max_attributions ? RateLimitResult::kAllowed
                                              : RateLimitResult::kNotAllowed;
}

RateLimitResult RateLimitTable::SourceAllowedForReportingOriginLimit(
    sql::Database* db,
    const StorableSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AllowedForReportingOriginLimit(db, Scope::kSource,
                                        source.common_info(),
                                        source.common_info().impression_time());
}

RateLimitResult RateLimitTable::AttributionAllowedForReportingOriginLimit(
    sql::Database* db,
    const AttributionInfo& attribution_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AllowedForReportingOriginLimit(db, Scope::kAttribution,
                                        attribution_info.source.common_info(),
                                        attribution_info.time);
}

RateLimitResult RateLimitTable::AllowedForReportingOriginLimit(
    sql::Database* db,
    Scope scope,
    const CommonSourceInfo& common_info,
    base::Time time) {
  const AttributionStorageDelegate::RateLimitConfig rate_limits =
      delegate_->GetRateLimits();
  DCHECK_GT(rate_limits.time_window, base::TimeDelta());

  int64_t max;
  switch (scope) {
    case Scope::kSource:
      max = rate_limits.max_source_registration_reporting_origins;
      break;
    case Scope::kAttribution:
      max = rate_limits.max_attribution_reporting_origins;
      break;
  }
  DCHECK_GT(max, 0);

  const std::string serialized_reporting_origin =
      SerializeOrigin(common_info.reporting_origin());

  base::Time min_timestamp = time - rate_limits.time_window;

  static constexpr char kSelectSql[] =
      "SELECT reporting_origin FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_reporting_origin_idx")
      "WHERE scope=? "
      "AND source_site=? "
      "AND destination_site=? "
      "AND time>?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindInt(0, static_cast<int>(scope));
  statement.BindString(1, common_info.ImpressionSite().Serialize());
  statement.BindString(2, common_info.ConversionDestination().Serialize());
  statement.BindTime(3, min_timestamp);

  base::flat_set<std::string> reporting_origins;
  while (statement.Step()) {
    std::string reporting_origin = statement.ColumnString(0);

    // The origin isn't new, so it doesn't change the count.
    if (reporting_origin == serialized_reporting_origin)
      return RateLimitResult::kAllowed;

    reporting_origins.insert(std::move(reporting_origin));

    if (reporting_origins.size() == static_cast<size_t>(max))
      return RateLimitResult::kNotAllowed;
  }

  return statement.Succeeded() ? RateLimitResult::kAllowed
                               : RateLimitResult::kError;
}

bool RateLimitTable::ClearAllDataInRange(sql::Database* db,
                                         base::Time delete_begin,
                                         base::Time delete_end) {
  DCHECK(!((delete_begin.is_null() || delete_begin.is_min()) &&
           delete_end.is_max()));

  static constexpr char kDeleteRateLimitRangeSql[] =
      // clang-format off
      "DELETE FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_time_idx")
      "WHERE time BETWEEN ? AND ?";  // clang-format on
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

  static constexpr char kDeleteSql[] = "DELETE FROM rate_limits WHERE id=?";
  sql::Statement delete_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT id,source_origin,destination_origin,"
      "reporting_origin "
      "FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_time_idx")
      "WHERE time BETWEEN ? AND ?";  // clang-format on
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
      DCHECK_SQL_INDEXED_BY("rate_limit_time_idx")
      "WHERE time <= ?";  // clang-format on
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
      "DELETE FROM rate_limits WHERE source_id = ?";
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
