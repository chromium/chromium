// Copyright 2015 The Chromium Authors
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

  base::Value::Dict settings;
  ASSERT_FALSE(settings.FindList("permissions"));
  settings =
      DictionaryBuilder().Set("permissions", permission_list.Build()).Build();

  base::Value::List* list_value = settings.FindList("permissions");

  ASSERT_TRUE(list_value);
  ASSERT_EQ(list_value->size(), 2u);
  ASSERT_TRUE(list_value->front().is_string());
  ASSERT_EQ(list_value->front().GetString(), "tabs");
  ASSERT_TRUE(list_value->back().is_string());
  ASSERT_EQ(list_value->back().GetString(), "history");
}

}  // namespace extensions
