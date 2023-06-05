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
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
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
  base::expected<SqlQueryPlan, SqlQueryPlanExplainer::Error> GetPlan(
      std::string query,
      absl::optional<SqlFullScanReason> reason = absl::nullopt) {
    return explainer_->GetPlan(std::move(query), reason);
  }

 protected:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<SqlQueryPlanExplainer> explainer_;
};

TEST_F(AttributionSqlQueryPlanTest, kMinPrioritySql) {
  const auto plan = GetPlan(attribution_queries::kMinPrioritySql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("reports_by_source_id_report_type"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetMatchingSourcesSql) {
  const auto plan = GetPlan(attribution_queries::kGetMatchingSourcesSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("sources_by_expiry_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectExpiredSourcesSql) {
  const auto plan = GetPlan(attribution_queries::kSelectExpiredSourcesSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(),
              AllOf(UsesCoveringIndex("sources_by_expiry_time"),
                    UsesIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectInactiveSourcesSql) {
  const auto plan = GetPlan(attribution_queries::kSelectInactiveSourcesSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(
      plan.value(),
      AllOf(UsesCoveringIndex("sources_by_active_reporting_origin",
                              {"event_level_active", "aggregatable_active"}),
            UsesIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest, kScanSourcesData) {
  const auto plan = GetPlan(attribution_queries::kScanSourcesData);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("sources_by_source_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kScanReportsData) {
  const auto plan = GetPlan(attribution_queries::kScanReportsData);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("reports_by_trigger_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteVestigialConversionSql) {
  const auto plan = GetPlan(attribution_queries::kDeleteVestigialConversionSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("reports_by_source_id_report_type"));
}

TEST_F(AttributionSqlQueryPlanTest, kCountSourcesSql) {
  const auto plan = GetPlan(attribution_queries::kCountSourcesSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("active_sources_by_source_origin"));
}

TEST_F(AttributionSqlQueryPlanTest, kDedupKeySql) {
  const auto plan = GetPlan(attribution_queries::kDedupKeySql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kGetSourcesDataKeysSql) {
  const auto plan = GetPlan(attribution_queries::kGetSourcesDataKeysSql,
                            SqlFullScanReason::kIntentional);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(),
              UsesCoveringIndex("sources_by_active_reporting_origin"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetNullReportsDataKeysSql) {
  const auto plan = GetPlan(attribution_queries::kGetNullReportsDataKeysSql,
                            SqlFullScanReason::kNotOptimized);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("reports_by_reporting_origin"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetRateLimitDataKeysSql) {
  const auto plan = GetPlan(attribution_queries::kGetRateLimitDataKeysSql,
                            SqlFullScanReason::kIntentional);
  ASSERT_TRUE(plan.has_value());
}

TEST_F(AttributionSqlQueryPlanTest, kCountReportsForDestinationSql) {
  const auto plan =
      GetPlan(attribution_queries::kCountReportsForDestinationSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(),
              AllOf(UsesCoveringIndex("sources_by_destination_site"),
                    UsesIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest, kNextReportTimeSql) {
  const auto plan = GetPlan(attribution_queries::kNextReportTimeSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesCoveringIndex("reports_by_report_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kSetReportTimeSql) {
  const auto plan = GetPlan(attribution_queries::kSetReportTimeSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("reports_by_report_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kReadSourceToAttributeSql) {
  const auto plan = GetPlan(attribution_queries::kReadSourceToAttributeSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kGetActiveSourcesSql) {
  const auto plan = GetPlan(attribution_queries::kGetActiveSourcesSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("sources_by_expiry_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetReportsSql) {
  const auto plan = GetPlan(attribution_queries::kGetReportsSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("reports_by_report_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetReportSql) {
  const auto plan = GetPlan(attribution_queries::kGetReportSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kUpdateFailedReportSql) {
  const auto plan = GetPlan(attribution_queries::kUpdateFailedReportSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitAttributionAllowedSql) {
  const auto plan =
      GetPlan(attribution_queries::kRateLimitAttributionAllowedSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(),
              UsesIndex("rate_limit_reporting_origin_idx",
                        {"scope", "destination_site", "source_site"}));
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitSourceAllowedSql) {
  const auto plan = GetPlan(attribution_queries::kRateLimitSourceAllowedSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(),
              UsesIndex("rate_limit_source_site_reporting_site_idx",
                        {"source_site", "reporting_site"}));
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitSourceReportingOriginsBySiteSql) {
  const auto plan = GetPlan(
      attribution_queries::kRateLimitSelectSourceReportingOriginsBySiteSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(),
              UsesIndex("rate_limit_source_site_reporting_site_idx",
                        {"source_site", "reporting_site"}));
}

TEST_F(AttributionSqlQueryPlanTest, kRateLimitSelectReportingOriginsSql) {
  const auto plan =
      GetPlan(attribution_queries::kRateLimitSelectReportingOriginsSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(),
              UsesIndex("rate_limit_reporting_origin_idx",
                        {"scope", "destination_site", "source_site"}));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteRateLimitRangeSql) {
  const auto plan = GetPlan(attribution_queries::kDeleteRateLimitRangeSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(),
              AllOf(UsesIndex("rate_limit_time_idx"),
                    UsesIndex("rate_limit_reporting_origin_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectRateLimitsForDeletionSql) {
  const auto plan =
      GetPlan(attribution_queries::kSelectRateLimitsForDeletionSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(),
              AllOf(UsesIndex("rate_limit_time_idx"),
                    UsesIndex("rate_limit_reporting_origin_idx")));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteExpiredRateLimitsSql) {
  const auto plan = GetPlan(attribution_queries::kDeleteExpiredRateLimitsSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("rate_limit_time_idx"));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteRateLimitsBySourceIdSql) {
  const auto plan =
      GetPlan(attribution_queries::kDeleteRateLimitsBySourceIdSql);
  ASSERT_TRUE(plan.has_value());
  EXPECT_THAT(plan.value(), UsesIndex("rate_limit_source_id_idx"));
}

}  // namespace
}  // namespace content
