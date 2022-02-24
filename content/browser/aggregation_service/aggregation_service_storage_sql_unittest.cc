// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

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
  std::unique_ptr<AggregationServiceKeyStorage> storage_;
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
  std::unique_ptr<AggregationServiceKeyStorage> storage_;
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

    // [urls], [keys], [meta].
    EXPECT_EQ(sql::test::CountSQLTables(&raw_db), 3u);

    // [urls_by_url_idx], [fetch_time_idx], [expiry_time_idx] and meta
    // table index.
    EXPECT_EQ(sql::test::CountSQLIndices(&raw_db), 4u);
  }
}

TEST_F(AggregationServiceStorageSqlTest, DatabaseReopened_DataPersisted) {
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
       ClearPublicKeysFetchedBetween_RangeDeleted) {
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
  storage_->ClearPublicKeysFetchedBetween(now - base::Days(5),
                                          now - base::Days(1));

  EXPECT_TRUE(storage_->GetPublicKeys(url_1).empty());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(url_2)));
}

TEST_F(AggregationServiceStorageSqlTest, ClearAllPublicKeys_AllDeleted) {
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

  storage_->ClearPublicKeysFetchedBetween(base::Time(), base::Time::Max());

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

TEST_F(AggregationServiceStorageSqlInMemoryTest,
       DatabaseInMemoryReopened_DataNotPersisted) {
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

}  // namespace content
