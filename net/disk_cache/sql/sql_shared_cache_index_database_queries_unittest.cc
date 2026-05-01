// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_index_database_queries.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_shared_cache_index_database.h"
#include "net/disk_cache/sql/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache_sql_queries {

constexpr auto kSchemaAndIndexQueries =
    base::MakeFixedFlatSet<SharedCacheIndexDatabaseQuery>({
        SharedCacheIndexDatabaseQuery::kCreateStoragesTable,
        SharedCacheIndexDatabaseQuery::kCreateUniqueIndex,
    });

class SqlSharedCacheIndexDatabaseQueriesTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void CreateDatabaseInTempDir() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    disk_cache::SqlSharedCacheIndexDatabase db(temp_dir_.GetPath());
    ASSERT_TRUE(db.Initialize().has_value());
  }

  std::string GetQueryPlan(std::string_view query) {
    return disk_cache::test::GetQueryPlan(
        temp_dir_.GetPath().Append(
            disk_cache::kSqlBackendSharedCacheIndexFileName),
        query);
  }

  base::ScopedTempDir temp_dir_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SqlSharedCacheIndexDatabaseQueriesTest, AllQueriesHaveValidPlan) {
  CreateDatabaseInTempDir();

  constexpr auto kAllQueriesAndPlans =
      base::MakeFixedFlatMap<SharedCacheIndexDatabaseQuery, std::string_view>({
          {SharedCacheIndexDatabaseQuery::kSelectDbIdByIsolationKey,
           "`--SEARCH storages USING COVERING INDEX unique_index "
           "(isolation_key=?)"},
          {SharedCacheIndexDatabaseQuery::kSelectIsolationKeyByDbId,
           "`--SEARCH storages USING INTEGER PRIMARY KEY (rowid=?)"},
          {SharedCacheIndexDatabaseQuery::kInsertStorage, ""},
          {SharedCacheIndexDatabaseQuery::kDeleteStorage,
           "`--SEARCH storages USING INTEGER PRIMARY KEY (rowid=?)"},
      });
  static_assert(kAllQueriesAndPlans.size() + kSchemaAndIndexQueries.size() ==
                static_cast<int>(SharedCacheIndexDatabaseQuery::kMaxValue) + 1);

  for (const auto& it : kAllQueriesAndPlans) {
    const std::string_view query_string =
        GetSharedCacheIndexDatabaseQuery(it.first);
    SCOPED_TRACE(query_string);
    EXPECT_EQ(GetQueryPlan(query_string), it.second);
  }
}

}  // namespace disk_cache_sql_queries
