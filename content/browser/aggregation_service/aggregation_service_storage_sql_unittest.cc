// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "sql/database.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

PublicKeysForOrigin CreateDummyKeys() {
  return PublicKeysForOrigin(
      url::Origin::Create(GURL("https://a.com")),
      {PublicKey(/*id=*/"dummy_id", /*key=*/kABCD1234AsBytes)});
}

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
  PublicKeysForOrigin keys = CreateDummyKeys();
  storage_->SetPublicKeys(keys, /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());
  CloseDatabase();

  histograms.ExpectUniqueSample(
      "PrivacySandbox.AggregationService.Storage.Sql.InitStatus",
      AggregationServiceStorageSql::InitStatus::kSuccess, 1);
}

TEST_F(AggregationServiceStorageSqlTest,
       DatabaseInitialized_TablesAndIndexesLazilyInitialized) {
  OpenDatabase();
  CloseDatabase();

  // An unused AggregationServiceStorageSql instance should not create the
  // database.
  EXPECT_FALSE(base::PathExists(db_path()));

  PublicKeysForOrigin keys = CreateDummyKeys();

  // Operations which don't need to run on an empty database should not create
  // the database.
  OpenDatabase();
  EXPECT_TRUE(storage_->GetPublicKeys(keys.origin).keys.empty());
  CloseDatabase();

  EXPECT_FALSE(base::PathExists(db_path()));

  // Storing a public key should create and initialize the database.
  OpenDatabase();
  storage_->SetPublicKeys(keys, /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());
  CloseDatabase();

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [origins], [keys], [meta].
    EXPECT_EQ(sql::test::CountSQLTables(&raw_db), 3u);

    // [origins_by_origin_idx], [fetch_time_idx], [expiry_time_idx] and meta
    // table index.
    EXPECT_EQ(sql::test::CountSQLIndices(&raw_db), 4u);
  }
}

TEST_F(AggregationServiceStorageSqlTest, DatabaseReopened_DataPersisted) {
  OpenDatabase();

  PublicKeysForOrigin keys = CreateDummyKeys();
  storage_->SetPublicKeys(keys, /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());
  EXPECT_EQ(storage_->GetPublicKeys(keys.origin).keys.size(), 1u);
  CloseDatabase();

  OpenDatabase();
  EXPECT_EQ(storage_->GetPublicKeys(keys.origin).keys.size(), 1u);
}

TEST_F(AggregationServiceStorageSqlTest, SetPublicKeys_ExpectedResult) {
  OpenDatabase();

  std::vector<PublicKey> expected_keys{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"bcde", /*key=*/kEFGH5678AsBytes)};

  url::Origin origin = url::Origin::Create(GURL("https://a.com"));

  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin, expected_keys),
                          /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());
  PublicKeysForOrigin keys_for_origin = storage_->GetPublicKeys(origin);
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(expected_keys,
                                                   keys_for_origin.keys));

  CloseDatabase();
}

TEST_F(AggregationServiceStorageSqlTest, GetPublicKeysExpired_EmptyResult) {
  OpenDatabase();

  std::vector<PublicKey> expected_keys{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"bcde", /*key=*/kEFGH5678AsBytes),
  };

  base::Time now = clock_.Now();
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));

  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin, expected_keys),
                          /*fetch_time=*/now,
                          /*expiry_time=*/now + base::TimeDelta::FromDays(7));
  clock_.Advance(base::TimeDelta::FromDays(8));
  PublicKeysForOrigin keys_for_origin = storage_->GetPublicKeys(origin);
  EXPECT_TRUE(keys_for_origin.keys.empty());

  CloseDatabase();
}

TEST_F(AggregationServiceStorageSqlTest, ClearPublicKeys) {
  OpenDatabase();

  std::vector<PublicKey> expected_keys{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"bcde", /*key=*/kEFGH5678AsBytes),
  };

  url::Origin origin = url::Origin::Create(GURL("https://a.com"));

  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin, expected_keys),
                          /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());
  storage_->ClearPublicKeys(origin);

  PublicKeysForOrigin keys_for_origin = storage_->GetPublicKeys(origin);
  EXPECT_TRUE(keys_for_origin.keys.empty());

  CloseDatabase();
}

TEST_F(AggregationServiceStorageSqlTest, ReplacePublicKeys) {
  OpenDatabase();

  base::Time now = clock_.Now();
  url::Origin origin = url::Origin::Create(GURL("https://a.com"));

  std::vector<PublicKey> old_keys{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"bcde", /*key=*/kEFGH5678AsBytes),
  };
  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin, old_keys),
                          /*fetch_time=*/now,
                          /*expiry_time=*/base::Time::Max());

  std::vector<PublicKey> expected_keys{
      PublicKey(/*id=*/"efgh", /*key=*/kEFGH5678AsBytes),
      PublicKey(/*id=*/"fghi", /*key=*/kABCD1234AsBytes),
  };
  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin, expected_keys),
                          /*fetch_time=*/now,
                          /*expiry_time=*/base::Time::Max());

  PublicKeysForOrigin keys_for_origin = storage_->GetPublicKeys(origin);
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(expected_keys,
                                                   keys_for_origin.keys));

  CloseDatabase();
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearPublicKeysFetchedBetween_RangeDeleted) {
  OpenDatabase();

  url::Origin origin_1 = url::Origin::Create(GURL("https://a.com"));
  std::vector<PublicKey> keys_1{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"bcde", /*key=*/kEFGH5678AsBytes)};
  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin_1, keys_1),
                          /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());

  clock_.Advance(base::TimeDelta::FromDays(3));

  url::Origin origin_2 = url::Origin::Create(GURL("https://b.com"));
  std::vector<PublicKey> keys_2{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"efgh", /*key=*/kEFGH5678AsBytes)};
  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin_2, keys_2),
                          /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());

  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_1, storage_->GetPublicKeys(origin_1).keys));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(origin_2).keys));

  base::Time now = clock_.Now();
  storage_->ClearPublicKeysFetchedBetween(now - base::TimeDelta::FromDays(5),
                                          now - base::TimeDelta::FromDays(1));

  EXPECT_TRUE(storage_->GetPublicKeys(origin_1).keys.empty());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(origin_2).keys));
}

TEST_F(AggregationServiceStorageSqlTest, ClearAllPublicKeys_AllDeleted) {
  OpenDatabase();

  url::Origin origin_1 = url::Origin::Create(GURL("https://a.com"));
  std::vector<PublicKey> keys_1{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"bcde", /*key=*/kEFGH5678AsBytes)};
  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin_1, keys_1),
                          /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());

  clock_.Advance(base::TimeDelta::FromDays(1));

  url::Origin origin_2 = url::Origin::Create(GURL("https://b.com"));
  std::vector<PublicKey> keys_2{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"efgh", /*key=*/kEFGH5678AsBytes)};
  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin_2, keys_2),
                          /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());

  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_1, storage_->GetPublicKeys(origin_1).keys));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(origin_2).keys));

  storage_->ClearPublicKeysFetchedBetween(base::Time(), base::Time::Max());

  EXPECT_TRUE(storage_->GetPublicKeys(origin_1).keys.empty());
  EXPECT_TRUE(storage_->GetPublicKeys(origin_2).keys.empty());
}

TEST_F(AggregationServiceStorageSqlTest,
       ClearPublicKeysExpiredBy_RangeDeleted) {
  OpenDatabase();

  base::Time now = clock_.Now();

  url::Origin origin_1 = url::Origin::Create(GURL("https://a.com"));
  std::vector<PublicKey> keys_1{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"bcde", /*key=*/kEFGH5678AsBytes)};
  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin_1, keys_1),
                          /*fetch_time=*/now,
                          /*expiry_time=*/now + base::TimeDelta::FromDays(1));

  url::Origin origin_2 = url::Origin::Create(GURL("https://b.com"));
  std::vector<PublicKey> keys_2{
      PublicKey(/*id=*/"abcd", /*key=*/kABCD1234AsBytes),
      PublicKey(/*id=*/"efgh", /*key=*/kEFGH5678AsBytes)};
  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin_2, keys_2),
                          /*fetch_time=*/now,
                          /*expiry_time=*/now + base::TimeDelta::FromDays(3));

  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_1, storage_->GetPublicKeys(origin_1).keys));
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(origin_2).keys));

  storage_->ClearPublicKeysExpiredBy(now + base::TimeDelta::FromDays(1));

  EXPECT_TRUE(storage_->GetPublicKeys(origin_1).keys.empty());
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(
      keys_2, storage_->GetPublicKeys(origin_2).keys));
}

TEST_F(AggregationServiceStorageSqlInMemoryTest,
       DatabaseInMemoryReopened_DataNotPersisted) {
  OpenDatabase();

  PublicKeysForOrigin keys = CreateDummyKeys();
  storage_->SetPublicKeys(keys, /*fetch_time=*/clock_.Now(),
                          /*expiry_time=*/base::Time::Max());
  EXPECT_EQ(storage_->GetPublicKeys(keys.origin).keys.size(), 1u);
  CloseDatabase();

  OpenDatabase();
  EXPECT_EQ(storage_->GetPublicKeys(keys.origin).keys.size(), 0u);
}

}  // namespace content
