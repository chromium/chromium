// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

absl::optional<base::Value> ParseTracedValue(
    std::unique_ptr<TracedValueJSON> value) {
  return base::JSONReader::Read(value->ToJSON().Utf8());
}

TEST(TracedValueTest, FlatDictionary) {
  auto value = std::make_unique<TracedValueJSON>();
  value->SetIntegerWithCopiedName("int", 2014);
  value->SetDoubleWithCopiedName("double", 0.0);
  value->SetBooleanWithCopiedName("bool", true);
  value->SetStringWithCopiedName("string", "string");

  absl::optional<base::Value> parsed = ParseTracedValue(std::move(value));
  ASSERT_TRUE(parsed->is_dict());
  absl::optional<int> int_value = parsed->FindIntKey("int");
  ASSERT_TRUE(int_value.has_value());
  EXPECT_EQ(2014, *int_value);
  absl::optional<double> double_value = parsed->FindDoubleKey("double");
  ASSERT_TRUE(double_value.has_value());
  EXPECT_EQ(0.0, *double_value);
  std::string* string_value = parsed->FindStringKey("string");
  ASSERT_NE(nullptr, string_value);
  EXPECT_EQ("string", *string_value);
}

TEST(TracedValueTest, Hierarchy) {
  auto value = std::make_unique<TracedValueJSON>();
  value->SetIntegerWithCopiedName("i0", 2014);
  value->BeginDictionaryWithCopiedName("dict1");
  value->SetIntegerWithCopiedName("i1", 2014);
  value->BeginDictionaryWithCopiedName("dict2");
  value->SetBooleanWithCopiedName("b2", false);
  value->EndDictionary();
  value->SetStringWithCopiedName("s1", "foo");
  value->EndDictionary();
  value->SetDoubleWithCopiedName("d0", 0.0);
  value->SetBooleanWithCopiedName("b0", true);
  value->BeginArrayWithCopiedName("a1");
  value->PushInteger(1);
  value->PushBoolean(true);
  value->BeginDictionary();
  value->SetIntegerWithCopiedName("i2", 3);
  value->EndDictionary();
  value->EndArray();
  value->SetStringWithCopiedName("s0", "foo");

  absl::optional<base::Value> parsed = ParseTracedValue(std::move(value));
  ASSERT_TRUE(parsed->is_dict());
  absl::optional<int> i0 = parsed->FindIntKey("i0");
  ASSERT_TRUE(i0.has_value());
  EXPECT_EQ(2014, *i0);
  absl::optional<int> i1 = parsed->FindIntPath("dict1.i1");
  ASSERT_TRUE(i1.has_value());
  EXPECT_EQ(2014, *i1);
  absl::optional<bool> b2 = parsed->FindBoolPath("dict1.dict2.b2");
  ASSERT_TRUE(b2.has_value());
  EXPECT_FALSE(*b2);
  std::string* s1 = parsed->FindStringPath("dict1.s1");
  ASSERT_NE(nullptr, s1);
  EXPECT_EQ("foo", *s1);
  absl::optional<double> d0 = parsed->FindDoubleKey("d0");
  ASSERT_TRUE(d0.has_value());
  EXPECT_EQ(0.0, *d0);
  absl::optional<bool> b0 = parsed->FindBoolKey("b0");
  ASSERT_TRUE(b0.has_value());
  EXPECT_TRUE(*b0);
  base::Value* a1_value = parsed->FindListKey("a1");
  ASSERT_NE(nullptr, a1_value);
  ASSERT_TRUE(a1_value->is_list());
  base::Value::ListView a1 = a1_value->GetListDeprecated();
  absl::optional<int> a1i0 = a1[0].GetIfInt();
  ASSERT_TRUE(a1i0.has_value());
  EXPECT_EQ(1, *a1i0);
  absl::optional<bool> a1b1 = a1[1].GetIfBool();
  ASSERT_TRUE(a1b1.has_value());
  EXPECT_TRUE(*a1b1);
  const base::Value& a1d2 = a1[2];
  ASSERT_TRUE(a1d2.is_dict());
  absl::optional<int> i2 = a1d2.FindIntKey("i2");
  ASSERT_TRUE(i2.has_value());
  EXPECT_EQ(3, *i2);
  std::string* s0 = parsed->FindStringKey("s0");
  ASSERT_NE(nullptr, s0);
  EXPECT_EQ("foo", *s0);
}

TEST(TracedValueTest, Escape) {
  auto value = std::make_unique<TracedValueJSON>();
  value->SetStringWithCopiedName("s0", "value0\\");
  value->SetStringWithCopiedName("s1", "value\n1");
  value->SetStringWithCopiedName("s2", "\"value2\"");
  value->SetStringWithCopiedName("s3\\", "value3");
  value->SetStringWithCopiedName("\"s4\"", "value4");

  absl::optional<base::Value> parsed = ParseTracedValue(std::move(value));
  ASSERT_TRUE(parsed->is_dict());
  std::string* s0 = parsed->FindStringKey("s0");
  ASSERT_NE(nullptr, s0);
  EXPECT_EQ("value0\\", *s0);
  std::string* s1 = parsed->FindStringKey("s1");
  ASSERT_NE(nullptr, s1);
  EXPECT_EQ("value\n1", *s1);
  std::string* s2 = parsed->FindStringKey("s2");
  ASSERT_NE(nullptr, s2);
  EXPECT_EQ("\"value2\"", *s2);
  std::string* s3 = parsed->FindStringKey("s3\\");
  ASSERT_NE(nullptr, s3);
  EXPECT_EQ("value3", *s3);
  std::string* s4 = parsed->FindStringKey("\"s4\"");
  ASSERT_NE(nullptr, s4);
  EXPECT_EQ("value4", *s4);
}

TEST(TracedValueTest, NonCopiedNames) {
  auto value = std::make_unique<TracedValueJSON>();
  const char* int_str = "int";
  const char* double_str = "double";
  const char* bool_str = "bool";
  const char* string_str = "string";
  const char* array_str = "array";
  value->SetInteger(int_str, 2014);
  value->SetDouble(double_str, 0.0);
  value->SetBoolean(bool_str, true);
  value->SetString(string_str, "string");
  value->BeginArray(array_str);
  value->PushInteger(1);
  value->PushInteger(2);
  value->EndArray();

  absl::optional<base::Value> parsed = ParseTracedValue(std::move(value));
  ASSERT_TRUE(parsed->is_dict());
  absl::optional<int> int_value = parsed->FindIntKey(int_str);
  ASSERT_TRUE(int_value.has_value());
  EXPECT_EQ(2014, *int_value);
  absl::optional<double> double_value = parsed->FindDoubleKey(double_str);
  ASSERT_TRUE(double_value.has_value());
  EXPECT_EQ(0.0, *double_value);
  absl::optional<bool> bool_value = parsed->FindBoolKey(bool_str);
  ASSERT_TRUE(bool_value.has_value());
  EXPECT_TRUE(*bool_value);
  std::string* string_value = parsed->FindStringKey(string_str);
  ASSERT_NE(nullptr, string_value);
  EXPECT_EQ("string", *string_value);
  base::Value* a1_value = parsed->FindListKey(array_str);
  ASSERT_TRUE(a1_value->is_list());
  base::Value::ListView a1 = a1_value->GetListDeprecated();
  absl::optional<int> el0 = a1[0].GetIfInt();
  absl::optional<int> el1 = a1[1].GetIfInt();
  ASSERT_TRUE(el0.has_value());
  ASSERT_TRUE(el1.has_value());
  EXPECT_EQ(1, *el0);
  EXPECT_EQ(2, *el1);
}

}  // namespace blink
