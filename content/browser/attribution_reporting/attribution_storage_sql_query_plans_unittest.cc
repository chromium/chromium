// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/sql_queries.h"
#include "content/browser/attribution_reporting/sql_query_plan_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
namespace {

using ::testing::AllOf;

class AttributionSqlQueryPlanTest : public testing::Test {
 public:
  AttributionSqlQueryPlanTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    std::unique_ptr<AttributionStorage> storage =
        std::make_unique<AttributionStorageSql>(
            temp_directory_.GetPath(),
            std::make_unique<ConfigurableStorageDelegate>());

    // Make sure lazy initialization happens by adding a record to the db, but
    // then ensure the database is closed so the sqlite_dev_shell can read it.
    storage->StoreSource(SourceBuilder().Build());
    storage.reset();
    explainer_ = std::make_unique<SqlQueryPlanExplainer>(
        temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions")));
  }

  // Helper method to make tests as readable as possible.
  SqlQueryPlan GetPlan(
      std::string query,
      absl::optional<SqlFullScanReason> reason = absl::nullopt) {
    auto plan = explainer_->GetPlan(std::move(query), reason);
    EXPECT_TRUE(plan.has_value()) << plan.error();
    return *plan;
  }

 protected:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<SqlQueryPlanExplainer> explainer_;
};

TEST_F(AttributionSqlQueryPlanTest, kMinPrioritySql) {
  EXPECT_THAT(GetPlan(attribution_queries::kMinPrioritySql),
              UsesIndex("event_level_reports_by_source_id"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetMatchingSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetMatchingSourcesSql),
              UsesIndex("sources_by_expiry_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectExpiredSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSelectExpiredSourcesSql),
              AllOf(UsesCoveringIndex("sources_by_expiry_time"),
                    UsesIndex("event_level_reports_by_source_id"),
                    UsesIndex("aggregate_source_id_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectInactiveSourcesSql) {
  EXPECT_THAT(
      GetPlan(attribution_queries::kSelectInactiveSourcesSql),
      AllOf(UsesCoveringIndex("sources_by_active_reporting_origin",
                              {"event_level_active", "aggregatable_active"}),
            UsesIndex("event_level_reports_by_source_id"),
            UsesIndex("aggregate_source_id_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kScanCandidateData) {
  EXPECT_THAT(GetPlan(attribution_queries::kScanCandidateData,
                      SqlFullScanReason::kNotOptimized),
              UsesIndex("event_level_reports_by_source_id"));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteVestigialConversionSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteVestigialConversionSql),
              UsesIndex("event_level_reports_by_source_id"));
}

TEST_F(AttributionSqlQueryPlanTest, kCountSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kCountSourcesSql),
              UsesIndex("active_sources_by_source_origin"));
}

TEST_F(AttributionSqlQueryPlanTest, kDedupKeySql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDedupKeySql), UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kScanCandidateDataAggregatable) {
  EXPECT_THAT(GetPlan(attribution_queries::kScanCandidateDataAggregatable,
                      SqlFullScanReason::kNotOptimized),
              UsesIndex("aggregate_source_id_idx"));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteAggregationsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteAggregationsSql),
              UsesCoveringIndex("aggregate_source_id_idx"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetContributionsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetContributionsSql),
              UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kGetSourcesDataKeysSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetSourcesDataKeysSql,
                      SqlFullScanReason::kIntentional),
              UsesCoveringIndex("sources_by_active_reporting_origin"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetRateLimitDataKeysSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetRateLimitDataKeysSql,
                      SqlFullScanReason::kIntentional),
              UsesCoveringIndex("rate_limit_source_site_reporting_origin_idx"));
}

TEST_F(AttributionSqlQueryPlanTest, kCountEventLevelReportsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kCountEventLevelReportsSql),
              AllOf(UsesCoveringIndex("sources_by_destination_site"),
                    UsesCoveringIndex("event_level_reports_by_source_id")));
}

TEST_F(AttributionSqlQueryPlanTest, kCountAggregatableReportsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kCountAggregatableReportsSql),
              AllOf(UsesCoveringIndex("sources_by_destination_site"),
                    UsesCoveringIndex("aggregate_source_id_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kNextEventLevelReportTimeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kNextEventLevelReportTimeSql),
              UsesCoveringIndex("event_level_reports_by_report_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kNextAggregatableReportTimeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kNextAggregatableReportTimeSql),
              UsesCoveringIndex("aggregate_report_time_idx"));
}

TEST_F(AttributionSqlQueryPlanTest, kSetEventLevelReportTimeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSetEventLevelReportTimeSql),
              UsesIndex("event_level_reports_by_report_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kSetAggregatableReportTimeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSetAggregatableReportTimeSql),
              UsesIndex("aggregate_report_time_idx"));
}

TEST_F(AttributionSqlQueryPlanTest, kReadSourceToAttributeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kReadSourceToAttributeSql),
              UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kGetActiveSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetActiveSourcesSql),
              UsesIndex("sources_by_expiry_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetEventLevelReportsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetEventLevelReportsSql),
              UsesIndex("event_level_reports_by_report_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetEventLevelReportSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetEventLevelReportSql),
              UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kGetAggregatableReportsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetAggregatableReportsSql),
              UsesIndex("aggregate_report_time_idx"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetAggregatableReportSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetAggregatableReportSql),
              UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kUpdateFailedEventLevelReportSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kUpdateFailedEventLevelReportSql),
              UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kUpdateFailedAggregatableReportSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kUpdateFailedAggregatableReportSql),
              UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitAttributionAllowedSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kRateLimitAttributionAllowedSql),
              UsesIndex("rate_limit_reporting_origin_idx",
                        {"scope", "destination_site", "source_site"}));
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitSourceAllowedSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kRateLimitSourceAllowedSql),
              UsesIndex("rate_limit_source_site_reporting_origin_idx",
                        {"scope", "source_site", "reporting_origin"}));
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitSelectReportingOriginsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kRateLimitSelectReportingOriginsSql),
              UsesIndex("rate_limit_reporting_origin_idx",
                        {"scope", "destination_site", "source_site"}));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteRateLimitRangeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteRateLimitRangeSql),
              AllOf(UsesIndex("rate_limit_time_idx"),
                    UsesIndex("rate_limit_reporting_origin_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectRateLimitsForDeletionSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSelectRateLimitsForDeletionSql),
              AllOf(UsesIndex("rate_limit_time_idx"),
                    UsesIndex("rate_limit_reporting_origin_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteExpiredRateLimitsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteExpiredRateLimitsSql),
              UsesIndex("rate_limit_time_idx"));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteRateLimitsBySourceIdSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteRateLimitsBySourceIdSql),
              UsesIndex("rate_limit_source_id_idx"));
}

}  // namespace
}  // namespace content
