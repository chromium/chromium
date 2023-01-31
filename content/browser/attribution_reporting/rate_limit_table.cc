// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/rate_limit_table.h"

#include <string>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/sql_queries.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/public/browser/attribution_config.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

RateLimitTable::RateLimitTable(const AttributionStorageDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
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
  // |destination_site| is the destination of the conversion.
  // |context_origin| is the source origin for `kSource` or the destination
  // origin for `kAttribution`.
  // |reporting_origin| is the reporting origin of the impression/conversion.
  // |time| is the time of the source registration.
  // |source_expiry_or_attribution_time| is either the source's expiry time or
  // the attribution time, depending on |scope|.
  static constexpr char kRateLimitTableSql[] =
      "CREATE TABLE rate_limits("
      "id INTEGER PRIMARY KEY NOT NULL,"
      "scope INTEGER NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "source_site TEXT NOT NULL,"
      "destination_site TEXT NOT NULL,"
      "context_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "time INTEGER NOT NULL,"
      "source_expiry_or_attribution_time INTEGER NOT NULL)";
  if (!db->Execute(kRateLimitTableSql)) {
    return false;
  }

  static_assert(static_cast<int>(Scope::kAttribution) == 1,
                "update `scope=1` clause below");

  // Optimizes calls to `SourceAllowedForDestinationLimit()`.
  static constexpr char kRateLimitSourceSiteReportingOriginIndexSql[] =
      "CREATE INDEX rate_limit_source_site_reporting_origin_idx "
      "ON rate_limits(scope,source_site,reporting_origin)";
  if (!db->Execute(kRateLimitSourceSiteReportingOriginIndexSql)) {
    return false;
  }

  // Optimizes calls to `AllowedForReportingOriginLimit()` and
  // `AttributionAllowedForAttributionLimit()`.
  static constexpr char kRateLimitReportingOriginIndexSql[] =
      "CREATE INDEX rate_limit_reporting_origin_idx "
      "ON rate_limits(scope,destination_site,source_site)";
  if (!db->Execute(kRateLimitReportingOriginIndexSql)) {
    return false;
  }

  // Optimizes calls to |DeleteExpiredRateLimits()|, |ClearAllDataInRange()|,
  // |ClearDataForOriginsInRange()|.
  static constexpr char kRateLimitTimeIndexSql[] =
      "CREATE INDEX rate_limit_time_idx ON rate_limits(time)";
  if (!db->Execute(kRateLimitTimeIndexSql)) {
    return false;
  }

  // Optimizes calls to |ClearDataForSourceIds()|.
  static constexpr char kRateLimitImpressionIdIndexSql[] =
      "CREATE INDEX rate_limit_source_id_idx "
      "ON rate_limits(source_id)";
  return db->Execute(kRateLimitImpressionIdIndexSql);
}

bool RateLimitTable::AddRateLimitForSource(sql::Database* db,
                                           const StoredSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddRateLimit(db, source, /*trigger_time=*/absl::nullopt);
}

bool RateLimitTable::AddRateLimitForAttribution(
    sql::Database* db,
    const AttributionInfo& attribution_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddRateLimit(db, attribution_info.source, attribution_info.time);
}

bool RateLimitTable::AddRateLimit(sql::Database* db,
                                  const StoredSource& source,
                                  absl::optional<base::Time> trigger_time) {
  const CommonSourceInfo& common_info = source.common_info();

  // Only delete expired rate limits periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredRateLimitsFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = base::Time::Now();
  if (now - last_cleared_ >= delete_frequency) {
    if (!DeleteExpiredRateLimits(db)) {
      return false;
    }
    last_cleared_ = now;
  }

  Scope scope;
  const attribution_reporting::SuitableOrigin* context_origin;
  base::Time source_expiry_or_attribution_time;
  if (trigger_time.has_value()) {
    scope = Scope::kAttribution;
    context_origin = &common_info.destination_origin();
    source_expiry_or_attribution_time = *trigger_time;
  } else {
    scope = Scope::kSource;
    context_origin = &common_info.source_origin();
    source_expiry_or_attribution_time = common_info.expiry_time();
  }

  static constexpr char kStoreRateLimitSql[] =
      "INSERT INTO rate_limits"
      "(scope,source_id,source_site,destination_site,context_origin,"
      "reporting_origin,time,source_expiry_or_attribution_time)"
      "VALUES(?,?,?,?,?,?,?,?)";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kStoreRateLimitSql));
  statement.BindInt(0, static_cast<int>(scope));
  statement.BindInt64(1, *source.source_id());
  statement.BindString(2, common_info.SourceSite().Serialize());
  statement.BindString(3, common_info.DestinationSite().Serialize());
  statement.BindString(4, context_origin->Serialize());
  statement.BindString(5, common_info.reporting_origin().Serialize());
  statement.BindTime(6, common_info.source_time());
  statement.BindTime(7, source_expiry_or_attribution_time);

  return statement.Run();
}

RateLimitResult RateLimitTable::AttributionAllowedForAttributionLimit(
    sql::Database* db,
    const AttributionInfo& attribution_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const CommonSourceInfo& common_info = attribution_info.source.common_info();

  const AttributionConfig::RateLimitConfig rate_limits =
      delegate_->GetRateLimits();
  DCHECK_GT(rate_limits.time_window, base::TimeDelta());
  DCHECK_GT(rate_limits.max_attributions, 0);

  base::Time min_timestamp = attribution_info.time - rate_limits.time_window;

  static_assert(static_cast<int>(Scope::kAttribution) == 1,
                "update `scope=1` in query below");

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kRateLimitAttributionAllowedSql));
  statement.BindString(0, common_info.DestinationSite().Serialize());
  statement.BindString(1, common_info.SourceSite().Serialize());
  statement.BindString(2, common_info.reporting_origin().Serialize());
  statement.BindTime(3, min_timestamp);

  if (!statement.Step()) {
    return RateLimitResult::kError;
  }

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
                                        source.common_info().source_time());
}

RateLimitResult RateLimitTable::SourceAllowedForDestinationLimit(
    sql::Database* db,
    const StorableSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static_assert(static_cast<int>(Scope::kSource) == 0,
                "update `scope=0` query below");

  // Check the number of unique destinations covered by all source registrations
  // whose [source_time, source_expiry_or_attribution_time] intersect with the
  // current source_time.
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kRateLimitSourceAllowedSql));

  const CommonSourceInfo& common_info = source.common_info();
  statement.BindString(0, common_info.SourceSite().Serialize());
  statement.BindString(1, common_info.reporting_origin().Serialize());
  statement.BindTime(2, common_info.source_time());

  const std::string serialized_destination_site =
      common_info.DestinationSite().Serialize();

  const int limit = delegate_->GetMaxDestinationsPerSourceSiteReportingOrigin();
  DCHECK_GT(limit, 0);

  base::flat_set<std::string> destination_sites;
  while (statement.Step()) {
    std::string destination_site = statement.ColumnString(0);

    // The destination site isn't new, so it doesn't change the count.
    //
    // TODO(linnan): Consider adding an early exit query which first checks for
    // the existence of `destination_site` for (source_site, reporting_origin),
    // to avoid querying all of the rows in the case of multiple sources.
    if (destination_site == serialized_destination_site) {
      return RateLimitResult::kAllowed;
    }

    destination_sites.insert(std::move(destination_site));

    if (destination_sites.size() == static_cast<size_t>(limit)) {
      return RateLimitResult::kNotAllowed;
    }
  }

  return statement.Succeeded() ? RateLimitResult::kAllowed
                               : RateLimitResult::kError;
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
  const AttributionConfig::RateLimitConfig rate_limits =
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
      common_info.reporting_origin().Serialize();

  base::Time min_timestamp = time - rate_limits.time_window;

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kRateLimitSelectReportingOriginsSql));
  statement.BindInt(0, static_cast<int>(scope));
  statement.BindString(1, common_info.SourceSite().Serialize());
  statement.BindString(2, common_info.DestinationSite().Serialize());
  statement.BindTime(3, min_timestamp);

  base::flat_set<std::string> reporting_origins;
  while (statement.Step()) {
    std::string reporting_origin = statement.ColumnString(0);

    // The origin isn't new, so it doesn't change the count.
    if (reporting_origin == serialized_reporting_origin) {
      return RateLimitResult::kAllowed;
    }

    reporting_origins.insert(std::move(reporting_origin));

    if (reporting_origins.size() == static_cast<size_t>(max)) {
      return RateLimitResult::kNotAllowed;
    }
  }

  return statement.Succeeded() ? RateLimitResult::kAllowed
                               : RateLimitResult::kError;
}

bool RateLimitTable::ClearAllDataInRange(sql::Database* db,
                                         base::Time delete_begin,
                                         base::Time delete_end) {
  DCHECK(!((delete_begin.is_null() || delete_begin.is_min()) &&
           delete_end.is_max()));

  // TODO(linnan): Optimize using a more appropriate index.
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteRateLimitRangeSql));
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
    StoragePartition::StorageKeyMatcherFunction filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (filter.is_null()) {
    return ClearAllDataInRange(db, delete_begin, delete_end);
  }

  static constexpr char kDeleteSql[] = "DELETE FROM rate_limits WHERE id=?";
  sql::Statement delete_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  // TODO(linnan): Optimize using a more appropriate index.
  sql::Statement select_statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kSelectRateLimitsForDeletionSql));
  select_statement.BindTime(0, delete_begin);
  select_statement.BindTime(1, delete_end);

  while (select_statement.Step()) {
    int64_t rate_limit_id = select_statement.ColumnInt64(0);
    if (filter.Run(blink::StorageKey(
            DeserializeOrigin(select_statement.ColumnString(1))))) {
      // See https://www.sqlite.org/isolation.html for why it's OK for this
      // DELETE to be interleaved in the surrounding SELECT.
      delete_statement.Reset(/*clear_bound_vars=*/false);
      delete_statement.BindInt64(0, rate_limit_id);
      if (!delete_statement.Run()) {
        return false;
      }
    }
  }

  if (!select_statement.Succeeded()) {
    return false;
  }

  return transaction.Commit();
}

bool RateLimitTable::DeleteExpiredRateLimits(sql::Database* db) {
  base::Time now = base::Time::Now();
  base::Time timestamp = now - delegate_->GetRateLimits().time_window;

  static_assert(static_cast<int>(Scope::kAttribution) == 1,
                "update `scope=1` query below");

  // Attribution rate limit entries can be deleted as long as their time falls
  // outside the rate limit window. For source entries, if the expiry time has
  // not passed, keep entries around to ensure
  // `SourceAllowedForDestinationLimit()` is computed properly.
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteExpiredRateLimitsSql));
  statement.BindTime(0, timestamp);
  statement.BindTime(1, now);
  return statement.Run();
}

bool RateLimitTable::ClearDataForSourceIds(
    sql::Database* db,
    const std::vector<StoredSource::Id>& source_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteRateLimitsBySourceIdSql));

  for (StoredSource::Id id : source_ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

void RateLimitTable::AppendRateLimitDataKeys(
    sql::Database* db,
    std::vector<AttributionDataModel::DataKey>& keys) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetRateLimitDataKeysSql));

  while (statement.Step()) {
    url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(0));
    if (reporting_origin.opaque()) {
      continue;
    }
    keys.emplace_back(std::move(reporting_origin));
  }
}

}  // namespace content
