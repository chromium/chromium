// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/rate_limit_table.h"

#include "base/check.h"
#include "base/time/clock.h"
#include "content/browser/attribution_reporting/attribution_report.h"
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
using AttributionType = ::content::AttributionStorage::AttributionType;

constexpr AttributionType kAttributionTypes[] = {
    AttributionType::kNavigation,
    AttributionType::kEvent,
    AttributionType::kAggregate,
};

WARN_UNUSED_RESULT AttributionType
AttributionTypeFromSourceType(StorableSource::SourceType source_type) {
  switch (source_type) {
    case StorableSource::SourceType::kNavigation:
      return AttributionType::kNavigation;
    case StorableSource::SourceType::kEvent:
      return AttributionType::kEvent;
  }
}

WARN_UNUSED_RESULT int SerializeAttributionType(
    AttributionType attribution_type) {
  return static_cast<int>(attribution_type);
}

}  // namespace

RateLimitTable::RateLimitTable(const AttributionStorage::Delegate* delegate,
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
  // |attribution_type| corresponds to the `AttributionType` enum.
  // |impression_id| is the primary key of a row in the |impressions| table,
  // though the row may not exist.
  // |impression_site| is the eTLD+1 of the impression.
  // |impression_origin| is the origin of the impression.
  // |conversion_destination| is the destination of the conversion.
  // |conversion_origin| is the origin of the conversion.
  // |conversion_time| is the report's conversion time.
  // |bucket| is the bucket for aggregate histogram contributions. It is unused
  // for now, but we want the table to be extensible to a number of different
  // types of limits. These include limiting total contributions across all
  // buckets, as well as limiting the number of contributions to any one bucket.
  // |value| is the magnitude of this contribution when compared against rate
  // limits. For event-level rows, this is 1. For aggregate contributions, this
  // is the value of the individual contribution to a bucket.
  static constexpr char kRateLimitTableSql[] =
      "CREATE TABLE IF NOT EXISTS rate_limits"
      "(rate_limit_id INTEGER PRIMARY KEY NOT NULL,"
      "attribution_type INTEGER NOT NULL,"
      "impression_id INTEGER NOT NULL,"
      "impression_site TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_destination TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "conversion_time INTEGER NOT NULL,"
      "bucket TEXT NOT NULL,"
      "value INTEGER NOT NULL)";
  if (!db->Execute(kRateLimitTableSql))
    return false;

  // Optimizes calls to |AttributionAllowed()|.
  static constexpr char kRateLimitImpressionSiteTypeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_site_type_idx "
      "ON rate_limits(attribution_type,conversion_destination,"
      "impression_site,conversion_time)";
  if (!db->Execute(kRateLimitImpressionSiteTypeIndexSql))
    return false;

  // Optimizes calls to |DeleteExpiredRateLimits()|, |ClearAllDataInRange()|,
  // |ClearDataForOriginsInRange()|.
  static constexpr char kRateLimitAttributionTypeConversionTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS "
      "rate_limit_attribution_type_conversion_time_idx "
      "ON rate_limits(attribution_type,conversion_time)";
  if (!db->Execute(kRateLimitAttributionTypeConversionTimeIndexSql))
    return false;

  // Optimizes calls to |ClearDataForImpressionIds()|.
  static constexpr char kRateLimitImpressionIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_id_idx "
      "ON rate_limits(impression_id)";
  return db->Execute(kRateLimitImpressionIndexSql);
}

bool RateLimitTable::AddRateLimit(sql::Database* db,
                                  const AttributionReport& report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(report.impression.impression_id().has_value());

  return AddRow(db,
                AttributionTypeFromSourceType(report.impression.source_type()),
                *report.impression.impression_id(),
                report.impression.ImpressionSite().Serialize(),
                SerializeOrigin(report.impression.impression_origin()),
                report.impression.ConversionDestination().Serialize(),
                SerializeOrigin(report.impression.conversion_origin()),
                report.conversion_time,
                // Rate limits for the event-level API do not have a bucket.
                /*bucket=*/"",
                // By supplying 1 here, rate limits for the event-level API act
                // as a count.
                /*value=*/1u);
}

bool RateLimitTable::AddRow(
    sql::Database* db,
    AttributionType attribution_type,
    StorableSource::Id impression_id,
    const std::string& serialized_impression_site,
    const std::string& serialized_impression_origin,
    const std::string& serialized_conversion_destination,
    const std::string& serialized_conversion_origin,
    base::Time time,
    const std::string& bucket,
    uint32_t value) {
  // Only delete expired rate limits periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredRateLimitsFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = clock_->Now();
  if (now - last_cleared_ >= delete_frequency) {
    if (!DeleteExpiredRateLimits(db, attribution_type))
      return false;
    last_cleared_ = now;
  }

  static constexpr char kStoreRateLimitSql[] =
      "INSERT INTO rate_limits"
      "(attribution_type,impression_id,impression_site,impression_origin,"
      "conversion_destination,conversion_origin,conversion_time,bucket,value)"
      "VALUES(?,?,?,?,?,?,?,?,?)";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kStoreRateLimitSql));
  statement.BindInt(0, SerializeAttributionType(attribution_type));
  statement.BindInt64(1, *impression_id);
  statement.BindString(2, serialized_impression_site);
  statement.BindString(3, serialized_impression_origin);
  statement.BindString(4, serialized_conversion_destination);
  statement.BindString(5, serialized_conversion_origin);
  statement.BindTime(6, time);
  statement.BindString(7, bucket);
  statement.BindInt64(8, static_cast<int64_t>(value));
  return statement.Run();
}

AttributionAllowedStatus RateLimitTable::AttributionAllowed(
    sql::Database* db,
    const AttributionReport& report,
    base::Time now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string serialized_impression_site =
      report.impression.ImpressionSite().Serialize();
  const std::string serialized_conversion_destination =
      report.impression.ConversionDestination().Serialize();

  const int64_t capacity = GetCapacity(
      db, AttributionTypeFromSourceType(report.impression.source_type()),
      serialized_impression_site, serialized_conversion_destination, now);
  // This should only be possible if there is DB corruption.
  if (capacity < 0)
    return AttributionAllowedStatus::kError;
  if (capacity == 0)
    return AttributionAllowedStatus::kNotAllowed;
  return AttributionAllowedStatus::kAllowed;
}

int64_t RateLimitTable::GetCapacity(
    sql::Database* db,
    AttributionType attribution_type,
    const std::string& serialized_impression_site,
    const std::string& serialized_conversion_destination,
    base::Time now) {
  const AttributionStorage::Delegate::RateLimitConfig rate_limits =
      delegate_->GetRateLimits(attribution_type);
  DCHECK_GT(rate_limits.time_window, base::TimeDelta());
  DCHECK_GT(rate_limits.max_contributions_per_window, 0);

  base::Time min_timestamp = now - rate_limits.time_window;

  static constexpr char kAttributionAllowedSql[] =
      "SELECT value FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_impression_site_type_idx")
      "WHERE attribution_type = ? "
      "AND impression_site = ? "
      "AND conversion_destination = ? "
      "AND conversion_time > ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kAttributionAllowedSql));
  statement.BindInt(0, SerializeAttributionType(attribution_type));
  statement.BindString(1, serialized_impression_site);
  statement.BindString(2, serialized_conversion_destination);
  statement.BindTime(3, min_timestamp);

  int64_t sum = 0;
  while (statement.Step()) {
    int64_t value = statement.ColumnInt64(0);
    sum += value;
  }
  if (!statement.Succeeded())
    return -1;

  return rate_limits.max_contributions_per_window > sum
             ? rate_limits.max_contributions_per_window - sum
             : 0;
}

bool RateLimitTable::ClearAllDataInRange(sql::Database* db,
                                         base::Time delete_begin,
                                         base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!((delete_begin.is_null() || delete_begin.is_min()) &&
           delete_end.is_max()));

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteRateLimitRangeSql[] =
      "DELETE FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_attribution_type_conversion_time_idx")
      "WHERE attribution_type = ? AND conversion_time BETWEEN ? AND ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteRateLimitRangeSql));

  for (AttributionType attribution_type : kAttributionTypes) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt(0, SerializeAttributionType(attribution_type));
    statement.BindTime(1, delete_begin);
    statement.BindTime(2, delete_end);
    if (!statement.Run())
      return false;
  }

  return transaction.Commit();
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
  DCHECK(!filter.is_null());

  std::vector<int64_t> rate_limit_ids_to_delete;
  {
    static constexpr char kScanCandidateData[] =
        "SELECT rate_limit_id,impression_site,impression_origin,"
        "conversion_destination,conversion_origin FROM rate_limits "
        DCHECK_SQL_INDEXED_BY("rate_limit_attribution_type_conversion_time_idx")
        "WHERE attribution_type = ? AND conversion_time BETWEEN ? AND ?";
    sql::Statement statement(
        db->GetCachedStatement(SQL_FROM_HERE, kScanCandidateData));

    // Issue deletes for different attribution_types so this can be easily
    // optimized by the rate_limit_attribution_type_conversion_time_idx.
    for (AttributionType attribution_type : kAttributionTypes) {
      statement.Reset(/*clear_bound_vars=*/true);
      statement.BindInt(0, SerializeAttributionType(attribution_type));
      statement.BindTime(1, delete_begin);
      statement.BindTime(2, delete_end);

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
  }

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteRateLimitSql[] =
      "DELETE FROM rate_limits WHERE rate_limit_id = ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteRateLimitSql));
  for (int64_t rate_limit_id : rate_limit_ids_to_delete) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, rate_limit_id);
    if (!statement.Run())
      return false;
  }

  return transaction.Commit();
}

bool RateLimitTable::DeleteExpiredRateLimits(sql::Database* db,
                                             AttributionType attribution_type) {
  base::Time timestamp =
      clock_->Now() - delegate_->GetRateLimits(attribution_type).time_window;

  static constexpr char kDeleteExpiredRateLimits[] =
      // clang-format off
      "DELETE FROM rate_limits "
      DCHECK_SQL_INDEXED_BY("rate_limit_attribution_type_conversion_time_idx")
      "WHERE attribution_type = ? AND conversion_time <= ?";  // clang-format on
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteExpiredRateLimits));
  statement.BindInt(0, SerializeAttributionType(attribution_type));
  statement.BindTime(1, timestamp);
  return statement.Run();
}

bool RateLimitTable::ClearDataForImpressionIds(
    sql::Database* db,
    const std::vector<StorableSource::Id>& impression_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteRateLimitSql[] =
      "DELETE FROM rate_limits WHERE impression_id = ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteRateLimitSql));

  for (StorableSource::Id id : impression_ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);
    if (!statement.Run())
      return false;
  }

  return transaction.Commit();
}

AttributionAllowedStatus
RateLimitTable::AddAggregateHistogramContributionsForTesting(
    sql::Database* db,
    const StorableSource& impression,
    const std::vector<AggregateHistogramContribution>& contributions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(impression.impression_id().has_value());

  base::Time now = clock_->Now();

  const std::string serialized_impression_site =
      impression.ImpressionSite().Serialize();
  const std::string serialized_conversion_destination =
      impression.ConversionDestination().Serialize();

  const int64_t capacity =
      GetCapacity(db, AttributionType::kAggregate, serialized_impression_site,
                  serialized_conversion_destination, now);
  // This should only be possible if there is DB corruption.
  if (capacity < 0)
    return AttributionAllowedStatus::kError;
  if (capacity == 0)
    return AttributionAllowedStatus::kNotAllowed;

  int64_t new_sum = 0;
  for (const auto& contribution : contributions) {
    DCHECK_GT(contribution.value, 0u);
    new_sum += contribution.value;
  }

  if (new_sum > capacity)
    return AttributionAllowedStatus::kNotAllowed;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return AttributionAllowedStatus::kError;

  const std::string serialized_impression_origin =
      SerializeOrigin(impression.impression_origin());
  const std::string serialized_conversion_origin =
      SerializeOrigin(impression.conversion_origin());

  for (const auto& contribution : contributions) {
    if (!AddRow(db, AttributionType::kAggregate, *impression.impression_id(),
                serialized_impression_site, serialized_impression_origin,
                serialized_conversion_destination, serialized_conversion_origin,
                now, contribution.bucket, contribution.value)) {
      return AttributionAllowedStatus::kError;
    }
  }

  return transaction.Commit() ? AttributionAllowedStatus::kAllowed
                              : AttributionAllowedStatus::kError;
}

}  // namespace content
