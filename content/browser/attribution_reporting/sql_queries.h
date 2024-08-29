// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERIES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERIES_H_

#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/rate_limit_table.h"

namespace content::attribution_queries {

static_assert(static_cast<int>(
                  attribution_reporting::mojom::ReportType::kEventLevel) == 0,
              "update `report_type=0` clause below");
inline constexpr const char kMinPrioritySql[] =
    "SELECT metadata,report_id FROM reports "
    "WHERE source_id=? AND initial_report_time=? AND report_type=0";

// Rows are ordered by source_id instead of source_time because the former is
// strictly increasing while the latter is subject to clock adjustments. This
// property is only guaranteed because of the use of AUTOINCREMENT on the
// source_id column, which prevents reuse upon row deletion.
inline constexpr const char kGetMatchingSourcesSql[] =
    "SELECT I.priority,I.source_id,I.num_attributions>0 OR "
    "I.num_aggregatable_attribution_reports>0,"
    "I.attribution_scopes_data,I.source_time "
    "FROM sources I "
    "WHERE I.reporting_origin=? "
    "AND(I.event_level_active=1 OR I.aggregatable_active=1)"
    "AND I.expiry_time>? "
    "AND I.source_id IN("
    "SELECT source_id FROM source_destinations D "
    "WHERE D.destination_site IN(?,?,?)"
    ")";

inline constexpr const char kSelectExpiredSourcesSql[] =
    "SELECT source_id FROM sources "
    "WHERE expiry_time<=? AND "
    "source_id NOT IN("
    "SELECT source_id FROM reports"
    ")LIMIT ?";

inline constexpr const char kSelectInactiveSourcesSql[] =
    "SELECT source_id FROM sources "
    "WHERE event_level_active=0 AND aggregatable_active=0 AND "
    "source_id NOT IN("
    "SELECT source_id FROM reports"
    ")LIMIT ?";

inline constexpr const char kScanSourcesData[] =
    "SELECT I.reporting_origin,I.source_id "
    "FROM sources I WHERE "
    "I.source_time BETWEEN ? AND ?";

inline constexpr const char kScanReportsData[] =
    "SELECT R.reporting_origin,R.source_id,R.report_id,R.report_type "
    "FROM reports R WHERE "
    "R.trigger_time BETWEEN ? AND ?";

inline constexpr const char kDeleteVestigialConversionSql[] =
    "DELETE FROM reports WHERE source_id=? RETURNING report_type";

inline constexpr const char kCountActiveSourcesFromSourceOriginSql[] =
    "SELECT COUNT(*)FROM sources "
    "WHERE source_origin=? "
    "AND(event_level_active=1 OR aggregatable_active=1)"
    "AND expiry_time>?";

inline constexpr const char kCountSourcesSql[] = "SELECT COUNT(*)FROM sources";

inline constexpr const char kDedupKeySql[] =
    "SELECT dedup_key,report_type FROM dedup_keys WHERE source_id=?";

inline constexpr const char kGetSourcesDataKeysSql[] =
    "SELECT reporting_origin FROM sources";

static_assert(
    static_cast<int>(
        attribution_reporting::mojom::ReportType::kNullAggregatable) == 2,
    "update `report_type=2` clause below");
inline constexpr const char kGetNullReportsDataKeysSql[] =
    "SELECT reporting_origin FROM reports WHERE report_type=2";

inline constexpr const char kGetRateLimitDataKeysSql[] =
    "SELECT reporting_origin FROM rate_limits";

inline constexpr const char kCountReportsForDestinationSql[] =
    "SELECT COUNT(*)FROM source_destinations D "
    "JOIN reports R "
    "ON R.source_id=D.source_id "
    "WHERE D.destination_site=? AND R.report_type=?";

inline constexpr char kNextReportTimeSql[] =
    "SELECT MIN(report_time)FROM reports WHERE report_time>?";

// Set the report time for all reports that should have been sent before now
// to now + a random number of microseconds between `min_delay` and
// `max_delay`, both inclusive. We use RANDOM, instead of a method on the
// delegate, to avoid having to pull all reports into memory and update them
// one by one. We use ABS because RANDOM may return a negative integer. We add
// 1 to the difference between `max_delay` and `min_delay` to ensure that the
// range of generated values is inclusive. If `max_delay == min_delay`, we
// take the remainder modulo 1, which is always 0.
inline constexpr const char kSetReportTimeSql[] =
    "UPDATE reports "
    "SET report_time=?+ABS(RANDOM()%?)"
    "WHERE report_time<?";

// clang-format off

#define ATTRIBUTION_SOURCE_COLUMNS_SQL(prefix) \
  prefix "source_id,"                          \
  prefix "source_event_id,"                    \
  prefix "source_origin,"                      \
  prefix "reporting_origin,"                   \
  prefix "source_time,"                        \
  prefix "expiry_time,"                        \
  prefix "aggregatable_report_window_time,"    \
  prefix "source_type,"                        \
  prefix "attribution_logic,"                  \
  prefix "priority,"                           \
  prefix "debug_key,"                          \
  prefix "num_attributions,"                   \
  prefix "remaining_aggregatable_attribution_budget," \
  prefix "num_aggregatable_attribution_reports," \
  prefix "remaining_aggregatable_debug_budget," \
  prefix "aggregatable_source,"                \
  prefix "filter_data,"                        \
  prefix "attribution_scopes_data,"            \
  prefix "event_level_active,"                 \
  prefix "aggregatable_active,"                \
  prefix "read_only_source_data"

inline constexpr const char kReadSourceToAttributeSql[] =
    "SELECT " ATTRIBUTION_SOURCE_COLUMNS_SQL("")
    " FROM sources "
    "WHERE source_id=?";

inline constexpr const char kGetActiveSourcesSql[] =
      "SELECT " ATTRIBUTION_SOURCE_COLUMNS_SQL("")
      " FROM sources "
      "WHERE(event_level_active=1 OR aggregatable_active=1)AND "
      "expiry_time>? LIMIT ?";

#define ATTRIBUTION_SELECT_REPORT_AND_SOURCE_COLUMNS_SQL                      \
  "SELECT "                                                                   \
  ATTRIBUTION_SOURCE_COLUMNS_SQL("I.")                                        \
  ",R.report_id,R.trigger_time,R.report_time,R.initial_report_time,"          \
  "R.failed_send_attempts,R.external_report_id,R.debug_key,R.context_origin," \
  "R.reporting_origin,R.report_type,R.metadata "                              \
  "FROM reports R "                                                           \
  "LEFT JOIN sources I ON R.source_id=I.source_id "

inline constexpr const char kGetReportsSql[] =
    ATTRIBUTION_SELECT_REPORT_AND_SOURCE_COLUMNS_SQL
    "WHERE R.report_time<=? LIMIT ?";

inline constexpr const char kGetReportSql[] =
    ATTRIBUTION_SELECT_REPORT_AND_SOURCE_COLUMNS_SQL
    "WHERE R.report_id=?";

#undef ATTRIBUTION_SELECT_REPORT_AND_SOURCE_COLUMNS_SQL

inline constexpr const char kUpdateFailedReportSql[] =
  "UPDATE reports "
  "SET report_time=?,"
  "failed_send_attempts=failed_send_attempts+1 "
  "WHERE report_id=?";

static_assert(static_cast<int>(
                  attribution_reporting::mojom::ReportType::kEventLevel) == 0,
              "update `report_type=0` clause below");
inline constexpr const char kDeletePendingEventLevelReportsForSourceSql[] =
  "DELETE FROM reports "
  "WHERE report_type=0 AND source_id=? AND trigger_time>=? "
  "RETURNING report_id";

static_assert(
    static_cast<int>(
        attribution_reporting::mojom::ReportType::kAggregatableAttribution) ==
        1,
    "update `report_type=1` clause below");
inline constexpr char kDeleteAggregatableReportsForDestinationLimitSql[] =
  "DELETE FROM reports "
  "WHERE report_type=1 AND source_id=? "
  "RETURNING report_id";

// clang-format on

inline constexpr const char kRateLimitAttributionAllowedSql[] =
    "SELECT COUNT(*)FROM rate_limits "
    "WHERE scope=? "
    "AND destination_site=? "
    "AND source_site=? "
    "AND reporting_site=? "
    "AND time>?";

static_assert(static_cast<int>(RateLimitTable::Scope::kSource) == 0,
              "update `scope=0` query below");
#define RATE_LIMIT_SOURCE_CONDITION "scope=0"

static_assert(static_cast<int>(RateLimitTable::Scope::kEventLevelAttribution) ==
                  1,
              "update `scope=1` query below");
static_assert(
    static_cast<int>(RateLimitTable::Scope::kAggregatableAttribution) == 2,
    "update `scope=2` query below");
#define RATE_LIMIT_ATTRIBUTION_CONDITION "(scope=1 OR scope=2)"

inline constexpr const char kRateLimitSourceAllowedSql[] =
    "SELECT destination_site,time,destination_limit_priority,source_id "
    "FROM rate_limits "
    "WHERE " RATE_LIMIT_SOURCE_CONDITION
    " AND source_site=?"
    " AND reporting_site=?"
    " AND source_expiry_or_attribution_time>?"
    " AND deactivated_for_source_destination_limit=0";

inline constexpr const char kRateLimitSourceAllowedDestinationRateLimitSql[] =
    "SELECT destination_site,reporting_site FROM rate_limits "
    "WHERE " RATE_LIMIT_SOURCE_CONDITION
    " AND source_site=?"
    " AND source_expiry_or_attribution_time>?"
    " AND time>?";

inline constexpr const char
    kRateLimitSourceAllowedDestinationPerDayRateLimitSql[] =
        "SELECT destination_site,reporting_site FROM rate_limits "
        "WHERE " RATE_LIMIT_SOURCE_CONDITION
        " AND source_site=?"
        " AND reporting_site=?"
        " AND source_expiry_or_attribution_time>?"
        " AND time>?";

#define RATE_LIMIT_SELECT_REPORTING_ORIGINS_QUERY \
  "SELECT reporting_origin FROM rate_limits "     \
  "WHERE source_site=? "                          \
  "AND destination_site=? "                       \
  "AND time>? "                                   \
  "AND "

inline constexpr const char kRateLimitSelectSourceReportingOriginsSql[] =
    RATE_LIMIT_SELECT_REPORTING_ORIGINS_QUERY RATE_LIMIT_SOURCE_CONDITION;

inline constexpr const char kRateLimitSelectAttributionReportingOriginsSql[] =
    RATE_LIMIT_SELECT_REPORTING_ORIGINS_QUERY RATE_LIMIT_ATTRIBUTION_CONDITION;

#undef RATE_LIMIT_SELECT_REPORTING_ORIGINS_QUERY

inline constexpr const char kRateLimitSelectSourceReportingOriginsBySiteSql[] =
    "SELECT reporting_origin FROM rate_limits "
    "WHERE " RATE_LIMIT_SOURCE_CONDITION
    " AND source_site=?"
    " AND reporting_site=?"
    " AND time>?";

static_assert(RateLimitTable::kUnsetRecordId == -1,
              "update `report_id!=-1` query below");
#define RATE_LIMIT_REPORT_ID_SET_CONDITION "report_id!=-1"

inline constexpr const char kDeleteAttributionRateLimitByReportIdSql[] =
    "DELETE FROM rate_limits "
    "WHERE " RATE_LIMIT_ATTRIBUTION_CONDITION
    " AND scope=? AND " RATE_LIMIT_REPORT_ID_SET_CONDITION " AND report_id=?";

inline constexpr const char kDeleteRateLimitRangeSql[] =
    "DELETE FROM rate_limits WHERE"
    "(time BETWEEN ?1 AND ?2)OR"
    "(" RATE_LIMIT_ATTRIBUTION_CONDITION
    "AND source_expiry_or_attribution_time BETWEEN ?1 AND ?2)";

inline constexpr const char kSelectRateLimitsForDeletionSql[] =
    "SELECT id,reporting_origin "
    "FROM rate_limits WHERE"
    "(time BETWEEN ?1 AND ?2)OR"
    "(" RATE_LIMIT_ATTRIBUTION_CONDITION
    "AND source_expiry_or_attribution_time BETWEEN ?1 AND ?2)";

inline constexpr const char kDeleteExpiredRateLimitsSql[] =
    "DELETE FROM rate_limits "
    "WHERE time<=? AND(" RATE_LIMIT_ATTRIBUTION_CONDITION
    "OR source_expiry_or_attribution_time<=?)";

inline constexpr const char kDeleteRateLimitsBySourceIdSql[] =
    "DELETE FROM rate_limits WHERE source_id=?";

inline constexpr const char kDeactivateForSourceDestinationLimitSql[] =
    "UPDATE rate_limits "
    "SET deactivated_for_source_destination_limit=1 "
    "WHERE " RATE_LIMIT_SOURCE_CONDITION " AND source_id=?";

#undef RATE_LIMIT_SOURCE_CONDITION

inline constexpr const char kAggregatableDebugReportAllowedForRateLimitSql[] =
    "SELECT reporting_site,consumed_budget "
    "FROM aggregatable_debug_rate_limits "
    "WHERE context_site=? AND time>?";

inline constexpr const char kDeleteExpiredAggregatableDebugRateLimitsSql[] =
    "DELETE FROM aggregatable_debug_rate_limits "
    "WHERE time<=?";

inline constexpr const char kSelectAggregatableDebugRateLimitsForDeletionSql[] =
    "SELECT id,reporting_origin "
    "FROM aggregatable_debug_rate_limits "
    "WHERE time BETWEEN ?1 AND ?2";

inline constexpr const char kDeleteAggregatableDebugRateLimitRangeSql[] =
    "DELETE FROM aggregatable_debug_rate_limits "
    "WHERE time BETWEEN ?1 AND ?2";

}  // namespace content::attribution_queries

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERIES_H_
