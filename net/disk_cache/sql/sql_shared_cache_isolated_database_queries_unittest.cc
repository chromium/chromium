// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_isolated_database_queries.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "net/disk_cache/sql/sql_backend_aliases.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"
#include "net/disk_cache/sql/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using disk_cache_sql_queries::GetSharedCacheIsolatedDatabaseQuery;
using disk_cache_sql_queries::SharedCacheIsolatedDatabaseQuery;

namespace disk_cache_sql_queries {

constexpr auto kSchemaAndIndexQueries =
    base::MakeFixedFlatSet<SharedCacheIsolatedDatabaseQuery>({
        SharedCacheIsolatedDatabaseQuery::kCreateResourcesTable,
        SharedCacheIsolatedDatabaseQuery::kCreateResourcesTableIndex,
    });

class SqlSharedCacheIsolatedDatabaseQueriesTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void CreateDatabaseInTempDir() {
    disk_cache::SqlSharedCacheIsolatedDatabase db(
        "test_nik", temp_dir_.GetPath(), disk_cache::SqlSharedCacheDbId(1),
        base::SequencedTaskRunner::GetCurrentDefault());
    EXPECT_TRUE(db.Init().has_value());
  }

  std::string GetQueryPlan(std::string_view query) {
    base::FilePath db_path = temp_dir_.GetPath().AppendASCII("shared_1.db");
    return disk_cache::test::GetQueryPlan(db_path, query);
  }

  base::ScopedTempDir temp_dir_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SqlSharedCacheIsolatedDatabaseQueriesTest, AllQueriesHaveValidPlan) {
  CreateDatabaseInTempDir();

  constexpr auto kAllQueriesAndPlans =
      base::MakeFixedFlatMap<SharedCacheIsolatedDatabaseQuery,
                             std::string_view>({
          {SharedCacheIsolatedDatabaseQuery::kInsertResource, ""},
          {SharedCacheIsolatedDatabaseQuery::kSetResourceReady,
           "`--SEARCH resources USING INTEGER PRIMARY KEY (rowid=?)"},
          {SharedCacheIsolatedDatabaseQuery::kSelectUrlAndReadyByRowId,
           "`--SEARCH resources USING INTEGER PRIMARY KEY (rowid=?)"},
          {SharedCacheIsolatedDatabaseQuery::kReaderSelectResource,
           "`--SEARCH resources USING INDEX index_resources_hash (hash=?)"},
          {SharedCacheIsolatedDatabaseQuery::kDeleteResourceByRowId,
           "`--SEARCH resources USING INTEGER PRIMARY KEY (rowid=?)"},
          {SharedCacheIsolatedDatabaseQuery::kSelectRowidLimit1,
           "`--SCAN resources USING COVERING INDEX index_resources_hash"},
      });

  static_assert(kAllQueriesAndPlans.size() + kSchemaAndIndexQueries.size() ==
                static_cast<int>(SharedCacheIsolatedDatabaseQuery::kMaxValue) +
                    1);

  for (const auto& it : kAllQueriesAndPlans) {
    const std::string_view query_string =
        GetSharedCacheIsolatedDatabaseQuery(it.first);
    SCOPED_TRACE(query_string);
    EXPECT_EQ(GetQueryPlan(query_string), it.second);
  }
}

}  // namespace disk_cache_sql_queries
