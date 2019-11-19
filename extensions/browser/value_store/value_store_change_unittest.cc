// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "extensions/browser/value_store/value_store_change.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::Value;
using extensions::DictionaryBuilder;
using extensions::ListBuilder;

namespace {

TEST(ValueStoreChangeTest, NullOldValue) {
  ValueStoreChange change("key", base::nullopt, base::Value("value"));

  EXPECT_EQ("key", change.key());
  EXPECT_EQ(NULL, change.old_value());
  {
    base::Value expected("value");
    EXPECT_TRUE(change.new_value()->Equals(&expected));
  }
}

TEST(ValueStoreChangeTest, NullNewValue) {
  ValueStoreChange change("key", base::Value("value"), base::nullopt);

  EXPECT_EQ("key", change.key());
  {
    base::Value expected("value");
    EXPECT_TRUE(change.old_value()->Equals(&expected));
  }
  EXPECT_EQ(NULL, change.new_value());
}

TEST(ValueStoreChangeTest, NonNullValues) {
  ValueStoreChange change("key", base::Value("old_value"),
                          base::Value("new_value"));

  EXPECT_EQ("key", change.key());
  {
    base::Value expected("old_value");
    EXPECT_TRUE(change.old_value()->Equals(&expected));
  }
  {
    base::Value expected("new_value");
    EXPECT_TRUE(change.new_value()->Equals(&expected));
  }
}

TEST(ValueStoreChangeTest, ToJson) {
  // Create a mildly complicated structure that has dots in it.
  std::unique_ptr<base::DictionaryValue> value =
      DictionaryBuilder()
          .Set("key", "value")
          .Set("key.with.dots", "value.with.dots")
          .Set("tricked", DictionaryBuilder().Set("you", "nodots").Build())
          .Set("tricked.you", "with.dots")
          .Build();

  ValueStoreChangeList change_list;
  change_list.push_back(
      ValueStoreChange("key", value->Clone(), value->Clone()));
  change_list.push_back(
      ValueStoreChange("key.with.dots", value->Clone(), value->Clone()));

  std::string json = ValueStoreChange::ToJson(change_list);
  base::Optional<base::Value> from_json = base::JSONReader::Read(json);
  ASSERT_TRUE(from_json);

  DictionaryBuilder v1(*value);
  DictionaryBuilder v2(*value);
  DictionaryBuilder v3(*value);
  DictionaryBuilder v4(*value);
  std::unique_ptr<base::DictionaryValue> expected_from_json =
      DictionaryBuilder()
          .Set("key", DictionaryBuilder()
                          .Set("oldValue", v1.Build())
                          .Set("newValue", v2.Build())
                          .Build())
          .Set("key.with.dots", DictionaryBuilder()
                                    .Set("oldValue", v3.Build())
                                    .Set("newValue", v4.Build())
                                    .Build())
          .Build();

  EXPECT_TRUE(from_json->Equals(expected_from_json.get()));
}

}  // namespace
