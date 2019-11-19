// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

std::unique_ptr<base::Value> ParseTracedValue(
    std::unique_ptr<TracedValue> value) {
  base::JSONReader reader;
  return reader.ReadDeprecated(value->ToString().Utf8());
}

TEST(TracedValueTest, FlatDictionary) {
  auto value = std::make_unique<TracedValue>();
  value->SetIntegerWithCopiedName("int", 2014);
  value->SetDoubleWithCopiedName("double", 0.0);
  value->SetBooleanWithCopiedName("bool", true);
  value->SetStringWithCopiedName("string", "string");

  std::unique_ptr<base::Value> parsed = ParseTracedValue(std::move(value));
  base::DictionaryValue* dictionary;
  ASSERT_TRUE(parsed->GetAsDictionary(&dictionary));
  int int_value;
  EXPECT_TRUE(dictionary->GetInteger("int", &int_value));
  EXPECT_EQ(2014, int_value);
  double double_value;
  EXPECT_TRUE(dictionary->GetDouble("double", &double_value));
  EXPECT_EQ(0.0, double_value);
  std::string string_value;
  EXPECT_TRUE(dictionary->GetString("string", &string_value));
  EXPECT_EQ("string", string_value);
}

TEST(TracedValueTest, Hierarchy) {
  auto value = std::make_unique<TracedValue>();
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

  std::unique_ptr<base::Value> parsed = ParseTracedValue(std::move(value));
  base::DictionaryValue* dictionary;
  ASSERT_TRUE(parsed->GetAsDictionary(&dictionary));
  int i0;
  EXPECT_TRUE(dictionary->GetInteger("i0", &i0));
  EXPECT_EQ(2014, i0);
  int i1;
  EXPECT_TRUE(dictionary->GetInteger("dict1.i1", &i1));
  EXPECT_EQ(2014, i1);
  bool b2;
  EXPECT_TRUE(dictionary->GetBoolean("dict1.dict2.b2", &b2));
  EXPECT_FALSE(b2);
  std::string s1;
  EXPECT_TRUE(dictionary->GetString("dict1.s1", &s1));
  EXPECT_EQ("foo", s1);
  double d0;
  EXPECT_TRUE(dictionary->GetDouble("d0", &d0));
  EXPECT_EQ(0.0, d0);
  bool b0;
  EXPECT_TRUE(dictionary->GetBoolean("b0", &b0));
  EXPECT_TRUE(b0);
  base::ListValue* a1;
  EXPECT_TRUE(dictionary->GetList("a1", &a1));
  int a1i0;
  EXPECT_TRUE(a1->GetInteger(0, &a1i0));
  EXPECT_EQ(1, a1i0);
  bool a1b1;
  EXPECT_TRUE(a1->GetBoolean(1, &a1b1));
  EXPECT_TRUE(a1b1);
  base::DictionaryValue* a1d2;
  EXPECT_TRUE(a1->GetDictionary(2, &a1d2));
  int i2;
  EXPECT_TRUE(a1d2->GetInteger("i2", &i2));
  EXPECT_EQ(3, i2);
  std::string s0;
  EXPECT_TRUE(dictionary->GetString("s0", &s0));
  EXPECT_EQ("foo", s0);
}

TEST(TracedValueTest, Escape) {
  auto value = std::make_unique<TracedValue>();
  value->SetStringWithCopiedName("s0", "value0\\");
  value->SetStringWithCopiedName("s1", "value\n1");
  value->SetStringWithCopiedName("s2", "\"value2\"");
  value->SetStringWithCopiedName("s3\\", "value3");
  value->SetStringWithCopiedName("\"s4\"", "value4");

  std::unique_ptr<base::Value> parsed = ParseTracedValue(std::move(value));
  base::DictionaryValue* dictionary;
  ASSERT_TRUE(parsed->GetAsDictionary(&dictionary));
  std::string s0;
  EXPECT_TRUE(dictionary->GetString("s0", &s0));
  EXPECT_EQ("value0\\", s0);
  std::string s1;
  EXPECT_TRUE(dictionary->GetString("s1", &s1));
  EXPECT_EQ("value\n1", s1);
  std::string s2;
  EXPECT_TRUE(dictionary->GetString("s2", &s2));
  EXPECT_EQ("\"value2\"", s2);
  std::string s3;
  EXPECT_TRUE(dictionary->GetString("s3\\", &s3));
  EXPECT_EQ("value3", s3);
  std::string s4;
  EXPECT_TRUE(dictionary->GetString("\"s4\"", &s4));
  EXPECT_EQ("value4", s4);
}

TEST(TracedValueTest, NonCopiedNames) {
  auto value = std::make_unique<TracedValue>();
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

  std::unique_ptr<base::Value> parsed = ParseTracedValue(std::move(value));
  base::DictionaryValue* dictionary;
  ASSERT_TRUE(parsed->GetAsDictionary(&dictionary));
  int int_value;
  EXPECT_TRUE(dictionary->GetInteger(int_str, &int_value));
  EXPECT_EQ(2014, int_value);
  double double_value;
  EXPECT_TRUE(dictionary->GetDouble(double_str, &double_value));
  EXPECT_EQ(0.0, double_value);
  bool bool_value;
  EXPECT_TRUE(dictionary->GetBoolean(bool_str, &bool_value));
  EXPECT_TRUE(bool_value);
  std::string string_value;
  EXPECT_TRUE(dictionary->GetString(string_str, &string_value));
  EXPECT_EQ("string", string_value);
  base::ListValue* a1;
  EXPECT_TRUE(dictionary->GetList(array_str, &a1));
  int el0, el1;
  EXPECT_TRUE(a1->GetInteger(0, &el0));
  EXPECT_TRUE(a1->GetInteger(1, &el1));
  EXPECT_EQ(el0, 1);
  EXPECT_EQ(el1, 2);
}

}  // namespace blink
