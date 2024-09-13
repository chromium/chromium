// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <stdint.h>

#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_config.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/attribution_features.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_resolver_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/process_aggregatable_debug_report_result.mojom.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"
#include "content/public/browser/attribution_data_model.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/sql_features.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::Pair;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

const char kDefaultReportOrigin[] = "https://reporter.test/";

struct AttributionSourceRecord {
  int64_t source_id;
  int64_t source_event_id;
  std::string source_origin;
  std::string reporting_origin;
  base::Time source_time;
  base::Time expiry_time;
  base::Time aggregatable_report_window_time;
  int source_type;
  int attribution_logic;
  int64_t priority;
  std::string source_site;
  int num_attributions;
  int num_aggregatable_attribution_reports;
  int event_level_active;
  int aggregatable_active;
  std::optional<uint64_t> debug_key;
  std::string aggregation_keys;
  std::string filter_data;
  std::string read_only_source_data;
  int remaining_aggregatable_debug_budget;
  int num_aggregatable_debug_reports;
  std::optional<std::string> attribution_scopes_data;
};

struct AttributionReportRecord {
  int64_t report_id;
  int64_t source_id;
  base::Time trigger_time;
  base::Time report_time;
  base::Time initial_report_time;
  int failed_send_attempts = 0;
  std::string external_report_id;
  std::optional<uint64_t> debug_key;
  std::string context_origin = "https://destination.test";
  std::string reporting_origin = kDefaultReportOrigin;
  int report_type;
  std::string metadata;
};

struct AttributionEventLevelMetadataRecord {
  std::optional<uint64_t> trigger_data;
  std::optional<int64_t> priority;
};

struct AttributionAggregatableMetadataRecord {
  struct Contribution {
    std::optional<uint64_t> high_bits;
    std::optional<uint64_t> low_bits;
    std::optional<uint32_t> value;
    std::optional<uint64_t> id;
  };
  std::vector<Contribution> contributions;
  std::optional<url::Origin> coordinator_origin;
  std::optional<
      proto::AttributionCommonAggregatableMetadata_SourceRegistrationTimeConfig>
      source_registration_time_config =
          proto::AttributionCommonAggregatableMetadata::INCLUDE;
  std::optional<std::string> trigger_context_id;
  std::optional<uint32_t> filtering_id_max_bytes;
};

struct AttributionNullAggregatableMetadataRecord {
  std::optional<int64_t> fake_source_time;
  std::optional<url::Origin> coordinator_origin;
  std::optional<
      proto::AttributionCommonAggregatableMetadata_SourceRegistrationTimeConfig>
      source_registration_time_config =
          proto::AttributionCommonAggregatableMetadata::INCLUDE;
  std::optional<std::string> trigger_context_id;
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

std::string SerializeReportMetadata(
    const AttributionEventLevelMetadataRecord& record) {
  proto::AttributionEventLevelMetadata msg;

  if (record.trigger_data) {
    msg.set_trigger_data(*record.trigger_data);
  }
  if (record.priority) {
    msg.set_priority(*record.priority);
  }

  std::string str;
  bool success = msg.SerializeToString(&str);
  CHECK(success);
  return str;
}

std::string SerializeReportMetadata(
    const AttributionAggregatableMetadataRecord& record) {
  proto::AttributionAggregatableMetadata msg;

  for (const auto& contribution : record.contributions) {
    proto::AttributionAggregatableMetadata_Contribution* contribution_msg =
        msg.add_contributions();
    if (contribution.high_bits) {
      contribution_msg->mutable_key()->set_high_bits(*contribution.high_bits);
    }
    if (contribution.low_bits) {
      contribution_msg->mutable_key()->set_low_bits(*contribution.low_bits);
    }
    if (contribution.value) {
      contribution_msg->set_value(*contribution.value);
    }
    if (contribution.id) {
      contribution_msg->set_filtering_id(*contribution.id);
    }
  }

  if (record.coordinator_origin.has_value()) {
    msg.mutable_common_data()->set_coordinator_origin(
        record.coordinator_origin->Serialize());
  }

  if (record.source_registration_time_config) {
    msg.mutable_common_data()->set_source_registration_time_config(
        *record.source_registration_time_config);
  }

  if (record.trigger_context_id.has_value()) {
    msg.mutable_common_data()->set_trigger_context_id(
        *record.trigger_context_id);
  }

  if (record.filtering_id_max_bytes.has_value()) {
    msg.mutable_common_data()->set_filtering_id_max_bytes(
        *record.filtering_id_max_bytes);
  }

  std::string str;
  bool success = msg.SerializeToString(&str);
  CHECK(success);
  return str;
}

std::string SerializeReportMetadata(
    const AttributionNullAggregatableMetadataRecord& record) {
  proto::AttributionNullAggregatableMetadata msg;

  if (record.fake_source_time) {
    msg.set_fake_source_time(*record.fake_source_time);
  }

  if (record.coordinator_origin.has_value()) {
    msg.mutable_common_data()->set_coordinator_origin(
        record.coordinator_origin->Serialize());
  }

  if (record.source_registration_time_config) {
    msg.mutable_common_data()->set_source_registration_time_config(
        *record.source_registration_time_config);
  }

  if (record.trigger_context_id.has_value()) {
    msg.mutable_common_data()->set_trigger_context_id(
        *record.trigger_context_id);
  }

  std::string str;
  bool success = msg.SerializeToString(&str);
  CHECK(success);
  return str;
}

class AttributionStorageSqlTest : public testing::Test {
 public:
  AttributionStorageSqlTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void OpenDatabase() {
    CloseDatabase();
    auto delegate = std::make_unique<ConfigurableStorageDelegate>();
    delegate_ = delegate.get();
    storage_ = std::make_unique<AttributionResolverImpl>(
        temp_directory_.GetPath(), std::move(delegate));
  }

  void CloseDatabase() {
    delegate_ = nullptr;
    storage_.reset();
  }

  void AddReportToStorage() {
    storage_->StoreSource(SourceBuilder().Build());
    storage_->MaybeCreateAndStoreReport(DefaultTrigger());
  }

  void ExpectAllTablesEmpty() {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    static constexpr const char* kTables[] = {
        "sources",     "reports",    "source_destinations",
        "rate_limits", "dedup_keys",
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

  AttributionResolver* storage() { return storage_.get(); }

  ConfigurableStorageDelegate* delegate() { return delegate_; }

  void ExpectImpressionRows(size_t expected) {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));
    size_t rows;
    sql::test::CountTableRows(&raw_db, "sources", &rows);
    EXPECT_EQ(expected, rows);
  }

  AttributionTrigger::EventLevelResult MaybeCreateAndStoreEventLevelReport(
      const AttributionTrigger& conversion) {
    return storage_->MaybeCreateAndStoreReport(conversion).event_level_status();
  }

  void StoreAttributionSource(const AttributionSourceRecord& record) {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(db_path()));

    static constexpr char kStoreSourceSql[] =
        "INSERT INTO sources "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,0,?,?,?,?,?,?,?)";
    sql::Statement statement(raw_db.GetUniqueStatement(kStoreSourceSql));
    statement.BindInt64(0, record.source_id);
    statement.BindInt64(1, record.source_event_id);
    statement.BindString(2, record.source_origin);
    statement.BindString(3, record.reporting_origin);
    statement.BindTime(4, record.source_time);
    statement.BindTime(5, record.expiry_time);
    statement.BindTime(6, record.aggregatable_report_window_time);
    statement.BindInt(7, record.num_attributions);
    statement.BindInt(8, record.event_level_active);
    statement.BindInt(9, record.aggregatable_active);
    statement.BindInt(10, record.source_type);
    statement.BindInt(11, record.attribution_logic);
    statement.BindInt64(12, record.priority);
    statement.BindString(13, record.source_site);

    if (record.debug_key) {
      statement.BindInt64(14, *record.debug_key);
    } else {
      statement.BindNull(14);
    }

    statement.BindInt(15, record.num_aggregatable_attribution_reports);
    statement.BindBlob(16, record.aggregation_keys);
    statement.BindBlob(17, record.filter_data);
    statement.BindBlob(18, record.read_only_source_data);
    statement.BindInt(19, record.remaining_aggregatable_debug_budget);
    statement.BindInt(20, record.num_aggregatable_debug_reports);
    if (record.attribution_scopes_data.has_value()) {
      statement.BindBlob(21, *record.attribution_scopes_data);
    } else {
      statement.BindNull(21);
    }
    ASSERT_TRUE(statement.Run());
  }

  void StoreAttributionReport(const AttributionReportRecord& record) {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(db_path()));

    static constexpr char kStoreReportSql[] =
        "INSERT INTO reports "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)";
    sql::Statement statement(raw_db.GetUniqueStatement(kStoreReportSql));
    statement.BindInt64(0, record.report_id);
    statement.BindInt64(1, record.source_id);
    statement.BindTime(2, record.trigger_time);
    statement.BindTime(3, record.report_time);
    statement.BindTime(4, record.initial_report_time);
    statement.BindInt(5, record.failed_send_attempts);
    statement.BindString(6, record.external_report_id);

    if (record.debug_key) {
      statement.BindInt64(7, *record.debug_key);
    } else {
      statement.BindNull(7);
    }
    statement.BindString(8, record.context_origin);
    statement.BindString(9, record.reporting_origin);

    statement.BindInt(10, record.report_type);
    statement.BindBlob(11, record.metadata);
    ASSERT_TRUE(statement.Run());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_directory_;

 private:
  std::unique_ptr<AttributionResolver> storage_;
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

  EXPECT_TRUE(base::PathExists(db_path()));
}

TEST_F(AttributionStorageSqlTest, DatabaseReopened_DataPersisted) {
  OpenDatabase();
  AddReportToStorage();
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(1));
  CloseDatabase();
  OpenDatabase();
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), SizeIs(1));
}

TEST_F(AttributionStorageSqlTest, CorruptDatabase_DeletedOnOpen) {
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

  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());

  EXPECT_TRUE(expecter.SawExpectedErrors());

  // The database should have been deleted.
  EXPECT_FALSE(base::PathExists(db_path()));
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
       StorageUsedAfterFailedInitialization_NoCrash) {
  // We create a failed initialization by writing a dir to the database file
  // path.
  ASSERT_TRUE(base::CreateDirectoryAndGetError(db_path(), nullptr));

  OpenDatabase();

  // Test all public methods on AttributionResolver.
  EXPECT_NO_FATAL_FAILURE(storage()->StoreSource(SourceBuilder().Build()));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kInternalError,
            storage()
                ->MaybeCreateAndStoreReport(DefaultTrigger())
                .event_level_status());
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Now()), IsEmpty());
  EXPECT_THAT(storage()->GetActiveSources(), IsEmpty());
  EXPECT_TRUE(storage()->DeleteReport(AttributionReport::Id(0)));
  EXPECT_NO_FATAL_FAILURE(storage()->ClearData(
      base::Time::Min(), base::Time::Max(), base::NullCallback()));
  EXPECT_EQ(storage()->AdjustOfflineReportTimes(), std::nullopt);

#if BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(base::PathExists(db_path()));
#else
  EXPECT_TRUE(base::PathExists(db_path()));
#endif
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
  storage()->StoreSource(SourceBuilder()
                             .SetReportingOrigin(*SuitableOrigin::Deserialize(
                                 "https://report1.test"))
                             .Build());

  delegate()->set_null_aggregatable_reports_lookback_days({0});
  AttributionTrigger trigger =
      DefaultAggregatableTriggerBuilder()
          .SetReportingOrigin(
              *SuitableOrigin::Deserialize("https://report2.test"))
          .Build();
  storage()->MaybeCreateAndStoreReport(trigger);

  std::set<AttributionDataModel::DataKey> keys = storage()->GetAllDataKeys();
  ASSERT_THAT(keys, SizeIs(2));

  for (const auto& key : keys) {
    storage()->DeleteByDataKey(key);
  }

  CloseDatabase();

  sql::Database raw_db;
  ASSERT_TRUE(raw_db.Open(db_path()));
  {
    sql::Statement s(raw_db.GetUniqueStatement("SELECT * FROM reports"));
    ASSERT_FALSE(s.Step());
  }
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
  sql::test::CountTableRows(&raw_db, "reports", &conversion_rows);
  EXPECT_EQ(2u, conversion_rows);
  size_t rate_limit_rows;
  sql::test::CountTableRows(&raw_db, "rate_limits", &rate_limit_rows);
  EXPECT_EQ(3u, rate_limit_rows);
}

TEST_F(AttributionStorageSqlTest, CantOpenDb_NoCrash) {
  // Force db creation to fail by creating a directory where the file would go.
  ASSERT_TRUE(base::CreateDirectoryAndGetError(db_path(), nullptr));

  std::unique_ptr<AttributionResolver> storage =
      std::make_unique<AttributionResolverImpl>(
          temp_directory_.GetPath(),
          std::make_unique<ConfigurableStorageDelegate>());

  StoreSourceResult result = storage->StoreSource(SourceBuilder().Build());
  ASSERT_TRUE(absl::holds_alternative<StoreSourceResult::InternalError>(
      result.result()));
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kInternalError,
            storage->MaybeCreateAndStoreReport(DefaultTrigger())
                .event_level_status());

#if BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(base::DirectoryExists(db_path()));
#else
  EXPECT_TRUE(base::DirectoryExists(db_path()));
  EXPECT_TRUE(base::IsDirectoryEmpty(db_path()));
#endif
}

TEST_F(AttributionStorageSqlTest, DatabaseDirDoesExist_CreateDirAndOpenDB) {
  // Give the storage layer a database directory that doesn't exist.
  std::unique_ptr<AttributionResolver> storage =
      std::make_unique<AttributionResolverImpl>(
          temp_directory_.GetPath().Append(
              FILE_PATH_LITERAL("ConversionFolder/")),
          std::make_unique<ConfigurableStorageDelegate>());

  // The directory should be created, and the database opened.
  storage->StoreSource(SourceBuilder().Build());
  EXPECT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            storage->MaybeCreateAndStoreReport(DefaultTrigger())
                .event_level_status());
}

TEST_F(AttributionStorageSqlTest, DBinitializationSucceeds_HistogramsRecorded) {
  {
    base::HistogramTester histograms;

    OpenDatabase();
    // We add two sources in the storage to be able to assert the per source
    // file size histogram on the following db initialization.
    storage()->StoreSource(SourceBuilder().Build());
    storage()->StoreSource(SourceBuilder().Build());
    CloseDatabase();

    histograms.ExpectUniqueSample("Conversions.Storage.Sql.InitStatus2",
                                  AttributionStorageSql::InitStatus::kSuccess,
                                  1);
    EXPECT_GT(histograms.GetTotalSum("Conversions.Storage.Sql.FileSize2"), 0);
    // The per source histogram should not be recorded when there is no sources
    // in the db.
    histograms.ExpectTotalCount("Conversions.Storage.Sql.FileSize2.PerSource",
                                0u);
  }
  {
    base::HistogramTester histograms;

    OpenDatabase();
    // Since the storage is initiated lazily, we need to execute an operation on
    // the db for it to be initiated and histograms to be reported.
    storage()->StoreSource(SourceBuilder().Build());
    CloseDatabase();

    histograms.ExpectUniqueSample("Conversions.Storage.Sql.InitStatus2",
                                  AttributionStorageSql::InitStatus::kSuccess,
                                  1);

    int64_t file_size =
        histograms.GetTotalSum("Conversions.Storage.Sql.FileSize2");
    EXPECT_GT(file_size, 0);
    int64_t file_size_per_source =
        histograms.GetTotalSum("Conversions.Storage.Sql.FileSize2.PerSource");
    EXPECT_EQ(file_size_per_source, file_size * 1024 / 2);
  }
}

TEST_F(AttributionStorageSqlTest, MaxUint64StorageSucceeds) {
  constexpr uint64_t kMaxUint64 = std::numeric_limits<uint64_t>::max();

  OpenDatabase();

  // Ensure that reading and writing `uint64_t` fields via
  // `sql::Statement::ColumnInt64()` and `sql::Statement::BindInt64()` works
  // with the maximum value.

  storage()->StoreSource(SourceBuilder().SetSourceEventId(kMaxUint64).Build());
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
  EXPECT_TRUE(storage()->DeleteReport(reports[0].id()));
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

  EXPECT_TRUE(storage()->DeleteReport(AttributionReport::Id(2)));
  EXPECT_THAT(storage()->GetAttributionReports(base::Time::Max()),
              ElementsAre(ReportTypeIs(AttributionReport::Type::kEventLevel)));

  CloseDatabase();
}

TEST_F(AttributionStorageSqlTest, NegativeTriggerMoment_HistogramRecorded) {
  const char sql[] = "UPDATE sources SET source_time=?";
  base::HistogramTester histograms;

  OpenDatabase();

  storage()->StoreSource(TestAggregatableSourceProvider().GetBuilder().Build());

  CloseDatabase();

  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(db_path()));

    sql::Statement statement(raw_db.GetUniqueStatement(sql));
    statement.BindTime(0, base::Time::Now() + base::Hours(1));
    ASSERT_TRUE(statement.Run());
  }

  OpenDatabase();

  EXPECT_THAT(storage()->MaybeCreateAndStoreReport(
                  DefaultAggregatableTriggerBuilder().Build()),
              AllOf(CreateReportEventLevelStatusIs(
                        AttributionTrigger::EventLevelResult::kSuccess),
                    CreateReportAggregatableStatusIs(
                        AttributionTrigger::AggregatableResult::kSuccess)));
  histograms.ExpectUniqueSample("Conversions.TriggerTimeLessThanSourceTime", 1,
                                1);

  CloseDatabase();
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

  EXPECT_TRUE(storage()->DeleteReport(AttributionReport::Id(1)));

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

  EXPECT_TRUE(storage()->DeleteReport(reports[0].id()));
  EXPECT_TRUE(storage()->DeleteReport(reports[1].id()));

  // Store another source to trigger the expiry logic.
  storage()->StoreSource(
      SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());

  CloseDatabase();
  ExpectImpressionRows(1u);
}

TEST_F(AttributionStorageSqlTest,
       InvalidSourceOriginOrSite_FailsDeserialization) {
  const struct {
    const std::string sql;
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

    storage()->StoreSource(
        SourceBuilder().SetExpiry(base::Milliseconds(3)).Build());
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
  storage()->StoreSource(SourceBuilder().SetPriority(1).Build());
  MaybeCreateAndStoreEventLevelReport(DefaultTrigger());
  storage()->StoreSource(SourceBuilder().SetPriority(2).Build());
  MaybeCreateAndStoreEventLevelReport(DefaultTrigger());
  CloseDatabase();

  ExpectImpressionRows(2);
}

// Tests that invalid filter keys present in the serialized data are removed.
TEST_F(AttributionStorageSqlTest, DeserializeFilterData_RemovesReservedKeys) {
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
    statement.BindBlob(0, CreateSerializedFilterData({
                              {"source_type", {"abc"}},
                              {"_some_key", {"y"}},
                              {"x", {"y"}},
                              {"_lookback_window", {"def"}},
                          }));
    ASSERT_TRUE(statement.Run());
  }

  OpenDatabase();

  std::vector<StoredSource> sources = storage()->GetActiveSources();
  ASSERT_EQ(sources.size(), 1u);
  ASSERT_THAT(sources.front().filter_data().filter_values(),
              ElementsAre(Pair("x", ElementsAre("y"))));
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
    sql::Statement s(
        raw_db.GetUniqueStatement("SELECT context_origin FROM reports"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(s.ColumnStringView(0), kDestinationOriginB);
  }
}

TEST_F(AttributionStorageSqlTest, FakeReportUsesSourceOriginAsContext) {
  OpenDatabase();

  delegate()->set_randomized_response(
      std::vector<attribution_reporting::FakeEventLevelReport>{
          {.trigger_data = 1, .window_index = 0}});

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
    sql::Statement s(
        raw_db.GetUniqueStatement("SELECT context_origin FROM reports"));
    ASSERT_TRUE(s.Step());
    ASSERT_EQ(s.ColumnStringView(0), "https://a.s.test");
  }
}

TEST_F(AttributionStorageSqlTest,
       InvalidExpiryOrReportTime_FailsDeserialization) {
  static constexpr const char* kUpdateSqls[] = {
      "UPDATE sources SET expiry_time=?",
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
          attribution_reporting::kMaxSourceExpiry,
          true,
      },
      {
          attribution_reporting::kMaxSourceExpiry + base::Milliseconds(1),
          false,
      },
  };

  for (const std::string update_sql : kUpdateSqls) {
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

TEST_F(AttributionStorageSqlTest,
       RandomizedResponseRateNotStored_RecalculatedWhenHandled) {
  {
    OpenDatabase();
    storage()->StoreSource(SourceBuilder().Build());
    CloseDatabase();
  }

  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(db_path()));

    static constexpr char kGetSql[] =
        "SELECT source_id,read_only_source_data FROM sources";
    sql::Statement get_statement(raw_db.GetUniqueStatement(kGetSql));

    static constexpr char kSetSql[] =
        "UPDATE sources SET read_only_source_data=? WHERE source_id=?";
    sql::Statement set_statement(raw_db.GetUniqueStatement(kSetSql));

    while (get_statement.Step()) {
      int64_t id = get_statement.ColumnInt64(0);

      proto::AttributionReadOnlySourceData msg;
      {
        base::span<const uint8_t> blob = get_statement.ColumnBlob(1);
        ASSERT_TRUE(msg.ParseFromArray(blob.data(), blob.size()));
      }

      msg.clear_randomized_response_rate();

      set_statement.Reset(/*clear_bound_vars=*/true);
      set_statement.BindBlob(0, msg.SerializeAsString());
      set_statement.BindInt64(1, id);
      ASSERT_TRUE(set_statement.Run());
    }
  }

  OpenDatabase();

  delegate()->set_randomized_response_rate(0.2);
  EXPECT_THAT(storage()->GetActiveSources(),
              ElementsAre(RandomizedResponseRateIs(0.2)));
}

TEST_F(AttributionStorageSqlTest, EpsilonNotStored_RecalculatedWhenHandled) {
  {
    OpenDatabase();
    storage()->StoreSource(SourceBuilder().Build());
    CloseDatabase();
  }

  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(db_path()));

    static constexpr char kGetSql[] =
        "SELECT source_id,read_only_source_data FROM sources";
    sql::Statement get_statement(raw_db.GetUniqueStatement(kGetSql));

    static constexpr char kSetSql[] =
        "UPDATE sources SET read_only_source_data=? WHERE source_id=?";
    sql::Statement set_statement(raw_db.GetUniqueStatement(kSetSql));

    while (get_statement.Step()) {
      int64_t id = get_statement.ColumnInt64(0);

      proto::AttributionReadOnlySourceData msg;
      {
        base::span<const uint8_t> blob = get_statement.ColumnBlob(1);
        ASSERT_TRUE(msg.ParseFromArray(blob.data(), blob.size()));
      }

      msg.clear_event_level_epsilon();

      set_statement.Reset(/*clear_bound_vars=*/true);
      set_statement.BindBlob(0, msg.SerializeAsString());
      set_statement.BindInt64(1, id);
      ASSERT_TRUE(set_statement.Run());
    }
  }

  OpenDatabase();

  EXPECT_THAT(
      storage()->GetActiveSources(),
      ElementsAre(Property(&StoredSource::event_level_epsilon,
                           attribution_reporting::EventLevelEpsilon())));
}

TEST_F(AttributionStorageSqlTest,
       TriggerDataNotStored_RecalculatedWhenHandled) {
  {
    OpenDatabase();
    storage()->StoreSource(SourceBuilder()
                               .SetSourceEventId(1u)
                               .SetSourceType(SourceType::kNavigation)
                               .Build());
    storage()->StoreSource(SourceBuilder()
                               .SetSourceEventId(2u)
                               .SetSourceType(SourceType::kEvent)
                               .Build());
    CloseDatabase();
  }

  {
    sql::Database raw_db;
    ASSERT_TRUE(raw_db.Open(db_path()));

    static constexpr char kGetSql[] =
        "SELECT source_id,read_only_source_data FROM sources";
    sql::Statement get_statement(raw_db.GetUniqueStatement(kGetSql));

    static constexpr char kSetSql[] =
        "UPDATE sources SET read_only_source_data=? WHERE source_id=?";
    sql::Statement set_statement(raw_db.GetUniqueStatement(kSetSql));

    while (get_statement.Step()) {
      int64_t id = get_statement.ColumnInt64(0);

      proto::AttributionReadOnlySourceData msg;
      {
        base::span<const uint8_t> blob = get_statement.ColumnBlob(1);
        ASSERT_TRUE(msg.ParseFromArray(blob.data(), blob.size()));
      }

      msg.clear_trigger_data();

      set_statement.Reset(/*clear_bound_vars=*/true);
      set_statement.BindBlob(0, msg.SerializeAsString());
      set_statement.BindInt64(1, id);
      ASSERT_TRUE(set_statement.Run());
    }
  }

  OpenDatabase();

  EXPECT_THAT(storage()->GetActiveSources(),
              UnorderedElementsAre(
                  AllOf(Property(&StoredSource::source_event_id, 1u),
                        Property(&StoredSource::trigger_specs,
                                 ElementsAre(Key(0), Key(1), Key(2), Key(3),
                                             Key(4), Key(5), Key(6), Key(7)))),
                  AllOf(Property(&StoredSource::source_event_id, 2u),
                        Property(&StoredSource::trigger_specs,
                                 ElementsAre(Key(0), Key(1))))));
}

// Having the missing field default to the correct value allows us to avoid a
// DB migration to populate the field.
TEST_F(AttributionStorageSqlTest,
       MissingTriggerDataMatchingProtoField_DefaultsToModulus) {
  proto::AttributionReadOnlySourceData msg;
  ASSERT_FALSE(msg.has_trigger_data_matching());
  EXPECT_EQ(msg.trigger_data_matching(),
            proto::AttributionReadOnlySourceData::MODULUS);
}

TEST_F(AttributionStorageSqlTest, InvalidReportingOrigin_FailsDeserialization) {
  const struct {
    const char* desc;
    const char* reporting_origin;
    bool valid;
  } kTestCases[] = {
      {
          .desc = "valid",
          .reporting_origin = kDefaultReportOrigin,
          .valid = true,
      },
      {
          .desc = "invalid",
          .reporting_origin = "https://a.test",
          .valid = false,
      },
  };

  for (auto test_case : kTestCases) {
    OpenDatabase();
    storage()->StoreSource(SourceBuilder()
                               .SetReportingOrigin(*SuitableOrigin::Deserialize(
                                   kDefaultReportOrigin))
                               .Build());
    auto sources = storage()->GetActiveSources();
    ASSERT_THAT(sources, SizeIs(1));
    CloseDatabase();

    StoreAttributionReport(AttributionReportRecord{
        .report_id = 1,
        .source_id = *sources.front().source_id(),
        .external_report_id = DefaultExternalReportID().AsLowercaseString(),
        .reporting_origin = test_case.reporting_origin,
        .report_type = static_cast<int>(AttributionReport::Type::kEventLevel),
        .metadata = SerializeReportMetadata(AttributionEventLevelMetadataRecord{
            .trigger_data = 0,
            .priority = 0,
        }),
    });

    OpenDatabase();
    EXPECT_THAT(
        storage()->GetAttributionReports(/*max_report_time=*/base::Time::Max()),
        SizeIs(test_case.valid))
        << test_case.desc;
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();
  }
}

TEST_F(AttributionStorageSqlTest,
       InvalidEventLevelMetadata_FailsDeserialization) {
  const struct {
    const char* desc;
    absl::variant<AttributionEventLevelMetadataRecord, std::string> record;
    bool valid;
  } kTestCases[] = {
      {
          .desc = "invalid_proto",
          .record = "!",
          .valid = false,
      },
      {
          .desc = "missing_priority",
          .record =
              AttributionEventLevelMetadataRecord{
                  .trigger_data = 1,
              },
          .valid = false,
      },
      {
          .desc = "missing_trigger_data",
          .record =
              AttributionEventLevelMetadataRecord{
                  .priority = 2,
              },
          .valid = false,
      },
      {
          .desc = "valid",
          .record =
              AttributionEventLevelMetadataRecord{
                  .trigger_data = 1,
                  .priority = 2,
              },
          .valid = true,
      },
  };

  for (auto test_case : kTestCases) {
    OpenDatabase();
    storage()->StoreSource(SourceBuilder()
                               .SetReportingOrigin(*SuitableOrigin::Deserialize(
                                   kDefaultReportOrigin))
                               .Build());
    auto sources = storage()->GetActiveSources();
    ASSERT_THAT(sources, SizeIs(1));
    CloseDatabase();

    std::string metadata =
        absl::visit(base::Overloaded{
                        [](const AttributionEventLevelMetadataRecord& record) {
                          return SerializeReportMetadata(record);
                        },
                        [](const std::string& str) { return str; },
                    },
                    test_case.record);

    StoreAttributionReport(AttributionReportRecord{
        .report_id = 1,
        .source_id = *sources.front().source_id(),
        .external_report_id = DefaultExternalReportID().AsLowercaseString(),
        .report_type = static_cast<int>(AttributionReport::Type::kEventLevel),
        .metadata = metadata,
    });

    base::HistogramTester histograms;
    OpenDatabase();
    EXPECT_THAT(
        storage()->GetAttributionReports(/*max_report_time=*/base::Time::Max()),
        SizeIs(test_case.valid))
        << test_case.desc;
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();

    histograms.ExpectUniqueSample("Conversions.ValidReportsInDatabase",
                                  test_case.valid, 1);

    if (!test_case.valid) {
      histograms.ExpectBucketCount(
          "Conversions.CorruptReportsInDatabase5",
          AttributionStorageSql::ReportCorruptionStatus::kAnyFieldCorrupted, 1);
      histograms.ExpectBucketCount(
          "Conversions.CorruptReportsInDatabase5",
          AttributionStorageSql::ReportCorruptionStatus::kInvalidMetadata, 1);
    }
  }
}

TEST_F(AttributionStorageSqlTest,
       InvalidAggregatableMetadata_FailsDeserialization) {
  const struct {
    const char* desc;
    absl::variant<AttributionAggregatableMetadataRecord, std::string> record;
    bool valid;
  } kTestCases[] = {
      {
          .desc = "invalid_proto",
          .record = "!",
          .valid = false,
      },
      {
          .desc = "missing_contribution",
          .record = AttributionAggregatableMetadataRecord(),
          .valid = false,
      },
      {
          .desc = "missing_contribution_value",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                          },
                      },
              },
          .valid = false,
      },
      {
          .desc = "missing_contribution_key",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .value = 3,
                          },
                      },
              },
          .valid = false,
      },
      {
          .desc = "valid",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                              .value =
                                  attribution_reporting::kMaxAggregatableValue,
                              .id = 125,
                          },
                      },
              },
          .valid = true,
      },
      {
          .desc = "invalid_contribution_value",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                              .value = 0,
                          },
                      },
              },
          .valid = false,
      },
      {
          .desc = "contribution_value_too_large",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                              .value =
                                  attribution_reporting::kMaxAggregatableValue +
                                  1,
                          },
                      },
              },
          .valid = false,
      },
      {
          .desc = "missing_source_registration_time_config",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                              .value = 3,
                          },
                      },
                  .source_registration_time_config = std::nullopt,
              },
          .valid = false,
      },
      {
          .desc = "invalid_coordinator_origin",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                              .value = 3,
                          },
                      },
                  .coordinator_origin =
                      url::Origin::Create(GURL("http://a.test")),
              },
          .valid = false,
      },
      {
          .desc = "invalid_trigger_context_id",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                              .value = 3,
                          },
                      },
                  .source_registration_time_config =
                      proto::AttributionCommonAggregatableMetadata::INCLUDE,
                  .trigger_context_id = "123",
              },
          .valid = false,
      },
      {
          .desc = "invalid_filtering_id_max_bytes_value",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                              .value =
                                  attribution_reporting::kMaxAggregatableValue,
                          },
                      },
                  .filtering_id_max_bytes = 10,
              },
          .valid = false,
      },
      {
          .desc = "invalid_filtering_id_max_bytes",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                              .value =
                                  attribution_reporting::kMaxAggregatableValue,
                          },
                      },
                  .source_registration_time_config =
                      proto::AttributionCommonAggregatableMetadata::INCLUDE,
                  .filtering_id_max_bytes = 2,
              },
          .valid = false,
      },
      {
          .desc = "invalid_filtering_id",
          .record =
              AttributionAggregatableMetadataRecord{
                  .contributions =
                      {
                          AttributionAggregatableMetadataRecord::Contribution{
                              .high_bits = 1,
                              .low_bits = 2,
                              .value =
                                  attribution_reporting::kMaxAggregatableValue,
                              .id = 256},
                      }},
          .valid = false,
      },
  };

  for (auto test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    OpenDatabase();
    storage()->StoreSource(SourceBuilder()
                               .SetReportingOrigin(*SuitableOrigin::Deserialize(
                                   kDefaultReportOrigin))
                               .Build());
    auto sources = storage()->GetActiveSources();
    ASSERT_THAT(sources, SizeIs(1));
    CloseDatabase();

    std::string metadata = absl::visit(
        base::Overloaded{
            [](const AttributionAggregatableMetadataRecord& record) {
              return SerializeReportMetadata(record);
            },
            [](const std::string& str) { return str; },
        },
        test_case.record);

    StoreAttributionReport(AttributionReportRecord{
        .report_id = 1,
        .source_id = *sources.front().source_id(),
        .external_report_id = DefaultExternalReportID().AsLowercaseString(),
        .report_type =
            static_cast<int>(AttributionReport::Type::kAggregatableAttribution),
        .metadata = metadata,
    });

    base::HistogramTester histograms;
    OpenDatabase();
    EXPECT_THAT(
        storage()->GetAttributionReports(/*max_report_time=*/base::Time::Max()),
        SizeIs(test_case.valid))
        << test_case.desc;
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();

    histograms.ExpectUniqueSample("Conversions.ValidReportsInDatabase",
                                  test_case.valid, 1);
    if (!test_case.valid) {
      histograms.ExpectBucketCount(
          "Conversions.CorruptReportsInDatabase5",
          AttributionStorageSql::ReportCorruptionStatus::kAnyFieldCorrupted, 1);
      histograms.ExpectBucketCount(
          "Conversions.CorruptReportsInDatabase5",
          AttributionStorageSql::ReportCorruptionStatus::kInvalidMetadata, 1);
    }
  }
}

TEST_F(AttributionStorageSqlTest,
       InvalidNullAggregatableMetadata_FailsDeserialization) {
  const struct {
    const char* desc;
    absl::variant<AttributionNullAggregatableMetadataRecord, std::string>
        record;
    bool valid;
  } kTestCases[] = {
      {
          .desc = "invalid_proto",
          .record = "!",
          .valid = false,
      },
      {
          .desc = "missing_fake_source_time",
          .record = AttributionNullAggregatableMetadataRecord(),
          .valid = false,
      },
      {
          .desc = "missing_source_registration_time_config",
          .record =
              AttributionNullAggregatableMetadataRecord{
                  .fake_source_time = 12345678900,
                  .source_registration_time_config = std::nullopt,
              },
          .valid = false,
      },
      {
          .desc = "valid",
          .record =
              AttributionNullAggregatableMetadataRecord{
                  .fake_source_time = 12345678900,
              },
          .valid = true,
      },
      {
          .desc = "invalid_coordinator_origin",
          .record =
              AttributionNullAggregatableMetadataRecord{
                  .fake_source_time = 12345678900,
                  .coordinator_origin =
                      url::Origin::Create(GURL("http://a.test")),
              },
          .valid = false,
      },
      {
          .desc = "invalid_trigger_context_id",
          .record =
              AttributionNullAggregatableMetadataRecord{
                  .fake_source_time = 12345678900,
                  .source_registration_time_config =
                      proto::AttributionCommonAggregatableMetadata::INCLUDE,
                  .trigger_context_id = "123",
              },
          .valid = false,
      },
  };

  for (auto test_case : kTestCases) {
    OpenDatabase();
    // Create the tables.
    storage()->StoreSource(SourceBuilder().Build());
    auto sources = storage()->GetActiveSources();
    ASSERT_THAT(sources, SizeIs(1));
    CloseDatabase();

    std::string metadata = absl::visit(
        base::Overloaded{
            [](const AttributionNullAggregatableMetadataRecord& record) {
              return SerializeReportMetadata(record);
            },
            [](const std::string& str) { return str; },
        },
        test_case.record);

    StoreAttributionReport(AttributionReportRecord{
        .report_id = 1,
        .source_id = -1,
        .external_report_id = DefaultExternalReportID().AsLowercaseString(),
        .report_type =
            static_cast<int>(AttributionReport::Type::kNullAggregatable),
        .metadata = metadata,
    });

    OpenDatabase();
    EXPECT_THAT(
        storage()->GetAttributionReports(/*max_report_time=*/base::Time::Max()),
        SizeIs(test_case.valid))
        << test_case.desc;
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();
  }
}

TEST_F(AttributionStorageSqlTest,
       NullAggregatableReport_ValidSourceMatched_FailsDeserialization) {
  OpenDatabase();
  storage()->StoreSource(SourceBuilder().Build());
  auto sources = storage()->GetActiveSources();
  ASSERT_THAT(sources, SizeIs(1));
  CloseDatabase();

  StoreAttributionReport(AttributionReportRecord{
      .report_id = 1,
      .source_id = 1,
      .external_report_id = DefaultExternalReportID().AsLowercaseString(),
      .report_type =
          static_cast<int>(AttributionReport::Type::kNullAggregatable),
      .metadata =
          SerializeReportMetadata(AttributionNullAggregatableMetadataRecord{
              .fake_source_time = 12345678900,
          }),
  });

  base::HistogramTester histograms;
  OpenDatabase();
  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  CloseDatabase();

  histograms.ExpectBucketCount(
      "Conversions.CorruptReportsInDatabase5",
      AttributionStorageSql::ReportCorruptionStatus::kAnyFieldCorrupted, 1);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceDataFoundNullAggregatable,
                               1);
}

TEST_F(AttributionStorageSqlTest,
       NullAggregatableReport_CorruptedSourceMatched_FailsDeserialization) {
  OpenDatabase();
  storage()->StoreSource(SourceBuilder().Build());
  auto sources = storage()->GetActiveSources();
  ASSERT_THAT(sources, SizeIs(1));
  CloseDatabase();

  StoreAttributionSource(AttributionSourceRecord{.source_id = 2});
  StoreAttributionReport(AttributionReportRecord{
      .report_id = 1,
      .source_id = 2,
      .external_report_id = DefaultExternalReportID().AsLowercaseString(),
      .report_type =
          static_cast<int>(AttributionReport::Type::kNullAggregatable),
      .metadata =
          SerializeReportMetadata(AttributionNullAggregatableMetadataRecord{
              .fake_source_time = 12345678900,
          }),
  });

  base::HistogramTester histograms;
  OpenDatabase();
  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  CloseDatabase();

  histograms.ExpectBucketCount(
      "Conversions.CorruptReportsInDatabase5",
      AttributionStorageSql::ReportCorruptionStatus::kAnyFieldCorrupted, 1);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceDataFoundNullAggregatable,
                               1);
}

TEST_F(AttributionStorageSqlTest, InvalidStoredReportFields_MarkedAsCorrupted) {
  const struct {
    const char* desc;
    bool source_id_mismatch = false;
    AttributionReportRecord record;
    AttributionStorageSql::ReportCorruptionStatus status;
  } kTestCases[] = {
      {
          .desc = "invalid_failed_send_attempts",
          .record =
              AttributionReportRecord{
                  .report_id = 1,
                  .failed_send_attempts = -1,
                  .external_report_id =
                      DefaultExternalReportID().AsLowercaseString(),
                  .report_type =
                      static_cast<int>(AttributionReport::Type::kEventLevel),
                  .metadata = SerializeReportMetadata(
                      AttributionEventLevelMetadataRecord{
                          .trigger_data = 1,
                          .priority = 2,
                      }),
              },
          .status = AttributionStorageSql::ReportCorruptionStatus::
              kInvalidFailedSendAttempts,
      },
      {
          .desc = "invalid_external_report_id",
          .record =
              AttributionReportRecord{
                  .report_id = 1,
                  .external_report_id = "0",
                  .report_type =
                      static_cast<int>(AttributionReport::Type::kEventLevel),
                  .metadata = SerializeReportMetadata(
                      AttributionEventLevelMetadataRecord{
                          .trigger_data = 1,
                          .priority = 2,
                      }),
              },
          .status = AttributionStorageSql::ReportCorruptionStatus::
              kInvalidExternalReportID,
      },
      {
          .desc = "invalid_context_origin",
          .record =
              AttributionReportRecord{
                  .report_id = 1,
                  .external_report_id =
                      DefaultExternalReportID().AsLowercaseString(),
                  .context_origin = "-1",
                  .report_type =
                      static_cast<int>(AttributionReport::Type::kEventLevel),
                  .metadata = SerializeReportMetadata(
                      AttributionEventLevelMetadataRecord{
                          .trigger_data = 1,
                          .priority = 2,
                      }),
              },
          .status = AttributionStorageSql::ReportCorruptionStatus::
              kInvalidContextOrigin,
      },
      {
          .desc = "invalid_reporting_origin",
          .record =
              AttributionReportRecord{
                  .report_id = 1,
                  .external_report_id =
                      DefaultExternalReportID().AsLowercaseString(),
                  .reporting_origin = "-1",
                  .report_type =
                      static_cast<int>(AttributionReport::Type::kEventLevel),
                  .metadata = SerializeReportMetadata(
                      AttributionEventLevelMetadataRecord{
                          .trigger_data = 1,
                          .priority = 2,
                      }),
              },
          .status = AttributionStorageSql::ReportCorruptionStatus::
              kInvalidReportingOrigin,
      },
      {
          .desc = "reporting_origin_mismatch",
          .record =
              AttributionReportRecord{
                  .report_id = 1,
                  .external_report_id =
                      DefaultExternalReportID().AsLowercaseString(),
                  .reporting_origin = "https://reporter2.test/",
                  .report_type =
                      static_cast<int>(AttributionReport::Type::kEventLevel),
                  .metadata = SerializeReportMetadata(
                      AttributionEventLevelMetadataRecord{
                          .trigger_data = 1,
                          .priority = 2,
                      }),
              },
          .status = AttributionStorageSql::ReportCorruptionStatus::
              kReportingOriginMismatch,
      },
      {
          .desc = "source_data_missing_event_level",
          .source_id_mismatch = true,
          .record =
              AttributionReportRecord{
                  .report_id = 1,
                  .source_id = 0,
                  .external_report_id =
                      DefaultExternalReportID().AsLowercaseString(),
                  .reporting_origin = "https://reporter2.test/",
                  .report_type =
                      static_cast<int>(AttributionReport::Type::kEventLevel),
                  .metadata = SerializeReportMetadata(
                      AttributionEventLevelMetadataRecord{
                          .trigger_data = 1,
                          .priority = 2,
                      }),
              },
          .status = AttributionStorageSql::ReportCorruptionStatus::
              kSourceDataMissingEventLevel,
      },
      {
          .desc = "source_data_missing_aggregatable",
          .source_id_mismatch = true,
          .record =
              AttributionReportRecord{
                  .report_id = 1,
                  .source_id = 0,
                  .external_report_id =
                      DefaultExternalReportID().AsLowercaseString(),
                  .reporting_origin = "https://reporter2.test/",
                  .report_type = static_cast<int>(
                      AttributionReport::Type::kAggregatableAttribution),
                  .metadata = SerializeReportMetadata(
                      AttributionEventLevelMetadataRecord{
                          .trigger_data = 1,
                          .priority = 2,
                      }),
              },
          .status = AttributionStorageSql::ReportCorruptionStatus::
              kSourceDataMissingAggregatable,
      },
      {
          .desc = "invalid_report_type",
          .record =
              AttributionReportRecord{
                  .report_id = 1,
                  .external_report_id =
                      DefaultExternalReportID().AsLowercaseString(),
                  .report_type = 123,
              },
          .status =
              AttributionStorageSql::ReportCorruptionStatus::kInvalidReportType,
      },
  };

  for (auto test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    OpenDatabase();
    storage()->StoreSource(SourceBuilder()
                               .SetReportingOrigin(*SuitableOrigin::Deserialize(
                                   kDefaultReportOrigin))
                               .Build());
    auto sources = storage()->GetActiveSources();
    if (!test_case.source_id_mismatch) {
      test_case.record.source_id = *sources.front().source_id();
    }
    CloseDatabase();

    StoreAttributionReport(test_case.record);

    base::HistogramTester histograms;
    OpenDatabase();
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();

    histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                                 test_case.status, 1);
    histograms.ExpectBucketCount(
        "Conversions.CorruptReportsInDatabase5",
        AttributionStorageSql::ReportCorruptionStatus::kAnyFieldCorrupted, 1);
    if (test_case.source_id_mismatch) {
      histograms.ExpectBucketCount(
          "Conversions.CorruptReportsInDatabase5",
          AttributionStorageSql::ReportCorruptionStatus::kSourceNotFound, 1);
      histograms.ExpectTotalCount("Conversions.CorruptReportsInDatabase5", 3);
    } else {
      histograms.ExpectTotalCount("Conversions.CorruptReportsInDatabase5", 2);
    }
  }
}

TEST_F(AttributionStorageSqlTest,
       InvalidReportCorrespondingSourceFields_MarkedAsCorrupted) {
  base::HistogramTester histograms;
  OpenDatabase();
  // Creates the tables.
  storage()->StoreSource(SourceBuilder().Build());
  auto sources = storage()->GetActiveSources();
  ASSERT_THAT(sources, SizeIs(1));
  CloseDatabase();

  AttributionSourceRecord source_record{
      .source_id = 2,
      .source_type = 3,
      .attribution_logic = 5,
      .num_attributions = -1,
      .num_aggregatable_attribution_reports = -1,
      .event_level_active = 2,
      .aggregation_keys = "foo",
      .filter_data = "bar",
      .read_only_source_data = "baz",
      .attribution_scopes_data = "qux"};
  AttributionReportRecord report_record{
      .report_id = 1,
      .source_id = 2,
      .external_report_id = DefaultExternalReportID().AsLowercaseString(),
      .reporting_origin = "https://reporter.test/",
      .report_type = static_cast<int>(AttributionReport::Type::kEventLevel),
      .metadata = SerializeReportMetadata(AttributionEventLevelMetadataRecord{
          .trigger_data = 1,
          .priority = 2,
      }),
  };
  StoreAttributionSource(source_record);
  // Stores a report as related metrics are tied to querying reports.
  StoreAttributionReport(report_record);

  // Tests invalid fields in ReadOnlySourceData proto.
  proto::AttributionReadOnlySourceData msg;
  msg.set_max_event_level_reports(-1);
  msg.set_event_level_epsilon(-1);
  source_record.read_only_source_data = msg.SerializeAsString();

  source_record.source_id = 3;
  report_record.source_id = 3;
  report_record.report_id = 2;

  StoreAttributionSource(source_record);
  StoreAttributionReport(report_record);

  OpenDatabase();
  storage()->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback());
  CloseDatabase();

  histograms.ExpectBucketCount(
      "Conversions.CorruptReportsInDatabase5",
      AttributionStorageSql::ReportCorruptionStatus::kAnyFieldCorrupted, 2);
  histograms.ExpectBucketCount(
      "Conversions.CorruptReportsInDatabase5",
      AttributionStorageSql::ReportCorruptionStatus::kSourceInvalidSourceOrigin,
      2);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidReportingOrigin,
                               2);
  histograms.ExpectBucketCount(
      "Conversions.CorruptReportsInDatabase5",
      AttributionStorageSql::ReportCorruptionStatus::kSourceInvalidSourceType,
      2);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidAttributionLogic,
                               2);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidNumConversions,
                               2);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidNumAggregatableReports,
                               2);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidAggregationKeys,
                               2);
  histograms.ExpectBucketCount(
      "Conversions.CorruptReportsInDatabase5",
      AttributionStorageSql::ReportCorruptionStatus::kSourceInvalidFilterData,
      2);
  histograms.ExpectBucketCount(
      "Conversions.CorruptReportsInDatabase5",
      AttributionStorageSql::ReportCorruptionStatus::kSourceInvalidActiveState,
      2);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidDestinationSites,
                               2);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidReadOnlySourceData,
                               1);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidMaxEventLevelReports,
                               1);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidEventLevelEpsilon,
                               1);
  histograms.ExpectBucketCount("Conversions.CorruptReportsInDatabase5",
                               AttributionStorageSql::ReportCorruptionStatus::
                                   kSourceInvalidAttributionScopesData,
                               2);
  histograms.ExpectTotalCount("Conversions.CorruptReportsInDatabase5", 29);
}

TEST_F(AttributionStorageSqlTest, SourceRemainingAggregatableBudget) {
  const struct {
    const char* desc;
    int remaining_aggregatable_attribution_budget;
    int remaining_aggregatable_debug_budget;
    bool expected;
  } kTestCases[] = {
      {
          .desc = "valid_attribution_only",
          .remaining_aggregatable_attribution_budget = 65536,
          .remaining_aggregatable_debug_budget = 0,
          .expected = true,
      },
      {
          .desc = "valid_debug_only",
          .remaining_aggregatable_attribution_budget = 0,
          .remaining_aggregatable_debug_budget = 65536,
          .expected = true,
      },
      {
          .desc = "attribution_below_0",
          .remaining_aggregatable_attribution_budget = -1,
          .remaining_aggregatable_debug_budget = 0,
          .expected = false,
      },
      {
          .desc = "attribution_above_max",
          .remaining_aggregatable_attribution_budget = 65537,
          .remaining_aggregatable_debug_budget = 0,
          .expected = false,
      },
      {
          .desc = "debug_below_0",
          .remaining_aggregatable_attribution_budget = 0,
          .remaining_aggregatable_debug_budget = -1,
          .expected = false,
      },
      {
          .desc = "debug_above_max",
          .remaining_aggregatable_attribution_budget = 0,
          .remaining_aggregatable_debug_budget = 65537,
          .expected = false,
      },
      {
          .desc = "total_above_max",
          .remaining_aggregatable_attribution_budget = 1,
          .remaining_aggregatable_debug_budget = 65536,
          .expected = false,
      },
  };

  constexpr char kUpdateSql[] =
      "UPDATE sources SET "
      "remaining_aggregatable_attribution_budget=?,"
      "remaining_aggregatable_debug_budget=?";

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    OpenDatabase();

    storage()->StoreSource(SourceBuilder().Build());
    ASSERT_THAT(storage()->GetActiveSources(), SizeIs(1));

    CloseDatabase();

    {
      sql::Database raw_db;
      ASSERT_TRUE(raw_db.Open(db_path()));

      sql::Statement update_statement(raw_db.GetUniqueStatement(kUpdateSql));
      update_statement.BindInt(
          0, test_case.remaining_aggregatable_attribution_budget);
      update_statement.BindInt(1,
                               test_case.remaining_aggregatable_debug_budget);
      ASSERT_TRUE(update_statement.Run());
    }

    OpenDatabase();
    ASSERT_THAT(storage()->GetActiveSources(), SizeIs(test_case.expected));
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();
  }
}

TEST_F(AttributionStorageSqlTest, SourceDebugKeyAndDebugCookieSetCombination) {
  const struct {
    const char* desc;
    std::optional<bool> debug_cookie_set;
    std::optional<uint64_t> debug_key;
    std::optional<bool> expected_debug_cookie_set;
  } kTestCases[] = {
      {
          .desc = "debug cookie missing, debug key set",
          .debug_cookie_set = std::nullopt,
          .debug_key = 123,
          .expected_debug_cookie_set = true,
      },
      {
          .desc = "debug cookie missing, debug key not set",
          .debug_cookie_set = std::nullopt,
          .debug_key = std::nullopt,
          .expected_debug_cookie_set = false,
      },
      {
          .desc = "debug cookie not set, debug key set",
          .debug_cookie_set = false,
          .debug_key = 123,
          .expected_debug_cookie_set = std::nullopt,
      },
      {
          .desc = "debug cookie not set, debug key not set",
          .debug_cookie_set = false,
          .debug_key = std::nullopt,
          .expected_debug_cookie_set = false,
      },
      {
          .desc = "debug cookie set, debug key set",
          .debug_cookie_set = true,
          .debug_key = 123,
          .expected_debug_cookie_set = true,
      },
      {
          .desc = "debug cookie set, debug key not set",
          .debug_cookie_set = true,
          .debug_key = std::nullopt,
          .expected_debug_cookie_set = true,
      },
  };

  constexpr char kReadSql[] = "SELECT read_only_source_data FROM sources";
  constexpr char kUpdateSql[] = "UPDATE sources SET read_only_source_data=?";

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    OpenDatabase();

    storage()->StoreSource(SourceBuilder()
                               .SetDebugKey(test_case.debug_key)
                               .SetDebugCookieSet(true)
                               .Build());
    ASSERT_THAT(storage()->GetActiveSources(), SizeIs(1));

    CloseDatabase();

    {
      sql::Database raw_db;
      ASSERT_TRUE(raw_db.Open(db_path()));

      sql::Statement read_statement(raw_db.GetUniqueStatement(kReadSql));
      ASSERT_TRUE(read_statement.Step());
      std::optional<proto::AttributionReadOnlySourceData>
          read_only_source_data_msg =
              DeserializeReadOnlySourceDataAsProto(read_statement, 0);
      ASSERT_TRUE(read_only_source_data_msg);

      if (test_case.debug_cookie_set.has_value()) {
        read_only_source_data_msg->set_debug_cookie_set(
            *test_case.debug_cookie_set);
      } else {
        read_only_source_data_msg->clear_debug_cookie_set();
      }

      sql::Statement update_statement(raw_db.GetUniqueStatement(kUpdateSql));
      update_statement.BindString(
          0, read_only_source_data_msg->SerializeAsString());
      ASSERT_TRUE(update_statement.Run());
    }

    OpenDatabase();
    auto sources = storage()->GetActiveSources();
    if (test_case.expected_debug_cookie_set.has_value()) {
      ASSERT_THAT(sources, ElementsAre(SourceDebugCookieSetIs(
                               *test_case.expected_debug_cookie_set)));
    } else {
      ASSERT_THAT(sources, IsEmpty());
    }
    storage()->ClearData(base::Time::Min(), base::Time::Max(),
                         base::NullCallback());
    CloseDatabase();
  }
}

TEST_F(AttributionStorageSqlTest, ClearData_AggregatableDebugDataDeleted) {
  OpenDatabase();

  const auto create_report = []() {
    return AggregatableDebugReport::CreateForTesting(
        {blink::mojom::AggregatableReportHistogramContribution(
            /*bucket=*/1, /*value=*/65536,
            /*filtering_id=*/std::nullopt)},
        /*context_site=*/net::SchemefulSite::Deserialize("https://c.test"),
        /*reporting_origin=*/
        *attribution_reporting::SuitableOrigin::Deserialize("https://r.test"),
        /*effective_destination=*/
        net::SchemefulSite::Deserialize("https://d.test"),
        /*aggregation_coordinator_origin=*/std::nullopt,
        /*scheduled_report_time=*/base::Time::Now());
  };

  EXPECT_THAT(storage()->ProcessAggregatableDebugReport(
                  create_report(), /*remaining_budget=*/std::nullopt,
                  /*source_id=*/std::nullopt),
              Field(&ProcessAggregatableDebugReportResult::result,
                    attribution_reporting::mojom::
                        ProcessAggregatableDebugReportResult::kSuccess));
  // Hits rate limits, null report.
  EXPECT_THAT(
      storage()->ProcessAggregatableDebugReport(
          create_report(), /*remaining_budget=*/std::nullopt,
          /*source_id=*/std::nullopt),
      Field(&ProcessAggregatableDebugReportResult::result,
            attribution_reporting::mojom::ProcessAggregatableDebugReportResult::
                kReportingSiteRateLimitReached));

  // This should delete the rate-limit record.
  storage()->ClearData(/*delete_begin=*/base::Time::Min(),
                       /*delete_end=*/base::Time::Max(), base::NullCallback());
  EXPECT_THAT(storage()->ProcessAggregatableDebugReport(
                  create_report(), /*remaining_budget=*/std::nullopt,
                  /*source_id=*/std::nullopt),
              Field(&ProcessAggregatableDebugReportResult::result,
                    attribution_reporting::mojom::
                        ProcessAggregatableDebugReportResult::kSuccess));

  // This should not delete the rate-limit record.
  storage()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(std::equal_to<blink::StorageKey>(),
                          blink::StorageKey::CreateFirstParty(
                              url::Origin::Create(GURL("https://r1.test")))));
  // Still hits rate limits, null report.
  EXPECT_THAT(
      storage()->ProcessAggregatableDebugReport(
          create_report(), /*remaining_budget=*/std::nullopt,
          /*source_id=*/std::nullopt),
      Field(&ProcessAggregatableDebugReportResult::result,
            attribution_reporting::mojom::ProcessAggregatableDebugReportResult::
                kReportingSiteRateLimitReached));

  // The should delete the rate-limit record.
  storage()->ClearData(
      base::Time::Min(), base::Time::Max(),
      base::BindRepeating(std::equal_to<blink::StorageKey>(),
                          blink::StorageKey::CreateFirstParty(
                              url::Origin::Create(GURL("https://r.test")))));
  EXPECT_THAT(storage()->ProcessAggregatableDebugReport(
                  create_report(), /*remaining_budget=*/std::nullopt,
                  /*source_id=*/std::nullopt),
              Field(&ProcessAggregatableDebugReportResult::result,
                    attribution_reporting::mojom::
                        ProcessAggregatableDebugReportResult::kSuccess));

  CloseDatabase();
}

TEST_F(AttributionStorageSqlTest, MaxImpressionsPerOrigin_LimitsStorage) {
  OpenDatabase();
  delegate()->set_max_sources_per_origin(2);

  base::HistogramTester histograms;

  ASSERT_EQ(storage()
                ->StoreSource(SourceBuilder()
                                  .SetSourceEventId(3)
                                  .SetPriority(1)
                                  .SetMaxEventLevelReports(1)
                                  .Build())
                .status(),
            StorableSource::Result::kSuccess);

  ASSERT_EQ(storage()
                ->StoreSource(SourceBuilder()
                                  .SetSourceEventId(5)
                                  .SetPriority(2)
                                  .SetMaxEventLevelReports(1)
                                  .Build())
                .status(),
            StorableSource::Result::kSuccess);

  // Force the lower-priority source to be deactivated.
  ASSERT_EQ(AttributionTrigger::EventLevelResult::kSuccess,
            MaybeCreateAndStoreEventLevelReport(DefaultTrigger()));

  ASSERT_THAT(storage()->GetActiveSources(), ElementsAre(SourceEventIdIs(5u)));

  // There's still room for this source, as the limit applies only to active
  // sources.
  ASSERT_EQ(storage()
                ->StoreSource(SourceBuilder()
                                  .SetSourceEventId(6)
                                  .SetMaxEventLevelReports(1)
                                  .Build())
                .status(),
            StorableSource::Result::kSuccess);

  ASSERT_EQ(storage()
                ->StoreSource(SourceBuilder()
                                  .SetSourceEventId(7)
                                  .SetMaxEventLevelReports(1)
                                  .Build())
                .status(),
            StorableSource::Result::kInsufficientSourceCapacity);

  int64_t file_size = histograms.GetTotalSum(
      "Conversions.Storage.Sql.FileSizeSourcesPerOriginLimitReached2");
  EXPECT_GT(file_size, 0);

  int64_t file_size_per_source = histograms.GetTotalSum(
      "Conversions.Storage.Sql.FileSizeSourcesPerOriginLimitReached2."
      "PerSource");
  EXPECT_EQ(file_size_per_source, file_size * 1024 / 2);

  ASSERT_THAT(storage()->GetActiveSources(),
              ElementsAre(SourceEventIdIs(5u), SourceEventIdIs(6u)));
}

}  // namespace
}  // namespace content
