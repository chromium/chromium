// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

namespace {

using aggregation_service::RequestIdIs;
using testing::ElementsAre;

using RequestId = AggregationServiceStorage::RequestId;

const char kExampleUrl[] =
    "https://helper.test/.well-known/aggregation-service/keys.json";

const std::vector<PublicKey> kExampleKeys{
    aggregation_service::GenerateKey("dummy_id").public_key};

}  // namespace

class AggregationServiceStorageSqlTest : public testing::Test {
 public:
  AggregationServiceStorageSqlTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    clock_.SetNow(base::Time::Now());
  }

  void OpenDatabase() {
    storage_ = std::make_unique<AggregationServiceStorageSql>(
        /*run_in_memory=*/false, temp_directory_.GetPath(), &clock_);
  }

  void CloseDatabase() { storage_.reset(); }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("AggregationService"));
  }

 protected:
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

 protected:
  std::unique_ptr<AggregationServiceStorage> storage_;
  base::SimpleTestClock clock_;
};

TEST_F(AggregationServiceStorageSqlTest,
       DBInitializationSucceeds_HistogramRecorded) {
  base::HistogramTester histograms;

  OpenDatabase();
  GURL url(kExampleUrl);
  PublicKeyset keyset(kExampleKeys, /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, keyset);
  CloseDatabase();

  histograms.ExpectUniqueSample(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus",
      AggregationServiceStorageSql::InitStatus::kSuccess, 1);
}

TEST_F(AggregationServiceStorageSqlTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  base::HistogramTester histograms;

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
  histograms.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql.CreationTime", 0);

  // Storing a public key should create and initialize the database.
  OpenDatabase();
  PublicKeyset keyset(kExampleKeys, /*fetch_time=*/clock_.Now(),
                      /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, keyset);
  CloseDatabase();

  // DB creation UMA should be recorded.
  histograms.ExpectTotalCount(
      "PrivacySandbox.AggregationService.Storage.Sql.CreationTime", 1);

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [urls], [keys], [report_requests], [meta], [sqlite_sequence] (for
    // AUTOINCREMENT support).
    EXPECT_EQ(sql::test::CountSQLTables(&raw_db), 5u);

    // [urls_by_url_idx], [fetch_time_idx], [expiry_time_idx],
    // [report_time_idx], [creation_time_idx] and meta table index.
    EXPECT_EQ(sql::test::CountSQLIndices(&raw_db), 6u);
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
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("bcde").public_key};

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
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("bcde").public_key};

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
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("bcde").public_key};

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
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("bcde").public_key};

  PublicKeyset old_keyset(old_keys, /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());
  storage_->SetPublicKeys(url, old_keyset);
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      old_keys, storage_->GetPublicKeys(url)));

  std::vector<PublicKey> expected_keys{
      aggregation_service::GenerateKey("efgh").public_key,
      aggregation_service::GenerateKey("fghi").public_key};

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
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("bcde").public_key};
  storage_->SetPublicKeys(url_1,
                          PublicKeyset(keys_1, /*fetch_time=*/clock_.Now(),
                                       /*expiry_time=*/base::Time::Max()));

  clock_.Advance(base::Days(3));

  GURL url_2("https://b.com/keys");
  std::vector<PublicKey> keys_2{
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("efgh").public_key};
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
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("bcde").public_key};
  storage_->SetPublicKeys(url_1,
                          PublicKeyset(keys_1, /*fetch_time=*/clock_.Now(),
                                       /*expiry_time=*/base::Time::Max()));

  clock_.Advance(base::Days(1));

  GURL url_2("https://b.com/keys");
  std::vector<PublicKey> keys_2{
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("efgh").public_key};
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
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("bcde").public_key};
  storage_->SetPublicKeys(url_1,
                          PublicKeyset(keys_1, /*fetch_time=*/clock_.Now(),
                                       /*expiry_time=*/base::Time::Max()));

  clock_.Advance(base::Days(1));

  GURL url_2("https://b.com/keys");
  std::vector<PublicKey> keys_2{
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("efgh").public_key};
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
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("bcde").public_key};
  storage_->SetPublicKeys(url_1,
                          PublicKeyset(keys_1, /*fetch_time=*/now,
                                       /*expiry_time=*/now + base::Days(1)));

  GURL url_2("https://b.com/keys");
  std::vector<PublicKey> keys_2{
      aggregation_service::GenerateKey("abcd").public_key,
      aggregation_service::GenerateKey("efgh").public_key};
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

    meta.SetVersionNumber(meta.GetVersionNumber() + 1);
    meta.SetCompatibleVersionNumber(meta.GetVersionNumber() + 1);
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
  EXPECT_TRUE(
      storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).empty());

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  ASSERT_TRUE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()).value(),
            request.shared_info().scheduled_report_time);

  std::vector<AggregationServiceStorage::RequestAndId> stored_requests_and_ids =
      storage_->GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests_and_ids.size(), 1u);

  // IDs autoincrement from 1.
  EXPECT_EQ(stored_requests_and_ids[0].id, RequestId(1));
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests_and_ids[0].request, request));
}

TEST_F(AggregationServiceStorageSqlTest, DeleteRequest_ExpectedResult) {
  OpenDatabase();

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  EXPECT_EQ(storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            1u);

  // IDs autoincrement from 1.
  storage_->DeleteRequest(RequestId(1));
  EXPECT_TRUE(
      storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).empty());
}

TEST_F(AggregationServiceStorageSqlTest,
       RepeatGetPendingRequests_RequestReturnedAgain) {
  OpenDatabase();

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  ASSERT_TRUE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()).value(),
            request.shared_info().scheduled_report_time);

  std::vector<AggregationServiceStorage::RequestAndId> stored_requests =
      storage_->GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests.size(), 1u);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests[0].request, request));

  stored_requests = storage_->GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests.size(), 1u);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests[0].request, request));
  ASSERT_TRUE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()).value(),
            request.shared_info().scheduled_report_time);
}

TEST_F(AggregationServiceStorageSqlTest, DatabaseReopened_RequestsPersisted) {
  OpenDatabase();

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));

  CloseDatabase();

  OpenDatabase();

  std::vector<AggregationServiceStorage::RequestAndId> stored_requests =
      storage_->GetRequestsReportingOnOrBefore(base::Time::Max());

  ASSERT_EQ(stored_requests.size(), 1u);
  EXPECT_TRUE(aggregation_service::ReportRequestsEqual(
      stored_requests[0].request, request));
}

TEST_F(AggregationServiceStorageSqlTest,
       GetRequestsReportingOnOrBefore_ReturnValuesAlignWithReportTime) {
  OpenDatabase();

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
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
    EXPECT_EQ(storage_->GetRequestsReportingOnOrBefore(test_case.not_after_time)
                  .size(),
              test_case.number_requests)
        << test_case.not_after_time;
  }
}

TEST_F(AggregationServiceStorageSqlTest, GetRequests_ReturnValuesAlignWithIds) {
  OpenDatabase();

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

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

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();
  base::Time report_time = request.shared_info().scheduled_report_time;

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));

  const struct {
    base::Time strictly_after_time;
    absl::optional<base::Time> expected_return_value;
  } kTestCases[] = {
      {base::Time::Min(), report_time},
      {report_time - base::Seconds(1), report_time},
      {report_time, absl::nullopt},
      {report_time + base::Seconds(1), absl::nullopt},
      {base::Time::Max(), absl::nullopt},

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

  const base::Time kExampleTime = base::Time::FromJavaTime(1652984901234);

  std::vector<base::Time> scheduled_report_times = {
      kExampleTime, kExampleTime, kExampleTime + base::Hours(1)};

  for (base::Time scheduled_report_time : scheduled_report_times) {
    AggregatableReportRequest example_request =
        aggregation_service::CreateExampleRequest();
    AggregatableReportSharedInfo shared_info =
        example_request.shared_info().Clone();
    shared_info.scheduled_report_time = scheduled_report_time;

    absl::optional<AggregatableReportRequest> request =
        AggregatableReportRequest::Create(example_request.payload_contents(),
                                          std::move(shared_info));
    ASSERT_TRUE(request.has_value());

    storage_->StoreRequest(std::move(request.value()));
  }
  ASSERT_TRUE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()).value(),
            kExampleTime);

  EXPECT_TRUE(
      storage_
          ->GetRequestsReportingOnOrBefore(kExampleTime - base::Milliseconds(1))
          .empty());

  ASSERT_TRUE(
      storage_->NextReportTimeAfter(kExampleTime - base::Milliseconds(1))
          .has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(kExampleTime - base::Milliseconds(1))
                .value(),
            kExampleTime);

  std::vector<AggregationServiceStorage::RequestAndId> example_time_reports =
      storage_->GetRequestsReportingOnOrBefore(kExampleTime);
  ASSERT_EQ(example_time_reports.size(), 2u);

  EXPECT_EQ(base::flat_set<RequestId>(
                {example_time_reports[0].id, example_time_reports[1].id}),
            // Request IDs autoincrement from 1.
            base::flat_set<RequestId>({RequestId(1), RequestId(2)}));

  ASSERT_TRUE(storage_->NextReportTimeAfter(kExampleTime).has_value());
  EXPECT_EQ(storage_->NextReportTimeAfter(kExampleTime).value(),
            kExampleTime + base::Hours(1));

  EXPECT_EQ(storage_
                ->GetRequestsReportingOnOrBefore(kExampleTime + base::Hours(1) -
                                                 base::Milliseconds(1))
                .size(),
            2u);

  std::vector<AggregationServiceStorage::RequestAndId> all_reports =
      storage_->GetRequestsReportingOnOrBefore(kExampleTime + base::Hours(1));
  ASSERT_EQ(all_reports.size(), 3u);
  EXPECT_EQ(all_reports[2].id, RequestId(3));

  EXPECT_FALSE(
      storage_->NextReportTimeAfter(kExampleTime + base::Hours(1)).has_value());
  EXPECT_EQ(storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            3u);
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearAllDataWithoutFilter_AllRequestsDeleted) {
  OpenDatabase();

  storage_->StoreRequest(aggregation_service::CreateExampleRequest());
  storage_->StoreRequest(aggregation_service::CreateExampleRequest());

  EXPECT_EQ(storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            2u);

  storage_->ClearDataBetween(base::Time(), base::Time(), base::NullCallback());

  EXPECT_EQ(storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            0u);
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearDataBetween_RequestsTimeRangeDeleted) {
  OpenDatabase();

  const base::Time kExampleTime = base::Time::FromJavaTime(1652984901234);

  clock_.SetNow(kExampleTime);
  storage_->StoreRequest(aggregation_service::CreateExampleRequest());

  clock_.Advance(base::Hours(1));
  storage_->StoreRequest(aggregation_service::CreateExampleRequest());

  clock_.Advance(base::Hours(1));
  storage_->StoreRequest(aggregation_service::CreateExampleRequest());

  EXPECT_EQ(storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            3u);

  // As the times are inclusive, this should delete the first two requests.
  storage_->ClearDataBetween(kExampleTime, kExampleTime + base::Hours(1),
                             base::NullCallback());

  std::vector<AggregationServiceStorage::RequestAndId> stored_reports =
      storage_->GetRequestsReportingOnOrBefore(base::Time::Max());
  ASSERT_EQ(stored_reports.size(), 1u);

  // Only the last request should be left. Request IDs start from 1.
  EXPECT_EQ(stored_reports[0].id, RequestId(3));
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearDataAllTimesWithFilter_OnlyRequestsSpecifiedAreDeleted) {
  const url::Origin reporting_origins[] = {
      url::Origin::Create(GURL("https://a.example")),
      url::Origin::Create(GURL("https://b.example")),
      url::Origin::Create(GURL("https://c.example"))};

  OpenDatabase();

  for (const url::Origin& reporting_origin : reporting_origins) {
    AggregatableReportRequest example_request =
        aggregation_service::CreateExampleRequest();
    AggregatableReportSharedInfo shared_info =
        example_request.shared_info().Clone();
    shared_info.reporting_origin = reporting_origin;
    storage_->StoreRequest(
        AggregatableReportRequest::Create(example_request.payload_contents(),
                                          std::move(shared_info))
            .value());
  }

  EXPECT_EQ(storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            3u);

  storage_->ClearDataBetween(
      base::Time::Min(), base::Time::Max(),
      base::BindLambdaForTesting(
          [&reporting_origins](const blink::StorageKey& storage_key) {
            return storage_key != blink::StorageKey(reporting_origins[2]);
          }));

  std::vector<AggregationServiceStorage::RequestAndId> stored_reports =
      storage_->GetRequestsReportingOnOrBefore(base::Time::Max());
  ASSERT_EQ(stored_reports.size(), 1u);

  // Only the last request should be left. Request IDs start from 1.
  EXPECT_EQ(stored_reports[0].id, RequestId(3));
}

TEST_F(AggregationServiceStorageSqlTest,
       AdjustOfflineReportTimes_AffectsPastReportsOnly) {
  OpenDatabase();

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

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
    absl::optional<base::Time> new_report_time =
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

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  base::Time original_report_time = request.shared_info().scheduled_report_time;

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
            original_report_time);

  {
    // The delay can be constant (i.e. min delay can be equal to max delay)
    absl::optional<base::Time> new_report_time =
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
    absl::optional<base::Time> new_report_time =
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

  const base::Time kExampleTime = base::Time::FromJavaTime(1652984901234);

  std::vector<base::Time> scheduled_report_times = {
      kExampleTime, kExampleTime + base::Hours(1),
      kExampleTime - base::Hours(1)};

  for (base::Time scheduled_report_time : scheduled_report_times) {
    AggregatableReportRequest example_request =
        aggregation_service::CreateExampleRequest();
    AggregatableReportSharedInfo shared_info =
        example_request.shared_info().Clone();
    shared_info.scheduled_report_time = scheduled_report_time;

    absl::optional<AggregatableReportRequest> request =
        AggregatableReportRequest::Create(example_request.payload_contents(),
                                          std::move(shared_info));
    ASSERT_TRUE(request.has_value());

    storage_->StoreRequest(std::move(request.value()));
  }

  EXPECT_EQ(storage_->NextReportTimeAfter(base::Time::Min()),
            kExampleTime - base::Hours(1));
  EXPECT_EQ(storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            3u);

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
}

TEST_F(AggregationServiceStorageSqlInMemoryTest,
       DatabaseInMemoryReopened_RequestsNotPersisted) {
  OpenDatabase();

  AggregatableReportRequest request =
      aggregation_service::CreateExampleRequest();

  storage_->StoreRequest(aggregation_service::CloneReportRequest(request));
  EXPECT_EQ(storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).size(),
            1u);

  CloseDatabase();

  OpenDatabase();

  EXPECT_FALSE(storage_->NextReportTimeAfter(base::Time::Min()).has_value());
  EXPECT_TRUE(
      storage_->GetRequestsReportingOnOrBefore(base::Time::Max()).empty());
}

}  // namespace content
