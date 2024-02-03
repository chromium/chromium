// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "services/data_decoder/cbor_parser_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

namespace {
void CopyResultCallback(std::optional<::base::Value>& output_result,
                        std::optional<std::string>& output_error,
                        std::optional<::base::Value> result,
                        const std::optional<std::string>& error) {
  output_result = std::move(result);
  output_error = error;
}

}  // namespace

using CborToValueTest = testing::Test;

TEST_F(CborToValueTest, SuccesfulParseValues) {
  struct {
    std::string name;
    std::vector<uint8_t> input;
    base::Value expected_result;
  } test_cases[] = {
      {
          "Unsigned",
          {0x18, 0x64},  // 100
          base::Value(100),
      },
      {
          "Negative",
          {0x38, 0x63},  //-100
          base::Value(-100),
      },
      {
          "Float",
          {0xf9, 0x00, 0x00},  // 0.0
          base::Value(0.0),
      },
      {
          "Array",  // [100, false, "string", {"k": "v"}]
          {0x84, 0x18, 0x64, 0xF4, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67,
           0xA1, 0x61, 0x6B, 0x61, 0x76},
          base::Value(base::Value::List()
                          .Append(100)
                          .Append(false)
                          .Append("string")
                          .Append(base::Value::Dict().Set("k", "v"))),
      },
      {"Map",  // {"bool": true, "array": [100], "number": 100}
       {0xA3, 0x64, 0x62, 0x6F, 0x6F, 0x6C, 0xF5, 0x65, 0x61,
        0x72, 0x72, 0x61, 0x79, 0x81, 0x18, 0x64, 0x66, 0x6E,
        0x75, 0x6D, 0x62, 0x65, 0x72, 0x18, 0x64},
       base::Value(base::Value::Dict()
                       .Set("bool", true)
                       .Set("array", base::Value::List().Append(100))
                       .Set("number", 100))},
      {"SimpleBooleanTrue", {0xF5}, base::Value(true)},
      {"SimpleBooleanFalse", {0xF4}, base::Value(false)},
      {"String", {0x64, 0x63, 0x62, 0x6F, 0x72}, base::Value("cbor")},
      {"ByteString",
       {0x44, 0x63, 0x62, 0x6f, 0x72},
       base::Value(cbor::Value::BinaryValue({0x63, 0x62, 0x6f, 0x72}))}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    CborParserImpl parser;
    std::optional<base::Value> result;
    std::optional<std::string> error;

    parser.Parse(
        test_case.input,
        base::BindOnce(&CopyResultCallback, std::ref(result), std::ref(error)));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), test_case.expected_result);
    EXPECT_FALSE(error.has_value());
  }
}

TEST_F(CborToValueTest, FailingParseValues) {
  const std::string invalid_error = "Error unexpected CBOR value.";
  struct {
    std::string name;
    std::vector<uint8_t> input;
    std::string expected_error;
  } test_cases[] = {
      {"Null", {0xF6}, invalid_error},

      {"InvalidMapKeyType",  // {100: "100", "k": "v"} - 100 is an invalid key
       {0xA2, 0x18, 0x64, 0x63, 0x31, 0x30, 0x30, 0x61, 0x6B, 0x61, 0x76},
       invalid_error},

      {"NestedInvalidKey",  // [{100: "100", "k": "v"}] - 100 is an invalid key
       {0x81, 0xA2, 0x18, 0x64, 0x63, 0x31, 0x30, 0x30, 0x61, 0x6B, 0x61, 0x76},
       invalid_error},

      {"DuplicateMapKeys",  //  {"k": "v1", "k": "v2"}
       {0xA2, 0x61, 0x6B, 0x62, 0x76, 0x31, 0x61, 0x6B, 0x62, 0x76, 0x32},
       cbor::Reader::ErrorCodeToString(
           cbor::Reader::DecoderError::DUPLICATE_KEY)}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    CborParserImpl parser;

    std::optional<base::Value> result;
    std::optional<std::string> error;

    parser.Parse(
        test_case.input,
        base::BindOnce(&CopyResultCallback, std::ref(result), std::ref(error)));

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error.value(), test_case.expected_error);
    EXPECT_FALSE(result.has_value());
  }
}

}  // namespace data_decoder
