// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/meta_table.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/sql_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using SQLMetaTableTest = sql::SQLTestBase;

TEST_F(SQLMetaTableTest, DoesTableExist) {
  EXPECT_FALSE(sql::MetaTable::DoesTableExist(&db()));

  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));
  }

  EXPECT_TRUE(sql::MetaTable::DoesTableExist(&db()));
}

TEST_F(SQLMetaTableTest, RazeIfDeprecated) {
  const int kDeprecatedVersion = 1;
  const int kVersion = 2;

  // Setup a current database.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), kVersion, kVersion));
    EXPECT_TRUE(db().Execute("CREATE TABLE t(c)"));
    EXPECT_TRUE(db().DoesTableExist("t"));
  }

  // Table should should still exist if the database version is new enough.
  sql::MetaTable::RazeIfDeprecated(&db(), kDeprecatedVersion);
  EXPECT_TRUE(db().DoesTableExist("t"));

  // TODO(shess): It may make sense to Raze() if meta isn't present or
  // version isn't present.  See meta_table.h TODO on RazeIfDeprecated().

  // Table should still exist if the version is not available.
  EXPECT_TRUE(db().Execute("DELETE FROM meta WHERE key = 'version'"));
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), kVersion, kVersion));
    EXPECT_EQ(0, meta_table.GetVersionNumber());
  }
  sql::MetaTable::RazeIfDeprecated(&db(), kDeprecatedVersion);
  EXPECT_TRUE(db().DoesTableExist("t"));

  // Table should still exist if meta table is missing.
  EXPECT_TRUE(db().Execute("DROP TABLE meta"));
  sql::MetaTable::RazeIfDeprecated(&db(), kDeprecatedVersion);
  EXPECT_TRUE(db().DoesTableExist("t"));

  // Setup meta with deprecated version.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), kDeprecatedVersion, kDeprecatedVersion));
  }

  // Deprecation check should remove the table.
  EXPECT_TRUE(db().DoesTableExist("t"));
  sql::MetaTable::RazeIfDeprecated(&db(), kDeprecatedVersion);
  EXPECT_FALSE(sql::MetaTable::DoesTableExist(&db()));
  EXPECT_FALSE(db().DoesTableExist("t"));
}

TEST_F(SQLMetaTableTest, VersionNumber) {
  // Compatibility versions one less than the main versions to make
  // sure the values aren't being crossed with each other.
  const int kVersionFirst = 2;
  const int kCompatVersionFirst = kVersionFirst - 1;
  const int kVersionSecond = 4;
  const int kCompatVersionSecond = kVersionSecond - 1;
  const int kVersionThird = 6;
  const int kCompatVersionThird = kVersionThird - 1;

  // First Init() sets the version info as expected.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), kVersionFirst, kCompatVersionFirst));
    EXPECT_EQ(kVersionFirst, meta_table.GetVersionNumber());
    EXPECT_EQ(kCompatVersionFirst, meta_table.GetCompatibleVersionNumber());
  }

  // Second Init() does not change the version info.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), kVersionSecond, kCompatVersionSecond));
    EXPECT_EQ(kVersionFirst, meta_table.GetVersionNumber());
    EXPECT_EQ(kCompatVersionFirst, meta_table.GetCompatibleVersionNumber());

    meta_table.SetVersionNumber(kVersionSecond);
    meta_table.SetCompatibleVersionNumber(kCompatVersionSecond);
  }

  // Version info from Set*() calls is seen.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), kVersionThird, kCompatVersionThird));
    EXPECT_EQ(kVersionSecond, meta_table.GetVersionNumber());
    EXPECT_EQ(kCompatVersionSecond, meta_table.GetCompatibleVersionNumber());
  }
}

TEST_F(SQLMetaTableTest, StringValue) {
  static const char kKey[] = "String Key";
  const std::string kFirstValue("First Value");
  const std::string kSecondValue("Second Value");

  // Initially, the value isn't there until set.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    std::string value;
    EXPECT_FALSE(meta_table.GetValue(kKey, &value));

    EXPECT_TRUE(meta_table.SetValue(kKey, kFirstValue));
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);
  }

  // Value is persistent across different instances.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    std::string value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);

    EXPECT_TRUE(meta_table.SetValue(kKey, kSecondValue));
  }

  // Existing value was successfully changed.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    std::string value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kSecondValue, value);
  }
}

TEST_F(SQLMetaTableTest, IntValue) {
  static const char kKey[] = "Int Key";
  const int kFirstValue = 17;
  const int kSecondValue = 23;

  // Initially, the value isn't there until set.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int value;
    EXPECT_FALSE(meta_table.GetValue(kKey, &value));

    EXPECT_TRUE(meta_table.SetValue(kKey, kFirstValue));
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);
  }

  // Value is persistent across different instances.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);

    EXPECT_TRUE(meta_table.SetValue(kKey, kSecondValue));
  }

  // Existing value was successfully changed.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kSecondValue, value);
  }
}

TEST_F(SQLMetaTableTest, Int64Value) {
  static const char kKey[] = "Int Key";
  const int64_t kFirstValue = 5000000017LL;
  const int64_t kSecondValue = 5000000023LL;

  // Initially, the value isn't there until set.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int64_t value;
    EXPECT_FALSE(meta_table.GetValue(kKey, &value));

    EXPECT_TRUE(meta_table.SetValue(kKey, kFirstValue));
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);
  }

  // Value is persistent across different instances.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int64_t value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);

    EXPECT_TRUE(meta_table.SetValue(kKey, kSecondValue));
  }

  // Existing value was successfully changed.
  {
    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

    int64_t value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kSecondValue, value);
  }
}

TEST_F(SQLMetaTableTest, DeleteKey) {
  static const char kKey[] = "String Key";
  const std::string kValue("String Value");

  sql::MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(&db(), 1, 1));

  // Value isn't present.
  std::string value;
  EXPECT_FALSE(meta_table.GetValue(kKey, &value));

  // Now value is present.
  EXPECT_TRUE(meta_table.SetValue(kKey, kValue));
  EXPECT_TRUE(meta_table.GetValue(kKey, &value));
  EXPECT_EQ(kValue, value);

  // After delete value isn't present.
  EXPECT_TRUE(meta_table.DeleteKey(kKey));
  EXPECT_FALSE(meta_table.GetValue(kKey, &value));
}

}  // namespace
