// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/leveldb_scoped_database.h"

#include <stddef.h>

#include <map>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestUMAClientName[] = "Test";
}  // namespace

class LeveldbScopedDatabaseUnitTest : public testing::Test {
 public:
  LeveldbScopedDatabaseUnitTest() {}
  ~LeveldbScopedDatabaseUnitTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    db_ =
        new LeveldbScopedDatabase(kTestUMAClientName, database_dir_.GetPath());
  }

  void TearDown() override {
    db_ = nullptr;
    base::DeleteFile(database_dir_.GetPath(), true);
  }

  ValueStore::Status ReadAllValues(
      std::map<std::string, std::string>* values) const {
    values->clear();
    leveldb::ReadOptions read_options;
    read_options.verify_checksums = true;
    std::unique_ptr<leveldb::Iterator> iterator;
    ValueStore::Status status = db_->CreateIterator(read_options, &iterator);
    if (!status.ok())
      return status;
    iterator->SeekToFirst();
    while (iterator->Valid()) {
      // The LeveldbProfileDatabase writes all values as JSON strings.
      // This method returns the encoded strings.
      (*values)[iterator->key().ToString()] = iterator->value().ToString();
      iterator->Next();
    }
    return db_->ToValueStoreError(iterator->status());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir database_dir_;
  scoped_refptr<LeveldbScopedDatabase> db_;
};

TEST_F(LeveldbScopedDatabaseUnitTest, TestSplitKey) {
  std::string scope;
  std::string key;
  EXPECT_TRUE(LeveldbScopedDatabase::SplitKey("s:k", &scope, &key));
  EXPECT_EQ(scope, "s");
  EXPECT_EQ(key, "k");
  EXPECT_TRUE(LeveldbScopedDatabase::SplitKey("s:", &scope, &key));
  EXPECT_EQ(scope, "s");
  EXPECT_EQ(key, "");
  EXPECT_TRUE(LeveldbScopedDatabase::SplitKey("s:k:o", &scope, &key));
  EXPECT_EQ(scope, "s");
  EXPECT_EQ(key, "k:o");
  EXPECT_FALSE(LeveldbScopedDatabase::SplitKey("s-k", &scope, &key));
  EXPECT_FALSE(LeveldbScopedDatabase::SplitKey("", &scope, &key));
  EXPECT_FALSE(LeveldbScopedDatabase::SplitKey(":k", &scope, &key));
}

TEST_F(LeveldbScopedDatabaseUnitTest, TestCreateKey) {
  std::string scoped_key;

  EXPECT_TRUE(LeveldbScopedDatabase::CreateKey("scope", "key", &scoped_key));
  EXPECT_EQ("scope:key", scoped_key);
  EXPECT_TRUE(LeveldbScopedDatabase::CreateKey("scope", "", &scoped_key));
  EXPECT_EQ("scope:", scoped_key);
  EXPECT_TRUE(LeveldbScopedDatabase::CreateKey("scope", "key:o", &scoped_key));
  EXPECT_EQ("scope:key:o", scoped_key);

  EXPECT_FALSE(LeveldbScopedDatabase::CreateKey("", "key", &scoped_key));
  EXPECT_FALSE(
      LeveldbScopedDatabase::CreateKey("scope:withdelim", "key", &scoped_key));
}

TEST_F(LeveldbScopedDatabaseUnitTest, TestWrite) {
  std::map<std::string, std::string> db_values;
  EXPECT_TRUE(ReadAllValues(&db_values).ok());
  EXPECT_EQ(0u, db_values.size());

  base::DictionaryValue scope1_values;
  scope1_values.SetString("s1_key1", "s1_value1");
  scope1_values.SetString("s1_key2", "s1_value2");
  EXPECT_FALSE(db_->Write("", scope1_values).ok());
  EXPECT_TRUE(db_->Write("scope1", scope1_values).ok());

  base::DictionaryValue scope2_values;
  scope2_values.SetString("s2_key1", "s2_value1");
  scope2_values.SetString("s2_key2", "s2_value2");
  EXPECT_TRUE(db_->Write("scope2", scope2_values).ok());

  // Read all values using raw leveldb. Values are JSON strings.
  EXPECT_TRUE(ReadAllValues(&db_values).ok());
  EXPECT_EQ(4u, db_values.size());
  EXPECT_EQ("\"s1_value1\"", db_values["scope1:s1_key1"]);
  EXPECT_EQ("\"s1_value2\"", db_values["scope1:s1_key2"]);
  EXPECT_EQ("\"s2_value1\"", db_values["scope2:s2_key1"]);
  EXPECT_EQ("\"s2_value2\"", db_values["scope2:s2_key2"]);

  // Intentionally overwrite value (with a new value).
  base::DictionaryValue changed_scope2_values;
  changed_scope2_values.SetString("s2_key1", "s2_value1");
  changed_scope2_values.SetString("s2_key2", "s2_value3");
  EXPECT_TRUE(db_->Write("scope2", changed_scope2_values).ok());

  EXPECT_TRUE(ReadAllValues(&db_values).ok());
  EXPECT_EQ(4u, db_values.size());
  EXPECT_EQ("\"s1_value1\"", db_values["scope1:s1_key1"]);
  EXPECT_EQ("\"s1_value2\"", db_values["scope1:s1_key2"]);
  EXPECT_EQ("\"s2_value1\"", db_values["scope2:s2_key1"]);
  EXPECT_EQ("\"s2_value3\"", db_values["scope2:s2_key2"]);
}

TEST_F(LeveldbScopedDatabaseUnitTest, TestRead) {
  base::DictionaryValue scope1_values;
  scope1_values.SetString("s1_key1", "s1_value1");
  scope1_values.SetString("s1_key2", "s1_value2");
  EXPECT_TRUE(db_->Write("scope1", scope1_values).ok());

  base::DictionaryValue scope2_values;
  scope2_values.SetString("s2_key1", "s2_value1");
  scope2_values.SetString("s2_key2", "s2_value2");
  EXPECT_TRUE(db_->Write("scope2", scope2_values).ok());

  // And test an empty scope.
  EXPECT_FALSE(db_->Write("", scope2_values).ok());

  base::DictionaryValue read_s1_vals;
  EXPECT_FALSE(db_->Read("", &read_s1_vals).ok());
  EXPECT_TRUE(db_->Read("scope1", &read_s1_vals).ok());
  EXPECT_TRUE(scope1_values.Equals(&read_s1_vals));

  base::DictionaryValue read_s2_vals;
  EXPECT_TRUE(db_->Read("scope2", &read_s2_vals).ok());
  EXPECT_TRUE(scope2_values.Equals(&read_s2_vals));
}

TEST_F(LeveldbScopedDatabaseUnitTest, TestEmptyValue) {
  base::DictionaryValue values;
  values.SetString("s1_key1", "");
  EXPECT_TRUE(db_->Write("scope1", values).ok());

  base::Optional<base::Value> value;
  ASSERT_TRUE(db_->Read("scope1", "s1_key1", &value).ok());
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(value->GetString(), "");
}

TEST_F(LeveldbScopedDatabaseUnitTest, TestValueContainingDelimiter) {
  base::DictionaryValue values;
  values.SetString("s1_key1", "with:delimiter");
  EXPECT_TRUE(db_->Write("scope1", values).ok());

  base::Optional<base::Value> value;
  ASSERT_TRUE(db_->Read("scope1", "s1_key1", &value).ok());
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(value->GetString(), "with:delimiter");
}

TEST_F(LeveldbScopedDatabaseUnitTest, TestDeleteValues) {
  base::DictionaryValue scope1_values;
  scope1_values.SetString("s1_key1", "s1_value1");
  scope1_values.SetString("s1_key2", "s1_value2");
  EXPECT_TRUE(db_->Write("scope1", scope1_values).ok());

  base::DictionaryValue scope2_values;
  scope2_values.SetString("s2_key1", "s2_value1");
  scope2_values.SetString("s2_key2", "s2_value2");
  EXPECT_TRUE(db_->Write("scope2", scope2_values).ok());

  std::vector<std::string> keys;
  keys.push_back("s2_key1");
  keys.push_back("s2_key2");
  keys.push_back("s1_key1");
  EXPECT_TRUE(db_->DeleteValues("scope2", keys).ok());

  base::DictionaryValue read_s1_vals;
  EXPECT_TRUE(db_->Read("scope1", &read_s1_vals).ok());
  EXPECT_TRUE(scope1_values.Equals(&read_s1_vals));

  base::DictionaryValue read_s2_vals;
  EXPECT_TRUE(db_->Read("scope2", &read_s2_vals).ok());
  EXPECT_TRUE(read_s2_vals.empty());
}
