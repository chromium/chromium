// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "dbus/values_util.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

TEST(ValuesUtilTest, PopBasicTypes) {
  std::unique_ptr<Response> response(Response::CreateEmpty());
  // Append basic type values.
  MessageWriter writer(response.get());
  const uint8_t kByteValue = 42;
  writer.AppendByte(kByteValue);
  const bool kBoolValue = true;
  writer.AppendBool(kBoolValue);
  const int16_t kInt16Value = -43;
  writer.AppendInt16(kInt16Value);
  const uint16_t kUint16Value = 44;
  writer.AppendUint16(kUint16Value);
  const int32_t kInt32Value = -45;
  writer.AppendInt32(kInt32Value);
  const uint32_t kUint32Value = 46;
  writer.AppendUint32(kUint32Value);
  const int64_t kInt64Value = -47;
  writer.AppendInt64(kInt64Value);
  const uint64_t kUint64Value = 48;
  writer.AppendUint64(kUint64Value);
  const double kDoubleValue = 4.9;
  writer.AppendDouble(kDoubleValue);
  const std::string kStringValue = "fifty";
  writer.AppendString(kStringValue);
  const std::string kEmptyStringValue;
  writer.AppendString(kEmptyStringValue);
  const ObjectPath kObjectPathValue("/ObjectPath");
  writer.AppendObjectPath(kObjectPathValue);

  MessageReader reader(response.get());
  base::Value value;
  // Pop a byte.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kByteValue));
  // Pop a bool.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kBoolValue));
  // Pop an int16_t.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kInt16Value));
  // Pop a uint16_t.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kUint16Value));
  // Pop an int32_t.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kInt32Value));
  // Pop a uint32_t.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(static_cast<double>(kUint32Value)));
  // Pop an int64_t.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(static_cast<double>(kInt64Value)));
  // Pop a uint64_t.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(static_cast<double>(kUint64Value)));
  // Pop a double.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kDoubleValue));
  // Pop a string.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kStringValue));
  // Pop an empty string.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kEmptyStringValue));
  // Pop an object path.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kObjectPathValue.value()));
}

TEST(ValuesUtilTest, PopVariant) {
  std::unique_ptr<Response> response(Response::CreateEmpty());
  // Append variant values.
  MessageWriter writer(response.get());
  const bool kBoolValue = true;
  writer.AppendVariantOfBool(kBoolValue);
  const int32_t kInt32Value = -45;
  writer.AppendVariantOfInt32(kInt32Value);
  const double kDoubleValue = 4.9;
  writer.AppendVariantOfDouble(kDoubleValue);
  const std::string kStringValue = "fifty";
  writer.AppendVariantOfString(kStringValue);

  MessageReader reader(response.get());
  base::Value value;
  // Pop a bool.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kBoolValue));
  // Pop an int32_t.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kInt32Value));
  // Pop a double.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kDoubleValue));
  // Pop a string.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(kStringValue));
}

// Pop extremely large integers which cannot be precisely represented in
// double.
TEST(ValuesUtilTest, PopExtremelyLargeIntegers) {
  std::unique_ptr<Response> response(Response::CreateEmpty());
  // Append large integers.
  MessageWriter writer(response.get());
  const int64_t kInt64Value = -123456789012345689LL;
  writer.AppendInt64(kInt64Value);
  const uint64_t kUint64Value = 9876543210987654321ULL;
  writer.AppendUint64(kUint64Value);

  MessageReader reader(response.get());
  base::Value value;
  // Pop an int64_t.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(static_cast<double>(kInt64Value)));
  ASSERT_TRUE(value.is_double());
  EXPECT_NE(kInt64Value, static_cast<int64_t>(value.GetDouble()));
  // Pop a uint64_t.
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, base::Value(static_cast<double>(kUint64Value)));
  ASSERT_TRUE(value.is_double());
  EXPECT_NE(kUint64Value, static_cast<uint64_t>(value.GetDouble()));
}

TEST(ValuesUtilTest, PopIntArray) {
  std::unique_ptr<Response> response(Response::CreateEmpty());
  // Append an int32_t array.
  MessageWriter writer(response.get());
  MessageWriter sub_writer(nullptr);
  std::vector<int32_t> data;
  data.push_back(0);
  data.push_back(1);
  data.push_back(2);
  writer.OpenArray("i", &sub_writer);
  for (size_t i = 0; i != data.size(); ++i)
    sub_writer.AppendInt32(data[i]);
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  base::Value::List list_value;
  for (size_t i = 0; i != data.size(); ++i)
    list_value.Append(data[i]);

  // Pop an int32_t array.
  MessageReader reader(response.get());
  base::Value value(PopDataAsValue(&reader));
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, list_value);
}

TEST(ValuesUtilTest, PopStringArray) {
  std::unique_ptr<Response> response(Response::CreateEmpty());
  // Append a string array.
  MessageWriter writer(response.get());
  MessageWriter sub_writer(nullptr);
  std::vector<std::string> data;
  data.push_back("Dreamlifter");
  data.push_back("Beluga");
  data.push_back("Mriya");
  writer.AppendArrayOfStrings(data);

  // Create the expected value.
  base::Value::List list_value;
  for (size_t i = 0; i != data.size(); ++i)
    list_value.Append(data[i]);

  // Pop a string array.
  MessageReader reader(response.get());
  base::Value value(PopDataAsValue(&reader));
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, list_value);
}

TEST(ValuesUtilTest, PopStruct) {
  std::unique_ptr<Response> response(Response::CreateEmpty());
  // Append a struct.
  MessageWriter writer(response.get());
  MessageWriter sub_writer(nullptr);
  writer.OpenStruct(&sub_writer);
  const bool kBoolValue = true;
  sub_writer.AppendBool(kBoolValue);
  const int32_t kInt32Value = -123;
  sub_writer.AppendInt32(kInt32Value);
  const double kDoubleValue = 1.23;
  sub_writer.AppendDouble(kDoubleValue);
  const std::string kStringValue = "one two three";
  sub_writer.AppendString(kStringValue);
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  base::Value::List list_value;
  list_value.Append(kBoolValue);
  list_value.Append(kInt32Value);
  list_value.Append(kDoubleValue);
  list_value.Append(kStringValue);

  // Pop a struct.
  MessageReader reader(response.get());
  base::Value value(PopDataAsValue(&reader));
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, list_value);
}

TEST(ValuesUtilTest, PopStringToVariantDictionary) {
  std::unique_ptr<Response> response(Response::CreateEmpty());
  // Append a dictionary.
  MessageWriter writer(response.get());
  MessageWriter sub_writer(nullptr);
  MessageWriter entry_writer(nullptr);
  writer.OpenArray("{sv}", &sub_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey1 = "one";
  entry_writer.AppendString(kKey1);
  const bool kBoolValue = true;
  entry_writer.AppendVariantOfBool(kBoolValue);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey2 = "two";
  entry_writer.AppendString(kKey2);
  const int32_t kInt32Value = -45;
  entry_writer.AppendVariantOfInt32(kInt32Value);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey3 = "three";
  entry_writer.AppendString(kKey3);
  const double kDoubleValue = 4.9;
  entry_writer.AppendVariantOfDouble(kDoubleValue);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey4 = "four";
  entry_writer.AppendString(kKey4);
  const std::string kStringValue = "fifty";
  entry_writer.AppendVariantOfString(kStringValue);
  sub_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  base::Value::Dict dictionary_value;
  dictionary_value.Set(kKey1, kBoolValue);
  dictionary_value.Set(kKey2, kInt32Value);
  dictionary_value.Set(kKey3, kDoubleValue);
  dictionary_value.Set(kKey4, kStringValue);

  // Pop a dictinoary.
  MessageReader reader(response.get());
  base::Value value(PopDataAsValue(&reader));
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, dictionary_value);
}

TEST(ValuesUtilTest, PopDictionaryWithDottedStringKey) {
  std::unique_ptr<Response> response(Response::CreateEmpty());
  // Append a dictionary.
  MessageWriter writer(response.get());
  MessageWriter sub_writer(nullptr);
  MessageWriter entry_writer(nullptr);
  writer.OpenArray("{sv}", &sub_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey1 = "www.example.com";  // String including dots.
  entry_writer.AppendString(kKey1);
  const bool kBoolValue = true;
  entry_writer.AppendVariantOfBool(kBoolValue);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey2 = ".example";  // String starting with a dot.
  entry_writer.AppendString(kKey2);
  const int32_t kInt32Value = -45;
  entry_writer.AppendVariantOfInt32(kInt32Value);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey3 = "example.";  // String ending with a dot.
  entry_writer.AppendString(kKey3);
  const double kDoubleValue = 4.9;
  entry_writer.AppendVariantOfDouble(kDoubleValue);
  sub_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  base::Value::Dict dictionary_value;
  dictionary_value.Set(kKey1, kBoolValue);
  dictionary_value.Set(kKey2, kInt32Value);
  dictionary_value.Set(kKey3, kDoubleValue);

  // Pop a dictinoary.
  MessageReader reader(response.get());
  base::Value value(PopDataAsValue(&reader));
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, dictionary_value);
}

TEST(ValuesUtilTest, PopDoubleToIntDictionary) {
  // Create test data.
  const int32_t kValues[] = {0, 1, 1, 2, 3, 5, 8, 13, 21};
  const std::vector<int32_t> values(kValues, kValues + std::size(kValues));
  std::vector<double> keys(values.size());
  for (size_t i = 0; i != values.size(); ++i)
    keys[i] = std::sqrt(values[i]);

  // Append a dictionary.
  std::unique_ptr<Response> response(Response::CreateEmpty());
  MessageWriter writer(response.get());
  MessageWriter sub_writer(nullptr);
  writer.OpenArray("{di}", &sub_writer);
  for (size_t i = 0; i != values.size(); ++i) {
    MessageWriter entry_writer(nullptr);
    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendDouble(keys[i]);
    entry_writer.AppendInt32(values[i]);
    sub_writer.CloseContainer(&entry_writer);
  }
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  base::Value::Dict dictionary_value;
  for (size_t i = 0; i != values.size(); ++i) {
    std::string key_string;
    base::JSONWriter::Write(base::Value(keys[i]), &key_string);
    dictionary_value.Set(key_string, values[i]);
  }

  // Pop a dictionary.
  MessageReader reader(response.get());
  base::Value value(PopDataAsValue(&reader));
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, dictionary_value);
}

TEST(ValuesUtilTest, AppendBasicTypes) {
  const base::Value kBoolValue(false);
  const base::Value kIntegerValue(42);
  const base::Value kDoubleValue(4.2);
  const base::Value kStringValue("string");

  std::unique_ptr<Response> response(Response::CreateEmpty());
  MessageWriter writer(response.get());
  AppendBasicTypeValueData(&writer, kBoolValue);
  AppendBasicTypeValueData(&writer, kIntegerValue);
  AppendBasicTypeValueData(&writer, kDoubleValue);
  AppendBasicTypeValueData(&writer, kStringValue);

  MessageReader reader(response.get());
  base::Value value;
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kBoolValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kIntegerValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kDoubleValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kStringValue);
}

TEST(ValuesUtilTest, AppendBasicTypesAsVariant) {
  const base::Value kBoolValue(false);
  const base::Value kIntegerValue(42);
  const base::Value kDoubleValue(4.2);
  const base::Value kStringValue("string");

  std::unique_ptr<Response> response(Response::CreateEmpty());
  MessageWriter writer(response.get());
  AppendBasicTypeValueDataAsVariant(&writer, kBoolValue);
  AppendBasicTypeValueDataAsVariant(&writer, kIntegerValue);
  AppendBasicTypeValueDataAsVariant(&writer, kDoubleValue);
  AppendBasicTypeValueDataAsVariant(&writer, kStringValue);

  MessageReader reader(response.get());
  base::Value value;
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kBoolValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kIntegerValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kDoubleValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kStringValue);
}

TEST(ValuesUtilTest, AppendValueDataBasicTypes) {
  const base::Value kBoolValue(false);
  const base::Value kIntegerValue(42);
  const base::Value kDoubleValue(4.2);
  const base::Value kStringValue("string");

  std::unique_ptr<Response> response(Response::CreateEmpty());
  MessageWriter writer(response.get());
  AppendValueData(&writer, kBoolValue);
  AppendValueData(&writer, kIntegerValue);
  AppendValueData(&writer, kDoubleValue);
  AppendValueData(&writer, kStringValue);

  MessageReader reader(response.get());
  base::Value value;
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kBoolValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kIntegerValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kDoubleValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kStringValue);
}

TEST(ValuesUtilTest, AppendValueDataAsVariantBasicTypes) {
  const base::Value kBoolValue(false);
  const base::Value kIntegerValue(42);
  const base::Value kDoubleValue(4.2);
  const base::Value kStringValue("string");

  std::unique_ptr<Response> response(Response::CreateEmpty());
  MessageWriter writer(response.get());
  AppendValueDataAsVariant(&writer, kBoolValue);
  AppendValueDataAsVariant(&writer, kIntegerValue);
  AppendValueDataAsVariant(&writer, kDoubleValue);
  AppendValueDataAsVariant(&writer, kStringValue);

  MessageReader reader(response.get());
  base::Value value;
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kBoolValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kIntegerValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kDoubleValue);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, kStringValue);
}

TEST(ValuesUtilTest, AppendDictionary) {
  // Set up the input dictionary.
  const std::string kKey1 = "one";
  const std::string kKey2 = "two";
  const std::string kKey3 = "three";
  const std::string kKey4 = "four";
  const std::string kKey5 = "five";
  const std::string kKey6 = "six";

  const bool kBoolValue = true;
  const int32_t kInt32Value = -45;
  const double kDoubleValue = 4.9;
  const std::string kStringValue = "fifty";

  base::Value::List list_value;
  list_value.Append(kBoolValue);
  list_value.Append(kInt32Value);

  base::Value::Dict dictionary_value;
  dictionary_value.Set(kKey1, kBoolValue);
  dictionary_value.Set(kKey2, kDoubleValue);

  base::Value::Dict test_dictionary;
  test_dictionary.Set(kKey1, kBoolValue);
  test_dictionary.Set(kKey2, kInt32Value);
  test_dictionary.Set(kKey3, kDoubleValue);
  test_dictionary.Set(kKey4, kStringValue);
  test_dictionary.Set(kKey5, std::move(list_value));
  test_dictionary.Set(kKey6, std::move(dictionary_value));

  std::unique_ptr<Response> response(Response::CreateEmpty());
  MessageWriter writer(response.get());
  AppendValueData(&writer, test_dictionary);
  base::Value int_value(kInt32Value);
  AppendValueData(&writer, int_value);

  // Read the data.
  MessageReader reader(response.get());
  base::Value value;
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, test_dictionary);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, int_value);
}

TEST(ValuesUtilTest, AppendDictionaryAsVariant) {
  // Set up the input dictionary.
  const std::string kKey1 = "one";
  const std::string kKey2 = "two";
  const std::string kKey3 = "three";
  const std::string kKey4 = "four";
  const std::string kKey5 = "five";
  const std::string kKey6 = "six";

  const bool kBoolValue = true;
  const int32_t kInt32Value = -45;
  const double kDoubleValue = 4.9;
  const std::string kStringValue = "fifty";

  base::Value::List list_value;
  list_value.Append(kBoolValue);
  list_value.Append(kInt32Value);

  base::Value::Dict dictionary_value;
  dictionary_value.Set(kKey1, kBoolValue);
  dictionary_value.Set(kKey2, kDoubleValue);

  base::Value::Dict test_dictionary;
  test_dictionary.Set(kKey1, kBoolValue);
  test_dictionary.Set(kKey2, kInt32Value);
  test_dictionary.Set(kKey3, kDoubleValue);
  test_dictionary.Set(kKey4, kStringValue);
  test_dictionary.Set(kKey5, std::move(list_value));
  test_dictionary.Set(kKey6, std::move(dictionary_value));

  std::unique_ptr<Response> response(Response::CreateEmpty());
  MessageWriter writer(response.get());
  AppendValueDataAsVariant(&writer, test_dictionary);
  base::Value int_value(kInt32Value);
  AppendValueData(&writer, int_value);

  // Read the data.
  MessageReader reader(response.get());
  base::Value value;
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, test_dictionary);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, int_value);
}

TEST(ValuesUtilTest, AppendList) {
  // Set up the input list.
  const std::string kKey1 = "one";
  const std::string kKey2 = "two";

  const bool kBoolValue = true;
  const int32_t kInt32Value = -45;
  const double kDoubleValue = 4.9;
  const std::string kStringValue = "fifty";

  base::Value::List list_value;
  list_value.Append(kBoolValue);
  list_value.Append(kInt32Value);

  base::Value::Dict dictionary_value;
  dictionary_value.Set(kKey1, kBoolValue);
  dictionary_value.Set(kKey2, kDoubleValue);

  base::Value::List test_list;
  test_list.Append(kBoolValue);
  test_list.Append(kInt32Value);
  test_list.Append(kDoubleValue);
  test_list.Append(kStringValue);
  test_list.Append(std::move(list_value));
  test_list.Append(std::move(dictionary_value));

  std::unique_ptr<Response> response(Response::CreateEmpty());
  MessageWriter writer(response.get());
  AppendValueData(&writer, test_list);
  base::Value int_value(kInt32Value);
  AppendValueData(&writer, int_value);

  // Read the data.
  MessageReader reader(response.get());
  base::Value value;
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, test_list);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, int_value);
}

TEST(ValuesUtilTest, AppendListAsVariant) {
  // Set up the input list.
  const std::string kKey1 = "one";
  const std::string kKey2 = "two";

  const bool kBoolValue = true;
  const int32_t kInt32Value = -45;
  const double kDoubleValue = 4.9;
  const std::string kStringValue = "fifty";

  base::Value::List list_value;
  list_value.Append(kBoolValue);
  list_value.Append(kInt32Value);

  base::Value::Dict dictionary_value;
  dictionary_value.Set(kKey1, kBoolValue);
  dictionary_value.Set(kKey2, kDoubleValue);

  base::Value::List test_list;
  test_list.Append(kBoolValue);
  test_list.Append(kInt32Value);
  test_list.Append(kDoubleValue);
  test_list.Append(kStringValue);
  test_list.Append(std::move(list_value));
  test_list.Append(std::move(dictionary_value));

  std::unique_ptr<Response> response(Response::CreateEmpty());
  MessageWriter writer(response.get());
  AppendValueDataAsVariant(&writer, test_list);
  base::Value int_value(kInt32Value);
  AppendValueData(&writer, int_value);

  // Read the data.
  MessageReader reader(response.get());
  base::Value value;
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, test_list);
  value = PopDataAsValue(&reader);
  ASSERT_FALSE(value.is_none());
  EXPECT_EQ(value, int_value);
}

}  // namespace dbus
