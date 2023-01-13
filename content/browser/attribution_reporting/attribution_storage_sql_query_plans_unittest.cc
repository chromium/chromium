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
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
namespace {

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
    EXPECT_TRUE(plan.has_value());
    return *plan;
  }

 protected:
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<SqlQueryPlanExplainer> explainer_;
};

TEST_F(AttributionSqlQueryPlanTest, kMinPrioritySql) {
  SqlQueryPlan plan = GetPlan(attribution_queries::kMinPrioritySql);
  auto matcher = SqlIndexMatcher("event_level_reports_by_source_id");
  EXPECT_TRUE(plan.UsesIndex(matcher)) << plan;
}

TEST_F(AttributionSqlQueryPlanTest, kGetMatchingSourcesSql) {
  SqlQueryPlan plan = GetPlan(attribution_queries::kGetMatchingSourcesSql);
  auto matcher = SqlIndexMatcher("sources_by_expiry_time");
  EXPECT_TRUE(plan.UsesIndex(matcher)) << plan;
}

TEST_F(AttributionSqlQueryPlanTest, kSelectExpiredSourcesSql) {
  SqlQueryPlan plan = GetPlan(attribution_queries::kSelectExpiredSourcesSql);
  EXPECT_TRUE(plan.UsesIndex(SqlIndexMatcher("sources_by_expiry_time")
                                 .set_type(SqlIndexMatcher::Type::kCovering)))
      << plan;
  EXPECT_TRUE(
      plan.UsesIndex(SqlIndexMatcher("event_level_reports_by_source_id")))
      << plan;
  EXPECT_TRUE(plan.UsesIndex(SqlIndexMatcher("aggregate_source_id_idx")))
      << plan;
}

TEST_F(AttributionSqlQueryPlanTest, kSelectInactiveSourcesSql) {
  SqlQueryPlan plan = GetPlan(attribution_queries::kSelectInactiveSourcesSql);
  EXPECT_TRUE(plan.UsesIndex(
      SqlIndexMatcher("sources_by_active_destination_site_reporting_origin")
          .set_type(SqlIndexMatcher::Type::kCovering)
          .set_columns({"event_level_active", "aggregatable_active"})))
      << plan;
  EXPECT_TRUE(
      plan.UsesIndex(SqlIndexMatcher("event_level_reports_by_source_id")))
      << plan;
  EXPECT_TRUE(plan.UsesIndex(SqlIndexMatcher("aggregate_source_id_idx")))
      << plan;
}

TEST_F(AttributionSqlQueryPlanTest, kScanCandidateData) {
  SqlQueryPlan plan = GetPlan(attribution_queries::kScanCandidateData,
                              SqlFullScanReason::kNotOptimized);
  EXPECT_TRUE(
      plan.UsesIndex(SqlIndexMatcher("event_level_reports_by_source_id")))
      << plan;
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteVestigialConversionSql) {
  SqlQueryPlan plan =
      GetPlan(attribution_queries::kDeleteVestigialConversionSql);
  EXPECT_TRUE(
      plan.UsesIndex(SqlIndexMatcher("event_level_reports_by_source_id")))
      << plan;
}

TEST_F(AttributionSqlQueryPlanTest, kCountSourcesSql) {
  SqlQueryPlan plan = GetPlan(attribution_queries::kCountSourcesSql);
  EXPECT_TRUE(
      plan.UsesIndex(SqlIndexMatcher("active_sources_by_source_origin")))
      << plan;
}
TEST_F(AttributionSqlQueryPlanTest, kDedupKeySql) {
  SqlQueryPlan plan = GetPlan(attribution_queries::kDedupKeySql);
  EXPECT_TRUE(plan.UsesIndex(
      SqlIndexMatcher().set_type(SqlIndexMatcher::Type::kPrimaryKey)))
      << plan;
}

TEST_F(AttributionSqlQueryPlanTest, kScanCandidateDataAggregatable) {
  SqlQueryPlan plan =
      GetPlan(attribution_queries::kScanCandidateDataAggregatable,
              SqlFullScanReason::kNotOptimized);
  EXPECT_TRUE(plan.UsesIndex(SqlIndexMatcher("aggregate_source_id_idx")))
      << plan;
}

TEST_F(AttributionSqlQueryPlanTest, kDeleteAggregationsSql) {
  SqlQueryPlan plan = GetPlan(attribution_queries::kDeleteAggregationsSql);
  EXPECT_TRUE(plan.UsesIndex(SqlIndexMatcher("aggregate_source_id_idx")
                                 .set_type(SqlIndexMatcher::Type::kCovering)))
      << plan;
}

}  // namespace
}  // namespace content
