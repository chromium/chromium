// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_index_database.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace disk_cache {

class SqlSharedCacheIndexDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<SqlSharedCacheIndexDatabase>(temp_dir_.GetPath());
  }

  void TearDown() override { db_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<SqlSharedCacheIndexDatabase> db_;
};

TEST_F(SqlSharedCacheIndexDatabaseTest, InitializeAndGetSet) {
  EXPECT_TRUE(db_->Initialize().has_value());

  net::NetworkIsolationKey nik1(net::SchemefulSite(GURL("https://foo.test")),
                                net::SchemefulSite(GURL("https://bar.test")));
  net::NetworkIsolationKey nik2(net::SchemefulSite(GURL("https://baz.test")),
                                net::SchemefulSite(GURL("https://qux.test")));

  auto id1_no_create = db_->GetDbIdByNetworkIsolationKey(nik1, false);
  EXPECT_FALSE(id1_no_create.has_value());
  EXPECT_EQ(id1_no_create.error(),
            SqlSharedCacheIndexDatabase::Error::kNotFound);

  auto id1 = db_->GetDbIdByNetworkIsolationKey(nik1, true);
  EXPECT_TRUE(id1.has_value());

  auto id1_again = db_->GetDbIdByNetworkIsolationKey(nik1, false);
  EXPECT_TRUE(id1_again.has_value());
  EXPECT_EQ(id1_again.value(), id1.value());

  auto id2 = db_->GetDbIdByNetworkIsolationKey(nik2, true);
  EXPECT_TRUE(id2.has_value());
  EXPECT_NE(id1.value(), id2.value());

  auto key1 = db_->GetIsolationKeyStringByDbId(id1.value());
  EXPECT_TRUE(key1.has_value());
  EXPECT_EQ(key1.value(), nik1.ToCacheKeyString().value());

  auto key2 = db_->GetIsolationKeyStringByDbId(id2.value());
  EXPECT_TRUE(key2.has_value());
  EXPECT_EQ(key2.value(), nik2.ToCacheKeyString().value());

  auto non_existent_key =
      db_->GetIsolationKeyStringByDbId(SqlSharedCacheDbId(999));
  EXPECT_FALSE(non_existent_key.has_value());
  EXPECT_EQ(non_existent_key.error(),
            SqlSharedCacheIndexDatabase::Error::kNotFound);
}

TEST_F(SqlSharedCacheIndexDatabaseTest, Uninitialized) {
  net::NetworkIsolationKey nik1(net::SchemefulSite(GURL("https://foo.test")),
                                net::SchemefulSite(GURL("https://bar.test")));

  EXPECT_EQ(db_->GetDbIdByNetworkIsolationKey(nik1, false).error(),
            SqlSharedCacheIndexDatabase::Error::kFailedToOpenDatabase);
  EXPECT_EQ(db_->GetIsolationKeyStringByDbId(SqlSharedCacheDbId(1)).error(),
            SqlSharedCacheIndexDatabase::Error::kFailedToOpenDatabase);
  EXPECT_EQ(db_->DeleteByDbId(SqlSharedCacheDbId(1)).error(),
            SqlSharedCacheIndexDatabase::Error::kFailedToOpenDatabase);
}

TEST_F(SqlSharedCacheIndexDatabaseTest, DatabaseFailure) {
  db_->SetSimulateDbFailureForTesting(true);
  EXPECT_EQ(db_->Initialize().error(),
            SqlSharedCacheIndexDatabase::Error::kFailedForTesting);

  net::NetworkIsolationKey nik1(net::SchemefulSite(GURL("https://foo.test")),
                                net::SchemefulSite(GURL("https://bar.test")));
  EXPECT_EQ(db_->GetDbIdByNetworkIsolationKey(nik1, false).error(),
            SqlSharedCacheIndexDatabase::Error::kFailedForTesting);
  EXPECT_EQ(db_->GetIsolationKeyStringByDbId(SqlSharedCacheDbId(1)).error(),
            SqlSharedCacheIndexDatabase::Error::kFailedForTesting);
  EXPECT_EQ(db_->DeleteByDbId(SqlSharedCacheDbId(1)).error(),
            SqlSharedCacheIndexDatabase::Error::kFailedForTesting);
}

TEST_F(SqlSharedCacheIndexDatabaseTest, Persistence) {
  EXPECT_TRUE(db_->Initialize().has_value());

  net::NetworkIsolationKey nik1(net::SchemefulSite(GURL("https://foo.test")),
                                net::SchemefulSite(GURL("https://bar.test")));
  auto id1 = db_->GetDbIdByNetworkIsolationKey(nik1, true);
  EXPECT_TRUE(id1.has_value());

  // Re-create the database object to test persistence
  db_ = std::make_unique<SqlSharedCacheIndexDatabase>(temp_dir_.GetPath());
  EXPECT_TRUE(db_->Initialize().has_value());

  auto id1_again = db_->GetDbIdByNetworkIsolationKey(nik1, false);
  EXPECT_TRUE(id1_again.has_value());
  EXPECT_EQ(id1_again.value(), id1.value());
}

TEST_F(SqlSharedCacheIndexDatabaseTest, DeleteByDbId) {
  EXPECT_TRUE(db_->Initialize().has_value());

  net::NetworkIsolationKey nik1(net::SchemefulSite(GURL("https://foo.test")),
                                net::SchemefulSite(GURL("https://bar.test")));

  auto id1 = db_->GetDbIdByNetworkIsolationKey(nik1, true);
  EXPECT_TRUE(id1.has_value());

  // Delete the entry.
  auto delete_result = db_->DeleteByDbId(id1.value());
  EXPECT_TRUE(delete_result.has_value());

  // Attempt to delete again, should fail.
  auto deleted_again = db_->DeleteByDbId(id1.value());
  EXPECT_FALSE(deleted_again.has_value());
  EXPECT_EQ(deleted_again.error(),
            SqlSharedCacheIndexDatabase::Error::kNotFound);

  // Attempt to get the deleted entry, should fail.
  auto id1_no_create = db_->GetDbIdByNetworkIsolationKey(nik1, false);
  EXPECT_FALSE(id1_no_create.has_value());
  EXPECT_EQ(id1_no_create.error(),
            SqlSharedCacheIndexDatabase::Error::kNotFound);

  // Attempt to get by db_id, should fail.
  auto key1_no_create = db_->GetIsolationKeyStringByDbId(id1.value());
  EXPECT_FALSE(key1_no_create.has_value());
  EXPECT_EQ(key1_no_create.error(),
            SqlSharedCacheIndexDatabase::Error::kNotFound);
}

TEST_F(SqlSharedCacheIndexDatabaseTest, TransientNetworkIsolationKey) {
  EXPECT_TRUE(db_->Initialize().has_value());

  net::NetworkIsolationKey transient_nik;

  auto result = db_->GetDbIdByNetworkIsolationKey(transient_nik, true);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            SqlSharedCacheIndexDatabase::Error::kInvalidNetworkIsolationKey);
}

TEST_F(SqlSharedCacheIndexDatabaseTest, Histograms) {
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(db_->Initialize().has_value());

  histogram_tester.ExpectUniqueSample(
      "Net.SqlSharedCacheIndexDatabase.Initialize.Result",
      SqlSharedCacheIndexDatabase::kNoErrorForMetrics, 1);
  histogram_tester.ExpectTotalCount(
      "Net.SqlSharedCacheIndexDatabase.Initialize.SuccessTime", 1);

  net::NetworkIsolationKey nik1(net::SchemefulSite(GURL("https://foo.test")),
                                net::SchemefulSite(GURL("https://bar.test")));

  // Successful GetDbIdByNetworkIsolationKey
  auto id1 = db_->GetDbIdByNetworkIsolationKey(nik1, true);
  EXPECT_TRUE(id1.has_value());

  histogram_tester.ExpectUniqueSample(
      "Net.SqlSharedCacheIndexDatabase.GetDbIdByNetworkIsolationKey."
      "Result",
      SqlSharedCacheIndexDatabase::kNoErrorForMetrics, 1);
  histogram_tester.ExpectTotalCount(
      "Net.SqlSharedCacheIndexDatabase.GetDbIdByNetworkIsolationKey."
      "SuccessTime",
      1);

  // Failed GetIsolationKeyStringByDbId
  auto non_existent_key =
      db_->GetIsolationKeyStringByDbId(SqlSharedCacheDbId(999));
  EXPECT_FALSE(non_existent_key.has_value());

  histogram_tester.ExpectUniqueSample(
      "Net.SqlSharedCacheIndexDatabase.GetIsolationKeyStringByDbId.Result",
      SqlSharedCacheIndexDatabase::Error::kNotFound, 1);
  histogram_tester.ExpectTotalCount(
      "Net.SqlSharedCacheIndexDatabase.GetIsolationKeyStringByDbId.FailureTime",
      1);
}

}  // namespace disk_cache
