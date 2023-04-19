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
              UsesIndex("reports_by_source_id_report_type"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetMatchingSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetMatchingSourcesSql),
              UsesIndex("sources_by_expiry_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectExpiredSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSelectExpiredSourcesSql),
              AllOf(UsesCoveringIndex("sources_by_expiry_time"),
                    UsesIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest, kSelectInactiveSourcesSql) {
  EXPECT_THAT(
      GetPlan(attribution_queries::kSelectInactiveSourcesSql),
      AllOf(UsesCoveringIndex("sources_by_active_reporting_origin",
                              {"event_level_active", "aggregatable_active"}),
            UsesIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest, kScanSourcesData) {
  EXPECT_THAT(GetPlan(attribution_queries::kScanSourcesData),
              UsesIndex("sources_by_source_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kScanReportsData) {
  EXPECT_THAT(GetPlan(attribution_queries::kScanReportsData),
              UsesIndex("reports_by_trigger_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteVestigialConversionSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDeleteVestigialConversionSql),
              UsesIndex("reports_by_source_id_report_type"));
}

TEST_F(AttributionSqlQueryPlanTest, kCountSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kCountSourcesSql),
              UsesIndex("active_sources_by_source_origin"));
}

TEST_F(AttributionSqlQueryPlanTest, kDedupKeySql) {
  EXPECT_THAT(GetPlan(attribution_queries::kDedupKeySql), UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kGetSourcesDataKeysSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetSourcesDataKeysSql,
                      SqlFullScanReason::kIntentional),
              UsesCoveringIndex("sources_by_active_reporting_origin"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetNullReportsDataKeysSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetNullReportsDataKeysSql,
                      SqlFullScanReason::kNotOptimized),
              UsesIndex("reports_by_reporting_origin"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetRateLimitDataKeysSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetRateLimitDataKeysSql,
                      SqlFullScanReason::kIntentional),
              UsesCoveringIndex("rate_limit_source_site_reporting_origin_idx"));
}

TEST_F(AttributionSqlQueryPlanTest, kCountReportsForDestinationSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kCountReportsForDestinationSql),
              AllOf(UsesCoveringIndex("sources_by_destination_site"),
                    UsesIndex("reports_by_source_id_report_type")));
}

TEST_F(AttributionSqlQueryPlanTest, kNextReportTimeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kNextReportTimeSql),
              UsesCoveringIndex("reports_by_report_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kSetReportTimeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kSetReportTimeSql),
              UsesIndex("reports_by_report_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kReadSourceToAttributeSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kReadSourceToAttributeSql),
              UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kGetActiveSourcesSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetActiveSourcesSql),
              UsesIndex("sources_by_expiry_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetReportsSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetReportsSql),
              UsesIndex("reports_by_report_time"));
}

TEST_F(AttributionSqlQueryPlanTest, kGetReportSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kGetReportSql), UsesPrimaryKey());
}

TEST_F(AttributionSqlQueryPlanTest, kUpdateFailedReportSql) {
  EXPECT_THAT(GetPlan(attribution_queries::kUpdateFailedReportSql),
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
