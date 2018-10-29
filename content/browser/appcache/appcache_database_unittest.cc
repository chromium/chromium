// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "content/browser/appcache/appcache_database.h"
#include "content/browser/appcache/appcache_entry.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/sqlite/sqlite3.h"

namespace {

const base::Time kZeroTime;

}  // namespace

namespace content {

class AppCacheDatabaseTest {};

TEST(AppCacheDatabaseTest, LazyOpen) {
  // Use an empty file path to use an in-memory sqlite database.
  const base::FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);

  EXPECT_FALSE(db.LazyOpen(false));
  EXPECT_TRUE(db.LazyOpen(true));

  int64_t group_id, cache_id, response_id, deleteable_response_rowid;
  group_id = cache_id = response_id = deleteable_response_rowid = 0;
  EXPECT_TRUE(db.FindLastStorageIds(&group_id, &cache_id, &response_id,
                                    &deleteable_response_rowid));
  EXPECT_EQ(0, group_id);
  EXPECT_EQ(0, cache_id);
  EXPECT_EQ(0, response_id);
  EXPECT_EQ(0, deleteable_response_rowid);

  std::set<url::Origin> origins;
  EXPECT_TRUE(db.FindOriginsWithGroups(&origins));
  EXPECT_TRUE(origins.empty());
}

TEST(AppCacheDatabaseTest, ReCreate) {
  // Real files on disk for this test.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = temp_dir.GetPath().AppendASCII("appcache.db");
  const base::FilePath kNestedDir = temp_dir.GetPath().AppendASCII("nested");
  const base::FilePath kOtherFile =  kNestedDir.AppendASCII("other_file");
  EXPECT_TRUE(base::CreateDirectory(kNestedDir));
  EXPECT_EQ(3, base::WriteFile(kOtherFile, "foo", 3));

  AppCacheDatabase db(kDbFile);
  EXPECT_FALSE(db.LazyOpen(false));
  EXPECT_TRUE(db.LazyOpen(true));

  EXPECT_TRUE(base::PathExists(kDbFile));
  EXPECT_TRUE(base::DirectoryExists(kNestedDir));
  EXPECT_TRUE(base::PathExists(kOtherFile));

  EXPECT_TRUE(db.DeleteExistingAndCreateNewDatabase());

  EXPECT_TRUE(base::PathExists(kDbFile));
  EXPECT_FALSE(base::DirectoryExists(kNestedDir));
  EXPECT_FALSE(base::PathExists(kOtherFile));
}

#ifdef NDEBUG
// Only run in release builds because sql::Database and familiy
// crank up DLOG(FATAL)'ness and this test presents it with
// intentionally bad data which causes debug builds to exit instead
// of run to completion. In release builds, errors the are delivered
// to the consumer so  we can test the error handling of the consumer.
// TODO: crbug/328576
TEST(AppCacheDatabaseTest, QuickIntegrityCheck) {
  // Real files on disk for this test too, a corrupt database file.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath mock_dir = temp_dir.GetPath().AppendASCII("mock");
  ASSERT_TRUE(base::CreateDirectory(mock_dir));

  const base::FilePath kDbFile = mock_dir.AppendASCII("appcache.db");
  const base::FilePath kOtherFile = mock_dir.AppendASCII("other_file");
  EXPECT_EQ(3, base::WriteFile(kOtherFile, "foo", 3));

  // First create a valid db file.
  {
    AppCacheDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    EXPECT_TRUE(base::PathExists(kOtherFile));
    EXPECT_TRUE(base::PathExists(kDbFile));
  }

  // Break it.
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(kDbFile));

  // Reopening will notice the corruption and delete/recreate the directory.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    AppCacheDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    EXPECT_FALSE(base::PathExists(kOtherFile));
    EXPECT_TRUE(base::PathExists(kDbFile));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}
#endif  // NDEBUG

TEST(AppCacheDatabaseTest, WasCorrutionDetected) {
  // Real files on disk for this test too, a corrupt database file.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = temp_dir.GetPath().AppendASCII("appcache.db");

  // First create a valid db file.
  AppCacheDatabase db(kDbFile);
  EXPECT_TRUE(db.LazyOpen(true));
  EXPECT_TRUE(base::PathExists(kDbFile));
  EXPECT_FALSE(db.was_corruption_detected());

  // Break it.
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(kDbFile));

  // See the the corruption is detected and reported.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);
    std::map<url::Origin, int64_t> usage_map;
    EXPECT_FALSE(db.GetAllOriginUsage(&usage_map));
    EXPECT_TRUE(db.was_corruption_detected());
    EXPECT_TRUE(base::PathExists(kDbFile));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST(AppCacheDatabaseTest, ExperimentalFlags) {
  const char kExperimentFlagsKey[] = "ExperimentFlags";
  std::string kInjectedFlags("exp1,exp2");

  // Real files on disk for this test.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile = temp_dir.GetPath().AppendASCII("appcache.db");
  const base::FilePath kOtherFile =
      temp_dir.GetPath().AppendASCII("other_file");
  EXPECT_EQ(3, base::WriteFile(kOtherFile, "foo", 3));
  EXPECT_TRUE(base::PathExists(kOtherFile));

  // Inject a non empty flags value, and verify it got there.
  {
    AppCacheDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(true));
    EXPECT_TRUE(db.meta_table_->SetValue(kExperimentFlagsKey, kInjectedFlags));
    std::string flags;
    EXPECT_TRUE(db.meta_table_->GetValue(kExperimentFlagsKey, &flags));
    EXPECT_EQ(kInjectedFlags, flags);
  }

  // If flags don't match the expected value, empty string by default,
  // the database should be recreated and other files should be cleared out.
  {
    AppCacheDatabase db(kDbFile);
    EXPECT_TRUE(db.LazyOpen(false));
    std::string flags;
    EXPECT_TRUE(db.meta_table_->GetValue(kExperimentFlagsKey, &flags));
    EXPECT_TRUE(flags.empty());
    EXPECT_FALSE(base::PathExists(kOtherFile));
  }
}

TEST(AppCacheDatabaseTest, EntryRecords) {
  const base::FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  sql::test::ScopedErrorExpecter expecter;
  // TODO(shess): Suppressing SQLITE_CONSTRAINT because the code
  // expects that and handles the resulting error.  Consider revising
  // the code to use INSERT OR IGNORE (which would not throw
  // SQLITE_CONSTRAINT) and then check ChangeCount() to see if any
  // changes were made.
  expecter.ExpectError(SQLITE_CONSTRAINT);

  AppCacheDatabase::EntryRecord entry;

  entry.cache_id = 1;
  entry.url = GURL("http://blah/1");
  entry.flags = AppCacheEntry::MASTER;
  entry.response_id = 1;
  entry.response_size = 100;
  EXPECT_TRUE(db.InsertEntry(&entry));
  EXPECT_FALSE(db.InsertEntry(&entry));

  entry.cache_id = 2;
  entry.url = GURL("http://blah/2");
  entry.flags = AppCacheEntry::EXPLICIT;
  entry.response_id = 2;
  entry.response_size = 200;
  EXPECT_TRUE(db.InsertEntry(&entry));

  entry.cache_id = 2;
  entry.url = GURL("http://blah/3");
  entry.flags = AppCacheEntry::MANIFEST;
  entry.response_id = 3;
  entry.response_size = 300;
  EXPECT_TRUE(db.InsertEntry(&entry));

  std::vector<AppCacheDatabase::EntryRecord> found;

  EXPECT_TRUE(db.FindEntriesForCache(1, &found));
  EXPECT_EQ(1U, found.size());
  EXPECT_EQ(1, found[0].cache_id);
  EXPECT_EQ(GURL("http://blah/1"), found[0].url);
  EXPECT_EQ(AppCacheEntry::MASTER, found[0].flags);
  EXPECT_EQ(1, found[0].response_id);
  EXPECT_EQ(100, found[0].response_size);
  found.clear();

  EXPECT_TRUE(db.AddEntryFlags(GURL("http://blah/1"), 1,
                               AppCacheEntry::FOREIGN));
  EXPECT_TRUE(db.FindEntriesForCache(1, &found));
  EXPECT_EQ(1U, found.size());
  EXPECT_EQ(AppCacheEntry::MASTER | AppCacheEntry::FOREIGN, found[0].flags);
  found.clear();

  EXPECT_TRUE(db.FindEntriesForCache(2, &found));
  EXPECT_EQ(2U, found.size());
  EXPECT_EQ(2, found[0].cache_id);
  EXPECT_EQ(GURL("http://blah/2"), found[0].url);
  EXPECT_EQ(AppCacheEntry::EXPLICIT, found[0].flags);
  EXPECT_EQ(2, found[0].response_id);
  EXPECT_EQ(200, found[0].response_size);
  EXPECT_EQ(2, found[1].cache_id);
  EXPECT_EQ(GURL("http://blah/3"), found[1].url);
  EXPECT_EQ(AppCacheEntry::MANIFEST, found[1].flags);
  EXPECT_EQ(3, found[1].response_id);
  EXPECT_EQ(300, found[1].response_size);
  found.clear();

  EXPECT_TRUE(db.DeleteEntriesForCache(2));
  EXPECT_TRUE(db.FindEntriesForCache(2, &found));
  EXPECT_TRUE(found.empty());
  found.clear();

  EXPECT_TRUE(db.DeleteEntriesForCache(1));
  EXPECT_FALSE(db.AddEntryFlags(GURL("http://blah/1"), 1,
                                AppCacheEntry::FOREIGN));

  ASSERT_TRUE(expecter.SawExpectedErrors());
}

TEST(AppCacheDatabaseTest, CacheRecords) {
  const base::FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  sql::test::ScopedErrorExpecter expecter;
  // TODO(shess): See EntryRecords test.
  expecter.ExpectError(SQLITE_CONSTRAINT);

  const AppCacheDatabase::CacheRecord kZeroRecord;
  AppCacheDatabase::CacheRecord record;
  EXPECT_FALSE(db.FindCache(1, &record));

  record.cache_id = 1;
  record.group_id = 1;
  record.online_wildcard = true;
  record.update_time = kZeroTime;
  record.cache_size = 100;
  EXPECT_TRUE(db.InsertCache(&record));
  EXPECT_FALSE(db.InsertCache(&record));

  record = kZeroRecord;
  EXPECT_TRUE(db.FindCache(1, &record));
  EXPECT_EQ(1, record.cache_id);
  EXPECT_EQ(1, record.group_id);
  EXPECT_TRUE(record.online_wildcard);
  EXPECT_TRUE(kZeroTime == record.update_time);
  EXPECT_EQ(100, record.cache_size);

  record = kZeroRecord;
  EXPECT_TRUE(db.FindCacheForGroup(1, &record));
  EXPECT_EQ(1, record.cache_id);
  EXPECT_EQ(1, record.group_id);
  EXPECT_TRUE(record.online_wildcard);
  EXPECT_TRUE(kZeroTime == record.update_time);
  EXPECT_EQ(100, record.cache_size);

  EXPECT_TRUE(db.DeleteCache(1));
  EXPECT_FALSE(db.FindCache(1, &record));
  EXPECT_FALSE(db.FindCacheForGroup(1, &record));

  EXPECT_TRUE(db.DeleteCache(1));

  ASSERT_TRUE(expecter.SawExpectedErrors());
}

TEST(AppCacheDatabaseTest, GroupRecords) {
  const base::FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  sql::test::ScopedErrorExpecter expecter;
  // TODO(shess): See EntryRecords test.
  expecter.ExpectError(SQLITE_CONSTRAINT);

  const GURL kManifestUrl("http://blah/manifest");
  const url::Origin kOrigin(url::Origin::Create(kManifestUrl));
  const base::Time kLastAccessTime = base::Time::Now();
  const base::Time kCreationTime =
      kLastAccessTime - base::TimeDelta::FromDays(7);

  const AppCacheDatabase::GroupRecord kZeroRecord;
  AppCacheDatabase::GroupRecord record;
  std::vector<AppCacheDatabase::GroupRecord> records;

  // Behavior with an empty table
  EXPECT_FALSE(db.FindGroup(1, &record));
  EXPECT_FALSE(db.FindGroupForManifestUrl(kManifestUrl, &record));
  EXPECT_TRUE(db.DeleteGroup(1));
  EXPECT_TRUE(db.FindGroupsForOrigin(kOrigin, &records));
  EXPECT_TRUE(records.empty());
  EXPECT_FALSE(db.FindGroupForCache(1, &record));

  record.group_id = 1;
  record.manifest_url = kManifestUrl;
  record.origin = kOrigin;
  record.last_access_time = kLastAccessTime;
  record.creation_time = kCreationTime;
  EXPECT_TRUE(db.InsertGroup(&record));
  EXPECT_FALSE(db.InsertGroup(&record));

  record.group_id = 2;
  EXPECT_FALSE(db.InsertGroup(&record));

  record = kZeroRecord;
  EXPECT_TRUE(db.FindGroup(1, &record));
  EXPECT_EQ(1, record.group_id);
  EXPECT_EQ(kManifestUrl, record.manifest_url);
  EXPECT_EQ(kOrigin, record.origin);
  EXPECT_EQ(kCreationTime.ToInternalValue(),
            record.creation_time.ToInternalValue());
  EXPECT_EQ(kLastAccessTime.ToInternalValue(),
            record.last_access_time.ToInternalValue());

  record = kZeroRecord;
  EXPECT_TRUE(db.FindGroupForManifestUrl(kManifestUrl, &record));
  EXPECT_EQ(1, record.group_id);
  EXPECT_EQ(kManifestUrl, record.manifest_url);
  EXPECT_EQ(kOrigin, record.origin);
  EXPECT_EQ(kCreationTime.ToInternalValue(),
            record.creation_time.ToInternalValue());
  EXPECT_EQ(kLastAccessTime.ToInternalValue(),
            record.last_access_time.ToInternalValue());

  record.group_id = 2;
  record.manifest_url = kOrigin.GetURL();
  record.origin = kOrigin;
  record.last_access_time = kLastAccessTime;
  record.creation_time = kCreationTime;
  EXPECT_TRUE(db.InsertGroup(&record));

  record = kZeroRecord;
  EXPECT_TRUE(db.FindGroupForManifestUrl(kOrigin.GetURL(), &record));
  EXPECT_EQ(2, record.group_id);
  EXPECT_EQ(kOrigin.GetURL(), record.manifest_url);
  EXPECT_EQ(kOrigin, record.origin);
  EXPECT_EQ(kCreationTime.ToInternalValue(),
            record.creation_time.ToInternalValue());
  EXPECT_EQ(kLastAccessTime.ToInternalValue(),
            record.last_access_time.ToInternalValue());

  EXPECT_TRUE(db.FindGroupsForOrigin(kOrigin, &records));
  EXPECT_EQ(2U, records.size());
  EXPECT_EQ(1, records[0].group_id);
  EXPECT_EQ(kManifestUrl, records[0].manifest_url);
  EXPECT_EQ(kOrigin, records[0].origin);
  EXPECT_EQ(2, records[1].group_id);
  EXPECT_EQ(kOrigin.GetURL(), records[1].manifest_url);
  EXPECT_EQ(kOrigin, records[1].origin);

  EXPECT_TRUE(db.DeleteGroup(1));

  records.clear();
  EXPECT_TRUE(db.FindGroupsForOrigin(kOrigin, &records));
  EXPECT_EQ(1U, records.size());
  EXPECT_EQ(2, records[0].group_id);
  EXPECT_EQ(kOrigin.GetURL(), records[0].manifest_url);
  EXPECT_EQ(kOrigin, records[0].origin);
  EXPECT_EQ(kCreationTime.ToInternalValue(),
            record.creation_time.ToInternalValue());
  EXPECT_EQ(kLastAccessTime.ToInternalValue(),
            record.last_access_time.ToInternalValue());

  std::set<url::Origin> origins;
  EXPECT_TRUE(db.FindOriginsWithGroups(&origins));
  EXPECT_EQ(1U, origins.size());
  EXPECT_EQ(kOrigin, *(origins.begin()));

  const GURL kManifest2("http://blah2/manifest");
  const url::Origin kOrigin2(url::Origin::Create(kManifest2));
  record.group_id = 1;
  record.manifest_url = kManifest2;
  record.origin = kOrigin2;
  EXPECT_TRUE(db.InsertGroup(&record));

  origins.clear();
  EXPECT_TRUE(db.FindOriginsWithGroups(&origins));
  EXPECT_EQ(2U, origins.size());
  EXPECT_TRUE(origins.end() != origins.find(kOrigin));
  EXPECT_TRUE(origins.end()  != origins.find(kOrigin2));

  AppCacheDatabase::CacheRecord cache_record;
  cache_record.cache_id = 1;
  cache_record.group_id = 1;
  cache_record.online_wildcard = true;
  cache_record.update_time = kZeroTime;
  EXPECT_TRUE(db.InsertCache(&cache_record));

  record = kZeroRecord;
  EXPECT_TRUE(db.FindGroupForCache(1, &record));
  EXPECT_EQ(1, record.group_id);
  EXPECT_EQ(kManifest2, record.manifest_url);
  EXPECT_EQ(kOrigin2, record.origin);

  ASSERT_TRUE(expecter.SawExpectedErrors());
}

TEST(AppCacheDatabaseTest, GroupAccessAndEvictionTimes) {
  const base::FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  const GURL kManifestUrl("http://blah/manifest");
  const url::Origin kOrigin(url::Origin::Create(kManifestUrl));
  const base::Time kDayOne =
      base::Time() + base::TimeDelta::FromDays(1);
  const base::Time kDayTwo = kDayOne + base::TimeDelta::FromDays(1);

  // See that the methods behave as expected with an empty db.
  // To accomodate lazy updating, for consistency, none of them fail
  // given ids not found in the db.
  EXPECT_TRUE(db.UpdateEvictionTimes(1, kDayOne, kDayTwo));
  EXPECT_TRUE(db.UpdateLastAccessTime(1, kDayOne));
  EXPECT_TRUE(db.CommitLazyLastAccessTimes());
  EXPECT_TRUE(db.LazyUpdateLastAccessTime(1, kDayTwo));
  EXPECT_TRUE(db.CommitLazyLastAccessTimes());

  // Insert a group at DAY1
  AppCacheDatabase::GroupRecord record;
  record.group_id = 1;
  record.manifest_url = kManifestUrl;
  record.origin = kOrigin;
  record.creation_time = kDayOne;
  record.last_access_time = kDayOne;
  record.last_full_update_check_time = kDayOne;
  record.first_evictable_error_time = kDayOne;
  EXPECT_TRUE(db.InsertGroup(&record));

  // Verify the round trip.
  record = AppCacheDatabase::GroupRecord();
  EXPECT_TRUE(db.FindGroup(1, &record));
  EXPECT_EQ(kDayOne, record.last_access_time);
  EXPECT_EQ(kDayOne, record.last_full_update_check_time);
  EXPECT_EQ(kDayOne, record.first_evictable_error_time);

  // Update the times to DAY2 and verify.
  EXPECT_TRUE(db.UpdateEvictionTimes(1, kDayTwo, kDayTwo));
  EXPECT_TRUE(db.UpdateLastAccessTime(1, kDayTwo));
  record = AppCacheDatabase::GroupRecord();
  EXPECT_TRUE(db.FindGroup(1, &record));
  EXPECT_EQ(kDayTwo, record.last_access_time);
  EXPECT_EQ(kDayTwo, record.last_full_update_check_time);
  EXPECT_EQ(kDayTwo, record.first_evictable_error_time);

  // Lazy update back to DAY1 and verify its reflected without having committed.
  EXPECT_TRUE(db.lazy_last_access_times_.empty());
  EXPECT_TRUE(db.LazyUpdateLastAccessTime(1, kDayOne));
  EXPECT_FALSE(db.lazy_last_access_times_.empty());
  record = AppCacheDatabase::GroupRecord();
  EXPECT_TRUE(db.FindGroup(1, &record));
  EXPECT_EQ(kDayOne, record.last_access_time);

  // Commit the lazy value and verify it sticks.
  EXPECT_TRUE(db.CommitLazyLastAccessTimes());
  EXPECT_TRUE(db.lazy_last_access_times_.empty());
  record = AppCacheDatabase::GroupRecord();
  EXPECT_TRUE(db.FindGroup(1, &record));
  EXPECT_EQ(kDayOne, record.last_access_time);

  // Verify a bad lazy group id doesn't fail to commit the good ones on DAY2.
  EXPECT_TRUE(db.LazyUpdateLastAccessTime(1, kDayTwo));
  EXPECT_TRUE(db.LazyUpdateLastAccessTime(2, kDayTwo));
  EXPECT_EQ(2u, db.lazy_last_access_times_.size());
  EXPECT_TRUE(db.CommitLazyLastAccessTimes());
  EXPECT_TRUE(db.lazy_last_access_times_.empty());
  record = AppCacheDatabase::GroupRecord();
  EXPECT_TRUE(db.FindGroup(1, &record));
  EXPECT_EQ(kDayTwo, record.last_access_time);
}

TEST(AppCacheDatabaseTest, NamespaceRecords) {
  const base::FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  sql::test::ScopedErrorExpecter expecter;
  // TODO(shess): See EntryRecords test.
  expecter.ExpectError(SQLITE_CONSTRAINT);

  const GURL kFooNameSpace1("http://foo/namespace1");
  const GURL kFooNameSpace2("http://foo/namespace2");
  const GURL kFooFallbackEntry("http://foo/entry");
  const url::Origin kFooOrigin(url::Origin::Create(kFooNameSpace1));
  const GURL kBarNameSpace1("http://bar/namespace1");
  const GURL kBarNameSpace2("http://bar/namespace2");
  const GURL kBarFallbackEntry("http://bar/entry");
  const url::Origin kBarOrigin(url::Origin::Create(kBarNameSpace1));

  const AppCacheDatabase::NamespaceRecord kZeroRecord;
  AppCacheDatabase::NamespaceRecord record;
  std::vector<AppCacheDatabase::NamespaceRecord> intercepts;
  std::vector<AppCacheDatabase::NamespaceRecord> fallbacks;

  // Behavior with an empty table
  EXPECT_TRUE(db.FindNamespacesForCache(1, &intercepts, &fallbacks));
  EXPECT_TRUE(fallbacks.empty());
  EXPECT_TRUE(db.FindNamespacesForOrigin(kFooOrigin, &intercepts, &fallbacks));
  EXPECT_TRUE(fallbacks.empty());
  EXPECT_TRUE(db.DeleteNamespacesForCache(1));

  // Two records for two differenent caches in the Foo origin.
  record.cache_id = 1;
  record.origin = kFooOrigin;
  record.namespace_.namespace_url = kFooNameSpace1;
  record.namespace_.target_url = kFooFallbackEntry;
  EXPECT_TRUE(db.InsertNamespace(&record));
  EXPECT_FALSE(db.InsertNamespace(&record));

  record.cache_id = 2;
  record.origin = kFooOrigin;
  record.namespace_.namespace_url = kFooNameSpace2;
  record.namespace_.target_url = kFooFallbackEntry;
  EXPECT_TRUE(db.InsertNamespace(&record));

  fallbacks.clear();
  EXPECT_TRUE(db.FindNamespacesForCache(1, &intercepts, &fallbacks));
  EXPECT_EQ(1U, fallbacks.size());
  EXPECT_EQ(1, fallbacks[0].cache_id);
  EXPECT_EQ(kFooOrigin, fallbacks[0].origin);
  EXPECT_EQ(kFooNameSpace1, fallbacks[0].namespace_.namespace_url);
  EXPECT_EQ(kFooFallbackEntry, fallbacks[0].namespace_.target_url);
  EXPECT_FALSE(fallbacks[0].namespace_.is_pattern);

  fallbacks.clear();
  EXPECT_TRUE(db.FindNamespacesForCache(2, &intercepts, &fallbacks));
  EXPECT_EQ(1U, fallbacks.size());
  EXPECT_EQ(2, fallbacks[0].cache_id);
  EXPECT_EQ(kFooOrigin, fallbacks[0].origin);
  EXPECT_EQ(kFooNameSpace2, fallbacks[0].namespace_.namespace_url);
  EXPECT_EQ(kFooFallbackEntry, fallbacks[0].namespace_.target_url);
  EXPECT_FALSE(fallbacks[0].namespace_.is_pattern);

  fallbacks.clear();
  EXPECT_TRUE(db.FindNamespacesForOrigin(kFooOrigin, &intercepts, &fallbacks));
  EXPECT_EQ(2U, fallbacks.size());
  EXPECT_EQ(1, fallbacks[0].cache_id);
  EXPECT_EQ(kFooOrigin, fallbacks[0].origin);
  EXPECT_EQ(kFooNameSpace1, fallbacks[0].namespace_.namespace_url);
  EXPECT_EQ(kFooFallbackEntry, fallbacks[0].namespace_.target_url);
  EXPECT_FALSE(fallbacks[0].namespace_.is_pattern);
  EXPECT_EQ(2, fallbacks[1].cache_id);
  EXPECT_EQ(kFooOrigin, fallbacks[1].origin);
  EXPECT_EQ(kFooNameSpace2, fallbacks[1].namespace_.namespace_url);
  EXPECT_EQ(kFooFallbackEntry, fallbacks[1].namespace_.target_url);
  EXPECT_FALSE(fallbacks[1].namespace_.is_pattern);

  EXPECT_TRUE(db.DeleteNamespacesForCache(1));
  fallbacks.clear();
  EXPECT_TRUE(db.FindNamespacesForOrigin(kFooOrigin, &intercepts, &fallbacks));
  EXPECT_EQ(1U, fallbacks.size());
  EXPECT_EQ(2, fallbacks[0].cache_id);
  EXPECT_EQ(kFooOrigin, fallbacks[0].origin);
  EXPECT_EQ(kFooNameSpace2, fallbacks[0].namespace_.namespace_url);
  EXPECT_EQ(kFooFallbackEntry, fallbacks[0].namespace_.target_url);
  EXPECT_FALSE(fallbacks[0].namespace_.is_pattern);

  // Two more records for the same cache in the Bar origin.
  record.cache_id = 3;
  record.origin = kBarOrigin;
  record.namespace_.namespace_url = kBarNameSpace1;
  record.namespace_.target_url = kBarFallbackEntry;
  record.namespace_.is_pattern = true;
  EXPECT_TRUE(db.InsertNamespace(&record));

  record.cache_id = 3;
  record.origin = kBarOrigin;
  record.namespace_.namespace_url = kBarNameSpace2;
  record.namespace_.target_url = kBarFallbackEntry;
  record.namespace_.is_pattern = true;
  EXPECT_TRUE(db.InsertNamespace(&record));

  fallbacks.clear();
  EXPECT_TRUE(db.FindNamespacesForCache(3, &intercepts, &fallbacks));
  EXPECT_EQ(2U, fallbacks.size());
  EXPECT_TRUE(fallbacks[0].namespace_.is_pattern);
  EXPECT_TRUE(fallbacks[1].namespace_.is_pattern);

  fallbacks.clear();
  EXPECT_TRUE(db.FindNamespacesForOrigin(kBarOrigin, &intercepts, &fallbacks));
  EXPECT_EQ(2U, fallbacks.size());
  EXPECT_TRUE(fallbacks[0].namespace_.is_pattern);
  EXPECT_TRUE(fallbacks[1].namespace_.is_pattern);

  ASSERT_TRUE(expecter.SawExpectedErrors());
}

TEST(AppCacheDatabaseTest, OnlineWhiteListRecords) {
  const base::FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  const GURL kFooNameSpace1("http://foo/namespace1");
  const GURL kFooNameSpace2("http://foo/namespace2");
  const GURL kBarNameSpace1("http://bar/namespace1");

  const AppCacheDatabase::OnlineWhiteListRecord kZeroRecord;
  AppCacheDatabase::OnlineWhiteListRecord record;
  std::vector<AppCacheDatabase::OnlineWhiteListRecord> records;

  // Behavior with an empty table
  EXPECT_TRUE(db.FindOnlineWhiteListForCache(1, &records));
  EXPECT_TRUE(records.empty());
  EXPECT_TRUE(db.DeleteOnlineWhiteListForCache(1));

  record.cache_id = 1;
  record.namespace_url = kFooNameSpace1;
  EXPECT_TRUE(db.InsertOnlineWhiteList(&record));
  record.namespace_url = kFooNameSpace2;
  record.is_pattern = true;
  EXPECT_TRUE(db.InsertOnlineWhiteList(&record));
  records.clear();
  EXPECT_TRUE(db.FindOnlineWhiteListForCache(1, &records));
  EXPECT_EQ(2U, records.size());
  EXPECT_EQ(1, records[0].cache_id);
  EXPECT_EQ(kFooNameSpace1, records[0].namespace_url);
  EXPECT_FALSE(records[0].is_pattern);
  EXPECT_EQ(1, records[1].cache_id);
  EXPECT_EQ(kFooNameSpace2, records[1].namespace_url);
  EXPECT_TRUE(records[1].is_pattern);

  record.cache_id = 2;
  record.namespace_url = kBarNameSpace1;
  EXPECT_TRUE(db.InsertOnlineWhiteList(&record));
  records.clear();
  EXPECT_TRUE(db.FindOnlineWhiteListForCache(2, &records));
  EXPECT_EQ(1U, records.size());

  EXPECT_TRUE(db.DeleteOnlineWhiteListForCache(1));
  records.clear();
  EXPECT_TRUE(db.FindOnlineWhiteListForCache(1, &records));
  EXPECT_TRUE(records.empty());
}

TEST(AppCacheDatabaseTest, DeletableResponseIds) {
  const base::FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  sql::test::ScopedErrorExpecter expecter;
  // TODO(shess): See EntryRecords test.
  expecter.ExpectError(SQLITE_CONSTRAINT);

  std::vector<int64_t> ids;

  EXPECT_TRUE(db.GetDeletableResponseIds(
      &ids, std::numeric_limits<int64_t>::max(), 100));
  EXPECT_TRUE(ids.empty());
  ids.push_back(0);
  EXPECT_TRUE(db.DeleteDeletableResponseIds(ids));
  EXPECT_TRUE(db.InsertDeletableResponseIds(ids));

  ids.clear();
  EXPECT_TRUE(db.GetDeletableResponseIds(
      &ids, std::numeric_limits<int64_t>::max(), 100));
  EXPECT_EQ(1U, ids.size());
  EXPECT_EQ(0, ids[0]);

  int64_t unused, deleteable_response_rowid;
  unused = deleteable_response_rowid = 0;
  EXPECT_TRUE(db.FindLastStorageIds(&unused, &unused, &unused,
                                    &deleteable_response_rowid));
  EXPECT_EQ(1, deleteable_response_rowid);


  // Expected to fail due to the duplicate id, 0 is already in the table.
  ids.clear();
  ids.push_back(0);
  ids.push_back(1);
  EXPECT_FALSE(db.InsertDeletableResponseIds(ids));

  ids.clear();
  for (int i = 1; i < 10; ++i)
    ids.push_back(i);
  EXPECT_TRUE(db.InsertDeletableResponseIds(ids));
  EXPECT_TRUE(db.FindLastStorageIds(&unused, &unused, &unused,
                                    &deleteable_response_rowid));
  EXPECT_EQ(10, deleteable_response_rowid);

  ids.clear();
  EXPECT_TRUE(db.GetDeletableResponseIds(
      &ids, std::numeric_limits<int64_t>::max(), 100));
  EXPECT_EQ(10U, ids.size());
  for (int i = 0; i < 10; ++i)
    EXPECT_EQ(i, ids[i]);

  // Ensure the limit is respected.
  ids.clear();
  EXPECT_TRUE(
      db.GetDeletableResponseIds(&ids, std::numeric_limits<int64_t>::max(), 5));
  EXPECT_EQ(5U, ids.size());
  for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    EXPECT_EQ(i, ids[i]);

  // Ensure the max_rowid is respected (the first rowid is 1).
  ids.clear();
  EXPECT_TRUE(db.GetDeletableResponseIds(&ids, 5, 100));
  EXPECT_EQ(5U, ids.size());
  for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    EXPECT_EQ(i, ids[i]);

  // Ensure that we can delete from the table.
  EXPECT_TRUE(db.DeleteDeletableResponseIds(ids));
  ids.clear();
  EXPECT_TRUE(db.GetDeletableResponseIds(
      &ids, std::numeric_limits<int64_t>::max(), 100));
  EXPECT_EQ(5U, ids.size());
  for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    EXPECT_EQ(i + 5, ids[i]);

  ASSERT_TRUE(expecter.SawExpectedErrors());
}

TEST(AppCacheDatabaseTest, OriginUsage) {
  const GURL kManifestUrl("http://blah/manifest");
  const GURL kManifestUrl2("http://blah/manifest2");
  const url::Origin kOrigin = url::Origin::Create(kManifestUrl);
  const GURL kOtherOriginManifestUrl("http://other/manifest");
  const url::Origin kOtherOrigin = url::Origin::Create(kOtherOriginManifestUrl);

  const base::FilePath kEmptyPath;
  AppCacheDatabase db(kEmptyPath);
  EXPECT_TRUE(db.LazyOpen(true));

  std::vector<AppCacheDatabase::CacheRecord> cache_records;
  EXPECT_EQ(0, db.GetOriginUsage(kOrigin));
  EXPECT_TRUE(db.FindCachesForOrigin(kOrigin, &cache_records));
  EXPECT_TRUE(cache_records.empty());

  AppCacheDatabase::GroupRecord group_record;
  group_record.group_id = 1;
  group_record.manifest_url = kManifestUrl;
  group_record.origin = kOrigin;
  EXPECT_TRUE(db.InsertGroup(&group_record));
  AppCacheDatabase::CacheRecord cache_record;
  cache_record.cache_id = 1;
  cache_record.group_id = 1;
  cache_record.online_wildcard = true;
  cache_record.update_time = kZeroTime;
  cache_record.cache_size = 100;
  EXPECT_TRUE(db.InsertCache(&cache_record));

  EXPECT_EQ(100, db.GetOriginUsage(kOrigin));

  group_record.group_id = 2;
  group_record.manifest_url = kManifestUrl2;
  group_record.origin = kOrigin;
  EXPECT_TRUE(db.InsertGroup(&group_record));
  cache_record.cache_id = 2;
  cache_record.group_id = 2;
  cache_record.online_wildcard = true;
  cache_record.update_time = kZeroTime;
  cache_record.cache_size = 1000;
  EXPECT_TRUE(db.InsertCache(&cache_record));

  EXPECT_EQ(1100, db.GetOriginUsage(kOrigin));

  group_record.group_id = 3;
  group_record.manifest_url = kOtherOriginManifestUrl;
  group_record.origin = kOtherOrigin;
  EXPECT_TRUE(db.InsertGroup(&group_record));
  cache_record.cache_id = 3;
  cache_record.group_id = 3;
  cache_record.online_wildcard = true;
  cache_record.update_time = kZeroTime;
  cache_record.cache_size = 5000;
  EXPECT_TRUE(db.InsertCache(&cache_record));

  EXPECT_EQ(5000, db.GetOriginUsage(kOtherOrigin));

  EXPECT_TRUE(db.FindCachesForOrigin(kOrigin, &cache_records));
  EXPECT_EQ(2U, cache_records.size());
  cache_records.clear();
  EXPECT_TRUE(db.FindCachesForOrigin(kOtherOrigin, &cache_records));
  EXPECT_EQ(1U, cache_records.size());

  std::map<url::Origin, int64_t> usage_map;
  EXPECT_TRUE(db.GetAllOriginUsage(&usage_map));
  EXPECT_EQ(2U, usage_map.size());
  EXPECT_EQ(1100, usage_map[kOrigin]);
  EXPECT_EQ(5000, usage_map[kOtherOrigin]);
}

TEST(AppCacheDatabaseTest, UpgradeSchemaNukesDeprecatedVersion) {
  // Real file on disk for this test.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kDbFile =
      temp_dir.GetPath().AppendASCII("deprecated.db");

  // Create a database with a table name that does not show up in the AppCache
  // schema. This table would not be touched by any migration, so its existence
  // indicates that the database was not nuked.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(kDbFile));

    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db, 6, 6));

    static const char kSchemaSql[] =
        "CREATE TABLE Unused(id INTEGER PRIMARY KEY)";
    EXPECT_TRUE(db.Execute(kSchemaSql));

    EXPECT_TRUE(db.DoesColumnExist("Unused", "id"));
  }

  // Open that database and verify that it got nuked.
  AppCacheDatabase db(kDbFile);
  EXPECT_TRUE(db.LazyOpen(/*create_if_needed=*/false));
  EXPECT_FALSE(db.db_->DoesColumnExist("Unused", "id"));
  EXPECT_TRUE(db.db_->DoesColumnExist("Groups",
                                      "last_full_update_check_time"));
  EXPECT_EQ(7, db.meta_table_->GetVersionNumber());
  EXPECT_EQ(7, db.meta_table_->GetCompatibleVersionNumber());
}

}  // namespace content
