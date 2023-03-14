// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <stdint.h>

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;

struct AggregatableReportMetadataRecord {
  int64_t aggregation_id;
  int64_t source_id;
  base::Time trigger_time;
  absl::optional<int64_t> debug_key;
  std::string external_report_id;
  base::Time report_time;
  int failed_send_attempts = 0;
  base::Time initial_report_time;
  int aggregation_coordinator = static_cast<int>(
      ::aggregation_service::mojom::AggregationCoordinator::kDefault);
  absl::optional<std::string> attestation_token;
  std::string destination_origin = "https://destination.test";
};

struct AggregatableContributionRecord {
  int64_t contribution_id;
  int64_t aggregation_id;
  int64_t key_high_bits;
  int64_t key_low_bits;
  int64_t value;
};

std::string CreateSerializedFilterData(
    const attribution_reporting::FilterValues& filter_values) {
  proto::AttributionFilterData msg;

  for (const auto& [filter, values] : filter_values) {
    proto::AttributionFilterValues filter_values_msg;
    for (std::string value : values) {
      filter_values_msg.mutable_values()->Add(std::move(value));
    }
    (*msg.mutable_filter_values())[filter] = std::move(filter_values_msg);
  }

  std::string string;
  bool success = msg.SerializeToString(&string);
  CHECK(success);
  return string;
}

class AttributionStorageSqlTest : public testing::Test {
 public:
  AttributionStorageSqlTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void OpenDatabase() {
    storage_.reset();
    auto delegate = std::make_unique<ConfigurableStorageDelegate>();
    delegate_ = delegate.get();
    storage_ = std::make_unique<AttributionStorageSql>(
        temp_directory_.GetPath(), std::move(delegate));
  }

  void CloseDatabase() { storage_.reset(); }

  void AddReportToStorage() {
    storage_->StoreSource(SourceBuilder().Build());
    storage_->MaybeCreateAndStoreReport(DefaultTrigger());
  }

  void ExpectAllTablesEmpty() {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    static constexpr const char* kTables[] = {
        "event_level_reports",
        "sources",
        "source_destinations",
        "rate_limits",
        "dedup_keys",
        "aggregatable_report_metadata",
        "aggregatable_contributions",
    };

    for (const char* table : kTables) {
      size_t rows;
      sql::test::CountTableRows(&raw_db, table, &rows);
      EXPECT_EQ(0u, rows) << table;
    }
  }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(FILE_PATH_LITERAL("Conversions"));
  }

  AttributionStorage* storage() { return storage_.get(); }

  ConfigurableStorageDelegate* delegate() { return delegate_; }

  void ExpectImpressionRows(size_t expected) {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));
    size_t rows;
    sql::test::CountTableRows(&raw_db, "sources", &rows);
    EXPECT_EQ(expected, rows);
  }

  void ExpectAggregatableContributionsRows(size_t expected) {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));
    size_t rows;
    sql::test::CountTableRows(&raw_db, "aggregatable_contributions", &rows);
    EXPECT_EQ(expected, rows);
  }

  AttributionTrigger::EventLevelResult MaybeCreateAndStoreEventLevelReport(
      const AttributionTrigger& conversion) {
    return storage_->MaybeCreateAndStoreReport(conversion).event_level_status();
  }

  void StoreAggregatableReportMetadata(
      const AggregatableReportMetadataRecord& record) {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(db_path()));

    static constexpr char kStoreMetadataSql[] =
        "INSERT INTO aggregatable_report_metadata "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?)";
    sql::Statement statement(raw_db.GetUniqueStatement(kStoreMetadataSql));
    statement.BindInt64(0, record.aggregation_id);
    statement.BindInt64(1, record.source_id);
    statement.BindTime(2, record.trigger_time);
    if (record.debug_key) {
      statement.BindInt64(3, *record.debug_key);
    } else {
      statement.BindNull(3);
    }
    statement.BindString(4, record.external_report_id);
    statement.BindTime(5, record.report_time);
    statement.BindInt(6, record.failed_send_attempts);
    statement.BindTime(7, record.initial_report_time);
    statement.BindInt(8, record.aggregation_coordinator);
    if (record.attestation_token.has_value()) {
      statement.BindString(9, record.attestation_token.value());
    } else {
      statement.BindNull(9);
    }
    statement.BindString(10, record.destination_origin);
    ASSERT_TRUE(statement.Run());
  }

  void StoreAggregatableContribution(
      const AggregatableContributionRecord& record) {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(db_path()));

    static constexpr char kStoreContributionSql[] =
        "INSERT INTO aggregatable_contributions "
        "VALUES(?,?,?,?,?)";
    sql::Statement statement(raw_db.GetUniqueStatement(kStoreContributionSql));
    statement.BindInt64(0, record.contribution_id);
    statement.BindInt64(1, record.aggregation_id);
    statement.BindInt64(2, record.key_high_bits);
    statement.BindInt64(3, record.key_low_bits);
    statement.BindInt64(4, record.value);
    ASSERT_TRUE(statement.Run());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_directory_;

 private:
  std::unique_ptr<AttributionStorage> storage_;
  raw_ptr<ConfigurableStorageDelegate> delegate_ = nullptr;
};

TEST_F(AttributionStorageSqlTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  base::HistogramTester histograms;

  OpenDatabase();
  CloseDatabase();

  // An unused AttributionStorageSql instance should not create the database.
  EXPECT_FALSE(base::PathExists(db_path()));

  // Operations which don't need to run on an empty database should not create
  // the database.
  OpenDatabase();
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
  CloseDatabase();

  EXPECT_FALSE(base::PathExists(db_path()));

  // DB init UMA should not be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 0);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);

  // Storing an impression should create and initialize the database.
  OpenDatabase();
  storage()->StoreSource(SourceBuilder().Build());
  CloseDatabase();

  // DB creation histograms should be recorded.
  histograms.ExpectTotalCount("Conversions.Storage.CreationTime", 1);
  histograms.ExpectTotalCount("Conversions.Storage.MigrationTime", 0);

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [sources], [event_level_reports], [meta], [rate_limits], [dedup_keys],
    // [aggregatable_report_metadata], [aggregatable_contributions],
    // [source_destinations], [sqlite_sequence] (for AUTOINCREMENT support).
    EXPECT_EQ(9u, sql::test::CountSQLTables(&raw_db));

    // [conversion_domain_idx], [impression_expiry_idx],
    // [impression_origin_idx], [conversion_report_time_idx],
    // [conversion_impression_id_idx],
    // [rate_limit_source_site_reporting_origin_idx],
    // [rate_limit_reporting_origin_idx], [rate_limit_time_idx],
    // [rate_limit_impression_id_idx], [aggregate_source_id_idx],
    // [aggregate_trigger_time_idx], [aggregate_report_time_idx],
    // [sources_by_destination_site], and the meta table index.
    EXPECT_EQ(14u, sql::test::CountSQLIndices(&raw_db));
  }
}

TEST_F(AttributionStorageSqlTest, DatabaseReopened_DataPersisted) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(1));
  CloseDatabase();
  OpenDatabase();
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(1));
}

TEST_F(AttributionStorageSqlTest, CorruptDatabase_RecoveredOnOpen) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(1));
  CloseDatabase();

  // Corrupt the database.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(db_path()));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(SQLITE_CORRUPT);

  // Open that database and ensure that it does not fail.
  EXPECT_NO_FATAL_FAILURE(OpenDatabase());

  // TODO(crbug.com/1418026): The recovery process does not recover tables
  // without row IDs, causing no data to be returned here. Data recovery should
  // be addressed in a separate CL.
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(0));

  EXPECT_TRUE(expecter.SawExpectedErrors());
}

TEST_F(AttributionStorageSqlTest, VersionTooNew_RazesDB) {
  OpenDatabase();
  AddReportToStorage();
  ASSERT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(1));
  CloseDatabase();

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    sql::MetaTable meta;
    // The values here are irrelevant, as the meta table already exists.
    ASSERT_TRUE(meta.Init(&raw_db, /*version=*/1, /*compatible_version=*/1));

    ASSERT_TRUE(meta.SetVersionNumber(meta.GetVersionNumber() + 1));
    ASSERT_TRUE(meta.SetCompatibleVersionNumber(meta.GetVersionNumber() + 1));
  }

  // The DB should be razed because the version is too new.
  ASSERT_NO_FATAL_FAILURE(OpenDatabase());
  ASSERT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
}

TEST_F(AttributionStorageSqlTest,
       StoreAndRetrieveReportWithAttestation_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingTriggerAttestation);
  base::HistogramTester histograms;

  OpenDatabase();

  StorableSource source = TestAggregatableSourceProvider()
                              .GetBuilder()
                              .SetExpiry(base::Days(30))
                              .Build();
  storage()->StoreSource(source);

  auto trigger_attestation = network::TriggerAttestation::Create(
      /*token=*/"attestation-token", /*aggregatable_report_id=*/
      "55865da3-fb0e-4b71-965e-64fc4bf0a323");
  AttributionTrigger trigger = DefaultAggregatableTriggerBuilder()
                                   .SetAttestation(trigger_attestation)
                                   .Build();
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(trigger),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));
  histograms.ExpectUniqueSample(
      "Conversions.TriggerAttestation.ReportHasAttestation", true,
      /*expected_bucket_count=*/1);

  AttributionReport aggregatable_report =
      storage()->GetAttributionReports(base::Time::Max()).at(1);
  // Should create the report with the id from the trigger attestation.
  EXPECT_EQ(aggregatable_report.external_report_id(),
            trigger_attestation->aggregatable_report_id());

  // Should store the attestation token on the report.
  const auto* data =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &aggregatable_report.data());
  EXPECT_EQ(data->attestation_token.value(), trigger_attestation->token());

  CloseDatabase();
}

TEST_F(AttributionStorageSqlTest,
       StoreAndRetrieveReportWithoutAttestation_FeatureEnabled) {
  OpenDatabase();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      network::features::kAttributionReportingTriggerAttestation);
  base::HistogramTester histograms;

  StorableSource source = TestAggregatableSourceProvider()
                              .GetBuilder()
                              .SetExpiry(base::Days(30))
                              .Build();
  storage()->StoreSource(source);
  AttributionTrigger trigger = DefaultAggregatableTriggerBuilder().Build();
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(trigger),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));
  histograms.ExpectUniqueSample(
      "Conversions.TriggerAttestation.ReportHasAttestation", false,
      /*expected_bucket_count=*/1);

  AttributionReport aggregatable_report =
      storage()->GetAttributionReports(base::Time::Max()).at(1);

  const auto* data =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &aggregatable_report.data());
  EXPECT_FALSE(data->attestation_token.has_value());

  CloseDatabase();
}

TEST_F(
    AttributionStorageSqlTest,
    StoreAndRetrieveReportWithoutAttestation_FeatureDisabled_HasAttestationNotRecorded) {
  OpenDatabase();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      network::features::kAttributionReportingTriggerAttestation);
  base::HistogramTester histograms;

  StorableSource source = TestAggregatableSourceProvider()
                              .GetBuilder()
                              .SetExpiry(base::Days(30))
                              .Build();
  storage()->StoreSource(source);
  AttributionTrigger trigger = DefaultAggregatableTriggerBuilder().Build();
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(trigger),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));
  histograms.ExpectUniqueSample(
      "Conversions.TriggerAttestation.ReportHasAttestation", false,
      /*expected_bucket_count=*/0);

  AttributionReport aggregatable_report =
      storage()->GetAttributionReports(base::Time::Max()).at(1);

  const auto* data =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &aggregatable_report.data());
  EXPECT_FALSE(data->attestation_token.has_value());

  CloseDatabase();
}

// Create a source with three triggers and craft a query that will target all.
TEST_F(AttributionStorageSqlTest, ClearDataRangeMultipleReports) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = base::Time::Now();
  auto source = TestAggregatableSourceProvider()
                    .GetBuilder(start)
                    .SetExpiry(base::Days(30))
                    .Build();
  storage()->StoreSource(source);

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  // Use a time range that targets all triggers.
  storage()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(std::equal_to<blink::StorageKey>(),
                          blink::StorageKey::CreateFirstParty(
                              source.common_info().reporting_origin())));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation.Event", 3, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation.Aggregatable", 3, 1);
}

//  Create a source with two triggers resulting in two event-level reports (C1
//  and C2) and two aggregatable reports (A1 and A2). Craft a query that  will
//  target C2 and A2, which will in turn delete the source. We should ensure
//  that C1 and A1 are properly deleted (reports should not be stored
//  unattributed).
TEST_F(AttributionStorageSqlTest, ClearDataWithVestigialConversion) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = base::Time::Now();
  auto source = TestAggregatableSourceProvider()
                    .GetBuilder(start)
                    .SetExpiry(base::Days(30))
                    .Build();
  storage()->StoreSource(source);

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  // Use a time range that only intersects the last trigger.
  storage()->ClearData(
      base::Time::Now(), base::Time::Now(),
      base::BindRepeating(std::equal_to<blink::StorageKey>(),
                          blink::StorageKey::CreateFirstParty(
                              source.common_info().reporting_origin())));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation.Event", 2, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation.Aggregatable", 2, 1);
}

// Same as the above test, but with a null filter.
TEST_F(AttributionStorageSqlTest, ClearAllDataWithVestigialConversion) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = base::Time::Now();
  auto source = TestAggregatableSourceProvider()
                    .GetBuilder(start)
                    .SetExpiry(base::Days(30))
                    .Build();
  storage()->StoreSource(source);

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  // Use a time range that only intersects the last trigger.
  storage()->ClearData(base::Time::Now(), base::Time::Now(),
                       base::NullCallback());
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation.Event", 2, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation.Aggregatable", 2, 1);
}

// The max time range with a null filter should delete everything.
TEST_F(AttributionStorageSqlTest, DeleteEverything) {
  base::HistogramTester histograms;

  OpenDatabase();

  base::Time start = base::Time::Now();
  for (int i = 0; i < 10; i++) {
    auto source = TestAggregatableSourceProvider()
                      .GetBuilder(start)
                      .SetExpiry(base::Days(30))
                      .Build();
    storage()->StoreSource(source);
    task_environment_.FastForwardBy(base::Days(1));
  }

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()), IsEmpty());

  CloseDatabase();

  // Verify that everything is deleted.
  ExpectAllTablesEmpty();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionsDeletedInDataClearOperation", 1, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation.Event", 2, 1);
  histograms.ExpectUniqueSample(
      "Conversions.ReportsDeletedInDataClearOperation.Aggregatable", 2, 1);
}

TEST_F(AttributionStorageSqlTest, ClearData_KeepRateLimitData) {
  OpenDatabase();
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  CloseDatabase();
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));
    size_t impression_rows;
    sql::test::CountTableRows(&raw_db, "sources", &impression_rows);
    EXPECT_EQ(1u, impression_rows);

    size_t rate_limit_rows;
    sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
    EXPECT_EQ(2u, rate_limit_rows);
  }

  OpenDatabase();
  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback(),
                       /*delete_rate_limit_data=*/false);
  CloseDatabase();

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));
    size_t impression_rows;
    sql::test::CountTableRows(&raw_db, "sources", &impression_rows);
    EXPECT_EQ(0u, impression_rows);

    size_t rate_limit_rows;
    sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
    EXPECT_EQ(2u, rate_limit_rows);
  }
}

TEST_F(AttributionStorageSqlTest, DeleteAttributionDataByDataKey) {
  OpenDatabase();
  storage()->StoreSource(SourceBuilder().Build());

  std::vector keys = storage()->GetAllDataKeys();
  ASSERT_THAT(keys, SizeIs(1));

  storage()->DeleteByDataKey(keys[0]);

  CloseDatabase();

  sql::Database raw_db;
  ASSERT_TRUE(raw_db.Open(db_path()));
  {
    sql::Statement s(raw_db.GetUniqueStatement("SELECT * FROM sources"));
    ASSERT_FALSE(s.Step());
  }
}

TEST_F(AttributionStorageSqlTest, MaxSourcesPerOrigin) {
  OpenDatabase();
  delegate()->set_max_sources_per_origin(2);
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t impression_rows;
  sql::test::CountTableRows(&raw_db, "sources", &impression_rows);
  EXPECT_EQ(1u, impression_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(3u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest, MaxReportsPerDestination) {
  OpenDatabase();
  delegate()->set_max_reports_per_destination(
      AttributionReport::Type::kEventLevel, 2);
  storage()->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));
  EXPECT_EQ(
      AttributionTrigger::EventLevelResult::kNoCapacityForConversionDestination,
      MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  CloseDatabase();
  sql::Database raw_db;
  EXPECT_TRUE(raw_db.Open(db_path()));
  size_t conversion_rows;
  sql::test::CountTableRows(&raw_db, "event_level_reports", &conversion_rows);
  EXPECT_EQ(2u, conversion_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(3u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest, CantOpenDb_FailsSilentlyInRelease) {
  base::CreateDirectoryAndGetError(db_path(), nullptr);

  auto sql_storage = std::make_unique<AttributionStorageSql>(
      temp_directory_.GetPath(),
      std::make_unique<ConfigurableStorageDelegate>());
  sql_storage->set_ignore_errors_for_testing(true);

  std::unique_ptr<AttributionStorage> storage = std::move(sql_storage);

  // These calls should be no-ops.
  storage->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kNoMatchingImpressions,
            storage->MaybeCreateAndStoreReport(DefaultTrigger())
                .event_level_status());
}

TEST_F(AttributionStorageSqlTest, DatabaseDirDoesExist_CreateDirAndOpenDB) {
  // Give the storage layer a database directory that doesn't exist.
  std::unique_ptr<AttributionStorage> storage =
      std::make_unique<AttributionStorageSql>(
          temp_directory_.GetPath().Append(
              FILE_PATH_LITERAL("ConversionFolder/")),
          std::make_unique<ConfigurableStorageDelegate>());

  // The directory should be created, and the database opened.
  storage->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            storage->MaybeCreateAndStoreReport(DefaultTrigger())
                .event_level_status());
}

TEST_F(AttributionStorageSqlTest, DBinitializationSucceeds_HistogramRecorded) {
  base::HistogramTester histograms;

  OpenDatabase();
  storage()->StoreSource(SourceBuilder().Build());
  CloseDatabase();

  histograms.ExpectUniqueSample("Conversions.Storage.Sql.InitStatus2",
                                AttributionStorageSql::InitStatus::kSuccess, 1);
}

TEST_F(AttributionStorageSqlTest, MaxUint64StorageSucceeds) {
  constexpr uint64_t kMaxUint64 = std::numeric_limits<uint64_t>::max();

  OpenDatabase();

  // Ensure that reading and writing `uint64_t` fields via
  // `sql::Statement::ColumnInt64()` and `sql::Statement::BindInt64()` works
  // with the maximum value.

  const auto impression = SourceBuilder().SetSourceEventId(kMaxUint64).Build();
  storage()->StoreSource(impression);
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(SourceEventIdIs(kMaxUint64)));

  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(
                TriggerBuilder().SetDebugKey(kMaxUint64).Build()));

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()),
              ElementsAre(TriggerDebugKeyIs(kMaxUint64)));
}

TEST_F(AttributionStorageSqlTest, ImpressionNotExpired_NotDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, ImpressionExpired_Deleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

TEST_F(AttributionStorageSqlTest, ImpressionExpired_TooFrequent_NotDeleted) {
  OpenDatabase();

  delegate()->set_delete_expired_sources_frequency(base::Milliseconds(4));

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest,
       ExpiredImpressionWithPendingConversion_NotDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, TwoImpressionsOneExpired_OneDeleted) {
  OpenDatabase();

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(4)).Build());

  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest, ExpiredImpressionWithSentConversion_Deleted) {
  OpenDatabase();

  const base::TimeDelta kReportDelay = base::Milliseconds(5);
  delegate()->set_report_delay(kReportDelay);

  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Advance past the default report time.
  task_environment_.FastForwardBy(kReportDelay);

  std::vector<AttributionReport> reports =
      storage()->GetAttributionReports(base::Time::Now());
  EXPECT_THAT(reports, SizeIs(1));
  EXPECT_TRUE(storage()->DeleteReport(reports[0].ReportId()));
  // Store another impression to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

TEST_F(AttributionStorageSqlTest, DeleteAggregatableAttributionReport) {
  OpenDatabase();

  storage()->StoreSource(TestAggregatableSourceProvider().GetBuilder().Build());

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  std::vector<AttributionReport> reports =
      storage()->GetAttributionReports(base::Time::Max());

  EXPECT_THAT(
      reports,
      ElementsAre(
          ReportTypeIs(AttributionReport::Type::kEventLevel),
          ReportTypeIs(AttributionReport::Type::kAggregatableAttribution)));

  EXPECT_TRUE(storage()->DeleteReport(
      AttributionReport::AggregatableAttributionData::Id(1)));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
              ElementsAre(ReportTypeIs(AttributionReport::Type::kEventLevel)));

  CloseDatabase();

  ExpectAggregatableContributionsRows(0u);
}

TEST_F(AttributionStorageSqlTest,
       ExpiredSourceWithPendingAggregatableAttribution_NotDeleted) {
  OpenDatabase();

  storage()->StoreSource(TestAggregatableSourceProvider()
                             .GetBuilder()
                             .SetExpiry(base::Milliseconds(3))
                             .Build());

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  std::vector<AttributionReport> reports =
      storage()->GetAttributionReports(base::Time::Max());

  EXPECT_THAT(
      reports,
      ElementsAre(
          ReportTypeIs(AttributionReport::Type::kEventLevel),
          ReportTypeIs(AttributionReport::Type::kAggregatableAttribution)));

  EXPECT_TRUE(
      storage()->DeleteReport(AttributionReport::EventLevelData::Id(1)));

  task_environment_.FastForwardBy(base::Milliseconds(3));
  // Store another source to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(2u);
}

TEST_F(AttributionStorageSqlTest,
       ExpiredSourceWithSentAggregatableAttribution_Deleted) {
  OpenDatabase();

  storage()->StoreSource(TestAggregatableSourceProvider()
                             .GetBuilder()
                             .SetExpiry(base::Milliseconds(3))
                             .Build());

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  std::vector<AttributionReport> reports =
      storage()->GetAttributionReports(base::Time::Max());

  EXPECT_THAT(
      reports,
      ElementsAre(
          ReportTypeIs(AttributionReport::Type::kEventLevel),
          ReportTypeIs(AttributionReport::Type::kAggregatableAttribution)));

  task_environment_.FastForwardBy(base::Milliseconds(3));

  EXPECT_TRUE(storage()->DeleteReport(reports[0].ReportId()));
  EXPECT_TRUE(storage()->DeleteReport(reports[1].ReportId()));

  // Store another source to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

TEST_F(AttributionStorageSqlTest,
       InvalidSourceOriginOrSite_FailsDeserialization) {
  const struct {
    const char* sql;
    const char* value;
  } kTestCases[] = {
      {
          .sql = "UPDATE sources SET source_origin=?",
          .value = "http://insecure.test",
      },
      {
          .sql = "UPDATE sources SET reporting_origin=?",
          .value = "http://insecure.test",
      },
      {
          .sql = "UPDATE source_destinations SET destination_site=?",
          .value = "wss://a.test",
      },
  };

  for (const auto& test_case : kTestCases) {
    OpenDatabase();

    SourceBuilder source_builder;
    storage()->StoreSource(
        source_builder.SetExpiry(base::Milliseconds(3)).Build());
    ASSERT_THAT(storage()->GetActiveSources(), SizeIs(1)) << test_case.sql;

    CloseDatabase();

    {
      sql::Database raw_db;
      ASSERT_TRUE(raw_db.Open(db_path())) << test_case.sql;

      sql::Statement statement(raw_db.GetUniqueStatement(test_case.sql));
      statement.BindString(0, test_case.value);
      ASSERT_TRUE(statement.Run()) << test_case.sql;
    }

    OpenDatabase();
    ASSERT_THAT(storage()->GetActiveSources(), IsEmpty()) << test_case.sql;
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();
  }
}

TEST_F(AttributionStorageSqlTest,
       InvalidAggregatableValue_FailsDeserialization) {
  const struct {
    int64_t value;
    int64_t budget;
    bool valid;
  } kTestCases[] = {
      {-1, 10, false},
      {0, 10, false},
      {10, 10, true},
      {11, 10, false},
      {std::numeric_limits<uint32_t>::max(),
       std::numeric_limits<int64_t>::max(), true},
      {std::numeric_limits<uint32_t>::max() + 1,
       std::numeric_limits<int64_t>::max(), false},
  };

  for (auto test_case : kTestCases) {
    OpenDatabase();
    storage()->StoreSource(SourceBuilder().Build());
    auto sources = storage()->GetActiveSources();
    ASSERT_THAT(sources, SizeIs(1));
    CloseDatabase();

    StoreAggregatableReportMetadata(AggregatableReportMetadataRecord{
        .aggregation_id = 1,
        .source_id = *sources.front().source_id(),
        .external_report_id = DefaultExternalReportID().AsLowercaseString(),
    });

    StoreAggregatableContribution(
        AggregatableContributionRecord{.contribution_id = 1,
                                       .aggregation_id = 1,
                                       .key_high_bits = 0,
                                       .key_low_bits = 0,
                                       .value = test_case.value});

    OpenDatabase();
    delegate()->set_aggregatable_budget_per_source(test_case.budget);
    EXPECT_THAT(
        storage()->GetAttributionReports(/*max_report_time=*/base::Time::Max()),
        SizeIs(test_case.valid))
        << test_case.value << "," << test_case.budget;
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();
  }
}

TEST_F(AttributionStorageSqlTest, CreateReport_DeletesUnattributedSources) {
  OpenDatabase();
  storage()->StoreSource(SourceBuilder().Build());
  storage()->StoreSource(SourceBuilder().Build());
  CloseDatabase();

  ExpectImpressionRows(2);

  OpenDatabase();
  MaybeCreateAndStoreEventLevelReport(DefaultTrigger());
  CloseDatabase();

  ExpectImpressionRows(1);
}

TEST_F(AttributionStorageSqlTest, CreateReport_DeactivatesAttributedSources) {
  OpenDatabase();
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(1).SetPriority(1).Build());
  MaybeCreateAndStoreEventLevelReport(DefaultTrigger());
  storage()->StoreSource(
      SourceBuilder().SetSourceEventId(2).SetPriority(2).Build());
  MaybeCreateAndStoreEventLevelReport(DefaultTrigger());
  CloseDatabase();

  ExpectImpressionRows(2);
}

// Tests that a "source_type" filter present in the serialized data is
// removed.
TEST_F(AttributionStorageSqlTest,
       DeserializeFilterData_RemovesSourceTypeFilter) {
  {
    OpenDatabase();
    storage()->StoreSource(SourceBuilder().Build());
    CloseDatabase();
  }

  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(db_path()));

    static constexpr char kUpdateSql[] = "UPDATE sources SET filter_data=?";
    sql::Statement statement(raw_db.GetUniqueStatement(kUpdateSql));
    statement.BindBlob(0, CreateSerializedFilterData(
                              {{"source_type", {"abc"}}, {"x", {"y"}}}));
    ASSERT_TRUE(statement.Run());
  }

  OpenDatabase();

  std::vector<StoredSource> sources = storage()->GetActiveSources();
  ASSERT_EQ(sources.size(), 1u);
  ASSERT_THAT(sources.front().filter_data().filter_values(),
              ElementsAre(Pair("x", ElementsAre("y"))));
}

TEST_F(AttributionStorageSqlTest,
       InvalidAggregationCoordinator_FailsDeserialization) {
  const struct {
    int aggregation_coordinator;
    bool valid;
  } kTestCases[] = {
      {0, true},
      {1, false},
  };

  for (auto test_case : kTestCases) {
    OpenDatabase();
    storage()->StoreSource(SourceBuilder().Build());
    auto sources = storage()->GetActiveSources();
    ASSERT_THAT(sources, SizeIs(1));
    CloseDatabase();

    StoreAggregatableReportMetadata(AggregatableReportMetadataRecord{
        .aggregation_id = 1,
        .source_id = *sources.front().source_id(),
        .external_report_id = DefaultExternalReportID().AsLowercaseString(),
        .aggregation_coordinator = test_case.aggregation_coordinator,
    });

    StoreAggregatableContribution(
        AggregatableContributionRecord{.contribution_id = 1,
                                       .aggregation_id = 1,
                                       .key_high_bits = 0,
                                       .key_low_bits = 0,
                                       .value = 1});

    OpenDatabase();
    EXPECT_THAT(
        storage()->GetAttributionReports(/*max_report_time=*/base::Time::Max()),
        SizeIs(test_case.valid))
        << test_case.aggregation_coordinator;
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();
  }
}

TEST_F(AttributionStorageSqlTest, ReportTablesStoreDestinationOrigin) {
  constexpr char kDestinationOriginA[] = "https://a.d.test";
  constexpr char kDestinationOriginB[] = "https://b.d.test";

  OpenDatabase();

  StorableSource source =
      TestAggregatableSourceProvider()
          .GetBuilder()
          .SetDestinationSites(
              {net::SchemefulSite::Deserialize(kDestinationOriginA)})
          .SetExpiry(base::Days(30))
          .Build();
  storage()->StoreSource(source);

  AttributionTrigger trigger =
      DefaultAggregatableTriggerBuilder()
          .SetDestinationOrigin(
              *SuitableOrigin::Deserialize(kDestinationOriginB))
          .Build();
  ASSERT_THAT(storage()->MaybeCreateAndStoreReport(trigger),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));

  CloseDatabase();

  sql::Database raw_db;
  ASSERT_TRUE(raw_db.Open(db_path()));

  {
    sql::Statement s(raw_db.GetUniqueStatement(
        "SELECT context_origin FROM event_level_reports"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(s.ColumnString(0), kDestinationOriginB);
  }

  {
    sql::Statement s(raw_db.GetUniqueStatement(
        "SELECT destination_origin FROM aggregatable_report_metadata"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(s.ColumnString(0), kDestinationOriginB);
  }
}

TEST_F(AttributionStorageSqlTest, FakeReportUsesSourceOriginAsContext) {
  OpenDatabase();

  delegate()->set_randomized_response(
      std::vector<AttributionStorageDelegate::FakeReport>{
          {.trigger_data = 1,
           .trigger_time = base::Time::Now() + base::Microseconds(1),
           .report_time = base::Time::Now() + base::Microseconds(2)}});

  storage()->StoreSource(
      SourceBuilder()
          .SetSourceOrigin(*SuitableOrigin::Deserialize("https://a.s.test"))
          .SetDestinationSites(
              {net::SchemefulSite::Deserialize("https://b.d.test")})
          .SetReportingOrigin(*SuitableOrigin::Deserialize("https://r.test"))
          .Build());

  CloseDatabase();

  sql::Database raw_db;
  ASSERT_TRUE(raw_db.Open(db_path()));

  {
    sql::Statement s(raw_db.GetUniqueStatement(
        "SELECT context_origin FROM event_level_reports"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(s.ColumnString(0), "https://a.s.test");
  }
}

TEST_F(AttributionStorageSqlTest, ReportTimes) {
  OpenDatabase();

  const attribution_reporting::DestinationSet destinations =
      *attribution_reporting::DestinationSet::Create(
          {net::SchemefulSite::Deserialize("https://dest.test")});

  const auto reporting_origin =
      *SuitableOrigin::Deserialize("https://report.test");

  const base::Time kSourceTime = base::Time::Now();

  const struct {
    const char* desc;
    absl::optional<base::TimeDelta> expiry;
    absl::optional<base::TimeDelta> event_report_window;
    absl::optional<base::TimeDelta> aggregatable_report_window;
    base::Time expected_expiry_time;
    base::Time expected_event_report_window_time;
    base::Time expected_aggregatable_report_window_time;
  } kTestCases[] = {
      {
          .desc = "expiry",
          .expiry = base::Days(4),
          .expected_expiry_time = kSourceTime + base::Days(4),
          .expected_event_report_window_time = kSourceTime + base::Days(4),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(4),
      },
      {
          .desc = "event-report-window",
          .event_report_window = base::Days(4),
          .expected_expiry_time = kSourceTime + base::Days(30),
          .expected_event_report_window_time = kSourceTime + base::Days(4),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(30),
      },
      {
          .desc = "clamp-event-report-window",
          .expiry = base::Days(4),
          .event_report_window = base::Days(30),
          .expected_expiry_time = kSourceTime + base::Days(4),
          .expected_event_report_window_time = kSourceTime + base::Days(4),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(4),
      },
      {
          .desc = "aggregatable-report-window",
          .aggregatable_report_window = base::Days(4),
          .expected_expiry_time = kSourceTime + base::Days(30),
          .expected_event_report_window_time = kSourceTime + base::Days(30),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(4),
      },
      {
          .desc = "clamp-aggregatable-report-window",
          .expiry = base::Days(4),
          .aggregatable_report_window = base::Days(30),
          .expected_expiry_time = kSourceTime + base::Days(4),
          .expected_event_report_window_time = kSourceTime + base::Days(4),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(4),
      },
      {
          .desc = "all",
          .expiry = base::Days(9),
          .event_report_window = base::Days(7),
          .aggregatable_report_window = base::Days(5),
          .expected_expiry_time = kSourceTime + base::Days(9),
          .expected_event_report_window_time = kSourceTime + base::Days(7),
          .expected_aggregatable_report_window_time =
              kSourceTime + base::Days(5),
      },
  };

  for (const auto& test_case : kTestCases) {
    attribution_reporting::SourceRegistration reg(destinations);
    reg.expiry = test_case.expiry.value_or(base::Days(30));
    reg.event_report_window = test_case.event_report_window;
    reg.aggregatable_report_window = test_case.aggregatable_report_window;

    storage()->StoreSource(
        StorableSource(reporting_origin, std::move(reg), kSourceTime,
                       *SuitableOrigin::Deserialize("https://source.test"),
                       attribution_reporting::mojom::SourceType::kNavigation,
                       /*is_within_fenced_frame=*/false));

    std::vector<StoredSource> sources = storage()->GetActiveSources();
    ASSERT_THAT(sources, SizeIs(1)) << test_case.desc;
    const StoredSource& actual = sources.front();

    EXPECT_EQ(actual.expiry_time(), test_case.expected_expiry_time)
        << test_case.desc;

    EXPECT_EQ(actual.event_report_window_time(),
              test_case.expected_event_report_window_time)
        << test_case.desc;

    EXPECT_EQ(actual.aggregatable_report_window_time(),
              test_case.expected_aggregatable_report_window_time)
        << test_case.desc;

    storage()->ClearData(/*delete_begin=*/base::Time::Min(),
                         /*delete_end=*/base::Time::Max(),
                         /*filter=*/base::NullCallback());
  }

  CloseDatabase();
}

TEST_F(AttributionStorageSqlTest,
       InvalidExpiryOrReportTime_FailsDeserialization) {
  static constexpr const char* kUpdateSqls[] = {
      "UPDATE sources SET expiry_time=?",
      "UPDATE sources SET event_report_window_time=?",
      "UPDATE sources SET aggregatable_report_window_time=?",
  };

  const struct {
    base::TimeDelta time_from_source;
    bool valid;
  } kTestCases[] = {
      {
          base::TimeDelta(),
          false,
      },
      {
          base::Milliseconds(1),
          true,
      },
      {
          kDefaultAttributionSourceExpiry,
          true,
      },
      {
          kDefaultAttributionSourceExpiry + base::Milliseconds(1),
          false,
      },
  };

  for (const char* update_sql : kUpdateSqls) {
    for (const auto& test_case : kTestCases) {
      OpenDatabase();

      base::Time now = base::Time::Now();
      storage()->StoreSource(SourceBuilder(now).Build());
      ASSERT_THAT(storage()->GetActiveSources(), SizeIs(1))
          << update_sql << "," << test_case.time_from_source;

      CloseDatabase();

      {
        sql::Database raw_db;
        ASSERT_TRUE(raw_db.Open(db_path()))
            << update_sql << "," << test_case.time_from_source;

        sql::Statement statement(raw_db.GetUniqueStatement(update_sql));
        statement.BindTime(0, now + test_case.time_from_source);
        ASSERT_TRUE(statement.Run())
            << update_sql << "," << test_case.time_from_source;
      }

      OpenDatabase();
      ASSERT_THAT(storage()->GetActiveSources(), SizeIs(test_case.valid))
          << update_sql << "," << test_case.time_from_source;
      storage()->ClearData(/*delete_begin=*/base::Time::Min(),
                           /*delete_end=*/base::Time::Max(),
                           /*filter=*/base::NullCallback());
      CloseDatabase();
    }
  }
}

}  // namespace
}  // namespace content
