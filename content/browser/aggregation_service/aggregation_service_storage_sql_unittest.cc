// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_base.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_coordinator_utils.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/public/common/content_paths.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using aggregation_service::RequestIdIs;
using testing::ElementsAre;

using RequestId = AggregationServiceStorage::RequestId;
using RequestAndId = AggregationServiceStorage::RequestAndId;

const char kExampleUrl[] =
    "https://helper.test/.well-known/aggregation-service/keys.json";

const std::vector<PublicKey> kExampleKeys{
    aggregation_service::TestHpkeKey("dummy_id").GetPublicKey()};

std::string RemoveQuotes(std::string_view input) {
  std::string output;
  base::RemoveChars(input, "\"", &output);
  return output;
}

AggregatableReportRequest CreateExampleRequestWithDelayType() {
  return aggregation_service::CreateExampleRequest(
      blink::mojom::AggregationServiceMode::kDefault,
      /*failed_send_attempts=*/0,
      /*aggregation_coordinator_origin=*/std::nullopt,
      AggregatableReportRequest::DelayType::ScheduledWithFullDelay);
}

}  // namespace

class AggregationServiceStorageSqlTest : public testing::Test {
 public:
  AggregationServiceStorageSqlTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    clock_.SetNow(base::Time::Now());
  }

  // Use the default limit unless specified.
  void OpenDatabase(
      std::optional<int> max_stored_requests_per_reporting_origin =
          std::nullopt) {
    if (max_stored_requests_per_reporting_origin.has_value()) {
      storage_ = std::make_unique<AggregationServiceStorageSql>(
          /*run_in_memory=*/false, temp_directory_.GetPath(), &clock_,
          max_stored_requests_per_reporting_origin.value());
    } else {
      storage_ = std::make_unique<AggregationServiceStorageSql>(
          /*run_in_memory=*/false, temp_directory_.GetPath(), &clock_);
    }
  }

  void CloseDatabase() { storage_.reset(); }

  std::vector<RequestAndId> GetRequestsReportingOnOrBefore(
      base::Time not_after_time) {
    CHECK(storage_);
    return storage_->GetRequestsReportingOnOrBefore(not_after_time,
                                                    /*limit=*/std::nullopt);
  }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("AggregationService"));
  }

 protected:
  base::HistogramTester histograms_;
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<AggregationServiceStorage> storage_;
  base::SimpleTestClock clock_;
};

class AggregationServiceStorageSqlInMemoryTest : public testing::Test {
 public:
  AggregationServiceStorageSqlInMemoryTest() = default;

  void SetUp() override { clock_.SetNow(base::Time::Now()); }

  void OpenDatabase() {
    storage_ = std::make_unique<AggregationServiceStorageSql>(
        /*run_in_memory=*/true, base::FilePath(), &clock_);
  }

  void CloseDatabase() { storage_.reset(); }

  std::vector<RequestAndId> GetRequestsReportingOnOrBefore(
      base::Time not_after_time) {
    CHECK(storage_);
    return storage_->GetRequestsReportingOnOrBefore(not_after_time,
                                                    /*limit=*/std::nullopt);
  }

 protected:
  std::unique_ptr<AggregationServiceStorage> storage_;
  base::SimpleTestClock clock_;
};

TEST_F(AggregationServiceStorageSqlTest,
       DBInitializationSucceeds_HistogramRecorded) {
  OpenDatabase();
  GURL url(kExampleUrl);
  PublicKeyset keyset(kExampleKeys, /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, keyset);
  CloseDatabase();

  histograms_.ExpectUniqueSample(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus",
      AggregationServiceStorageSql::InitStatus::kSuccess, 1);
}

TEST_F(AggregationServiceStorageSqlTest, CantOpenDb_HistogramRecorded) {
  ASSERT_TRUE(base::CreateDirectory(db_path()));

  OpenDatabase();
  GURL url(kExampleUrl);
  PublicKeyset keyset(kExampleKeys, /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, keyset);
  CloseDatabase();

  histograms_.ExpectUniqueSample(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus",
      AggregationServiceStorageSql::InitStatus::kFailedToOpenDbFile, 1);
}

TEST_F(AggregationServiceStorageSqlTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  OpenDatabase();
  CloseDatabase();

  GURL url(kExampleUrl);

  // An unused AggregationServiceStorageSql instance should not create the
  // database.
  EXPECT_FALSE(base::PathExists(db_path()));

  // Operations which don't need to run on an empty database should not create
  // the database.
  OpenDatabase();
  EXPECT_TRUE(storage_->GetPublicKeys(url).empty());
  CloseDatabase();

  EXPECT_FALSE(base::PathExists(db_path()));

  // DB creation UMA should not be recorded.
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql.CreationTime2", 0);

  // Storing a public key should create and initialize the database.
  OpenDatabase();
  PublicKeyset keyset(kExampleKeys, /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, keyset);
  CloseDatabase();

  // DB creation UMA should be recorded if ThreadTicks is supported
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql.CreationTime2",
      base::ThreadTicks::IsSupported() ? 1 : 0);

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [urls], [keys], [report_requests], [meta], [sqlite_sequence] (for
    // AUTOINCREMENT support).
    EXPECT_EQ(sql::test::CountSQLTables(&raw_db), 5u);

    // [urls_by_url_idx], [fetch_time_idx], [expiry_time_idx],
    // [report_time_idx], [creation_time_idx], [reporting_origin_idx] and meta
    // table index.
    EXPECT_EQ(sql::test::CountSQLIndices(&raw_db), 7u);
  }
}

TEST_F(AggregationServiceStorageSqlTest, DatabaseReopened_KeysPersisted) {
  OpenDatabase();

  GURL url(kExampleUrl);
  PublicKeyset keyset(kExampleKeys, /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, keyset);
  EXPECT_EQ(storage_->GetPublicKeys(url).size(), 1u);
  CloseDatabase();

  OpenDatabase();
  EXPECT_EQ(storage_->GetPublicKeys(url).size(), 1u);
}

TEST_F(AggregationServiceStorageSqlTest, SetPublicKeys_ExpectedResult) {
  OpenDatabase();

  std::vector<PublicKey> expected_keys{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("bcde").GetPublicKey()};

  GURL url(kExampleUrl);
  PublicKeyset keyset(expected_keys, /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());

  storage_->SetPublicKeys(url, keyset);
  std::vector<PublicKey> actual_keys = storage_->GetPublicKeys(url);
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(expected_keys, actual_keys));

  CloseDatabase();
}

TEST_F(AggregationServiceStorageSqlTest, GetPublicKeysExpired_EmptyResult) {
  OpenDatabase();

  std::vector<PublicKey> keys{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("bcde").GetPublicKey()};

  base::Time now = clock_.Now();
  GURL url(kExampleUrl);
  PublicKeyset keyset(std::move(keys), /*fetch_time=*/now,
                      /*expiry_time=*/now + base::Days(7));

  storage_->SetPublicKeys(url, keyset);
  clock_.Advance(base::Days(8));
  EXPECT_TRUE(storage_->GetPublicKeys(url).empty());

  CloseDatabase();
}

TEST_F(AggregationServiceStorageSqlTest, ClearPublicKeys) {
  OpenDatabase();

  std::vector<PublicKey> keys{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("bcde").GetPublicKey()};

  GURL url(kExampleUrl);
  PublicKeyset keyset(std::move(keys), /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());

  storage_->SetPublicKeys(url, keyset);
  storage_->ClearPublicKeys(url);

  EXPECT_TRUE(storage_->GetPublicKeys(url).empty());

  CloseDatabase();
}

TEST_F(AggregationServiceStorageSqlTest, ReplacePublicKeys) {
  OpenDatabase();

  GURL url(kExampleUrl);

  std::vector<PublicKey> old_keys{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("bcde").GetPublicKey()};

  PublicKeyset old_keyset(old_keys, /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, old_keyset);
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      old_keys, storage_->GetPublicKeys(url)));

  std::vector<PublicKey> expected_keys{
      aggregation_service::TestHpkeKey("efgh").GetPublicKey(),
      aggregation_service::TestHpkeKey("fghi").GetPublicKey()};

  PublicKeyset expected_keyset(expected_keys, /*fetch_time=*/clock_.Now(),
                               /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, expected_keyset);
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      expected_keys, storage_->GetPublicKeys(url)));

  CloseDatabase();
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearDataBetween_PublicKeyRangeDeleted) {
  OpenDatabase();

  GURL url_1("https://a.com/keys");
  std::vector<PublicKey> keys_1{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("bcde").GetPublicKey()};
  storage_->SetPublicKeys(url_1,
                          PublicKeyset(keys_1, /*fetch_time=*/clock_.Now(),
                                       /*expiry_time=*/base::Time::Max()));

  clock_.Advance(base::Days(3));

  GURL url_2("https://b.com/keys");
  std::vector<PublicKey> keys_2{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("efgh").GetPublicKey()};
  storage_->SetPublicKeys(url_2,
                          PublicKeyset(keys_2, /*fetch_time=*/clock_.Now(),
                                       /*expiry_time=*/base::Time::Max()));

  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_1, storage_->GetPublicKeys(url_1)));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(url_2)));

  base::Time now = clock_.Now();
  storage_->ClearDataBetween(
      now - base::Days(5), now - base::Days(1),
      // The filter should be ignored.
      base::BindLambdaForTesting(
          [](const blink::StorageKey& storage_key) { return false; }));

  EXPECT_TRUE(storage_->GetPublicKeys(url_1).empty());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(url_2)));
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearAllDataWithFilter_PublicKeysAllDeleted) {
  OpenDatabase();

  GURL url_1("https://a.com/keys");
  std::vector<PublicKey> keys_1{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("bcde").GetPublicKey()};
  storage_->SetPublicKeys(url_1,
                          PublicKeyset(keys_1, /*fetch_time=*/clock_.Now(),
                                       /*expiry_time=*/base::Time::Max()));

  clock_.Advance(base::Days(1));

  GURL url_2("https://b.com/keys");
  std::vector<PublicKey> keys_2{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("efgh").GetPublicKey()};
  storage_->SetPublicKeys(url_2,
                          PublicKeyset(keys_2, /*fetch_time=*/clock_.Now(),
                                       /*expiry_time=*/base::Time::Max()));

  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_1, storage_->GetPublicKeys(url_1)));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(url_2)));

  storage_->ClearDataBetween(
      base::Time(), base::Time::Max(),
      // The filter should be ignored.
      base::BindLambdaForTesting(
          [](const blink::StorageKey& storage_key) { return false; }));

  EXPECT_TRUE(storage_->GetPublicKeys(url_1).empty());
  EXPECT_TRUE(storage_->GetPublicKeys(url_2).empty());
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearAllDataWithoutFilter_AllPublicKeysDeleted) {
  OpenDatabase();

  GURL url_1("https://a.com/keys");
  std::vector<PublicKey> keys_1{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("bcde").GetPublicKey()};
  storage_->SetPublicKeys(url_1,
                          PublicKeyset(keys_1, /*fetch_time=*/clock_.Now(),
                                       /*expiry_time=*/base::Time::Max()));

  clock_.Advance(base::Days(1));

  GURL url_2("https://b.com/keys");
  std::vector<PublicKey> keys_2{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("efgh").GetPublicKey()};
  storage_->SetPublicKeys(url_2,
                          PublicKeyset(keys_2, /*fetch_time=*/clock_.Now(),
                                       /*expiry_time=*/base::Time::Max()));

  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_1, storage_->GetPublicKeys(url_1)));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(url_2)));

  storage_->ClearDataBetween(base::Time(), base::Time::Max(),
                             base::NullCallback());

  EXPECT_TRUE(storage_->GetPublicKeys(url_1).empty());
  EXPECT_TRUE(storage_->GetPublicKeys(url_2).empty());
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearPublicKeysExpiredBy_RangeDeleted) {
  OpenDatabase();

  base::Time now = clock_.Now();

  GURL url_1("https://a.com/keys");
  std::vector<PublicKey> keys_1{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("bcde").GetPublicKey()};
  storage_->SetPublicKeys(url_1,
                          PublicKeyset(keys_1, /*fetch_time=*/now,
                                       /*expiry_time=*/now + base::Days(1)));

  GURL url_2("https://b.com/keys");
  std::vector<PublicKey> keys_2{
      aggregation_service::TestHpkeKey("abcd").GetPublicKey(),
      aggregation_service::TestHpkeKey("efgh").GetPublicKey()};
  storage_->SetPublicKeys(url_2,
                          PublicKeyset(keys_2, /*fetch_time=*/now,
                                       /*expiry_time=*/now + base::Days(3)));

  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_1, storage_->GetPublicKeys(url_1)));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(url_2)));

  storage_->ClearPublicKeysExpiredBy(now + base::Days(1));

  EXPECT_TRUE(storage_->GetPublicKeys(url_1).empty());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(url_2)));
}

TEST_F(AggregationServiceStorageSqlTest, VersionTooNew_RazesDB) {
  OpenDatabase();

  GURL url(kExampleUrl);
  PublicKeyset keyset(kExampleKeys, /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, keyset);
  EXPECT_EQ(storage_->GetPublicKeys(url).size(), 1u);
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
  EXPECT_TRUE(storage_->GetPublicKeys(url).empty());
}

TEST_F(AggregationServiceStorageSqlInMemoryTest,
       DatabaseInMemoryReopened_PublicKeyDataNotPersisted) {
  OpenDatabase();

  GURL url(kExampleUrl);
  PublicKeyset keyset(kExampleKeys, /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, keyset);
  EXPECT_EQ(storage_->GetPublicKeys(url).size(), 1u);
  CloseDatabase();

  OpenDatabase();
  EXPECT_TRUE(storage_->GetPublicKeys(url).empty());
}

TEST_F(AggregationServiceStorageSqlTest, StoreRequest_ExpectedResult) {
  OpenDatabase();

  EXPECT_FALSE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_TRUE(GetRequestsReportingOnOrBefore(base::Time::Max()).empty());

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  ASSERT_TRUE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()).value(),
            request.shared_info().scheduled_report_time);

  std::vector<AggregationServiceStorage::RequestAndId> stored_requests_and_ids =
      GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests_and_ids.size(), 1u);

  // IDs autoincrement from 1.
  EXPECT_EQ(stored_requests_and_ids[0].id, RequestId(1));
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests_and_ids[0].request, request));

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest, DeleteRequest_ExpectedResult) {
  OpenDatabase();

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), 1u);

  // IDs autoincrement from 1.
  storage_->DeleteRequest(RequestId(1));
  EXPECT_TRUE(GetRequestsReportingOnOrBefore(base::Time::Max()).empty());

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest,
       UpdateReportForSendFailure_ExpectedResult) {
  OpenDatabase();

  // Trying to update an non-existing report should not crash
  storage_->UpdateReportForSendFailure(RequestId(1),
                                       /*new_report_time=*/base::Time::Now());

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));

  base::Time next_run_time = base::Time::Now() + base::Minutes(5);

  storage_->UpdateReportForSendFailure(RequestId(1), next_run_time);

  // Report time is updated as expected
  std::vector<RequestAndId> requests_before_next_run_time =
      GetRequestsReportingOnOrBefore(next_run_time - base::Microseconds(1));
  EXPECT_EQ(requests_before_next_run_time.size(), 0u);
  std::vector<RequestAndId> requests_at_run_time =
      GetRequestsReportingOnOrBefore(next_run_time);
  ASSERT_EQ(requests_at_run_time.size(), 1u);

  // Failed send attempts has been increased
  EXPECT_EQ(requests_at_run_time[0].request.failed_send_attempts(), 1);

  // Fail again to ensure the number of failed attempts is increased
  storage_->UpdateReportForSendFailure(RequestId(1), next_run_time);
  requests_at_run_time = GetRequestsReportingOnOrBefore(next_run_time);
  ASSERT_EQ(requests_at_run_time.size(), 1u);
  EXPECT_EQ(requests_at_run_time[0].request.failed_send_attempts(), 2);

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      2);
}

TEST_F(AggregationServiceStorageSqlTest,
       RepeatGetPendingRequests_RequestReturnedAgain) {
  OpenDatabase();

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  ASSERT_TRUE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()).value(),
            request.shared_info().scheduled_report_time);

  std::vector<AggregationServiceStorage::RequestAndId> stored_requests =
      GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests.size(), 1u);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests[0].request, request));

  stored_requests = GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests.size(), 1u);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests[0].request, request));
  ASSERT_TRUE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()).value(),
            request.shared_info().scheduled_report_time);

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest, DatabaseReopened_RequestsPersisted) {
  OpenDatabase();

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));

  CloseDatabase();

  OpenDatabase();

  std::vector<AggregationServiceStorage::RequestAndId> stored_requests =
      GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests.size(), 1u);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests[0].request, request));

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest,
       GetRequestsReportingOnOrBefore_ReturnValuesAlignWithReportTime) {
  OpenDatabase();

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();
  base::Time report_time = request.shared_info().scheduled_report_time;

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));

  const struct {
    base::Time not_after_time;
    size_t number_requests;
  } kTestCases[] = {
      {base::Time::Min(), 0}, {report_time - base::Seconds(1), 0},
      {report_time, 1},       {report_time + base::Seconds(1), 1},
      {base::Time::Max(), 1},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(GetRequestsReportingOnOrBefore(test_case.not_after_time).size(),
              test_case.number_requests)
        << test_case.not_after_time;
  }

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      2);
}

TEST_F(AggregationServiceStorageSqlTest,
       GetRequestsReportingOnOrBefore_ReturnValuesAlignWithLimit) {
  OpenDatabase();

  storage_->StoreRequest(CreateExampleRequestWithDelayType());
  storage_->StoreRequest(CreateExampleRequestWithDelayType());
  storage_->StoreRequest(CreateExampleRequestWithDelayType());

  // IDs autoincrement from 1.
  EXPECT_THAT(
      storage_->GetRequestsReportingOnOrBefore(
          /*not_after_time=*/base::Time::Max(), /*limit=*/2),
      ElementsAre(RequestIdIs(RequestId(1)), RequestIdIs(RequestId(2))));

  EXPECT_THAT(storage_->GetRequestsReportingOnOrBefore(
                  /*not_after_time=*/base::Time::Max(), /*limit=*/std::nullopt),
              ElementsAre(RequestIdIs(RequestId(1)), RequestIdIs(RequestId(2)),
                          RequestIdIs(RequestId(3))));

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest, GetRequests_ReturnValuesAlignWithIds) {
  OpenDatabase();

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  storage_->StoreRequest(std::move(request));

  // IDs autoincrement from 1.
  EXPECT_THAT(
      storage_->GetRequests({RequestId(1), RequestId(3), RequestId(4)}),
      ElementsAre(RequestIdIs(RequestId(1)), RequestIdIs(RequestId(3))));
}

TEST_F(AggregationServiceStorageSqlTest,
       NextReportTimeAfter_ReturnValuesAlignWithReportTime) {
  OpenDatabase();

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();
  base::Time report_time = request.shared_info().scheduled_report_time;

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));

  const struct {
    base::Time strictly_after_time;
    std::optional<base::Time> expected_return_value;
  } kTestCases[] = {
      {base::Time::Min(), report_time},
      {report_time - base::Seconds(1), report_time},
      {report_time, std::nullopt},
      {report_time + base::Seconds(1), std::nullopt},
      {base::Time::Max(), std::nullopt},

  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(storage_->NextReportTimeAfter(test_case.strictly_after_time),
              test_case.expected_return_value)
        << test_case.strictly_after_time;
  }
}

TEST_F(AggregationServiceStorageSqlTest,
       MultipleRequests_ReturnValuesAlignsWithReportTime) {
  OpenDatabase();

  constexpr auto kExampleTime =
      base::Time::FromMillisecondsSinceUnixEpoch(1652984901234);

  std::vector<base::Time> scheduled_report_times = {
      kExampleTime, kExampleTime, kExampleTime + base::Hours(1)};

  for (base::Time scheduled_report_time : scheduled_report_times) {
    AggregatableReportRequest example_request =
        aggregation_service::CreateExampleRequest();
    AggregatableReportSharedInfo shared_info =
        example_request.shared_info().Clone();
    shared_info.scheduled_report_time = scheduled_report_time;

    std::optional<AggregatableReportRequest> request =
        AggregatableReportRequest::Create(
            example_request.payload_contents(), std::move(shared_info),
            AggregatableReportRequest::DelayType::ScheduledWithReducedDelay);
    ASSERT_TRUE(request.has_value());

    storage_->StoreRequest(std::move(request.value()));
  }
  ASSERT_TRUE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()).value(),
            kExampleTime);

  EXPECT_TRUE(
      GetRequestsReportingOnOrBefore(kExampleTime - base::Milliseconds(1))
          .empty());

  ASSERT_TRUE(
      storage_->NextReportTimeAfter(kExampleTime - base::Milliseconds(1))
          .has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(kExampleTime - base::Milliseconds(1))
                .value(),
            kExampleTime);
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);

  std::vector<AggregationServiceStorage::RequestAndId> example_time_reports =
      GetRequestsReportingOnOrBefore(kExampleTime);
  ASSERT_EQ(example_time_reports.size(), 2u);

  EXPECT_EQ(base::flat_set<RequestId>(
                {example_time_reports[0].id, example_time_reports[1].id}),
            // Request IDs autoincrement from 1.
            base::flat_set<RequestId>({RequestId(1), RequestId(2)}));

  ASSERT_TRUE(storage_->NextReportTimeAfter(kExampleTime).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(kExampleTime).value(),
            kExampleTime + base::Hours(1));
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      2);

  EXPECT_EQ(GetRequestsReportingOnOrBefore(kExampleTime + base::Hours(1) -
                                           base::Milliseconds(1))
                .size(),
            2u);
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      4);

  std::vector<AggregationServiceStorage::RequestAndId> all_reports =
      GetRequestsReportingOnOrBefore(kExampleTime + base::Hours(1));
  ASSERT_EQ(all_reports.size(), 3u);
  EXPECT_EQ(all_reports[2].id, RequestId(3));
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      7);

  EXPECT_FALSE(
      storage_->NextReportTimeAfter(kExampleTime + base::Hours(1)).has_value());
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), 3u);
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      7);
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearAllDataWithoutFilter_AllRequestsDeleted) {
  OpenDatabase();

  storage_->StoreRequest(CreateExampleRequestWithDelayType());
  storage_->StoreRequest(CreateExampleRequestWithDelayType());

  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), 2u);

  storage_->ClearDataBetween(base::Time(), base::Time(), base::NullCallback());

  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), 0u);

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearDataBetween_RequestsTimeRangeDeleted) {
  OpenDatabase();

  constexpr auto kExampleTime =
      base::Time::FromMillisecondsSinceUnixEpoch(1652984901234);

  clock_.SetNow(kExampleTime);
  storage_->StoreRequest(CreateExampleRequestWithDelayType());

  clock_.Advance(base::Hours(1));
  storage_->StoreRequest(CreateExampleRequestWithDelayType());

  clock_.Advance(base::Hours(1));
  storage_->StoreRequest(CreateExampleRequestWithDelayType());

  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), 3u);

  // As the times are inclusive, this should delete the first two requests.
  storage_->ClearDataBetween(kExampleTime, kExampleTime + base::Hours(1),
                             base::NullCallback());

  std::vector<AggregationServiceStorage::RequestAndId> stored_reports =
      GetRequestsReportingOnOrBefore(base::Time::Max());
  ASSERT_EQ(stored_reports.size(), 1u);

  // Only the last request should be left. Request IDs start from 1.
  EXPECT_EQ(stored_reports[0].id, RequestId(3));

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearDataAllTimesWithFilter_OnlyRequestsSpecifiedAreDeleted) {
  const auto reporting_origins = std::to_array({
      url::Origin::Create(GURL("https://a.example")),
      url::Origin::Create(GURL("https://b.example")),
      url::Origin::Create(GURL("https://c.example")),
  });

  OpenDatabase();

  for (const url::Origin& reporting_origin : reporting_origins) {
    AggregatableReportRequest example_request =
        CreateExampleRequestWithDelayType();
    AggregatableReportSharedInfo shared_info =
        example_request.shared_info().Clone();
    shared_info.reporting_origin = reporting_origin;
    storage_->StoreRequest(
        AggregatableReportRequest::Create(
            example_request.payload_contents(), std::move(shared_info),
            AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
            .value());
  }

  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), 3u);

  storage_->ClearDataBetween(
      base::Time::Min(), base::Time::Max(),
      base::BindLambdaForTesting(
          [&reporting_origins](const blink::StorageKey& storage_key) {
            return storage_key !=
                   blink::StorageKey::CreateFirstParty(reporting_origins[2]);
          }));

  std::vector<AggregationServiceStorage::RequestAndId> stored_reports =
      GetRequestsReportingOnOrBefore(base::Time::Max());
  ASSERT_EQ(stored_reports.size(), 1u);

  // Only the last request should be left. Request IDs start from 1.
  EXPECT_EQ(stored_reports[0].id, RequestId(3));

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest, GetReportRequestReportingOrigins) {
  const url::Origin origins[] = {
      url::Origin::Create(GURL("https://a.example")),
      url::Origin::Create(GURL("https://b.example")),
      url::Origin::Create(GURL("https://c.example"))};

  OpenDatabase();

  for (const url::Origin& origin : origins) {
    AggregatableReportRequest example_request =
        aggregation_service::CreateExampleRequest();
    AggregatableReportSharedInfo shared_info =
        example_request.shared_info().Clone();
    shared_info.reporting_origin = origin;
    storage_->StoreRequest(
        AggregatableReportRequest::Create(
            example_request.payload_contents(), std::move(shared_info),
            AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
            .value());
  }

  ASSERT_EQ(storage_->GetReportRequestReportingOrigins().size(), 3u);
  EXPECT_THAT(
      storage_->GetReportRequestReportingOrigins(),
      testing::UnorderedElementsAre(origins[0], origins[1], origins[2]));
}

TEST_F(AggregationServiceStorageSqlTest,
       AdjustOfflineReportTimes_AffectsPastReportsOnly) {
  OpenDatabase();

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();

  base::Time original_report_time = request.shared_info().scheduled_report_time;

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
            original_report_time);

  // As `now` is before the report time, the report shouldn't be affected.
  EXPECT_EQ(storage_->AdjustOfflineReportTimes(
                /*now=*/original_report_time - base::Minutes(1),
                /*min_delay=*/base::Minutes(1),
                /*max_delay=*/base::Minutes(2)),
            original_report_time);
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
            original_report_time);

  // Report times equal to `now` are also unaffected.
  EXPECT_EQ(storage_->AdjustOfflineReportTimes(/*now=*/original_report_time,
                                               /*min_delay=*/base::Minutes(1),
                                               /*max_delay=*/base::Minutes(2)),
            original_report_time);
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
            original_report_time);

  {
    // The report time is now considered in the past so it is adjusted.
    std::optional<base::Time> new_report_time =
        storage_->AdjustOfflineReportTimes(
            /*now=*/original_report_time + base::Minutes(1),
            /*min_delay=*/base::Minutes(1),
            /*max_delay=*/base::Minutes(2));
    EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
              new_report_time);
    ASSERT_TRUE(new_report_time.has_value());
    EXPECT_GE(new_report_time.value(), original_report_time + base::Minutes(2));
    EXPECT_LE(new_report_time.value(), original_report_time + base::Minutes(3));
  }
}

TEST_F(AggregationServiceStorageSqlTest,
       AdjustOfflineReportTimes_SupportsZeroMinAndConstantDelay) {
  OpenDatabase();

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();

  base::Time original_report_time = request.shared_info().scheduled_report_time;

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
            original_report_time);

  {
    // The delay can be constant (i.e. min delay can be equal to max delay)
    std::optional<base::Time> new_report_time =
        storage_->AdjustOfflineReportTimes(
            /*now=*/original_report_time + base::Minutes(1),
            /*min_delay=*/base::Minutes(1),
            /*max_delay=*/base::Minutes(1));

    EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
              new_report_time);
    ASSERT_TRUE(new_report_time.has_value());
    EXPECT_EQ(new_report_time.value(), original_report_time + base::Minutes(2));
  }

  {
    // The min delay can be zero
    std::optional<base::Time> new_report_time =
        storage_->AdjustOfflineReportTimes(
            /*now=*/original_report_time + base::Minutes(5),
            /*min_delay=*/base::Minutes(0),
            /*max_delay=*/base::Minutes(1));
    EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
              new_report_time);
    ASSERT_TRUE(new_report_time.has_value());
    EXPECT_GE(new_report_time.value(), original_report_time + base::Minutes(5));
    EXPECT_LE(new_report_time.value(), original_report_time + base::Minutes(6));
  }
}

TEST_F(AggregationServiceStorageSqlTest,
       AdjustOfflineReportTimes_MultipleReports) {
  OpenDatabase();

  constexpr auto kExampleTime =
      base::Time::FromMillisecondsSinceUnixEpoch(1652984901234);

  std::vector<base::Time> scheduled_report_times = {
      kExampleTime, kExampleTime + base::Hours(1),
      kExampleTime - base::Hours(1)};

  for (base::Time scheduled_report_time : scheduled_report_times) {
    AggregatableReportRequest example_request =
        aggregation_service::CreateExampleRequest();
    AggregatableReportSharedInfo shared_info =
        example_request.shared_info().Clone();
    shared_info.scheduled_report_time = scheduled_report_time;

    std::optional<AggregatableReportRequest> request =
        AggregatableReportRequest::Create(
            example_request.payload_contents(), std::move(shared_info),
            AggregatableReportRequest::DelayType::ScheduledWithReducedDelay);
    ASSERT_TRUE(request.has_value());

    storage_->StoreRequest(std::move(request.value()));
  }

  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
            kExampleTime - base::Hours(1));
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), 3u);

  // Should only affect the third report.
  EXPECT_EQ(storage_->AdjustOfflineReportTimes(
                /*now=*/kExampleTime,
                /*min_delay=*/base::Minutes(1),
                /*max_delay=*/base::Minutes(1)),
            kExampleTime);

  // Next report is now the first.
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()), kExampleTime);

  // After that is the third report with a delay applied
  EXPECT_EQ(storage_->NextReportTimeAfter(kExampleTime),
            kExampleTime + base::Minutes(1));

  // Finally there's the unaffected second report.
  EXPECT_EQ(storage_->NextReportTimeAfter(kExampleTime + base::Minutes(1)),
            kExampleTime + base::Hours(1));

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

// This test verifies that `AggregationServiceStorageSql::Open()` deletes the
// database file when it encounters catastrophic errors.
TEST_F(AggregationServiceStorageSqlTest,
       AdjustOfflineReportTimes_DiskCorruption) {
  constexpr auto kExampleTime =
      base::Time::FromMillisecondsSinceUnixEpoch(1701791394350);

  // Ensure the database schema is initialized. This is kind of an off-label use
  // of StoreRequest(), and could be replaced with any other method that calls
  // `EnsureDatabaseOpen(DbCreationPolicy::kCreateIfAbsent)`.
  OpenDatabase();
  storage_->StoreRequest(CreateExampleRequestWithDelayType());
  CloseDatabase();

  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path()));
  OpenDatabase();

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    storage_->AdjustOfflineReportTimes(kExampleTime, base::Hours(1),
                                       base::Hours(2));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  EXPECT_FALSE(base::PathExists(db_path()));

  // Internally, `sql::Database::Open()` will attempt to open the database a
  // second time when it detects that it was poisoned during the first attempt.
  histograms_.ExpectUniqueSample(
      "PrivacySandbox.AggregationService.Storage.Sql.Error",
      base::checked_cast<base::HistogramBase::Sample>(
          sql::SqliteLoggedResultCode::kCorrupt),
      /*expected_bucket_count=*/2);

  histograms_.ExpectBucketCount(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus",
      AggregationServiceStorageSql::InitStatus::kFailedToOpenDbFile, 1);
}

TEST_F(AggregationServiceStorageSqlTest, StoreRequest_RespectsLimit) {
  size_t example_limit = 10;
  OpenDatabase(example_limit);

  for (size_t i = 0; i < example_limit; ++i) {
    EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), i);

    storage_->StoreRequest(CreateExampleRequestWithDelayType());
  }

  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            example_limit);

  // Storing one more report will silently fail.
  storage_->StoreRequest(CreateExampleRequestWithDelayType());
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            example_limit);

  // Deleting a request frees up space.
  storage_->DeleteRequest(RequestId{5});
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            example_limit - 1);

  // We can then store another request.
  storage_->StoreRequest(CreateExampleRequestWithDelayType());
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            example_limit);

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
  histograms_.ExpectBucketCount(
      "PrivacySandbox.AggregationService.Storage.Sql.StoreRequestHasCapacity",
      true, example_limit + 1);
  histograms_.ExpectBucketCount(
      "PrivacySandbox.AggregationService.Storage.Sql.StoreRequestHasCapacity",
      false, 1);
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "StoredRequestsPerReportingOrigin",
      example_limit + 2);
}

TEST_F(AggregationServiceStorageSqlTest, StoreRequest_LimitIsScopedCorrectly) {
  size_t example_limit = 10;
  OpenDatabase(example_limit);

  for (size_t i = 0; i < example_limit; ++i) {
    EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), i);

    storage_->StoreRequest(CreateExampleRequestWithDelayType());
  }

  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            example_limit);

  // Storing one more report will silently fail.
  storage_->StoreRequest(CreateExampleRequestWithDelayType());
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            example_limit);

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  // Different APIs using the same reporting origin share a limit so storage
  // will silently fail.
  AggregatableReportSharedInfo different_api_shared_info =
      example_request.shared_info().Clone();
  different_api_shared_info.api_identifier = "some-other-api";
  storage_->StoreRequest(
      AggregatableReportRequest::Create(
          example_request.payload_contents(),
          std::move(different_api_shared_info),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
          .value());
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            example_limit);

  // Different reporting origins have separate limits so storage will succeed.
  AggregatableReportSharedInfo different_reporting_origin_shared_info =
      example_request.shared_info().Clone();
  different_reporting_origin_shared_info.reporting_origin =
      url::Origin::Create(GURL("https://some-other-reporting-origin.example"));
  storage_->StoreRequest(
      AggregatableReportRequest::Create(
          example_request.payload_contents(),
          std::move(different_reporting_origin_shared_info),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
          .value());
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            example_limit + 1);

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
  histograms_.ExpectBucketCount(
      "PrivacySandbox.AggregationService.Storage.Sql.StoreRequestHasCapacity",
      true, example_limit + 1);
  histograms_.ExpectBucketCount(
      "PrivacySandbox.AggregationService.Storage.Sql.StoreRequestHasCapacity",
      false, 2);
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "StoredRequestsPerReportingOrigin",
      example_limit + 3);
}

TEST_F(AggregationServiceStorageSqlTest,
       StoreRequestWithDebugKey_DeserializedWithDebugKey) {
  OpenDatabase();

  EXPECT_FALSE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_TRUE(GetRequestsReportingOnOrBefore(base::Time::Max()).empty());

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();
  AggregatableReportSharedInfo shared_info =
      example_request.shared_info().Clone();
  shared_info.debug_mode = AggregatableReportSharedInfo::DebugMode::kEnabled;

  AggregatableReportRequest request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(), std::move(shared_info),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay,
          /*reporting_path=*/std::string(), /*debug_key=*/1234)
          .value();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));

  std::vector<AggregationServiceStorage::RequestAndId> stored_requests_and_ids =
      GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests_and_ids.size(), 1u);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests_and_ids[0].request, request));

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest,
       StoreRequestWithAdditionalFields_DeserializedWithFields) {
  OpenDatabase();

  EXPECT_FALSE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_TRUE(GetRequestsReportingOnOrBefore(base::Time::Max()).empty());

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregatableReportRequest request =
      AggregatableReportRequest::Create(
          example_request.payload_contents(),
          example_request.shared_info().Clone(),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay,
          /*reporting_path=*/std::string(),
          /*debug_key=*/std::nullopt,
          /*additional_fields=*/{{"additional_key", "example_value"}})
          .value();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));

  std::vector<AggregationServiceStorage::RequestAndId> stored_requests_and_ids =
      GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests_and_ids.size(), 1u);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests_and_ids[0].request, request));

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest,
       StoreRequestWithCoordinatorOrigin_DeserializedWithOrigin) {
  ::aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting
      scoped_coordinator_allowlist(
          {url::Origin::Create(GURL("https://coordinator.example"))});

  OpenDatabase();

  EXPECT_FALSE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_TRUE(GetRequestsReportingOnOrBefore(base::Time::Max()).empty());

  AggregatableReportRequest example_request =
      aggregation_service::CreateExampleRequest();

  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.aggregation_coordinator_origin =
      url::Origin::Create(GURL("https://coordinator.example"));

  AggregatableReportRequest request =
      AggregatableReportRequest::Create(
          payload_contents, example_request.shared_info().Clone(),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay,
          /*reporting_path=*/std::string(),
          /*debug_key=*/std::nullopt,
          /*additional_fields=*/{})
          .value();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));

  std::vector<AggregationServiceStorage::RequestAndId> stored_requests_and_ids =
      GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests_and_ids.size(), 1u);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests_and_ids[0].request, request));
  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlInMemoryTest,
       DatabaseInMemoryReopened_RequestsNotPersisted) {
  base::HistogramTester histograms;
  OpenDatabase();

  AggregatableReportRequest request = CreateExampleRequestWithDelayType();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), 1u);

  CloseDatabase();

  OpenDatabase();

  EXPECT_FALSE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_TRUE(GetRequestsReportingOnOrBefore(base::Time::Max()).empty());
  histograms.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

TEST_F(AggregationServiceStorageSqlTest,
       AggregationCoordinatorAllowlistChanges_ReportDeleted) {
  std::optional<
      ::aggregation_service::ScopedAggregationCoordinatorAllowlistForTesting>
      scoped_coordinator_allowlist;

  scoped_coordinator_allowlist.emplace(
      {url::Origin::Create(GURL("https://a.test"))});

  OpenDatabase();

  AggregatableReportRequest example_request =
      CreateExampleRequestWithDelayType();

  AggregationServicePayloadContents payload_contents =
      example_request.payload_contents();
  payload_contents.aggregation_coordinator_origin =
      url::Origin::Create(GURL("https://a.test"));

  storage_->StoreRequest(
      AggregatableReportRequest::Create(
          payload_contents, example_request.shared_info().Clone(),
          AggregatableReportRequest::DelayType::ScheduledWithReducedDelay)
          .value());
  EXPECT_EQ(GetRequestsReportingOnOrBefore(base::Time::Max()).size(), 1u);

  // If the origin is removed from the allowlist, the report is dropped.
  scoped_coordinator_allowlist.reset();
  scoped_coordinator_allowlist.emplace(
      {url::Origin::Create(GURL("https://b.test"))});

  EXPECT_TRUE(GetRequestsReportingOnOrBefore(base::Time::Max()).empty());

  // Check that the report is not just ignored, but actually deleted.
  scoped_coordinator_allowlist.reset();
  scoped_coordinator_allowlist.emplace(
      {url::Origin::Create(GURL("https://a.test"))});

  EXPECT_TRUE(GetRequestsReportingOnOrBefore(base::Time::Max()).empty());

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql."
      "RequestDelayFromUpdatedReportTime2",
      0);
}

class AggregationServiceStorageSqlMigrationsTest
    : public AggregationServiceStorageSqlTest {
 public:
  AggregationServiceStorageSqlMigrationsTest() = default;

  void MigrateDatabase() {
    AggregationServiceStorageSql storage(
        /*run_in_memory=*/false, temp_directory_.GetPath(), &clock_);

    // We need to run an operation on storage to force the lazy initialization.
    std::ignore = storage.NextReportTimeAfter(base::Time::Min());
  }

  void LoadDatabase(int version_id, const base::FilePath* db_path = nullptr) {
    std::string contents = GetDatabaseData(version_id);
    ASSERT_FALSE(contents.empty());

    sql::Database db;
    // Use `db_path()` if none is specified.
    ASSERT_TRUE(db.Open(db_path ? *db_path : this->db_path()));
    ASSERT_TRUE(db.Execute(contents));
  }

  std::string GetCurrentSchema() {
    base::FilePath current_version_path = temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("TestCurrentVersion.db"));
    LoadDatabase(AggregationServiceStorageSql::kCurrentVersionNumber,
                 &current_version_path);
    sql::Database db;
    EXPECT_TRUE(db.Open(current_version_path));
    return db.GetSchema();
  }

  static int VersionFromDatabase(sql::Database* db) {
    sql::Statement statement(
        db->GetUniqueStatement("SELECT value FROM meta WHERE key='version'"));
    if (!statement.Step())
      return 0;
    return statement.ColumnInt(0);
  }

 private:
  // Returns empty string in case of an error.
  std::string GetDatabaseData(int version_id) {
    base::FilePath source_path;
    base::PathService::Get(content::DIR_TEST_DATA, &source_path);
    source_path = source_path.Append(base::FilePath::FromASCII(
        base::StrCat({"aggregation_service/databases/version_",
                      base::NumberToString(version_id), ".sql"})));

    if (!base::PathExists(source_path))
      return std::string();

    std::string contents;
    base::ReadFileToString(source_path, &contents);

    return contents;
  }
};

TEST_F(AggregationServiceStorageSqlMigrationsTest, MigrateEmptyToCurrent) {
  {
    OpenDatabase();

    // We need to perform an operation that is non-trivial on an empty database
    // to force initialization.
    storage_->StoreRequest(CreateExampleRequestWithDelayType());

    CloseDatabase();
  }

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(VersionFromDatabase(&db),
              AggregationServiceStorageSql::kCurrentVersionNumber);

    EXPECT_TRUE(db.DoesTableExist("urls"));
    EXPECT_TRUE(db.DoesTableExist("keys"));
    EXPECT_TRUE(db.DoesTableExist("report_requests"));
    EXPECT_TRUE(db.DoesTableExist("meta"));

    EXPECT_EQ(db.GetSchema(), GetCurrentSchema());
  }

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql.CreationTime2",
      base::ThreadTicks::IsSupported() ? 1 : 0);
  histograms_.ExpectUniqueSample(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus",
      AggregationServiceStorageSql::InitStatus::kSuccess, 1);
}

// Note: We should add a MigrateLatestDeprecatedVersion test when we first
// deprecate a version.

TEST_F(AggregationServiceStorageSqlMigrationsTest, MigrateVersion1ToCurrent) {
  LoadDatabase(/*version_id=*/1);

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));
    ASSERT_FALSE(db.DoesTableExist("report_requests"));

    sql::Statement s(db.GetUniqueStatement("SELECT * FROM urls"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(s.ColumnString(1), "https://url.example/path");  // url
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(VersionFromDatabase(&db),
              AggregationServiceStorageSql::kCurrentVersionNumber);

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(db.GetSchema()), RemoveQuotes(GetCurrentSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(db.GetUniqueStatement("SELECT * FROM urls"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(s.ColumnString(1), "https://url.example/path");  // url
    ASSERT_FALSE(s.Step());
  }

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql.CreationTime2", 0);
  histograms_.ExpectUniqueSample(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus",
      AggregationServiceStorageSql::InitStatus::kSuccess, 1);
}

TEST_F(AggregationServiceStorageSqlMigrationsTest, MigrateVersion2ToCurrent) {
  LoadDatabase(/*version_id=*/2);

  // Verify pre-conditions.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));
    ASSERT_TRUE(db.DoesTableExist("report_requests"));
    ASSERT_FALSE(db.DoesIndexExist("reporting_origin_idx"));

    sql::Statement s(db.GetUniqueStatement("SELECT * FROM report_requests"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(s.ColumnString(3),
              "https://reporting.example");  // reporting_origin
    ASSERT_FALSE(s.Step());
  }

  MigrateDatabase();

  // Verify schema is current.
  {
    sql::Database db;
    ASSERT_TRUE(db.Open(db_path()));

    EXPECT_EQ(VersionFromDatabase(&db),
              AggregationServiceStorageSql::kCurrentVersionNumber);

    // Compare without quotes as sometimes migrations cause table names to be
    // string literals.
    EXPECT_EQ(RemoveQuotes(db.GetSchema()), RemoveQuotes(GetCurrentSchema()));

    // Verify that data is preserved across the migration.
    sql::Statement s(db.GetUniqueStatement("SELECT * FROM report_requests"));

    ASSERT_TRUE(s.Step());
    ASSERT_EQ(s.ColumnString(3),
              "https://reporting.example");  // reporting_origin
    ASSERT_FALSE(s.Step());
  }

  histograms_.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql.CreationTime2", 0);
  histograms_.ExpectUniqueSample(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus",
      AggregationServiceStorageSql::InitStatus::kSuccess, 1);
}

}  // namespace content
