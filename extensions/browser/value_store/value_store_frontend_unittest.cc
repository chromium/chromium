// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store_frontend.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/value_store/test_value_store_factory.h"
#include "extensions/common/extension_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

class ValueStoreFrontendTest : public testing::Test {
 public:
  ValueStoreFrontendTest() {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(extensions::DIR_TEST_DATA, &test_data_dir));
    base::FilePath src_db(test_data_dir.AppendASCII("value_store_db"));
    db_path_ = temp_dir_.GetPath().AppendASCII("temp_db");
    base::CopyDirectory(src_db, db_path_, true);

    factory_ = new extensions::TestValueStoreFactory(db_path_);

    ResetStorage();
  }

  void TearDown() override {
    content::RunAllTasksUntilIdle();
    storage_.reset();
  }

  // Reset the value store, reloading the DB from disk.
  void ResetStorage() {
    storage_.reset(new ValueStoreFrontend(
        factory_, ValueStoreFrontend::BackendType::RULES));
  }

  bool Get(const std::string& key, std::unique_ptr<base::Value>* output) {
    storage_->Get(key, base::Bind(&ValueStoreFrontendTest::GetAndWait,
                                  base::Unretained(this), output));
    content::RunAllTasksUntilIdle();
    return !!output->get();
  }

 protected:
  void GetAndWait(std::unique_ptr<base::Value>* output,
                  std::unique_ptr<base::Value> result) {
    *output = std::move(result);
  }

  scoped_refptr<extensions::TestValueStoreFactory> factory_;
  std::unique_ptr<ValueStoreFrontend> storage_;
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ValueStoreFrontendTest, GetExistingData) {
  std::unique_ptr<base::Value> value;
  ASSERT_FALSE(Get("key0", &value));

  // Test existing keys in the DB.
  {
    ASSERT_TRUE(Get("key1", &value));
    std::string result;
    ASSERT_TRUE(value->GetAsString(&result));
    EXPECT_EQ("value1", result);
  }

  {
    ASSERT_TRUE(Get("key2", &value));
    int result;
    ASSERT_TRUE(value->GetAsInteger(&result));
    EXPECT_EQ(2, result);
  }
}

TEST_F(ValueStoreFrontendTest, ChangesPersistAfterReload) {
  storage_->Set("key0", std::unique_ptr<base::Value>(new base::Value(0)));
  storage_->Set("key1", std::unique_ptr<base::Value>(new base::Value("new1")));
  storage_->Remove("key2");

  // Reload the DB and test our changes.
  ResetStorage();

  std::unique_ptr<base::Value> value;
  {
    ASSERT_TRUE(Get("key0", &value));
    int result;
    ASSERT_TRUE(value->GetAsInteger(&result));
    EXPECT_EQ(0, result);
  }

  {
    ASSERT_TRUE(Get("key1", &value));
    std::string result;
    ASSERT_TRUE(value->GetAsString(&result));
    EXPECT_EQ("new1", result);
  }

  ASSERT_FALSE(Get("key2", &value));
}
