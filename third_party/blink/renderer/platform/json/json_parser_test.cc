// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/json/json_parser.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

TEST(JSONParserTest, Reading) {
  JSONParseError error;
  JSONValue* tmp_value;
  std::unique_ptr<JSONValue> root;
  std::unique_ptr<JSONValue> root2;
  String str_val;
  int int_val = 0;

  // Successfull parsing returns kNoError.
  root = ParseJSON("1", &error);
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONParseErrorType::kNoError, error.type);
  root = ParseJSON("\"string\"", &error);
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONParseErrorType::kNoError, error.type);
  root = ParseJSON("[]", &error);
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONParseErrorType::kNoError, error.type);
  root = ParseJSON("{}", &error);
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONParseErrorType::kNoError, error.type);

  // some whitespace checking
  root = ParseJSON("    null    ", &error);
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeNull, root->GetType());
  EXPECT_EQ(JSONParseErrorType::kNoError, error.type);

  // Invalid JSON string
  root = ParseJSON("nu", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1, Syntax error.", error.message);

  // Error reporting
  root = ParseJSON("\n\n  nu", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 3, column: 3, Syntax error.", error.message);
  EXPECT_EQ(JSONParseErrorType::kSyntaxError, error.type);
  EXPECT_EQ(3, error.line);
  EXPECT_EQ(3, error.column);

  // Simple bool
  root = ParseJSON("true  ");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeBoolean, root->GetType());

  // Embedded comment
  root = ParseJSON("40 /*/", &error);
  // EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Syntax error.", error.message);
  root = ParseJSON("/* comment */null");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeNull, root->GetType());
  root = ParseJSON("40 /* comment */");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeInteger, root->GetType());
  EXPECT_TRUE(root->AsInteger(&int_val));
  EXPECT_EQ(40, int_val);
  root = ParseJSON("/**/ 40 /* multi-line\n comment */ // more comment");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeInteger, root->GetType());
  EXPECT_TRUE(root->AsInteger(&int_val));
  EXPECT_EQ(40, int_val);
  root = ParseJSON("true // comment");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeBoolean, root->GetType());
  root = ParseJSON("/* comment */\"sample string\"");
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->AsString(&str_val));
  EXPECT_EQ("sample string", str_val);
  root = ParseJSON("[1, /* comment, 2 ] */ \n 3]");
  ASSERT_TRUE(root.get());
  JSONArray* list = JSONArray::Cast(root.get());
  ASSERT_TRUE(list);
  EXPECT_EQ(2u, list->size());
  tmp_value = list->at(0);
  ASSERT_TRUE(tmp_value);
  EXPECT_TRUE(tmp_value->AsInteger(&int_val));
  EXPECT_EQ(1, int_val);
  tmp_value = list->at(1);
  ASSERT_TRUE(tmp_value);
  EXPECT_TRUE(tmp_value->AsInteger(&int_val));
  EXPECT_EQ(3, int_val);
  root = ParseJSON("[1, /*a*/2, 3]");
  ASSERT_TRUE(root.get());
  list = JSONArray::Cast(root.get());
  ASSERT_TRUE(list);
  EXPECT_EQ(3u, list->size());
  root = ParseJSON("/* comment **/42");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeInteger, root->GetType());
  EXPECT_TRUE(root->AsInteger(&int_val));
  EXPECT_EQ(42, int_val);
  root = ParseJSON(
      "/* comment **/\n"
      "// */ 43\n"
      "44");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeInteger, root->GetType());
  EXPECT_TRUE(root->AsInteger(&int_val));
  EXPECT_EQ(44, int_val);

  // Test number formats
  root = ParseJSON("43");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeInteger, root->GetType());
  EXPECT_TRUE(root->AsInteger(&int_val));
  EXPECT_EQ(43, int_val);

  // According to RFC4627, oct, hex, and leading zeros are invalid JSON.
  root = ParseJSON("043", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 2, Syntax error.", error.message);
  root = ParseJSON("0x43", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 2, Unexpected data after root element.",
            error.message);
  root = ParseJSON("00", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 2, Syntax error.", error.message);

  // Test 0 (which needs to be special cased because of the leading zero
  // clause).
  root = ParseJSON("0");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeInteger, root->GetType());
  int_val = 1;
  EXPECT_TRUE(root->AsInteger(&int_val));
  EXPECT_EQ(0, int_val);

  // Numbers that overflow ints should succeed, being internally promoted to
  // storage as doubles
  root = ParseJSON("2147483648");
  ASSERT_TRUE(root.get());
  double double_val;
  EXPECT_EQ(JSONValue::kTypeDouble, root->GetType());
  double_val = 0.0;
  EXPECT_TRUE(root->AsDouble(&double_val));
  EXPECT_DOUBLE_EQ(2147483648.0, double_val);
  root = ParseJSON("-2147483649");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeDouble, root->GetType());
  double_val = 0.0;
  EXPECT_TRUE(root->AsDouble(&double_val));
  EXPECT_DOUBLE_EQ(-2147483649.0, double_val);

  // Parse a double
  root = ParseJSON("43.1");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeDouble, root->GetType());
  double_val = 0.0;
  EXPECT_TRUE(root->AsDouble(&double_val));
  EXPECT_DOUBLE_EQ(43.1, double_val);

  root = ParseJSON("4.3e-1");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeDouble, root->GetType());
  double_val = 0.0;
  EXPECT_TRUE(root->AsDouble(&double_val));
  EXPECT_DOUBLE_EQ(.43, double_val);

  root = ParseJSON("2.1e0");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeDouble, root->GetType());
  double_val = 0.0;
  EXPECT_TRUE(root->AsDouble(&double_val));
  EXPECT_DOUBLE_EQ(2.1, double_val);

  root = ParseJSON("2.1e+0001");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeInteger, root->GetType());
  double_val = 0.0;
  EXPECT_TRUE(root->AsDouble(&double_val));
  EXPECT_DOUBLE_EQ(21.0, double_val);

  root = ParseJSON("0.01");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeDouble, root->GetType());
  double_val = 0.0;
  EXPECT_TRUE(root->AsDouble(&double_val));
  EXPECT_DOUBLE_EQ(0.01, double_val);

  root = ParseJSON("1.00");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeInteger, root->GetType());
  double_val = 0.0;
  EXPECT_TRUE(root->AsDouble(&double_val));
  EXPECT_DOUBLE_EQ(1.0, double_val);

  // Fractional parts must have a digit before and after the decimal point.
  root = ParseJSON("1.", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 3, Syntax error.", error.message);
  root = ParseJSON(".1", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1, Syntax error.", error.message);
  root = ParseJSON("1.e10", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 3, Syntax error.", error.message);

  // Exponent must have a digit following the 'e'.
  root = ParseJSON("1e", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 3, Syntax error.", error.message);
  root = ParseJSON("1E", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 3, Syntax error.", error.message);
  root = ParseJSON("1e1.", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Unexpected data after root element.",
            error.message);
  root = ParseJSON("1e1.0", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Unexpected data after root element.",
            error.message);

  // INF/-INF/NaN are not valid
  root = ParseJSON("NaN", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1, Syntax error.", error.message);
  root = ParseJSON("nan", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1, Syntax error.", error.message);
  root = ParseJSON("inf", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1, Syntax error.", error.message);

  // Invalid number formats
  root = ParseJSON("4.3.1", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Unexpected data after root element.",
            error.message);
  root = ParseJSON("4e3.1", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Unexpected data after root element.",
            error.message);

  // Test string parser
  root = ParseJSON("\"hello world\"");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeString, root->GetType());
  EXPECT_TRUE(root->AsString(&str_val));
  EXPECT_EQ("hello world", str_val);

  // Empty string
  root = ParseJSON("\"\"");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeString, root->GetType());
  EXPECT_TRUE(root->AsString(&str_val));
  EXPECT_EQ("", str_val);

  // Test basic string escapes
  root = ParseJSON("\" \\\"\\\\\\/\\b\\f\\n\\r\\t\\v\"");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeString, root->GetType());
  EXPECT_TRUE(root->AsString(&str_val));
  EXPECT_EQ(" \"\\/\b\f\n\r\t\v", str_val);

  // Test hex and unicode escapes including the null character.
  root = ParseJSON("\"\\x41\\x00\\u1234\"", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Invalid escape sequence.", error.message);

  // Test invalid strings
  root = ParseJSON("\"no closing quote", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 18, Syntax error.", error.message);
  root = ParseJSON("\"\\z invalid escape char\"", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Invalid escape sequence.", error.message);
  root = ParseJSON("\"not enough escape chars\\u123\"", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 27, Invalid escape sequence.", error.message);
  root = ParseJSON("\"extra backslash at end of input\\\"", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 35, Syntax error.", error.message);
  root = ParseJSON("\"a\"extra data", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Unexpected data after root element.",
            error.message);

  // Bare control characters (including newlines) are not permitted in string
  // literals.
  root = ParseJSON("\"\n\"", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 3, Syntax error.", error.message);
  root = ParseJSON("[\"\n\"]", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Syntax error.", error.message);
  root = ParseJSON("{\"\n\": true}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Syntax error.", error.message);
  root = ParseJSON("{\"key\": \"\n\"}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 11, Syntax error.", error.message);
  root = ParseJSON("\"\x1b\"", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 3, Syntax error.", error.message);
  root = ParseJSON("[\"\x07\"]", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Syntax error.", error.message);
  root = ParseJSON("{\"\x09\": true}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Syntax error.", error.message);
  root = ParseJSON("{\"key\": \"\x01\"}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 11, Syntax error.", error.message);

  // Basic array
  root = ParseJSON("[true, false, null]");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeArray, root->GetType());
  list = JSONArray::Cast(root.get());
  ASSERT_TRUE(list);
  EXPECT_EQ(3U, list->size());

  // Empty array
  root = ParseJSON("[]");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeArray, root->GetType());
  list = JSONArray::Cast(root.get());
  ASSERT_TRUE(list);
  EXPECT_EQ(0U, list->size());

  // Nested arrays
  root = ParseJSON("[[true], [], [false, [], [null]], null]");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeArray, root->GetType());
  list = JSONArray::Cast(root.get());
  ASSERT_TRUE(list);
  EXPECT_EQ(4U, list->size());

  // Invalid, missing close brace.
  root = ParseJSON("[[true], [], [false, [], [null]], null", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 39, Syntax error.", error.message);

  // Invalid, too many commas
  root = ParseJSON("[true,, null]", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 7, Unexpected token.", error.message);

  // Invalid, no commas
  root = ParseJSON("[true null]", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 7, Unexpected token.", error.message);

  // Invalid, trailing comma
  root = ParseJSON("[true,]", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 7, Unexpected token.", error.message);

  root = ParseJSON("[true]");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeArray, root->GetType());
  list = JSONArray::Cast(root.get());
  ASSERT_TRUE(list);
  EXPECT_EQ(1U, list->size());
  tmp_value = list->at(0);
  ASSERT_TRUE(tmp_value);
  EXPECT_EQ(JSONValue::kTypeBoolean, tmp_value->GetType());
  bool bool_value = false;
  EXPECT_TRUE(tmp_value->AsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);

  // Don't allow empty elements.
  root = ParseJSON("[,]", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 2, Unexpected token.", error.message);
  root = ParseJSON("[true,,]", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 7, Unexpected token.", error.message);
  root = ParseJSON("[,true,]", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 2, Unexpected token.", error.message);
  root = ParseJSON("[true,,false]", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 7, Unexpected token.", error.message);

  // Test objects
  root = ParseJSON("{}");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeObject, root->GetType());

  root = ParseJSON("{\"number\":9.87654321, \"null\":null , \"S\" : \"str\" }");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeObject, root->GetType());
  JSONObject* object_val = JSONObject::Cast(root.get());
  ASSERT_TRUE(object_val);
  double_val = 0.0;
  EXPECT_TRUE(object_val->GetDouble("number", &double_val));
  EXPECT_DOUBLE_EQ(9.87654321, double_val);
  JSONValue* null_val = object_val->Get("null");
  ASSERT_TRUE(null_val);
  EXPECT_EQ(JSONValue::kTypeNull, null_val->GetType());
  EXPECT_TRUE(object_val->GetString("S", &str_val));
  EXPECT_EQ("str", str_val);

  // Test newline equivalence.
  root2 = ParseJSON(
      "{\n"
      "  \"number\":9.87654321,\n"
      "  \"null\":null,\n"
      "  \"S\":\"str\"\n"
      "}\n");
  ASSERT_TRUE(root2.get());
  EXPECT_EQ(root->ToJSONString(), root2->ToJSONString());

  root2 = ParseJSON(
      "{\r\n"
      "  \"number\":9.87654321,\r\n"
      "  \"null\":null,\r\n"
      "  \"S\":\"str\"\r\n"
      "}\r\n");
  ASSERT_TRUE(root2.get());
  EXPECT_EQ(root->ToJSONString(), root2->ToJSONString());

  // Test that allowed whitespace is limited to TAB, CR, LF and SP. There are
  // several other Unicode characters defined as whitespace, so a selection of
  // them are tested to ensure that they are not allowed.
  // U+0009 CHARACTER TABULATION is allowed
  root = ParseJSON("\t{\t\"key\"\t:\t[\t\"value1\"\t,\t\"value2\"\t]\t}\t");
  ASSERT_TRUE(root.get());
  // U+000A LINE FEED is allowed
  root = ParseJSON("\n{\n\"key\"\n:\n[\n\"value1\"\n,\n\"value2\"\n]\n}\n");
  ASSERT_TRUE(root.get());
  // U+000D CARRIAGE RETURN is allowed
  root = ParseJSON("\r{\r\"key\"\r:\r[\r\"value1\"\r,\r\"value2\"\r]\r}\r");
  ASSERT_TRUE(root.get());
  // U+0020 SPACE is allowed
  root = ParseJSON(" { \"key\" : [ \"value1\" , \"value2\" ] } ");
  ASSERT_TRUE(root.get());
  // U+000B LINE TABULATION is not allowed
  root = ParseJSON("[\x0b\"value\"]");
  ASSERT_FALSE(root.get());
  // U+00A0 NO-BREAK SPACE is not allowed
  UChar invalid_space_1[] = {0x5b, 0x00a0, 0x5d};  // [<U+00A0>]
  root = ParseJSON(String(invalid_space_1, base::size(invalid_space_1)));
  ASSERT_FALSE(root.get());
  // U+3000 IDEOGRAPHIC SPACE is not allowed
  UChar invalid_space_2[] = {0x5b, 0x3000, 0x5d};  // [<U+3000>]
  root = ParseJSON(String(invalid_space_2, base::size(invalid_space_2)));
  ASSERT_FALSE(root.get());

  // Test nesting
  root = ParseJSON("{\"inner\":{\"array\":[true]},\"false\":false,\"d\":{}}");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeObject, root->GetType());
  object_val = JSONObject::Cast(root.get());
  ASSERT_TRUE(object_val);
  JSONObject* inner_object = object_val->GetJSONObject("inner");
  ASSERT_TRUE(inner_object);
  JSONArray* inner_array = inner_object->GetArray("array");
  ASSERT_TRUE(inner_array);
  EXPECT_EQ(1U, inner_array->size());
  bool_value = true;
  EXPECT_TRUE(object_val->GetBoolean("false", &bool_value));
  EXPECT_FALSE(bool_value);
  inner_object = object_val->GetJSONObject("d");
  EXPECT_TRUE(inner_object);

  // Test keys with periods
  root = ParseJSON("{\"a.b\":3,\"c\":2,\"d.e.f\":{\"g.h.i.j\":1}}");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeObject, root->GetType());
  object_val = JSONObject::Cast(root.get());
  ASSERT_TRUE(object_val);
  int integer_value = 0;
  EXPECT_TRUE(object_val->GetInteger("a.b", &integer_value));
  EXPECT_EQ(3, integer_value);
  EXPECT_TRUE(object_val->GetInteger("c", &integer_value));
  EXPECT_EQ(2, integer_value);
  inner_object = object_val->GetJSONObject("d.e.f");
  ASSERT_TRUE(inner_object);
  EXPECT_EQ(1U, inner_object->size());
  EXPECT_TRUE(inner_object->GetInteger("g.h.i.j", &integer_value));
  EXPECT_EQ(1, integer_value);

  root = ParseJSON("{\"a\":{\"b\":2},\"a.b\":1}");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeObject, root->GetType());
  object_val = JSONObject::Cast(root.get());
  ASSERT_TRUE(object_val);
  inner_object = object_val->GetJSONObject("a");
  ASSERT_TRUE(inner_object);
  EXPECT_TRUE(inner_object->GetInteger("b", &integer_value));
  EXPECT_EQ(2, integer_value);
  EXPECT_TRUE(object_val->GetInteger("a.b", &integer_value));
  EXPECT_EQ(1, integer_value);

  // Invalid, no closing brace
  root = ParseJSON("{\"a\": true");
  EXPECT_FALSE(root.get());

  // Invalid, keys must be quoted
  root = ParseJSON("{foo:true}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 2, Syntax error.", error.message);

  // Invalid, trailing comma
  root = ParseJSON("{\"a\":true,}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 11, Unexpected token.", error.message);

  // Invalid, too many commas
  root = ParseJSON("{\"a\":true,,\"b\":false}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 11, Unexpected token.", error.message);

  // Invalid, no separator
  root = ParseJSON("{\"a\" \"b\"}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 6, Unexpected token.", error.message);

  // Invalid, lone comma.
  root = ParseJSON("{,}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 2, Unexpected token.", error.message);
  root = ParseJSON("{\"a\":true,,}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 11, Unexpected token.", error.message);
  root = ParseJSON("{,\"a\":true}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 2, Unexpected token.", error.message);
  root = ParseJSON("{\"a\":true,,\"b\":false}", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 11, Unexpected token.", error.message);

  // Test stack overflow
  StringBuilder evil;
  evil.ReserveCapacity(2000000);
  for (int i = 0; i < 1000000; ++i)
    evil.Append('[');
  for (int i = 0; i < 1000000; ++i)
    evil.Append(']');
  root = ParseJSON(evil.ToString(), &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1001, Too much nesting.", error.message);

  // A few thousand adjacent lists is fine.
  StringBuilder not_evil;
  not_evil.ReserveCapacity(15010);
  not_evil.Append('[');
  for (int i = 0; i < 5000; ++i)
    not_evil.Append("[],");
  not_evil.Append("[]]");
  root = ParseJSON(not_evil.ToString());
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeArray, root->GetType());
  list = JSONArray::Cast(root.get());
  ASSERT_TRUE(list);
  EXPECT_EQ(5001U, list->size());

  // Test utf8 encoded input
  root = ParseJSON("\"\\xe7\\xbd\\x91\\xe9\\xa1\\xb5\"", &error);
  ASSERT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 4, Invalid escape sequence.", error.message);

  // Test utf16 encoded strings.
  root = ParseJSON("\"\\u20ac3,14\"");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeString, root->GetType());
  EXPECT_TRUE(root->AsString(&str_val));
  UChar tmp2[] = {0x20ac, 0x33, 0x2c, 0x31, 0x34};
  EXPECT_EQ(String(tmp2, base::size(tmp2)), str_val);

  root = ParseJSON("\"\\ud83d\\udca9\\ud83d\\udc6c\"");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeString, root->GetType());
  EXPECT_TRUE(root->AsString(&str_val));
  UChar tmp3[] = {0xd83d, 0xdca9, 0xd83d, 0xdc6c};
  EXPECT_EQ(String(tmp3, base::size(tmp3)), str_val);

  // Invalid unicode in a string literal after applying escape sequences.
  root = ParseJSON("\n\n    \"\\ud800\"", &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ(
      "Line: 3, column: 5, Unsupported encoding. JSON and all string literals "
      "must contain valid Unicode characters.",
      error.message);

  // Invalid unicode in a JSON itself.
  UChar tmp4[] = {0x22, 0xd800, 0x22};  // "?"
  root = ParseJSON(String(tmp4, base::size(tmp4)), &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ(
      "Line: 1, column: 1, Unsupported encoding. JSON and all string literals "
      "must contain valid Unicode characters.",
      error.message);

  // Invalid unicode in a JSON itself.
  UChar tmp5[] = {0x7b, 0x22, 0xd800, 0x22, 0x3a, 0x31, 0x7d};  // {"?":1}
  root = ParseJSON(String(tmp5, base::size(tmp5)), &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ(
      "Line: 1, column: 2, Unsupported encoding. JSON and all string literals "
      "must contain valid Unicode characters.",
      error.message);

  // Test literal root objects.
  root = ParseJSON("null");
  ASSERT_TRUE(root.get());
  EXPECT_EQ(JSONValue::kTypeNull, root->GetType());

  root = ParseJSON("true");
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->AsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);

  root = ParseJSON("10");
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->AsInteger(&integer_value));
  EXPECT_EQ(10, integer_value);

  root = ParseJSON("\"root\"");
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->AsString(&str_val));
  EXPECT_EQ("root", str_val);
}

TEST(JSONParserTest, InvalidSanity) {
  const char* const kInvalidJson[] = {
      "/* test *", "{\"foo\"", "{\"foo\":", "  [", "\"\\u123g\"", "{\n\"eh:\n}",
      "////",      "*/**/",    "/**/",      "/*/", "//**/",       "\"\\"};

  for (size_t i = 0; i < base::size(kInvalidJson); ++i) {
    std::unique_ptr<JSONValue> result = ParseJSON(kInvalidJson[i]);
    EXPECT_FALSE(result.get());
  }
}

// Test that the nesting depth can be limited to values less than 1000, but
// cannot be extended past that maximum.
TEST(JSONParserTest, LimitedDepth) {
  std::unique_ptr<JSONValue> root;
  JSONParseError error;

  // Test cases. Each pair is a JSON string, and the minimum depth required
  // to successfully parse that string.
  Vector<std::pair<const char*, int>> test_cases = {
      {"[[[[[]]]]]", 5},
      {"[[[[[\"a\"]]]]]", 6},
      {"[[],[],[],[],[]]", 2},
      {"{\"a\":{\"a\":{\"a\":{\"a\":{\"a\": \"a\"}}}}}", 6},
      {"\"root\"", 1}};

  for (const auto& test_case : test_cases) {
    // Each test case should parse successfully at the default depth
    root = ParseJSON(test_case.first);
    EXPECT_TRUE(root.get());

    // ... and should parse successfully at the minimum depth
    root = ParseJSON(test_case.first, test_case.second);
    EXPECT_TRUE(root.get());

    // ... but should fail to parse at a shallower depth.
    root = ParseJSON(test_case.first, test_case.second - 1);
    EXPECT_FALSE(root.get());
  }

  // Test that everything fails to parse with depth 0
  root = ParseJSON("", 0, &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1, Syntax error.", error.message);
  root = ParseJSON("", -1, &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1, Syntax error.", error.message);
  root = ParseJSON("true", 0, &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1, Too much nesting.", error.message);

  // Test that the limit can be set to the constant maximum.
  StringBuilder evil;
  evil.ReserveCapacity(2002);
  for (int i = 0; i < 1000; ++i)
    evil.Append('[');
  for (int i = 0; i < 1000; ++i)
    evil.Append(']');
  root = ParseJSON(evil.ToString());
  EXPECT_TRUE(root.get());
  root = ParseJSON(evil.ToString(), 1000);
  EXPECT_TRUE(root.get());

  // Test that the limit cannot be set higher than the constant maximum.
  evil.Clear();
  for (int i = 0; i < 1001; ++i)
    evil.Append('[');
  for (int i = 0; i < 1001; ++i)
    evil.Append(']');
  root = ParseJSON(evil.ToString(), &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1001, Too much nesting.", error.message);
  root = ParseJSON(evil.ToString(), 1001, &error);
  EXPECT_FALSE(root.get());
  EXPECT_EQ("Line: 1, column: 1001, Too much nesting.", error.message);
}

}  // namespace blink
