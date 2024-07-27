// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/meta_table.h"

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

namespace {

class SQLMetaTableTest : public testing::Test {
 public:
  ~SQLMetaTableTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        db_.Open(temp_dir_.GetPath().AppendASCII("meta_table_test.sqlite")));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  Database db_;
};

TEST_F(SQLMetaTableTest, DoesTableExist) {
  EXPECT_FALSE(MetaTable::DoesTableExist(&db_));

  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));
  }

  EXPECT_TRUE(MetaTable::DoesTableExist(&db_));
}

TEST_F(SQLMetaTableTest, DeleteTableForTesting) {
  MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

  EXPECT_TRUE(MetaTable::DeleteTableForTesting(&db_));
  EXPECT_FALSE(MetaTable::DoesTableExist(&db_));
}

TEST_F(SQLMetaTableTest, RazeIfIncompatiblePreservesDatabasesWithoutMetadata) {
  EXPECT_TRUE(db_.Execute("CREATE TABLE data(id INTEGER PRIMARY KEY)"));
  ASSERT_TRUE(db_.DoesTableExist("data"));

  // The table should not have been cleared, since the database does not have a
  // metadata table.
  EXPECT_EQ(RazeIfIncompatibleResult::kCompatible,
            MetaTable::RazeIfIncompatible(&db_, 1,
                                          /*current_version=*/1));
  EXPECT_TRUE(db_.DoesTableExist("data"));
}

TEST_F(SQLMetaTableTest, RazeIfIncompatibleRazesIncompatiblyOldTables) {
  constexpr int kWrittenVersion = 1;
  constexpr int kCompatibleVersion = 1;

  // Setup a current database.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, kWrittenVersion, kCompatibleVersion));
    EXPECT_TRUE(db_.Execute("CREATE TABLE data(id INTEGER PRIMARY KEY)"));
    ASSERT_TRUE(db_.DoesTableExist("data"));
  }

  // The table should have been cleared, since the least version compatible with
  // the written database is greater than the current version.
  EXPECT_EQ(
      RazeIfIncompatibleResult::kRazedSuccessfully,
      MetaTable::RazeIfIncompatible(&db_, kWrittenVersion + 1,
                                    /*current_version=*/kWrittenVersion + 1));
  EXPECT_FALSE(db_.DoesTableExist("data"));
}

TEST_F(SQLMetaTableTest, RazeIfIncompatibleRazesIncompatiblyNewTables) {
  constexpr int kCompatibleVersion = 2;
  constexpr int kWrittenVersion = 3;

  // Setup a current database.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, kWrittenVersion, kCompatibleVersion));
    EXPECT_TRUE(db_.Execute("CREATE TABLE data(id INTEGER PRIMARY KEY)"));
    ASSERT_TRUE(db_.DoesTableExist("data"));
  }

  // The table should have been cleared, since the least version compatible with
  // the written database is greater than the current version.
  EXPECT_EQ(RazeIfIncompatibleResult::kRazedSuccessfully,
            MetaTable::RazeIfIncompatible(
                &db_, MetaTable::kNoLowestSupportedVersion,
                /*current_version=*/kCompatibleVersion - 1));
  EXPECT_FALSE(db_.DoesTableExist("data"));
}

TEST_F(SQLMetaTableTest, RazeIfIncompatibleDoesntRazeWhenItShouldnt) {
  constexpr int kVersion = 2;

  {
    MetaTable meta_table;
    EXPECT_TRUE(
        meta_table.Init(&db_, kVersion, /*compatible_version=*/kVersion - 1));
    EXPECT_TRUE(db_.Execute("CREATE TABLE data(id INTEGER PRIMARY KEY)"));
    EXPECT_TRUE(db_.DoesTableExist("data"));
  }

  EXPECT_EQ(RazeIfIncompatibleResult::kCompatible,
            MetaTable::RazeIfIncompatible(&db_, kVersion,
                                          /*current_version=*/kVersion));
  EXPECT_TRUE(db_.DoesTableExist("data"))
      << "Table should still exist if the database version is exactly right.";

  EXPECT_EQ(RazeIfIncompatibleResult::kCompatible,
            MetaTable::RazeIfIncompatible(&db_, kVersion - 1,
                                          /*current_version=*/kVersion));
  EXPECT_TRUE(db_.DoesTableExist("data"))
      << "... or if the lower bound is less than the actual version";

  EXPECT_EQ(
      RazeIfIncompatibleResult::kCompatible,
      MetaTable::RazeIfIncompatible(&db_, MetaTable::kNoLowestSupportedVersion,
                                    /*current_version=*/kVersion));
  EXPECT_TRUE(db_.DoesTableExist("data"))
      << "... or if the lower bound is not set";

  EXPECT_EQ(
      RazeIfIncompatibleResult::kCompatible,
      MetaTable::RazeIfIncompatible(&db_, MetaTable::kNoLowestSupportedVersion,
                                    /*current_version=*/kVersion - 1));
  EXPECT_TRUE(db_.DoesTableExist("data"))
      << "... even if the current version exactly matches the written "
         "database's least compatible version.";
}

TEST_F(SQLMetaTableTest, VersionNumber) {
  // Compatibility versions one less than the main versions to make
  // sure the values aren't being crossed with each other.
  constexpr int kVersionFirst = 2;
  constexpr int kCompatVersionFirst = kVersionFirst - 1;
  constexpr int kVersionSecond = 4;
  constexpr int kCompatVersionSecond = kVersionSecond - 1;
  constexpr int kVersionThird = 6;
  constexpr int kCompatVersionThird = kVersionThird - 1;

  // First Init() sets the version info as expected.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, kVersionFirst, kCompatVersionFirst));
    EXPECT_EQ(kVersionFirst, meta_table.GetVersionNumber());
    EXPECT_EQ(kCompatVersionFirst, meta_table.GetCompatibleVersionNumber());
  }

  // Second Init() does not change the version info.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, kVersionSecond, kCompatVersionSecond));
    EXPECT_EQ(kVersionFirst, meta_table.GetVersionNumber());
    EXPECT_EQ(kCompatVersionFirst, meta_table.GetCompatibleVersionNumber());

    EXPECT_TRUE(meta_table.SetVersionNumber(kVersionSecond));
    EXPECT_TRUE(meta_table.SetCompatibleVersionNumber(kCompatVersionSecond));
  }

  // Version info from Set*() calls is seen.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, kVersionThird, kCompatVersionThird));
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
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

    std::string value;
    EXPECT_FALSE(meta_table.GetValue(kKey, &value));

    EXPECT_TRUE(meta_table.SetValue(kKey, kFirstValue));
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);
  }

  // Value is persistent across different instances.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

    std::string value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);

    EXPECT_TRUE(meta_table.SetValue(kKey, kSecondValue));
  }

  // Existing value was successfully changed.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

    std::string value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kSecondValue, value);
  }
}

TEST_F(SQLMetaTableTest, IntValue) {
  static const char kKey[] = "Int Key";
  constexpr int kFirstValue = 17;
  constexpr int kSecondValue = 23;

  // Initially, the value isn't there until set.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

    int value;
    EXPECT_FALSE(meta_table.GetValue(kKey, &value));

    EXPECT_TRUE(meta_table.SetValue(kKey, kFirstValue));
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);
  }

  // Value is persistent across different instances.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

    int value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);

    EXPECT_TRUE(meta_table.SetValue(kKey, kSecondValue));
  }

  // Existing value was successfully changed.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

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
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

    int64_t value;
    EXPECT_FALSE(meta_table.GetValue(kKey, &value));

    EXPECT_TRUE(meta_table.SetValue(kKey, kFirstValue));
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);
  }

  // Value is persistent across different instances.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

    int64_t value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kFirstValue, value);

    EXPECT_TRUE(meta_table.SetValue(kKey, kSecondValue));
  }

  // Existing value was successfully changed.
  {
    MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

    int64_t value;
    EXPECT_TRUE(meta_table.GetValue(kKey, &value));
    EXPECT_EQ(kSecondValue, value);
  }
}

TEST_F(SQLMetaTableTest, DeleteKey) {
  static const char kKey[] = "String Key";
  const std::string kValue("String Value");

  MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(&db_, 1, 1));

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

}  // namespace sql
