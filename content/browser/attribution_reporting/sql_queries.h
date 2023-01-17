// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERIES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERIES_H_

namespace content::attribution_queries {

inline constexpr const char kMinPrioritySql[] =
    "SELECT priority,trigger_time,report_id "
    "FROM event_level_reports "
    "WHERE source_id=? AND report_time=?";

inline constexpr const char kGetMatchingSourcesSql[] =
    "SELECT source_id,num_attributions,aggregatable_budget_consumed "
    "FROM sources "
    "WHERE destination_site=? AND reporting_origin=? "
    "AND(event_level_active=1 OR aggregatable_active=1)"
    "AND expiry_time>? "
    "ORDER BY priority DESC,source_time DESC";

inline constexpr const char kSelectExpiredSourcesSql[] =
    "SELECT source_id FROM sources "
    "WHERE expiry_time<=? AND "
    "source_id NOT IN("
    "SELECT source_id FROM event_level_reports"
    ")AND source_id NOT IN("
    "SELECT source_id FROM aggregatable_report_metadata"
    ")LIMIT ?";

inline constexpr const char kSelectInactiveSourcesSql[] =
    "SELECT source_id FROM sources "
    "WHERE event_level_active=0 AND aggregatable_active=0 AND "
    "source_id NOT IN("
    "SELECT source_id FROM event_level_reports"
    ")AND source_id NOT IN("
    "SELECT source_id FROM aggregatable_report_metadata"
    ")LIMIT ?";

inline constexpr const char kScanCandidateData[] =
    "SELECT I.source_origin,I.destination_origin,I.reporting_origin,"
    "I.source_id,C.report_id "
    "FROM sources I LEFT JOIN event_level_reports C ON "
    "C.source_id=I.source_id WHERE"
    "(I.source_time BETWEEN ?1 AND ?2)OR"
    "(C.trigger_time BETWEEN ?1 AND ?2)";

inline constexpr const char kDeleteVestigialConversionSql[] =
    "DELETE FROM event_level_reports WHERE source_id=?";

inline constexpr const char kCountSourcesSql[] =
    "SELECT COUNT(*)FROM sources "
    "WHERE source_origin=? "
    "AND(event_level_active=1 OR aggregatable_active=1)";

inline constexpr const char kCountReportsSql[] =
    "SELECT COUNT(*)FROM dedup_keys "
    "WHERE source_id=? AND report_type=? AND dedup_key=?";

inline constexpr const char kDedupKeySql[] =
    "SELECT dedup_key FROM dedup_keys WHERE source_id=? AND report_type=?";

inline constexpr const char kScanCandidateDataAggregatable[] =
    "SELECT I.source_origin,I.destination_origin,I.reporting_origin,"
    "I.source_id,A.aggregation_id "
    "FROM sources I LEFT JOIN aggregatable_report_metadata A "
    "ON A.source_id=I.source_id WHERE"
    "(I.source_time BETWEEN ?1 AND ?2)OR"
    "(A.trigger_time BETWEEN ?1 AND ?2)";

inline constexpr const char kDeleteAggregationsSql[] =
    "DELETE FROM aggregatable_report_metadata "
    "WHERE source_id=? "
    "RETURNING aggregation_id";

inline constexpr const char kGetContributionsSql[] =
    "SELECT key_high_bits,key_low_bits,value "
    "FROM aggregatable_contributions "
    "WHERE aggregation_id=?";

// We need to hint to the query planner that/ `event_level_active` and
// `aggregatable_active` are booleans, so include them in the conditional.
#define ATTRIBUTION_COUNT_REPORTS_SQL(table) \
  "SELECT COUNT(*)FROM " table               \
  " R "                                      \
  "JOIN sources I "                          \
  "ON I.source_id=R.source_id "              \
  "WHERE I.destination_site=? "              \
  "AND(event_level_active BETWEEN 0 AND 1)"  \
  "AND(aggregatable_active BETWEEN 0 AND 1)"

inline constexpr const char kCountEventLevelReportsSql[] =
    ATTRIBUTION_COUNT_REPORTS_SQL("event_level_reports");

inline constexpr const char kCountAggregatableReportsSql[] =
    ATTRIBUTION_COUNT_REPORTS_SQL("aggregatable_report_metadata");

#undef ATTRIBUTION_COUNT_REPORTS_SQL

#define ATTRIBUTION_NEXT_REPORT_TIME_SQL(table) \
  "SELECT MIN(report_time)FROM " table " WHERE report_time>?"

inline constexpr char kNextEventLevelReportTimeSql[] =
    ATTRIBUTION_NEXT_REPORT_TIME_SQL("event_level_reports");

inline constexpr char kNextAggregatableReportTimeSql[] =
    ATTRIBUTION_NEXT_REPORT_TIME_SQL("aggregatable_report_metadata");

#undef ATTRIBUTION_NEXT_REPORT_TIME_SQL

// Set the report time for all reports that should have been sent before now
// to now + a random number of microseconds between `min_delay` and
// `max_delay`, both inclusive. We use RANDOM, instead of a method on the
// delegate, to avoid having to pull all reports into memory and update them
// one by one. We use ABS because RANDOM may return a negative integer. We add
// 1 to the difference between `max_delay` and `min_delay` to ensure that the
// range of generated values is inclusive. If `max_delay == min_delay`, we
// take the remainder modulo 1, which is always 0.
#define ATTRIBUTION_SET_REPORT_TIME_SQL(table) \
  "UPDATE " table                              \
  " SET report_time=?+ABS(RANDOM()%?)"         \
  "WHERE report_time<?"

inline constexpr const char kSetEventLevelReportTimeSql[] =
    ATTRIBUTION_SET_REPORT_TIME_SQL("event_level_reports");

inline constexpr const char kSetAggregatableReportTimeSql[] =
    ATTRIBUTION_SET_REPORT_TIME_SQL("aggregatable_report_metadata");

#undef ATTRIBUTION_SET_REPORT_TIME_SQL

}  // namespace content::attribution_queries

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERIES_H_
