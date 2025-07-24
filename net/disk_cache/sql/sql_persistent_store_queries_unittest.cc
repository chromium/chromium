// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_queries.h"

#include <cstdint>
#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache_sql_queries {

class SqlPersistentStoreQueriesTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Creates a database file with the correct schema in the temporary directory.
  // This is done by instantiating and initializing a SqlPersistentStore, which
  // handles schema creation, and then closing it to ensure all changes are
  // flushed to disk.
  void CreateDatabaseInTemDir() {
    const base::FilePath path = temp_dir_.GetPath();
    const int64_t kDefaultMaxBytes = 10 * 1024 * 1024;
    auto background_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

    // Create and initialize a store to create the DB file with schema.
    auto store = disk_cache::SqlPersistentStore::Create(
        path, kDefaultMaxBytes, net::CacheType::DISK_CACHE,
        background_task_runner);

    base::test::TestFuture<disk_cache::SqlPersistentStore::Error> future;
    store->Initialize(future.GetCallback());
    ASSERT_EQ(future.Get(), disk_cache::SqlPersistentStore::Error::kOk);

    // Close the store by destroying it. This ensures all data is flushed.
    store.reset();
    // Wait for background tasks to finish by posting a task to the runner and
    // waiting for it to execute.
    base::RunLoop run_loop;
    background_task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Executes `EXPLAIN QUERY PLAN` for a given SQL query using the
  // `sqlite_dev_shell` tool. This allows verifying that the query optimizer is
  // using the expected indexes, which is critical for performance.
  std::string GetQueryPlan(base::cstring_view query) {
    base::CommandLine command_line(GetExecSqlShellPath());
    command_line.AppendArgPath(
        temp_dir_.GetPath().Append(disk_cache::kSqlBackendDatabaseFileName));

    std::string explain_query = base::StrCat({"EXPLAIN QUERY PLAN ", query});
    command_line.AppendArg(explain_query);

    std::string output;
    if (!base::GetAppOutput(command_line, &output)) {
      return "Failed to execute sqlite_dev_shell";
    }
    base::TrimWhitespaceASCII(output, base::TRIM_ALL, &output);
    if (base::StartsWith(output, "QUERY PLAN")) {
      std::string_view temp = output;
      temp.remove_prefix(strlen("QUERY PLAN"));
      temp = base::TrimWhitespaceASCII(temp, base::TRIM_LEADING);
      output = std::string(temp);
    }
    return output;
  }

  base::ScopedTempDir temp_dir_;

 private:
  // Helper to locate the `sqlite_dev_shell` executable, which is expected to be
  // in the same directory as the test executable.
  base::FilePath GetExecSqlShellPath() {
    base::FilePath path;
    base::PathService::Get(base::DIR_EXE, &path);
    return path.AppendASCII("sqlite_dev_shell");
  }

  base::test::TaskEnvironment task_environment_;
};

// This test verifies that all SQL query strings defined in
// `sql_persistent_store_queries.h` are syntactically valid. This acts as a
// basic sanity check to catch typos and other errors at test time rather than
// at runtime.
TEST_F(SqlPersistentStoreQueriesTest, AllQueriesAreValid) {
  CreateDatabaseInTemDir();
  std::unique_ptr<sql::Database> db = std::make_unique<sql::Database>(
      sql::DatabaseOptions(), sql::Database::Tag("HttpCacheDiskCache"));
  ASSERT_TRUE(db->Open(
      temp_dir_.GetPath().Append(disk_cache::kSqlBackendDatabaseFileName)));

  for (int i = 0; i <= static_cast<int>(Query::kMaxValue); ++i) {
    const Query query_id = static_cast<Query>(i);
    const base::cstring_view query = GetQuery(query_id);
    SCOPED_TRACE(query);
    EXPECT_TRUE(db->IsSQLValid(query));
  }
}

// This test verifies that critical SQL queries use the intended indexes by
// checking their query plans. This is essential for ensuring the performance of
// database operations. A query that performs a full table scan instead of using
// an index can lead to significant performance degradation.
TEST_F(SqlPersistentStoreQueriesTest, AllQueriesHaveValidPlan) {
  CreateDatabaseInTemDir();

  // Defines the expected query plan for each query. An empty plan indicates
  // that no specific plan is expected (e.g., for INSERT statements), or that
  // the query is a schema-related query that cannot be explained.
  constexpr auto kAllQueriesAndPlans =
      base::MakeFixedFlatMap<Query, std::string_view>(
          {{Query::kInitSchema_CreateTableResources, ""},
           {Query::kInitSchema_CreateTableBlobs, ""},
           {Query::kIndex_ResourcesToken, ""},
           {Query::kIndex_ResourcesCacheKeyDoomed, ""},
           {Query::kIndex_ResourcesDoomedLastUsed, ""},
           {Query::kIndex_ResourcesDoomedResId, ""},
           {Query::kIndex_BlobsTokenStart, ""},
           {Query::kOpenEntry_SelectLiveResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_cache_key_doomed "
            "(cache_key=? AND doomed=?)"},
           {Query::kCreateEntry_InsertIntoResources, ""},
           {Query::kDoomEntry_MarkDoomedResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_token "
            "(token_high=? AND token_low=?)"},
           {Query::kDeleteDoomedEntry_DeleteFromResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_token "
            "(token_high=? AND token_low=?)"},
           {Query::kDeleteDoomedEntries_SelectDoomedResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_doomed_res_id "
            "(doomed=?)"},
           {Query::kDeleteLiveEntry_DeleteFromResources,
            "`--SEARCH resources USING "
            "COVERING INDEX index_resources_cache_key_doomed "
            "(cache_key=? AND doomed=?)"},
           {Query::kDeleteAllEntries_DeleteFromResources, ""},
           {Query::kDeleteAllEntries_DeleteFromBlobs, ""},
           {Query::kDeleteLiveEntriesBetween_SelectLiveResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_doomed_last_used "
            "(doomed=? AND last_used>? AND last_used<?)"},
           {Query::kDeleteResourcesByResIds_DeleteFromResources,
            "`--SEARCH resources USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kUpdateEntryLastUsed_UpdateResourceLastUsed,
            "`--SEARCH resources USING "
            "INDEX index_resources_cache_key_doomed "
            "(cache_key=? AND doomed=?)"},
           {Query::kUpdateEntryHeaderAndLastUsed_UpdateResource,
            "`--SEARCH resources USING INDEX "
            "index_resources_token "
            "(token_high=? AND token_low=?)"},
           {Query::kWriteEntryData_UpdateResource,
            "`--SEARCH resources USING "
            "INDEX index_resources_token "
            "(token_high=? AND token_low=?)"},
           {Query::kTrimOverlappingBlobs_DeleteContained,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_token_start "
            "(token_high=? AND token_low=? AND start>?)"},
           {Query::kTrimOverlappingBlobs_SelectOverlapping,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_token_start "
            "(token_high=? AND token_low=? AND start<?)"},
           {Query::kTruncateBlobsAfter_DeleteAfter,
            "`--SEARCH blobs USING "
            "COVERING INDEX index_blobs_token_start "
            "(token_high=? AND token_low=? AND start>?)"},
           {Query::kInsertNewBlob_InsertIntoBlobs, ""},
           {Query::kDeleteBlobById_DeleteFromBlobs,
            "`--SEARCH blobs USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kDeleteBlobsByToken_DeleteFromBlobs,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_token_start "
            "(token_high=? AND token_low=?)"},
           {Query::kReadEntryData_SelectOverlapping,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_token_start "
            "(token_high=? AND token_low=? AND start<?)"},
           {Query::kGetEntryAvailableRange_SelectOverlapping,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_token_start "
            "(token_high=? AND token_low=? AND start<?)"},
           {Query::kCalculateSizeOfEntriesBetween_SelectLiveResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_doomed_last_used "
            "(doomed=? AND last_used>? AND last_used<?)"},
           {Query::kOpenLatestEntryBeforeResId_SelectLiveResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_doomed_res_id "
            "(doomed=? AND res_id<?)"},
           {Query::kRunEviction_SelectLiveResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_doomed_last_used "
            "(doomed=?)"},
           {Query::kRunEviction_DeleteFromResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_token "
            "(token_high=? AND token_low=?)"},
           {Query::kCalculateResourceEntryCount_SelectCountFromLiveResources,
            "`--SEARCH resources USING "
            "COVERING INDEX index_resources_doomed_res_id "
            "(doomed=?)"},
           {Query::kCalculateTotalSize_SelectTotalSizeFromLiveResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_doomed_res_id "
            "(doomed=?)"}});
  static_assert(kAllQueriesAndPlans.size() ==
                static_cast<int>(Query::kMaxValue) + 1);

  for (int i = 0; i <= static_cast<int>(Query::kMaxValue); ++i) {
    const Query query_id = static_cast<Query>(i);
    const base::cstring_view query_string = GetQuery(query_id);
    SCOPED_TRACE(query_string);
    auto it = kAllQueriesAndPlans.find(query_id);
    ASSERT_NE(it, kAllQueriesAndPlans.end());
    EXPECT_EQ(GetQueryPlan(query_string), it->second);
  }
}

}  // namespace disk_cache_sql_queries
