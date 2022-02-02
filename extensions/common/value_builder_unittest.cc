// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/value_builder.h"

#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

using ValueBuilderTest = testing::Test;

namespace extensions {

TEST(ValueBuilderTest, Basic) {
  ListBuilder permission_list;
  permission_list.Append("tabs").Append("history");

  std::unique_ptr<base::DictionaryValue> settings(new base::DictionaryValue);

  ASSERT_FALSE(settings->GetList("permissions", nullptr));
  settings =
      DictionaryBuilder().Set("permissions", permission_list.Build()).Build();
  base::ListValue* list_value;
  ASSERT_TRUE(settings->GetList("permissions", &list_value));

  ASSERT_EQ(2U, list_value->GetListDeprecated().size());
  ASSERT_TRUE(list_value->GetListDeprecated()[0].is_string());
  ASSERT_EQ(list_value->GetListDeprecated()[0].GetString(), "tabs");
  ASSERT_TRUE(list_value->GetListDeprecated()[1].is_string());
  ASSERT_EQ(list_value->GetListDeprecated()[1].GetString(), "history");
}

TEST(ValueBuilderTest, AppendList) {
  auto get_json = [](const base::Value& value) -> std::string {
    std::string json;
    if (!base::JSONWriter::Write(value, &json)) {
      // Since this isn't valid JSON, there shouldn't be any risk of this
      // matching expected output.
      return "JSONWriter::Write() failed!";
    }
    return json;
  };

  {
    std::vector<std::string> strings = {"hello", "world", "!"};
    std::unique_ptr<base::Value> value =
        ListBuilder().Append(strings.begin(), strings.end()).Build();
    EXPECT_EQ(R"(["hello","world","!"])", get_json(*value));
  }

  {
    std::set<int> ints = {0, 1, 2, 3};
    std::unique_ptr<base::Value> value =
        ListBuilder().Append(ints.begin(), ints.end()).Build();
    EXPECT_EQ(R"([0,1,2,3])", get_json(*value));
  }

  {
    std::list<bool> bools = {false, true, false, true};
    std::unique_ptr<base::Value> value =
        ListBuilder().Append(bools.begin(), bools.end()).Build();
    EXPECT_EQ(R"([false,true,false,true])", get_json(*value));
  }
}

}  // namespace extensions
