// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/gurl.h"

namespace storage {

namespace {

// Declared to shorten the line lengths.
static const blink::mojom::StorageType kTemp =
    blink::mojom::StorageType::kTemporary;
static const blink::mojom::StorageType kPerm =
    blink::mojom::StorageType::kPersistent;

const char kDefaultBucket[] = "default";

// TODO(crbug.com/889590): Replace with common converter.
url::Origin ToOrigin(const std::string& url) {
  return url::Origin::Create(GURL(url));
}

}  // namespace

// Test parameter indicates if the database should be created for incognito
// mode. True will create the database in memory.
class QuotaDatabaseTest : public testing::TestWithParam<bool> {
 protected:
  using QuotaTableEntry = QuotaDatabase::QuotaTableEntry;
  using BucketTableEntry = QuotaDatabase::BucketTableEntry;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  void TearDown() override { ASSERT_TRUE(temp_directory_.Delete()); }

  bool use_in_memory_db() const { return GetParam(); }

  base::FilePath DbPath() {
    return temp_directory_.GetPath().AppendASCII("quota_manager.db");
  }

  bool LazyOpen(QuotaDatabase* db, bool create_if_needed) {
    return db->LazyOpen(create_if_needed);
  }

  template <typename EntryType>
  struct EntryVerifier {
    std::set<EntryType> table;

    template <typename Iterator>
    EntryVerifier(Iterator itr, Iterator end)
        : table(itr, end) {}

    bool Run(const EntryType& entry) {
      EXPECT_EQ(1u, table.erase(entry));
      return true;
    }
  };

  bool DumpQuotaTable(QuotaDatabase* quota_database,
                      const QuotaDatabase::QuotaTableCallback& callback) {
    return quota_database->DumpQuotaTable(callback);
  }

  bool DumpBucketTable(QuotaDatabase* quota_database,
                       const QuotaDatabase::BucketTableCallback& callback) {
    return quota_database->DumpBucketTable(callback);
  }

  template <typename Container>
  void AssignQuotaTable(QuotaDatabase* quota_database, Container&& entries) {
    ASSERT_NE(quota_database->db_.get(), nullptr);
    for (const auto& entry : entries) {
      const char* kSql =
          // clang-format off
          "INSERT INTO quota(host, type, quota) "
            "VALUES (?, ?, ?)";
      // clang-format on
      sql::Statement statement;
      statement.Assign(
          quota_database->db_.get()->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      statement.BindString(0, entry.host);
      statement.BindInt(1, static_cast<int>(entry.type));
      statement.BindInt64(2, entry.quota);
      EXPECT_TRUE(statement.Run());
    }
    quota_database->Commit();
  }

  template <typename Container>
  void AssignBucketTable(QuotaDatabase* quota_database, Container&& entries) {
    ASSERT_NE(quota_database->db_.get(), (sql::Database*)nullptr);
    for (const auto& entry : entries) {
      const char* kSql =
          // clang-format off
          "INSERT INTO buckets("
              "id,"
              "origin,"
              "type,"
              "name,"
              "use_count,"
              "last_accessed,"
              "last_modified,"
              "expiration,"
              "quota) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, 0, 0)";
      // clang-format on
      sql::Statement statement;
      statement.Assign(
          quota_database->db_->GetCachedStatement(SQL_FROM_HERE, kSql));
      ASSERT_TRUE(statement.is_valid());

      statement.BindInt64(0, entry.bucket_id);
      statement.BindString(1, entry.origin.GetURL().spec());
      statement.BindInt(2, static_cast<int>(entry.type));
      statement.BindString(3, entry.name);
      statement.BindInt(4, entry.use_count);
      statement.BindTime(5, entry.last_accessed);
      statement.BindTime(6, entry.last_modified);
      EXPECT_TRUE(statement.Run());
    }
    quota_database->Commit();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_directory_;
};

TEST_P(QuotaDatabaseTest, LazyOpen) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_FALSE(LazyOpen(&db, /*create_if_needed=*/false));
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));

  if (GetParam()) {
    // Path should not exist for incognito mode.
    ASSERT_FALSE(base::PathExists(DbPath()));
  } else {
    ASSERT_TRUE(base::PathExists(DbPath()));
  }
}

TEST_P(QuotaDatabaseTest, HostQuota) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));

  const char* kHost = "foo.com";
  const int kQuota1 = 13579;
  const int kQuota2 = kQuota1 + 1024;

  int64_t quota = -1;
  EXPECT_FALSE(db.GetHostQuota(kHost, kTemp, &quota));
  EXPECT_FALSE(db.GetHostQuota(kHost, kPerm, &quota));

  // Insert quota for temporary.
  EXPECT_TRUE(db.SetHostQuota(kHost, kTemp, kQuota1));
  EXPECT_TRUE(db.GetHostQuota(kHost, kTemp, &quota));
  EXPECT_EQ(kQuota1, quota);

  // Update quota for temporary.
  EXPECT_TRUE(db.SetHostQuota(kHost, kTemp, kQuota2));
  EXPECT_TRUE(db.GetHostQuota(kHost, kTemp, &quota));
  EXPECT_EQ(kQuota2, quota);

  // Quota for persistent must not be updated.
  EXPECT_FALSE(db.GetHostQuota(kHost, kPerm, &quota));

  // Delete temporary storage quota.
  EXPECT_TRUE(db.DeleteHostQuota(kHost, kTemp));
  EXPECT_FALSE(db.GetHostQuota(kHost, kTemp, &quota));

  // Delete persistent quota by setting it to zero.
  EXPECT_TRUE(db.SetHostQuota(kHost, kPerm, 0));
  EXPECT_FALSE(db.GetHostQuota(kHost, kPerm, &quota));
}

TEST_P(QuotaDatabaseTest, CreateBucket) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));
  url::Origin origin = ToOrigin("http://google/");
  std::string bucket_name = "google_bucket";

  QuotaErrorOr<BucketId> result = db.CreateBucket(origin, bucket_name);
  ASSERT_TRUE(result.ok());
  ASSERT_FALSE(result.value().is_null());

  // Trying to create an existing bucket should return false.
  result = db.CreateBucket(origin, bucket_name);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error(), QuotaError::kEntryExistsError);
}

TEST_P(QuotaDatabaseTest, GetBucketId) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));

  // Add a bucket entry into the bucket table.
  url::Origin origin = ToOrigin("http://google/");
  std::string bucket_name = "google_bucket";
  QuotaErrorOr<BucketId> result = db.CreateBucket(origin, bucket_name);
  ASSERT_TRUE(result.ok());

  BucketId created_bucket_id = result.value();
  ASSERT_FALSE(created_bucket_id.is_null());

  db.GetBucketId(origin, bucket_name);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), created_bucket_id);

  // Can't retrieve buckets with name mismatch.
  result = db.GetBucketId(origin, "does_not_exist");
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().is_null());

  // Can't retrieve buckets with origin mismatch.
  result = db.GetBucketId(ToOrigin("http://example/"), bucket_name);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().is_null());
}

TEST_P(QuotaDatabaseTest, OriginLastAccessTimeLRU) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));

  std::set<url::Origin> exceptions;
  absl::optional<url::Origin> origin;
  EXPECT_TRUE(db.GetLRUOrigin(kTemp, exceptions, nullptr, &origin));
  EXPECT_FALSE(origin.has_value());

  const url::Origin kOrigin1 = ToOrigin("http://a/");
  const url::Origin kOrigin2 = ToOrigin("http://b/");
  const url::Origin kOrigin3 = ToOrigin("http://c/");
  const url::Origin kOrigin4 = ToOrigin("http://p/");

  // Adding three temporary storages, and
  EXPECT_TRUE(db.SetOriginLastAccessTime(kOrigin1, kTemp,
                                         base::Time::FromJavaTime(10)));
  EXPECT_TRUE(db.SetOriginLastAccessTime(kOrigin2, kTemp,
                                         base::Time::FromJavaTime(20)));
  EXPECT_TRUE(db.SetOriginLastAccessTime(kOrigin3, kTemp,
                                         base::Time::FromJavaTime(30)));

  // one persistent.
  EXPECT_TRUE(db.SetOriginLastAccessTime(kOrigin4, kPerm,
                                         base::Time::FromJavaTime(40)));

  EXPECT_TRUE(db.GetLRUOrigin(kTemp, exceptions, nullptr, &origin));
  EXPECT_EQ(kOrigin1, origin);

  // Test that unlimited origins are excluded from eviction, but
  // protected origins are not excluded.
  scoped_refptr<MockSpecialStoragePolicy> policy(new MockSpecialStoragePolicy);
  policy->AddUnlimited(kOrigin1.GetURL());
  policy->AddProtected(kOrigin2.GetURL());
  EXPECT_TRUE(db.GetLRUOrigin(kTemp, exceptions, policy.get(), &origin));
  EXPECT_EQ(kOrigin2, origin);

  // Test that durable origins are excluded from eviction.
  policy->AddDurable(kOrigin2.GetURL());
  EXPECT_TRUE(db.GetLRUOrigin(kTemp, exceptions, policy.get(), &origin));
  EXPECT_EQ(kOrigin3, origin);

  exceptions.insert(kOrigin1);
  EXPECT_TRUE(db.GetLRUOrigin(kTemp, exceptions, nullptr, &origin));
  EXPECT_EQ(kOrigin2, origin);

  exceptions.insert(kOrigin2);
  EXPECT_TRUE(db.GetLRUOrigin(kTemp, exceptions, nullptr, &origin));
  EXPECT_EQ(kOrigin3, origin);

  exceptions.insert(kOrigin3);
  EXPECT_TRUE(db.GetLRUOrigin(kTemp, exceptions, nullptr, &origin));
  EXPECT_FALSE(origin.has_value());

  EXPECT_TRUE(db.SetOriginLastAccessTime(kOrigin1, kTemp, base::Time::Now()));

  // Delete origin/type last access time information.
  EXPECT_TRUE(db.DeleteOriginInfo(kOrigin3, kTemp));

  // Querying again to see if the deletion has worked.
  exceptions.clear();
  EXPECT_TRUE(db.GetLRUOrigin(kTemp, exceptions, nullptr, &origin));
  EXPECT_EQ(kOrigin2, origin);

  exceptions.insert(kOrigin1);
  exceptions.insert(kOrigin2);
  EXPECT_TRUE(db.GetLRUOrigin(kTemp, exceptions, nullptr, &origin));
  EXPECT_FALSE(origin.has_value());
}

TEST_P(QuotaDatabaseTest, BucketLastAccessTimeLRU) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));

  std::set<url::Origin> exceptions;
  absl::optional<int64_t> bucket_id;
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_FALSE(bucket_id.has_value());

  // Insert bucket entries into BucketTable.
  base::Time now(base::Time::Now());
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry bucket1 = Entry(0, ToOrigin("http://a/"), kTemp, "A", 99, now, now);
  Entry bucket2 = Entry(1, ToOrigin("http://b/"), kTemp, "B", 0, now, now);
  Entry bucket3 = Entry(2, ToOrigin("http://c/"), kTemp, "C", 1, now, now);
  Entry bucket4 = Entry(3, ToOrigin("http://d/"), kPerm, "D", 5, now, now);
  Entry kTableEntries[] = {bucket1, bucket2, bucket3, bucket4};
  AssignBucketTable(&db, kTableEntries);

  // Update access time for three temporary storages, and
  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket1.bucket_id,
                                         base::Time::FromJavaTime(10)));
  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket2.bucket_id,
                                         base::Time::FromJavaTime(20)));
  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket3.bucket_id,
                                         base::Time::FromJavaTime(30)));

  // one persistent.
  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket4.bucket_id,
                                         base::Time::FromJavaTime(40)));

  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_EQ(bucket1.bucket_id, bucket_id);

  // Test that unlimited origins are excluded from eviction, but
  // protected origins are not excluded.
  scoped_refptr<MockSpecialStoragePolicy> policy(new MockSpecialStoragePolicy);
  policy->AddUnlimited(bucket1.origin.GetURL());
  policy->AddProtected(bucket2.origin.GetURL());
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, policy.get(), &bucket_id));
  EXPECT_EQ(bucket2.bucket_id, bucket_id);

  // Test that durable origins are excluded from eviction.
  policy->AddDurable(bucket2.origin.GetURL());
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, policy.get(), &bucket_id));
  EXPECT_EQ(bucket3.bucket_id, bucket_id);

  exceptions.insert(bucket1.origin);
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_EQ(bucket2.bucket_id, bucket_id);

  exceptions.insert(bucket2.origin);
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_EQ(bucket3.bucket_id, bucket_id);

  exceptions.insert(bucket3.origin);
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_FALSE(bucket_id.has_value());

  EXPECT_TRUE(db.SetBucketLastAccessTime(bucket1.bucket_id, base::Time::Now()));

  // Delete origin/type last access time information.
  EXPECT_TRUE(db.DeleteBucketInfo(bucket3.bucket_id));

  // Querying again to see if the deletion has worked.
  exceptions.clear();
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_EQ(bucket2.bucket_id, bucket_id);

  exceptions.insert(bucket1.origin);
  exceptions.insert(bucket2.origin);
  EXPECT_TRUE(db.GetLRUBucket(kTemp, exceptions, nullptr, &bucket_id));
  EXPECT_FALSE(bucket_id.has_value());
}

TEST_P(QuotaDatabaseTest, OriginLastModifiedBetween) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));

  std::set<url::Origin> origins;
  EXPECT_TRUE(db.GetOriginsModifiedBetween(kTemp, &origins, base::Time(),
                                           base::Time::Max()));
  EXPECT_TRUE(origins.empty());

  const url::Origin kOrigin1 = ToOrigin("http://a/");
  const url::Origin kOrigin2 = ToOrigin("http://b/");
  const url::Origin kOrigin3 = ToOrigin("http://c/");

  // Report last mod time for the test origins.
  EXPECT_TRUE(db.SetOriginLastModifiedTime(kOrigin1, kTemp,
                                           base::Time::FromJavaTime(0)));
  EXPECT_TRUE(db.SetOriginLastModifiedTime(kOrigin2, kTemp,
                                           base::Time::FromJavaTime(10)));
  EXPECT_TRUE(db.SetOriginLastModifiedTime(kOrigin3, kTemp,
                                           base::Time::FromJavaTime(20)));

  EXPECT_TRUE(db.GetOriginsModifiedBetween(kTemp, &origins, base::Time(),
                                           base::Time::Max()));
  EXPECT_EQ(3U, origins.size());
  EXPECT_EQ(1U, origins.count(kOrigin1));
  EXPECT_EQ(1U, origins.count(kOrigin2));
  EXPECT_EQ(1U, origins.count(kOrigin3));

  EXPECT_TRUE(db.GetOriginsModifiedBetween(
      kTemp, &origins, base::Time::FromJavaTime(5), base::Time::Max()));
  EXPECT_EQ(2U, origins.size());
  EXPECT_EQ(0U, origins.count(kOrigin1));
  EXPECT_EQ(1U, origins.count(kOrigin2));
  EXPECT_EQ(1U, origins.count(kOrigin3));

  EXPECT_TRUE(db.GetOriginsModifiedBetween(
      kTemp, &origins, base::Time::FromJavaTime(15), base::Time::Max()));
  EXPECT_EQ(1U, origins.size());
  EXPECT_EQ(0U, origins.count(kOrigin1));
  EXPECT_EQ(0U, origins.count(kOrigin2));
  EXPECT_EQ(1U, origins.count(kOrigin3));

  EXPECT_TRUE(db.GetOriginsModifiedBetween(
      kTemp, &origins, base::Time::FromJavaTime(25), base::Time::Max()));
  EXPECT_TRUE(origins.empty());

  EXPECT_TRUE(db.GetOriginsModifiedBetween(kTemp, &origins,
                                           base::Time::FromJavaTime(5),
                                           base::Time::FromJavaTime(15)));
  EXPECT_EQ(1U, origins.size());
  EXPECT_EQ(0U, origins.count(kOrigin1));
  EXPECT_EQ(1U, origins.count(kOrigin2));
  EXPECT_EQ(0U, origins.count(kOrigin3));

  EXPECT_TRUE(db.GetOriginsModifiedBetween(kTemp, &origins,
                                           base::Time::FromJavaTime(0),
                                           base::Time::FromJavaTime(20)));
  EXPECT_EQ(2U, origins.size());
  EXPECT_EQ(1U, origins.count(kOrigin1));
  EXPECT_EQ(1U, origins.count(kOrigin2));
  EXPECT_EQ(0U, origins.count(kOrigin3));

  // Update origin1's mod time but for persistent storage.
  EXPECT_TRUE(db.SetOriginLastModifiedTime(kOrigin1, kPerm,
                                           base::Time::FromJavaTime(30)));

  // Must have no effects on temporary origins info.
  EXPECT_TRUE(db.GetOriginsModifiedBetween(
      kTemp, &origins, base::Time::FromJavaTime(5), base::Time::Max()));
  EXPECT_EQ(2U, origins.size());
  EXPECT_EQ(0U, origins.count(kOrigin1));
  EXPECT_EQ(1U, origins.count(kOrigin2));
  EXPECT_EQ(1U, origins.count(kOrigin3));

  // One more update for persistent origin2.
  EXPECT_TRUE(db.SetOriginLastModifiedTime(kOrigin2, kPerm,
                                           base::Time::FromJavaTime(40)));

  EXPECT_TRUE(db.GetOriginsModifiedBetween(
      kPerm, &origins, base::Time::FromJavaTime(25), base::Time::Max()));
  EXPECT_EQ(2U, origins.size());
  EXPECT_EQ(1U, origins.count(kOrigin1));
  EXPECT_EQ(1U, origins.count(kOrigin2));
  EXPECT_EQ(0U, origins.count(kOrigin3));

  EXPECT_TRUE(db.GetOriginsModifiedBetween(
      kPerm, &origins, base::Time::FromJavaTime(35), base::Time::Max()));
  EXPECT_EQ(1U, origins.size());
  EXPECT_EQ(0U, origins.count(kOrigin1));
  EXPECT_EQ(1U, origins.count(kOrigin2));
  EXPECT_EQ(0U, origins.count(kOrigin3));
}

TEST_P(QuotaDatabaseTest, BucketLastModifiedBetween) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));

  std::set<int64_t> bucket_ids;
  EXPECT_TRUE(db.GetBucketsModifiedBetween(kTemp, &bucket_ids, base::Time(),
                                           base::Time::Max()));
  EXPECT_TRUE(bucket_ids.empty());

  // Insert bucket entries into BucketTable.
  base::Time now(base::Time::Now());
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry bucket1 = Entry(0, ToOrigin("http://a/"), kTemp, "A", 0, now, now);
  Entry bucket2 = Entry(1, ToOrigin("http://b/"), kTemp, "B", 0, now, now);
  Entry bucket3 = Entry(2, ToOrigin("http://c/"), kTemp, "C", 0, now, now);
  Entry bucket4 = Entry(3, ToOrigin("http://d/"), kPerm, "D", 0, now, now);
  Entry kTableEntries[] = {bucket1, bucket2, bucket3, bucket4};
  AssignBucketTable(&db, kTableEntries);

  // Report last mod time for the buckets.
  EXPECT_TRUE(db.SetBucketLastModifiedTime(bucket1.bucket_id,
                                           base::Time::FromJavaTime(0)));
  EXPECT_TRUE(db.SetBucketLastModifiedTime(bucket2.bucket_id,
                                           base::Time::FromJavaTime(10)));
  EXPECT_TRUE(db.SetBucketLastModifiedTime(bucket3.bucket_id,
                                           base::Time::FromJavaTime(20)));
  EXPECT_TRUE(db.SetBucketLastModifiedTime(bucket4.bucket_id,
                                           base::Time::FromJavaTime(30)));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(kTemp, &bucket_ids, base::Time(),
                                           base::Time::Max()));
  EXPECT_EQ(3U, bucket_ids.size());
  EXPECT_EQ(1U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(
      kTemp, &bucket_ids, base::Time::FromJavaTime(5), base::Time::Max()));
  EXPECT_EQ(2U, bucket_ids.size());
  EXPECT_EQ(0U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(
      kTemp, &bucket_ids, base::Time::FromJavaTime(15), base::Time::Max()));
  EXPECT_EQ(1U, bucket_ids.size());
  EXPECT_EQ(0U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(
      kTemp, &bucket_ids, base::Time::FromJavaTime(25), base::Time::Max()));
  EXPECT_TRUE(bucket_ids.empty());

  EXPECT_TRUE(db.GetBucketsModifiedBetween(kTemp, &bucket_ids,
                                           base::Time::FromJavaTime(5),
                                           base::Time::FromJavaTime(15)));
  EXPECT_EQ(1U, bucket_ids.size());
  EXPECT_EQ(0U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(kTemp, &bucket_ids,
                                           base::Time::FromJavaTime(0),
                                           base::Time::FromJavaTime(20)));
  EXPECT_EQ(2U, bucket_ids.size());
  EXPECT_EQ(1U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket4.bucket_id));

  EXPECT_TRUE(db.GetBucketsModifiedBetween(kPerm, &bucket_ids,
                                           base::Time::FromJavaTime(0),
                                           base::Time::FromJavaTime(35)));
  EXPECT_EQ(1U, bucket_ids.size());
  EXPECT_EQ(0U, bucket_ids.count(bucket1.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket2.bucket_id));
  EXPECT_EQ(0U, bucket_ids.count(bucket3.bucket_id));
  EXPECT_EQ(1U, bucket_ids.count(bucket4.bucket_id));
}

TEST_P(QuotaDatabaseTest, OriginLastEvicted) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));

  const url::Origin kOrigin1 = ToOrigin("http://a/");
  const url::Origin kOrigin2 = ToOrigin("http://b/");
  const url::Origin kOrigin3 = ToOrigin("http://c/");

  base::Time last_eviction_time;
  EXPECT_FALSE(
      db.GetOriginLastEvictionTime(kOrigin1, kTemp, &last_eviction_time));
  EXPECT_EQ(base::Time(), last_eviction_time);

  // Report last eviction time for the test origins.
  EXPECT_TRUE(db.SetOriginLastEvictionTime(kOrigin1, kTemp,
                                           base::Time::FromJavaTime(10)));
  EXPECT_TRUE(db.SetOriginLastEvictionTime(kOrigin2, kTemp,
                                           base::Time::FromJavaTime(20)));
  EXPECT_TRUE(db.SetOriginLastEvictionTime(kOrigin3, kTemp,
                                           base::Time::FromJavaTime(30)));

  EXPECT_TRUE(
      db.GetOriginLastEvictionTime(kOrigin1, kTemp, &last_eviction_time));
  EXPECT_EQ(base::Time::FromJavaTime(10), last_eviction_time);
  EXPECT_TRUE(
      db.GetOriginLastEvictionTime(kOrigin2, kTemp, &last_eviction_time));
  EXPECT_EQ(base::Time::FromJavaTime(20), last_eviction_time);
  EXPECT_TRUE(
      db.GetOriginLastEvictionTime(kOrigin3, kTemp, &last_eviction_time));
  EXPECT_EQ(base::Time::FromJavaTime(30), last_eviction_time);

  // Delete last eviction times for the test origins.
  EXPECT_TRUE(db.DeleteOriginLastEvictionTime(kOrigin1, kTemp));
  EXPECT_TRUE(db.DeleteOriginLastEvictionTime(kOrigin2, kTemp));
  EXPECT_TRUE(db.DeleteOriginLastEvictionTime(kOrigin3, kTemp));

  last_eviction_time = base::Time();
  EXPECT_FALSE(
      db.GetOriginLastEvictionTime(kOrigin1, kTemp, &last_eviction_time));
  EXPECT_EQ(base::Time(), last_eviction_time);
  EXPECT_FALSE(
      db.GetOriginLastEvictionTime(kOrigin2, kTemp, &last_eviction_time));
  EXPECT_EQ(base::Time(), last_eviction_time);
  EXPECT_FALSE(
      db.GetOriginLastEvictionTime(kOrigin3, kTemp, &last_eviction_time));
  EXPECT_EQ(base::Time(), last_eviction_time);

  // Deleting an origin that is not present should not fail.
  EXPECT_TRUE(db.DeleteOriginLastEvictionTime(ToOrigin("http://notpresent.com"),
                                              kTemp));
}

TEST_P(QuotaDatabaseTest, RegisterInitialOriginInfo) {
  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());

  const url::Origin kOrigins[] = {ToOrigin("http://a/"), ToOrigin("http://b/"),
                                  ToOrigin("http://c/")};
  std::set<url::Origin> origins(kOrigins, std::end(kOrigins));

  EXPECT_TRUE(db.RegisterInitialOriginInfo(origins, kTemp));

  QuotaDatabase::BucketTableEntry info;
  info.use_count = -1;
  EXPECT_TRUE(db.GetOriginInfo(ToOrigin("http://a/"), kTemp, &info));
  EXPECT_EQ(0, info.use_count);

  EXPECT_TRUE(db.SetOriginLastAccessTime(ToOrigin("http://a/"), kTemp,
                                         base::Time::FromDoubleT(1.0)));
  info.use_count = -1;
  EXPECT_TRUE(db.GetOriginInfo(ToOrigin("http://a/"), kTemp, &info));
  EXPECT_EQ(1, info.use_count);

  EXPECT_TRUE(db.RegisterInitialOriginInfo(origins, kTemp));

  info.use_count = -1;
  EXPECT_TRUE(db.GetOriginInfo(ToOrigin("http://a/"), kTemp, &info));
  EXPECT_EQ(1, info.use_count);
}

TEST_P(QuotaDatabaseTest, DumpQuotaTable) {
  QuotaTableEntry kTableEntries[] = {
      {.host = "http://go/", .type = kTemp, .quota = 1},
      {.host = "http://oo/", .type = kTemp, .quota = 2},
      {.host = "http://gle/", .type = kPerm, .quota = 3}};

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));
  AssignQuotaTable(&db, kTableEntries);

  using Verifier = EntryVerifier<QuotaTableEntry>;
  Verifier verifier(kTableEntries, std::end(kTableEntries));
  EXPECT_TRUE(DumpQuotaTable(
      &db, base::BindRepeating(&Verifier::Run, base::Unretained(&verifier))));
  EXPECT_TRUE(verifier.table.empty());
}

TEST_P(QuotaDatabaseTest, DumpBucketTable) {
  base::Time now(base::Time::Now());
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry kTableEntries[] = {
      Entry(0, ToOrigin("http://go/"), kTemp, kDefaultBucket, 2147483647, now,
            now),
      Entry(1, ToOrigin("http://oo/"), kTemp, kDefaultBucket, 0, now, now),
      Entry(2, ToOrigin("http://gle/"), kTemp, kDefaultBucket, 1, now, now),
  };

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));
  AssignBucketTable(&db, kTableEntries);

  using Verifier = EntryVerifier<Entry>;
  Verifier verifier(kTableEntries, std::end(kTableEntries));
  EXPECT_TRUE(DumpBucketTable(
      &db, base::BindRepeating(&Verifier::Run, base::Unretained(&verifier))));
  EXPECT_TRUE(verifier.table.empty());
}

TEST_P(QuotaDatabaseTest, GetOriginInfo) {
  const url::Origin kOrigin = ToOrigin("http://go/");
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry kTableEntries[] = {Entry(0, kOrigin, kTemp, kDefaultBucket, 100,
                                 base::Time(), base::Time())};

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));
  AssignBucketTable(&db, kTableEntries);

  {
    Entry entry;
    EXPECT_TRUE(db.GetOriginInfo(kOrigin, kTemp, &entry));
    EXPECT_EQ(kTableEntries[0].type, entry.type);
    EXPECT_EQ(kTableEntries[0].origin, entry.origin);
    EXPECT_EQ(kTableEntries[0].name, entry.name);
    EXPECT_EQ(kTableEntries[0].use_count, entry.use_count);
    EXPECT_EQ(kTableEntries[0].last_accessed, entry.last_accessed);
    EXPECT_EQ(kTableEntries[0].last_modified, entry.last_modified);
  }

  {
    Entry entry;
    EXPECT_FALSE(
        db.GetOriginInfo(ToOrigin("http://notpresent.org/"), kTemp, &entry));
  }
}

TEST_P(QuotaDatabaseTest, GetBucketInfo) {
  using Entry = QuotaDatabase::BucketTableEntry;
  Entry kTableEntries[] = {Entry(123, ToOrigin("http://go/"), kTemp,
                                 "TestBucket", 100, base::Time(),
                                 base::Time())};

  QuotaDatabase db(use_in_memory_db() ? base::FilePath() : DbPath());
  EXPECT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));
  AssignBucketTable(&db, kTableEntries);

  {
    Entry entry;
    EXPECT_TRUE(db.GetBucketInfo(kTableEntries[0].bucket_id, &entry));
    EXPECT_EQ(kTableEntries[0].bucket_id, entry.bucket_id);
    EXPECT_EQ(kTableEntries[0].type, entry.type);
    EXPECT_EQ(kTableEntries[0].origin, entry.origin);
    EXPECT_EQ(kTableEntries[0].name, entry.name);
    EXPECT_EQ(kTableEntries[0].use_count, entry.use_count);
    EXPECT_EQ(kTableEntries[0].last_accessed, entry.last_accessed);
    EXPECT_EQ(kTableEntries[0].last_modified, entry.last_modified);
  }

  {
    Entry entry;
    EXPECT_FALSE(db.GetBucketInfo(456, &entry));
  }
}

// Non-parameterized tests.
TEST_F(QuotaDatabaseTest, BootstrapFlag) {
  QuotaDatabase db(DbPath());

  EXPECT_FALSE(db.IsOriginDatabaseBootstrapped());
  EXPECT_TRUE(db.SetOriginDatabaseBootstrapped(true));
  EXPECT_TRUE(db.IsOriginDatabaseBootstrapped());
  EXPECT_TRUE(db.SetOriginDatabaseBootstrapped(false));
  EXPECT_FALSE(db.IsOriginDatabaseBootstrapped());
}

TEST_F(QuotaDatabaseTest, OpenCorruptedDatabase) {
  // Create database, force corruption and close db by leaving scope.
  {
    QuotaDatabase db(DbPath());
    ASSERT_TRUE(LazyOpen(&db, /*create_if_needed=*/true));
    ASSERT_TRUE(sql::test::CorruptSizeInHeader(DbPath()));
  }
  // Reopen database and verify schema reset on reopen.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    QuotaDatabase db(DbPath());
    ASSERT_TRUE(LazyOpen(&db, /*create_if_needed=*/false));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         QuotaDatabaseTest,
                         /* is_incognito */ testing::Bool());

}  // namespace storage
