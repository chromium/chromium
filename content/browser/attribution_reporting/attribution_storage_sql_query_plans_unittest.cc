// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "content/browser/attribution_reporting/attribution_resolver.h"
#include "content/browser/attribution_reporting/attribution_resolver_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/sql_queries.h"
#include "content/browser/attribution_reporting/sql_query_plan_test_util.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using base::test::ValueIs;
using ::testing::AllOf;

class AttributionSqlQueryPlanTest : public testing::Test {
 public:
  AttributionSqlQueryPlanTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

    {
      std::unique_ptr<AttributionResolver> storage =
          std::make_unique<AttributionResolverImpl>(
              temp_directory_.GetPath(),
              std::make_unique<ConfigurableStorageDelegate>());

      // Make sure lazy initialization happens by adding a record to the db, but
      // then ensure the database is closed so the sqlite_dev_shell can read it.
      auto result = storage->StoreSource(SourceBuilder().Build());
      ASSERT_EQ(result.status(), StorableSource::Result::kSuccess);
    }

    explainer_ = std::make_unique<SqlQueryPlanExplainer>(
        temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions")));
  }

  // Helper method to make tests as readable as possible.
  base::expected<SqlQueryPlan, SqlQueryPlanExplainer::Error> GetPlan(
      std::string query,
      std::optional<SqlFullScanReason> reason = std::nullopt) {
    return explainer_->GetPlan(std::move(query), reason);
  }

 protected:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<SqlQueryPlanExplainer> explainer_;
};

TEST_F(AttributionSqlQueryPlanTest, kMinPrioritySql) {
  EXPECT_THAT(GetPlan(attribution_queries::kMinPrioritySql),
              ValueIs(UsesIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest, kGetMatchingSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetMatchingSourcesSql),
              ValueIs(AllOf(UsesCoveringIndex("sources_by_destination_site"),
                            UsesPrimaryKey())));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectExpiredSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSelectExpiredSourcesSql),
              ValueIs(AllOf(UsesCoveringIndex("sources_by_expiry_time"),
                            UsesIndex("reports_by_source_id_report_type"))));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectInactiveSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSelectInactiveSourcesSql),
              ValueIs(AllOf(UsesCoveringIndex(
                                "sources_by_active_reporting_origin",
                                {"event_level_active", "aggregatable_active"}),
                            UsesIndex("reports_by_source_id_report_type"))));
}

TEST_F(AttributionSqlQueryPlanTest, kScanSourcesData) {
  EXPECT_THAT(GetPlan(attribution_queries::kScanSourcesData),
              ValueIs(UsesIndex("sources_by_source_time")));
}

TEST_F(AttributionSqlQueryPlanTest, kScanReportsData) {
  EXPECT_THAT(GetPlan(attribution_queries::kScanReportsData),
              ValueIs(UsesIndex("reports_by_trigger_time")));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteVestigialConversionSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteVestigialConversionSql),
              ValueIs(UsesCoveringIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest, kCountSourcesSql) {
  EXPECT_THAT(
      GetPlan(attribution_queries::kCountActiveSourcesFromSourceOriginSql),
      ValueIs(UsesIndex("active_sources_by_source_origin")));
}

TEST_F(AttributionSqlQueryPlanTest, kDedupKeySql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDedupKeySql),
              ValueIs(UsesPrimaryKey()));
}

TEST_F(AttributionSqlQueryPlanTest, kGetSourcesDataKeysSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetSourcesDataKeysSql,
                      SqlFullScanReason::kIntentional),
              ValueIs(UsesCoveringIndex("sources_by_active_reporting_origin")));
}

TEST_F(AttributionSqlQueryPlanTest, kGetNullReportsDataKeysSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetNullReportsDataKeysSql,
                      SqlFullScanReason::kNotOptimized),
              ValueIs(UsesCoveringIndex("reports_by_reporting_origin")));
}

TEST_F(AttributionSqlQueryPlanTest, kGetRateLimitDataKeysSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetRateLimitDataKeysSql,
                      SqlFullScanReason::kIntentional),
              base::test::HasValue());
}

TEST_F(AttributionSqlQueryPlanTest, kCountReportsForDestinationSql) {
  EXPECT_THAT(
      GetPlan(attribution_queries::kCountReportsForDestinationSql),
      ValueIs(AllOf(UsesCoveringIndex("sources_by_destination_site"),
                    UsesCoveringIndex("reports_by_source_id_report_type"))));
}

TEST_F(AttributionSqlQueryPlanTest, kNextReportTimeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kNextReportTimeSql),
              ValueIs(UsesCoveringIndex("reports_by_report_time")));
}

TEST_F(AttributionSqlQueryPlanTest, kSetReportTimeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSetReportTimeSql),
              ValueIs(UsesIndex("reports_by_report_time")));
}

TEST_F(AttributionSqlQueryPlanTest, kReadSourceToAttributeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kReadSourceToAttributeSql),
              ValueIs(UsesPrimaryKey()));
}

TEST_F(AttributionSqlQueryPlanTest, kGetActiveSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetActiveSourcesSql),
              ValueIs(UsesIndex("sources_by_expiry_time")));
}

TEST_F(AttributionSqlQueryPlanTest, kGetReportsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetReportsSql),
              ValueIs(UsesIndex("reports_by_report_time")));
}

TEST_F(AttributionSqlQueryPlanTest, kGetReportSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetReportSql),
              ValueIs(UsesPrimaryKey()));
}

TEST_F(AttributionSqlQueryPlanTest, kUpdateFailedReportSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kUpdateFailedReportSql),
              ValueIs(UsesPrimaryKey()));
}

TEST_F(AttributionSqlQueryPlanTest,
       kDeleteEventLevelReportsForDestinationLimitSql) {
  EXPECT_THAT(
      GetPlan(attribution_queries::kDeletePendingEventLevelReportsForSourceSql),
      ValueIs(UsesIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest,
       kDeleteAggregatableReportsForDestinationLimitSql) {
  EXPECT_THAT(GetPlan(attribution_queries::
                          kDeleteAggregatableReportsForDestinationLimitSql),
              ValueIs(UsesCoveringIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitAttributionAllowedSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kRateLimitAttributionAllowedSql),
              ValueIs(UsesIndex("rate_limit_reporting_origin_idx",
                                {"scope", "destination_site", "source_site"})));
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitSourceAllowedSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kRateLimitSourceAllowedSql),
              ValueIs(UsesIndex("rate_limit_reporting_origin_idx",
                                {"scope", "source_site"})));
}

TEST_F(AttributionSqlQueryPlanTest,
       kRateLimitSourceAllowedDestinationRateLimitSql) {
  EXPECT_THAT(
      GetPlan(
          attribution_queries::kRateLimitSourceAllowedDestinationRateLimitSql),
      ValueIs(UsesIndex("rate_limit_reporting_origin_idx",
                        {"scope", "source_site"})));
}

TEST_F(AttributionSqlQueryPlanTest,
       kRateLimitSourceAllowedDestinationPerDayRateLimitSql) {
  EXPECT_THAT(GetPlan(attribution_queries::
                          kRateLimitSourceAllowedDestinationPerDayRateLimitSql),
              ValueIs(UsesIndex("rate_limit_reporting_origin_idx",
                                {"scope", "source_site"})));
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitSourceReportingOriginsBySiteSql) {
  EXPECT_THAT(
      GetPlan(
          attribution_queries::kRateLimitSelectSourceReportingOriginsBySiteSql),
      ValueIs(UsesIndex("rate_limit_reporting_origin_idx",
                        {"scope", "source_site"})));
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitSelectSourceReportingOriginsSql) {
  EXPECT_THAT(
      GetPlan(attribution_queries::kRateLimitSelectSourceReportingOriginsSql),
      ValueIs(UsesIndex("rate_limit_reporting_origin_idx",
                        {"scope", "destination_site", "source_site"})));
}

TEST_F(AttributionSqlQueryPlanTest,
       kRateLimitSelectAttributionReportingOriginsSql) {
  EXPECT_THAT(
      GetPlan(
          attribution_queries::kRateLimitSelectAttributionReportingOriginsSql),
      ValueIs(UsesIndex("rate_limit_reporting_origin_idx",
                        {"scope", "destination_site", "source_site"})));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteRateLimitRangeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteRateLimitRangeSql),
              ValueIs(AllOf(UsesIndex("rate_limit_time_idx"),
                            UsesIndex("rate_limit_reporting_origin_idx"))));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectRateLimitsForDeletionSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSelectRateLimitsForDeletionSql),
              ValueIs(AllOf(UsesIndex("rate_limit_time_idx"),
                            UsesIndex("rate_limit_reporting_origin_idx"))));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteExpiredRateLimitsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteExpiredRateLimitsSql),
              ValueIs(UsesIndex("rate_limit_time_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteRateLimitsBySourceIdSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteRateLimitsBySourceIdSql),
              ValueIs(UsesIndex("rate_limit_source_id_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kDeactivateForSourceDestinationLimitSql) {
  EXPECT_THAT(
      GetPlan(attribution_queries::kDeactivateForSourceDestinationLimitSql),
      ValueIs(UsesIndex("rate_limit_source_id_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteAttributionRateLimitByReportIdSql) {
  EXPECT_THAT(
      GetPlan(attribution_queries::kDeleteAttributionRateLimitByReportIdSql),
      ValueIs(UsesIndex("rate_limit_report_id_idx")));
}

TEST_F(AttributionSqlQueryPlanTest,
       kAggregatableDebugReportAllowedForRateLimitSql) {
  EXPECT_THAT(
      GetPlan(
          attribution_queries::kAggregatableDebugReportAllowedForRateLimitSql),
      ValueIs(UsesIndex("aggregatable_debug_rate_limits_context_site_idx")));
}

TEST_F(AttributionSqlQueryPlanTest,
       kDeleteExpiredAggregatableDebugRateLimitsSql) {
  EXPECT_THAT(
      GetPlan(
          attribution_queries::kDeleteExpiredAggregatableDebugRateLimitsSql),
      ValueIs(UsesIndex("aggregatable_debug_rate_limits_time_idx")));
}

TEST_F(AttributionSqlQueryPlanTest,
       kSelectAggregatableDebugRateLimitsForDeletionSql) {
  EXPECT_THAT(GetPlan(attribution_queries::
                          kSelectAggregatableDebugRateLimitsForDeletionSql),
              ValueIs(UsesIndex("aggregatable_debug_rate_limits_time_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteAggregatableDebugRateLimitRangeSql) {
  EXPECT_THAT(
      GetPlan(attribution_queries::kDeleteAggregatableDebugRateLimitRangeSql),
      ValueIs(UsesIndex("aggregatable_debug_rate_limits_time_idx")));
}

}  // namespace
}  // namespace content
