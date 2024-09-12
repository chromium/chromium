// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/aggregatable_debug_rate_limit_table.h"

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/browser/attribution_reporting/sql_queries.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

AggregatableDebugRateLimitTable::AggregatableDebugRateLimitTable(
    const AttributionResolverDelegate* delegate)
    : delegate_(
          raw_ref<const AttributionResolverDelegate>::from_ptr(delegate)) {}

AggregatableDebugRateLimitTable::~AggregatableDebugRateLimitTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool AggregatableDebugRateLimitTable::CreateTable(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  // All columns in this table are const.
  // |context_site| is the top-level site of the source or trigger.
  // |reporting_origin| is the reporting origin of the source or trigger.
  // |reporting_site| is the reporting site of the source or trigger.
  // |time| is the time of the source or trigger registration.
  // |consumed_budget| is the budget consumed for the aggregatable debug report.
  static constexpr char kAggregatableDebugRateLimitsTableSql[] =
      "CREATE TABLE aggregatable_debug_rate_limits("
      "id INTEGER PRIMARY KEY NOT NULL,"
      "context_site TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "reporting_site TEXT NOT NULL,"
      "time INTEGER NOT NULL,"
      "consumed_budget INTEGER NOT NULL)";
  if (!db->Execute(kAggregatableDebugRateLimitsTableSql)) {
    return false;
  }

  // Optimizes calls to `AllowedForRateLimit()`.
  static constexpr char kAggregatableDebugRateLimitsContextSiteIndex[] =
      "CREATE INDEX aggregatable_debug_rate_limits_context_site_idx "
      "ON aggregatable_debug_rate_limits(context_site)";
  if (!db->Execute(kAggregatableDebugRateLimitsContextSiteIndex)) {
    return false;
  }

  // Optimizes calls to `DeleteExpiredRateLimits()`, `ClearAllDataInRange()`,
  // `ClearDataForOriginsInRange()`.
  static constexpr char kAggregatableDebugRateLimitsTimeIndex[] =
      "CREATE INDEX aggregatable_debug_rate_limits_time_idx "
      "ON aggregatable_debug_rate_limits(time)";
  if (!db->Execute(kAggregatableDebugRateLimitsTimeIndex)) {
    return false;
  }

  return transaction.Commit();
}

bool AggregatableDebugRateLimitTable::AddRateLimit(
    sql::Database* db,
    const AggregatableDebugReport& report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only delete expired rate limits periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredRateLimitsFrequency();
  CHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = base::Time::Now();
  if (now - last_cleared_ >= delete_frequency) {
    if (!DeleteExpiredRateLimits(db)) {
      return false;
    }
    last_cleared_ = now;
  }

  static constexpr char kStoreRateLimitSql[] =
      "INSERT INTO aggregatable_debug_rate_limits"
      "(context_site,reporting_origin,reporting_site,time,consumed_budget)"
      "VALUES(?,?,?,?,?)";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kStoreRateLimitSql));

  statement.BindString(0, report.context_site().Serialize());
  statement.BindString(1, report.reporting_origin().Serialize());
  statement.BindString(2, report.ReportingSite().Serialize());
  statement.BindTime(3, report.scheduled_report_time());
  statement.BindInt(4, report.BudgetRequired());

  return statement.Run();
}

AggregatableDebugRateLimitTable::Result
AggregatableDebugRateLimitTable::AllowedForRateLimit(
    sql::Database* db,
    const AggregatableDebugReport& report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kAggregatableDebugReportAllowedForRateLimitSql));
  statement.BindString(0, report.context_site().Serialize());
  statement.BindTime(
      1, report.scheduled_report_time() -
             AttributionConfig::AggregatableDebugRateLimit::kRateLimitWindow);

  base::CheckedNumeric<int> total_global_budget = report.BudgetRequired();
  base::CheckedNumeric<int> total_reporting_budget = total_global_budget;

  const std::string serialized_reporting_site =
      report.ReportingSite().Serialize();

  while (statement.Step()) {
    int consumed_budget = statement.ColumnInt(1);
    // Note that this could occur in practice, e.g. with deliberate DB
    // modification or corruption, which would cause this to continue failing
    // until the offending row expires.
    if (!attribution_reporting::IsAggregatableValueInRange(consumed_budget)) {
      return Result::kError;
    }

    total_global_budget += consumed_budget;
    if (serialized_reporting_site == statement.ColumnStringView(0)) {
      total_reporting_budget += consumed_budget;
    }
  }

  if (!statement.Succeeded()) {
    return Result::kError;
  }

  AttributionConfig::AggregatableDebugRateLimit aggregatable_debug_rate_limit =
      delegate_->GetAggregatableDebugRateLimit();

  const int global_limit =
      aggregatable_debug_rate_limit.max_budget_per_context_site;
  CHECK_GT(global_limit, 0);

  const int reporting_limit =
      aggregatable_debug_rate_limit.max_budget_per_context_reporting_site;
  CHECK_GT(reporting_limit, 0);

  const auto is_limit_hit = [](base::CheckedNumeric<int> value, int limit) {
    return !value.IsValid() || value.ValueOrDie() > limit;
  };

  const bool global_limit_hit = is_limit_hit(total_global_budget, global_limit);
  const bool reporting_limit_hit =
      is_limit_hit(total_reporting_budget, reporting_limit);

  if (global_limit_hit && reporting_limit_hit) {
    return Result::kHitBothLimits;
  }

  if (!global_limit_hit && !reporting_limit_hit) {
    return Result::kAllowed;
  }

  return global_limit_hit ? Result::kHitGlobalLimit
                          : Result::kHitReportingLimit;
}

bool AggregatableDebugRateLimitTable::DeleteExpiredRateLimits(
    sql::Database* db) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kDeleteExpiredAggregatableDebugRateLimitsSql));
  statement.BindTime(
      0, base::Time::Now() -
             AttributionConfig::AggregatableDebugRateLimit::kRateLimitWindow);
  return statement.Run();
}

bool AggregatableDebugRateLimitTable::ClearAllDataAllTime(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kDeleteAllRateLimitsSql[] =
      "DELETE FROM aggregatable_debug_rate_limits";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteAllRateLimitsSql));
  return statement.Run();
}

bool AggregatableDebugRateLimitTable::ClearDataForOriginsInRange(
    sql::Database* db,
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (filter.is_null()) {
    return ClearAllDataInRange(db, delete_begin, delete_end);
  }

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kDeleteSql[] =
      "DELETE FROM aggregatable_debug_rate_limits WHERE id=?";
  sql::Statement delete_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));

  sql::Statement select_statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kSelectAggregatableDebugRateLimitsForDeletionSql));
  select_statement.BindTime(0, delete_begin);
  select_statement.BindTime(1, delete_end);

  while (select_statement.Step()) {
    int64_t rate_limit_id = select_statement.ColumnInt64(0);
    if (filter.Run(blink::StorageKey::CreateFirstParty(
            DeserializeOrigin(select_statement.ColumnStringView(1))))) {
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

bool AggregatableDebugRateLimitTable::ClearAllDataInRange(
    sql::Database* db,
    base::Time delete_begin,
    base::Time delete_end) {
  CHECK(!((delete_begin.is_null() || delete_begin.is_min()) &&
          delete_end.is_max()));

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kDeleteAggregatableDebugRateLimitRangeSql));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);
  return statement.Run();
}

void AggregatableDebugRateLimitTable::SetDelegate(
    const AttributionResolverDelegate& delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = delegate;
}

}  // namespace content
