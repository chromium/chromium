// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_UNITTEST_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_UNITTEST_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/value_store/value_store.h"
#include "testing/gtest/include/gtest/gtest.h"

// Parameter type for the value-parameterized tests.
typedef ValueStore* (*ValueStoreTestParam)(const base::FilePath& file_path);

// Test fixture for ValueStore tests.  Tests are defined in
// settings_storage_unittest.cc with configurations for both cached
// and non-cached leveldb storage, and cached no-op storage.
class ValueStoreTest : public testing::TestWithParam<ValueStoreTestParam> {
 public:
  ValueStoreTest();
  virtual ~ValueStoreTest();

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<ValueStore> storage_;

  std::string key1_;
  std::string key2_;
  std::string key3_;

  std::unique_ptr<base::Value> val1_;
  std::unique_ptr<base::Value> val2_;
  std::unique_ptr<base::Value> val3_;

  std::vector<std::string> empty_list_;
  std::vector<std::string> list1_;
  std::vector<std::string> list2_;
  std::vector<std::string> list3_;
  std::vector<std::string> list12_;
  std::vector<std::string> list13_;
  std::vector<std::string> list123_;

  std::set<std::string> empty_set_;
  std::set<std::string> set1_;
  std::set<std::string> set2_;
  std::set<std::string> set3_;
  std::set<std::string> set12_;
  std::set<std::string> set13_;
  std::set<std::string> set123_;

  std::unique_ptr<base::DictionaryValue> empty_dict_;
  std::unique_ptr<base::DictionaryValue> dict1_;
  std::unique_ptr<base::DictionaryValue> dict3_;
  std::unique_ptr<base::DictionaryValue> dict12_;
  std::unique_ptr<base::DictionaryValue> dict123_;

 private:
  base::ScopedTempDir temp_dir_;

  content::BrowserTaskEnvironment task_environment_;
};

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_UNITTEST_H_
