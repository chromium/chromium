// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_queries.h"

#include <cstdint>
#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
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

// Defines the set of queries that are used for schema and index creation. These
// queries are not suitable for checking plans against an already-initialized
// database.
constexpr auto kSchemaAndIndexQueries = base::MakeFixedFlatSet<Query>({
    Query::kInitSchema_CreateTableResources,
    Query::kInitSchema_CreateTableBlobs,
    Query::kIndex_ResourcesCacheKeyHashDoomed,
    Query::kIndex_LiveResourcesLastUsed,
    Query::kIndex_LiveResourcesHints,
    Query::kIndex_BlobsResIdStart,
});

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
    auto store = std::make_unique<disk_cache::SqlPersistentStore>(
        path, kDefaultMaxBytes, net::CacheType::DISK_CACHE,
        std::vector<scoped_refptr<base::SequencedTaskRunner>>(
            {background_task_runner}));

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
    command_line.AppendArgPath(temp_dir_.GetPath().Append(
        disk_cache::kSqlBackendDatabaseShard0FileName));

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
          {{Query::kOpenEntry_SelectLiveResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_cache_key_hash_doomed "
            "(cache_key_hash=? AND doomed=?)"},
           {Query::kCreateEntry_InsertIntoResources, ""},
           {Query::kDoomEntry_MarkDoomedResources,
            "`--SEARCH resources USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kDeleteDoomedEntry_DeleteFromResources,
            "`--SEARCH resources USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kDeleteLiveEntry_DeleteFromResources,
            "`--SEARCH resources USING "
            "INDEX index_resources_cache_key_hash_doomed "
            "(cache_key_hash=? AND doomed=?)"},
           {Query::kDeleteAllEntries_DeleteFromResources, ""},
           {Query::kDeleteAllEntries_DeleteFromBlobs, ""},
           {Query::kDeleteLiveEntriesBetween_SelectLiveResources,
            "`--SEARCH resources USING "
            "COVERING INDEX index_live_resources_last_used_bytes_usage "
            "(last_used>? AND last_used<?)"},
           {Query::kDeleteResourceByResIds_DeleteFromResources,
            "`--SEARCH resources USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kUpdateEntryLastUsedByKey_UpdateResourceLastUsed,
            "`--SEARCH resources USING "
            "INDEX index_resources_cache_key_hash_doomed "
            "(cache_key_hash=? AND doomed=?)"},
           {Query::kUpdateEntryLastUsedByResId_UpdateResourceLastUsed,
            "`--SEARCH resources USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kUpdateEntryHeaderAndLastUsed_UpdateResource,
            "`--SEARCH resources USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kUpdateEntryHeaderAndLastUsed_UpdateResourceAndHints,
            "`--SEARCH resources USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kWriteEntryData_UpdateResource,
            "`--SEARCH resources USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kTrimOverlappingBlobs_DeleteContained,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_res_id_start "
            "(res_id=? AND start>?)"},
           {Query::kTrimOverlappingBlobs_SelectOverlapping,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_res_id_start "
            "(res_id=? AND start<?)"},
           {Query::kTruncateBlobsAfter_DeleteAfter,
            "`--SEARCH blobs USING "
            "COVERING INDEX index_blobs_res_id_start "
            "(res_id=? AND start>?)"},
           {Query::kInsertNewBlob_InsertIntoBlobs, ""},
           {Query::kDeleteBlobById_DeleteFromBlobs,
            "`--SEARCH blobs USING "
            "INTEGER PRIMARY KEY (rowid=?)"},
           {Query::kDeleteBlobsByResId_DeleteFromBlobs,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_res_id_start "
            "(res_id=?)"},
           {Query::kReadEntryData_SelectOverlapping,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_res_id_start "
            "(res_id=? AND start<?)"},
           {Query::kGetEntryAvailableRange_SelectOverlapping,
            "`--SEARCH blobs USING "
            "INDEX index_blobs_res_id_start "
            "(res_id=? AND start<?)"},
           {Query::kCalculateSizeOfEntriesBetween_SelectLiveResources,
            "`--SEARCH resources USING "
            "COVERING INDEX index_live_resources_last_used_bytes_usage "
            "(last_used>? AND last_used<?)"},
           {Query::kOpenNextEntry_SelectLiveResources,
            "`--SEARCH resources USING "
            "INTEGER PRIMARY KEY (rowid<?)"},
           {Query::kStartEviction_SelectLiveResources,
            "`--SCAN resources USING "
            "COVERING INDEX index_live_resources_last_used_bytes_usage"},
           {Query::kCalculateResourceEntryCount_SelectCountFromLiveResources,
            "`--SCAN resources USING "
            "COVERING INDEX index_live_resources_last_used_bytes_usage"},
           {Query::kCalculateTotalSize_SelectTotalSizeFromLiveResources,
            "`--SCAN resources USING "
            "COVERING INDEX index_live_resources_last_used_bytes_usage"},
           {Query::kLoadInMemoryIndex_SelectCacheKeyHashFromLiveResources,
            "`--SCAN resources USING COVERING INDEX "
            "index_resources_cache_key_hash_doomed"},
           {Query::kLoadInMemoryIndex_SelectHintsFromLiveResources,
            "`--SCAN resources USING COVERING INDEX "
            "index_live_resources_hints"}});
  static_assert(kAllQueriesAndPlans.size() + kSchemaAndIndexQueries.size() ==
                static_cast<int>(Query::kMaxValue) + 1);
  for (const auto& it : kAllQueriesAndPlans) {
    const base::cstring_view query_string = GetQuery(it.first);
    SCOPED_TRACE(query_string);
    EXPECT_EQ(GetQueryPlan(query_string), it.second);
  }
}

}  // namespace disk_cache_sql_queries
